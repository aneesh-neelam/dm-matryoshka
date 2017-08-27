#include <linux/device-mapper.h>
#include <linux/bio.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>

#include "../include/integrity.h"
#include "../include/target.h"
#include "../include/fs_fat.h"
#include "../include/utility.h"

#include "../lib/jerasure/jerasure.h"
#include "../lib/jerasure/reed_sol.h"


struct metadata_io* alloc_metadata_io(struct matryoshka_context *mc, struct matryoshka_io *io) {
  struct metadata_io *mio;

  int i;

  mio = kmalloc(sizeof(struct metadata_io), GFP_NOIO);

  mio->io = io;

  mutex_init(&(mio->lock));

  mio->base_bio = matryoshka_alloc_bio(mc, mc->cluster_size);

  for (i = 0; i < mc->num_carrier; ++i) {
    mio->carrier_bios[i] = matryoshka_alloc_bio(mc, mc->cluster_size);
  }
  for (i = 0; i < mc->num_entropy; ++i) {
    mio->entropy_bios[i] = matryoshka_alloc_bio(mc, mc->cluster_size);
  }

  mio->erasures = (int *)kmalloc(sizeof(int) * (1 + mc->num_entropy + mc->num_carrier), GFP_KERNEL);
  mio->erasures[0] = 1;
  mio->erasures[1] = -1;

  return mio;
}

void free_metadata_io(struct matryoshka_context *mc, struct metadata_io *mio) {
  int i;

  matryoshka_free_buffer_pages(mc, mio->base_bio);
  bio_put(mio->base_bio);

  for (i = 0; i < mc->num_carrier; ++i) {
    matryoshka_free_buffer_pages(mc, mio->carrier_bios[i]);
    bio_put(mio->carrier_bios[i]);
  }

  for (i = 0; i < mc->num_entropy; ++i) {
    matryoshka_free_buffer_pages(mc, mio->entropy_bios[i]);
    bio_put(mio->entropy_bios[i]);
  }

  kfree(mio->erasures);
  kfree(mio);
}

void mio_update_erasures(struct matryoshka_context *mc, struct metadata_io *mio, int index) {
  int last = 0;
  int i;
  
  mutex_lock(&mio->lock);

  for (i = 0; i < 1 + mc->num_entropy + mc->num_carrier; ++i) {
    if (mio->erasures[i] == -1) {
      last = i;
      break;
    }
  }

  if (last != 1 + mc->num_entropy + mc->num_carrier) {
    mio->erasures[last] = index;
    mio->erasures[last + 1] = -1;
  }

  mutex_unlock(&mio->lock);
}

void init_metadata_bios(struct matryoshka_context *mc, struct metadata_io *mio) {
  sector_t generated_sector;
  int i;
  int skip;

  for (i = 0; i < mc->num_carrier; ++i) {
    skip = -1;
    
    bio_map_dev(mio->carrier_bios[i], mc->carrier);
    bio_map_operation(mio->entropy_bios[i], READ);

    do {
      if (skip == -1) {
        skip = 0;
      } else {
        skip++;
      }
      if (skip > 10) {
        mio_update_erasures(mc, mio, i);
        break;
      }
      generated_sector = get_sector_in_sequence(mc->passphrase, mio->logical_sector, i + skip, mc->cluster_count * mc->sectors_per_cluster);
    } while (fat_is_cluster_used(mc->fs, generated_sector / mc->sectors_per_cluster));
    if (!mio->error) {
      bio_map_sector(mio->entropy_bios[i], generated_sector);
    }
  }

  for (i = 0; i < mc->num_entropy; ++i) {
    generated_sector = get_sector_in_sequence(mc->passphrase, mio->logical_sector, i, mc->entropy_max_sectors);

    bio_map_operation(mio->entropy_bios[i], READ);
    bio_map_dev(mio->entropy_bios[i], mc->entropy);
    bio_map_sector(mio->entropy_bios[i], generated_sector);
  }

  mio->erasures = (int *)kmalloc(sizeof(int) * (1 + mc->num_entropy + mc->num_carrier), GFP_KERNEL);
  mio->erasures[0] = 1;
  mio->erasures[1] = -1;
}

void init_metadata_bvec(struct matryoshka_context *mc, struct metadata_io *mio) {
  int i;

  if (mio->base_bio) {
    mio->iter_base = mio->base_bio->bi_iter;
  }
  for (i = 0; i < mc->num_entropy; ++i) {
    if (mio->entropy_bios[i]) {
      mio->iter_entropy[i] = mio->entropy_bios[i]->bi_iter;
    }
  }
  for (i = 0; i < mc->num_carrier; ++i) {
    if (mio->carrier_bios[i]) {
      mio->iter_carrier[i] = mio->carrier_bios[i]->bi_iter;
    }
  }
}

int metadata_erasure_decode(struct matryoshka_context *mc, struct metadata_io *mio) {
  int status = 0;

  int *matrix = NULL;

  int k = mc->num_entropy + 1;
  int m = mc->num_carrier;

  int blocksize;

  struct bio_vec data_vecs[k];
  struct bio_vec coding_vecs[m];

  char *data[k];
  char *coding[m];

  int w = WORD_SIZE;
  int *erasures = mio->erasures;

  int i;
  
  matrix = reed_sol_vandermonde_coding_matrix(k, m, w);

  init_metadata_bvec(mc, mio);

  while (1) {
    if (!mio->iter_base.bi_size) {
      break;
    }
    for (i = 0; i < mc->num_entropy; ++i) {
      if (!mio->iter_entropy[i].bi_size) {
        break;
      }
    }
    for (i = 0; i < mc->num_carrier; ++i) {
      if (!mio->iter_carrier[i].bi_size) {
        break;
      }
    }

    data_vecs[0] = bio_iter_iovec(mio->base_bio, mio->iter_base);
    data[0] = page_address(data_vecs[0].bv_page);
    blocksize = mc->sector_size;
    for (i = 1; i < k; ++i) {
      data_vecs[i] = bio_iter_iovec(mio->entropy_bios[i-1], mio->iter_entropy[i-1]);
      data[i] = page_address(data_vecs[i].bv_page);
    }
    for (i = 0; i < m; ++i) {
      coding_vecs[i] = bio_iter_iovec(mio->carrier_bios[i], mio->iter_carrier[i]);
      coding[i] = page_address(coding_vecs[i].bv_page);
    }

    status = jerasure_matrix_decode(k, m, w, matrix, ROW_K_ONES, erasures, data, coding, blocksize);

    bio_advance_iter(mio->base_bio, &mio->iter_base, mc->sector_size);
    for (i = 0; i < mc->num_entropy; ++i) {
      bio_advance_iter(mio->entropy_bios[i], &mio->iter_entropy[i], mc->sector_size);
    }
    for (i = 0; i < mc->num_carrier; ++i) {
      bio_advance_iter(mio->carrier_bios[i], &mio->iter_carrier[i], mc->sector_size);
    }
  }

  return status;
}

void metadata_erasure_encode(struct matryoshka_context *mc, struct metadata_io *mio) {
  int *matrix = NULL;
  
  int k = mc->num_entropy + 1;
  int m = mc->num_carrier;

  int blocksize;

  struct bio_vec data_vecs[k];
  struct bio_vec coding_vecs[m];

  char *data[k];
  char *coding[m];
  
  int w = WORD_SIZE;

  int i;
    
  matrix = reed_sol_vandermonde_coding_matrix(k, m, w);

  init_metadata_bvec(mc, mio);

  while (1) {
    if (!mio->iter_base.bi_size) {
      break;
    }
    for (i = 0; i < mc->num_entropy; ++i) {
      if (!mio->iter_entropy[i].bi_size) {
        break;
      }
    }
    for (i = 0; i < mc->num_carrier; ++i) {
      if (!mio->iter_carrier[i].bi_size) {
        break;
      }
    }

    data_vecs[0] = bio_iter_iovec(mio->base_bio, mio->iter_base);
    data[0] = page_address(data_vecs[0].bv_page);
    blocksize = mc->sector_size;
    for (i = 1; i < k; ++i) {
      data_vecs[i] = bio_iter_iovec(mio->entropy_bios[i-1], mio->iter_entropy[i-1]);
      data[i] = page_address(data_vecs[i].bv_page);
    }
    for (i = 0; i < m; ++i) {
      coding_vecs[i] = bio_iter_iovec(mio->carrier_bios[i], mio->iter_carrier[i]);
      coding[i] = page_address(coding_vecs[i].bv_page);
    }

    jerasure_matrix_encode(k, m, w, matrix, data, coding, blocksize);

    bio_advance_iter(mio->base_bio, &mio->iter_base, mc->sector_size);
    for (i = 0; i < mc->num_entropy; ++i) {
      bio_advance_iter(mio->entropy_bios[i], &mio->iter_entropy[i], mc->sector_size);
    }
    for (i = 0; i < mc->num_carrier; ++i) {
      bio_advance_iter(mio->carrier_bios[i], &mio->iter_carrier[i], mc->sector_size);
    }
  }
}

char* metadata_parse_bio(struct matryoshka_context *mc, struct bio *base_bio) {
  char *metadata = kmalloc(mc->cluster_size, GFP_KERNEL);
  char *buffer;
  int seek;

  struct bvec_iter iter_base;
  struct bio_vec base_vec;

  iter_base = base_bio->bi_iter;
  while (1) {
    if (iter_base.bi_size) {
      break;
    }

    base_vec = bio_iter_iovec(base_bio, iter_base);
    buffer = page_address(base_vec.bv_page);

    memcpy(metadata + seek, buffer, base_vec.bv_len);

    seek = seek + base_vec.bv_len;
    bio_advance_iter(base_bio, &iter_base, base_vec.bv_len);
  }

  return metadata;
}

void metadata_update_bio(struct matryoshka_context *mc, struct bio *base_bio, char *metadata) {
  char *buffer;
  int seek;

  struct bvec_iter iter_base;
  struct bio_vec base_vec;

  iter_base = base_bio->bi_iter;
  while (1) {
    if (iter_base.bi_size) {
      break;
    }

    base_vec = bio_iter_iovec(base_bio, iter_base);
    buffer = page_address(base_vec.bv_page);

    memcpy(buffer, metadata + seek, base_vec.bv_len);

    seek = seek + base_vec.bv_len;
    bio_advance_iter(base_bio, &iter_base, base_vec.bv_len);
  }
}

int metadata_verify(char *metadata, __kernel_size_t size) {
  char *checksum;
  u32 parsed_crc;
  u32 computed_crc;

  checksum = kmalloc(sizeof(u32), GFP_KERNEL);

  memcpy(checksum, metadata, sizeof(u32));

  parsed_crc = *((u32*) checksum);
  computed_crc = crc32le(metadata + sizeof(u32), size - sizeof(u32));

  kfree(checksum);

  if (parsed_crc == computed_crc) {
    return 0;
  }
  return 1;
}

int metadata_update_checksum(char *metadata, __kernel_size_t size) {
  char *checksum;
  u32 computed_crc;

  checksum = kmalloc(sizeof(u32), GFP_KERNEL);

  computed_crc = crc32le(metadata + sizeof(u32), size - sizeof(u32));

  checksum = ((char *)&computed_crc);
  memcpy(metadata, checksum, sizeof(u32));

  kfree(checksum);

  return 0;
}

char *metadata_init(__kernel_size_t size) {
  char *metadata;
  u32 computed_crc;
  char *checksum;

  metadata = kmalloc(size, GFP_KERNEL);

  memset(metadata, 1, size);

  computed_crc = crc32le(metadata + sizeof(u32), size - sizeof(u32));
  checksum = ((char*) &computed_crc);

  memcpy(metadata, checksum, sizeof(u32));

  return metadata;
}

int metadata_update(struct matryoshka_context *mc, char *metadata, __kernel_size_t size, struct bio *bio) {
  int jump = sizeof(u32);
  int seek = jump;
  int next_present = -1;

  sector_t sector;
  u32 computed_crc;

  char *buffer;
  char *tmp_buf;

  buffer = metadata_parse_bio(mc, bio);
  computed_crc = crc32le(buffer, bio->bi_iter.bi_size);

  tmp_buf = kmalloc(jump, GFP_KERNEL);

  memcpy(tmp_buf, metadata + seek, jump);
  sector = *((u32*) tmp_buf);
  if (UINT_MAX != sector) {
    next_present = 1;
  }

  seek = seek + jump;

  while (1) {
    if (seek >= size) {
      break;
    }
    memcpy(tmp_buf, metadata + seek, jump);
    sector = *((u32*) tmp_buf);
    seek = seek + jump;

    if (sector == bio->bi_iter.bi_sector) {
      tmp_buf = ((char *)&computed_crc);
      memcpy(tmp_buf, metadata + seek, jump);

      computed_crc = crc32le(metadata + sizeof(u32), size - sizeof(u32));
      tmp_buf = ((char *)&computed_crc);
      memcpy(metadata, tmp_buf, jump);

      kfree(tmp_buf);
      return 0;
    } else {
      seek = seek + jump;
    }
  }
  kfree(tmp_buf);

  return next_present;
}

int metadata_check(struct matryoshka_context *mc, char *metadata, __kernel_size_t size, struct bio *bio) {
  int jump = sizeof(u32);
  int seek = jump;
  int next_present = -1;

  sector_t sector;
  u32 parsed_crc;
  u32 computed_crc;

  char* buffer;
  char* tmp_buf;

  buffer = metadata_parse_bio(mc, bio);
  computed_crc = crc32le(buffer, bio->bi_iter.bi_size);

  tmp_buf = kmalloc(jump, GFP_KERNEL);

  memcpy(tmp_buf, metadata + seek, jump);
  sector = *((u32*) tmp_buf);
  if (UINT_MAX != sector) {
    next_present = 1;
  }

  seek = seek + jump;

  while (1) {
    if (seek >= size) {
      break;
    }
    memcpy(tmp_buf, metadata + seek, jump);
    sector = *((u32*) tmp_buf);
    seek = seek + jump;
    memcpy(tmp_buf, metadata + seek, jump);
    parsed_crc = *((u32*) tmp_buf);
    seek = seek + jump;

    if (sector == bio->bi_iter.bi_sector) {
      kfree(tmp_buf);
      if (computed_crc == parsed_crc) {
        return 0;
      } else {
        return next_present;
      }
    }
  }
  kfree(tmp_buf);

  return next_present;
}

sector_t metadata_next_in_list(char *metadata, __kernel_size_t size) {
  char *sector_bytes;
  sector_t sector;
  int seek = sizeof(u32);

  sector_bytes = kmalloc(sizeof(u32), GFP_KERNEL);

  memcpy(sector_bytes, metadata + seek, sizeof(u32));
  sector = *((u32 *)sector_bytes);

  kfree(sector_bytes);

  return sector;
}

int metadata_update_link(struct matryoshka_context *mc, struct metadata_io *mio, char *metadata, sector_t next) {
  char *sector_bytes;
  int seek = sizeof(u32);
  int i;
  int ret;

  sector_bytes = kmalloc(sizeof(u32), GFP_KERNEL);

  sector_bytes = ((char *)&next);
  memcpy(metadata + seek, sector_bytes, sizeof(u32));

  kfree(sector_bytes);

  metadata_update_bio(mc, mio->base_bio, metadata);

  metadata_erasure_encode(mc, mio);

  for (i = 0; i < mc->num_carrier; ++i) {
    bio_map_operation(mio->carrier_bios[i], WRITE);

    ret = submit_bio_wait(mio->carrier_bios[i]);
    if (!mio->error && ret) {
      mio->error = ret;
    }
  }

  if (mio->error) {
    return mio->error;
  }
  return 0;
}

int matryoshka_bio_integrity_check(struct matryoshka_context *mc, struct matryoshka_io *io, struct bio *bio) {
  int i, j;
  int ret;
  int error = 0;
  int reached_end_of_list = 0;

  char *metadata;

  struct metadata_io *mio = alloc_metadata_io(mc, io);
  mio->logical_sector = mc->metadata_logical_sector;
  do {
    init_metadata_bios(mc, mio);
    if (mio->error) {
      error = mio->error;
      free_metadata_io(mc, mio);
      return error;
    }
    
    printk(KERN_DEBUG "dm-matryoshka: matryoshka_bio_integrity_check(), after init_bios");

    for (i = 0; i < mc->num_entropy; ++i) {
      ret = submit_bio_wait(mio->entropy_bios[i]);
      if (ret) {
        error = ret;
        free_metadata_io(mc, mio);
        return error;
      }
    }

    for (i = 0; i < mc->num_carrier; ++i) {
      for (j = i + mc->num_entropy; j < mc->num_carrier + mc->num_entropy + 1; ++j) {
        mio_update_erasures(mc, mio, j);
      }

      ret = submit_bio_wait(mio->carrier_bios[i]);
      if (ret) {
        error = ret;
        free_metadata_io(mc, mio);
        return error;
      }
      
      mio->error = metadata_erasure_decode(mc, mio);
      if (mio->error) {
        mio_update_erasures(mc, mio, i);
      } else {
        break;
      }
    }

    metadata = metadata_parse_bio(mc, mio->base_bio);
    if (metadata_verify(metadata, mc->cluster_size)) {
      error = -EIO;
      free_metadata_io(mc, mio);
      return error;
    }

    mio->error = metadata_check(mc, metadata, mc->cluster_size, bio);
    if (mio->error == 1)  {
      mio->logical_sector = metadata_next_in_list(metadata, mc->cluster_size);
    }
    else {
      reached_end_of_list = 1;
    }
  } while(!reached_end_of_list);

  if (mio) {
    error = mio->error;
    free_metadata_io(mc, mio);
  }


  return error;
}

int matryoshka_bio_integrity_update(struct matryoshka_context *mc, struct matryoshka_io *io, struct bio *bio) {
  int i, j;
  int ret;
  int error = 0;
  int reached_end_of_list = 0;

  char *metadata;
  u32 next;

  struct metadata_io *mio = alloc_metadata_io(mc, io);
  mio->logical_sector = mc->metadata_logical_sector;
  do {
    init_metadata_bios(mc, mio);
    if (mio->error) {
      error = mio->error;
      free_metadata_io(mc, mio);
      return error;
    }

    for (i = 0; i < mc->num_entropy; ++i) {
      ret = submit_bio_wait(mio->entropy_bios[i]);
      if (ret) {
        error = ret;
        free_metadata_io(mc, mio);
        return error;
      }
    }

    for (i = 0; i < mc->num_carrier; ++i) {
      for (j = i + mc->num_entropy; j < mc->num_carrier + mc->num_entropy + 1; ++j) {
        mio_update_erasures(mc, mio, j);
      }

      ret = submit_bio_wait(mio->carrier_bios[i]);
      if (ret) {
        error = ret;
        free_metadata_io(mc, mio);
        return error;
      }

      mio->error = metadata_erasure_decode(mc, mio);
      if (mio->error) {
        mio_update_erasures(mc, mio, i);
      } else {
        break;
      }
    }

    metadata = metadata_parse_bio(mc, mio->base_bio);
    if (metadata_verify(metadata, mc->cluster_size)) {
      error = -EIO;
      free_metadata_io(mc, mio);
      return error;
    }

    mio->error = metadata_update(mc, metadata, mc->cluster_size, bio);
    if (mio->error == 1) {
      mio->logical_sector = metadata_next_in_list(metadata, mc->cluster_size);
    } else {
      reached_end_of_list = 1;

      if (mio->error == -1) {
        get_32bit_random_number(&next);

        mio->error = metadata_update_link(mc, mio, metadata, next);
        if (mio->error) {
          error = mio->error;
          free_metadata_io(mc, mio);
          return error;
        }

        metadata = metadata_init(mc->cluster_size);
        mio->error = metadata_update(mc, metadata, mc->cluster_size, mio->carrier_bios[i]);
        metadata_update_bio(mc, mio->base_bio, metadata);
        init_metadata_bios(mc, mio);

        for (i = 0; i < mc->num_entropy; ++i) {
          ret = submit_bio_wait(mio->entropy_bios[i]);
          if (ret) {
            error = ret;
            free_metadata_io(mc, mio);
            return error;
          }
        }

        metadata_erasure_encode(mc, mio);

        for (i = 0; i < mc->num_carrier; ++i) {
          bio_map_operation(mio->carrier_bios[i], WRITE);

          ret = submit_bio_wait(mio->carrier_bios[i]);
          if (!mio->error && ret) {
            mio->error = ret;
          }
        }

      } else {
        metadata_update_bio(mc, mio->base_bio, metadata);

        metadata_erasure_encode(mc, mio);

        for (i = 0; i < mc->num_carrier; ++i) {
          bio_map_operation(mio->carrier_bios[i], WRITE);

          ret = submit_bio_wait(mio->carrier_bios[i]);
          if (!mio->error) {
            mio->error = ret;
          }
        }
      }
    }

  } while(!reached_end_of_list);

  if (mio) {
    error = mio->error;
    free_metadata_io(mc, mio);
  }

  return error;
}

int matryoshka_metadata_init(struct matryoshka_context *mc) {
  int i, j;
  int ret;
  struct metadata_io *mio;
  int error = 0;

  char *metadata;

  mio = alloc_metadata_io(mc, NULL);
  mio->logical_sector = mc->metadata_logical_sector;

  init_metadata_bios(mc, mio);
  if (mio->error) {
    error = mio->error;
    free_metadata_io(mc, mio);
    return error;
  }

  for (i = 0; i < mc->num_entropy; ++i) {
    ret = submit_bio_wait(mio->entropy_bios[i]);
    if (ret) {
      error = ret;
      free_metadata_io(mc, mio);
      return error;
    }
  }

  for (i = 0; i < mc->num_carrier; ++i) {
    for (j = i + mc->num_entropy; j < mc->num_carrier + mc->num_entropy + 1; ++j) {
      mio_update_erasures(mc, mio, j);
    }

    printk(KERN_DEBUG "dm-matryoshka: matryoshka_metadata_init(), after updating erasures for Carrier: %d", i);

    ret = submit_bio_wait(mio->carrier_bios[i]);
    if (ret) {
      error = ret;
      free_metadata_io(mc, mio);
      return error;
    }

    printk(KERN_DEBUG "dm-matryoshka: matryoshka_metadata_init(), after Carrier: %d read", i);

    mio->error = metadata_erasure_decode(mc, mio);
    if (mio->error) {
      mio_update_erasures(mc, mio, i);
    } else {
      break;
    }
  }

  printk(KERN_DEBUG "dm-matryoshka: matryoshka_metadata_init(), after carrier read and erasure decoding");

  metadata = metadata_parse_bio(mc, mio->base_bio);
  if (metadata_verify(metadata, mc->cluster_size)) {
    metadata = metadata_init(mc->cluster_size);

    metadata_update_bio(mc, mio->base_bio, metadata);

    metadata_erasure_encode(mc, mio);

    printk(KERN_DEBUG "dm-matryoshka: matryoshka_metadata_init(), after erasure encoding for new metadata block");

    for (i = 0; i < mc->num_carrier; ++i) {
      bio_map_operation(mio->carrier_bios[i], WRITE);

      ret = submit_bio_wait(mio->carrier_bios[i]);
      if (ret) {
        error = ret;
        free_metadata_io(mc, mio);
        return error;
      }
    }
  }

  printk(KERN_DEBUG "dm-matryoshka: matryoshka_metadata_init(), existing metadata block is fine");

  if (mio) {
    error = mio->error;
    free_metadata_io(mc, mio);
  }

  return error;
}
