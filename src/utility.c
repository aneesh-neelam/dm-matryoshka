#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/atomic.h>
#include <linux/crc32.h>
#include <linux/random.h>
#include <crypto/hash.h>
#include <linux/string.h>

#include "../include/target.h"
#include "../include/utility.h"

#include "../lib/jerasure/jerasure.h"
#include "../lib/jerasure/reed_sol.h"


int convertStringToU8(u8* res, char* str) {
  // Parse string to unsigned long int
  unsigned long long tmp;
  char dummy;

  if (sscanf(str, "%llu%c", &tmp, &dummy) != 1) {
    return -1;
  }
  *res = tmp;

  return 0;
}

int convertStringToSector_t(sector_t* res, char* str) {
  // Parse string to unsigned long int
  unsigned long long tmp;
  char dummy;

  if (sscanf(str, "%llu%c", &tmp, &dummy) != 1) {
    return -1;
  }
  *res = tmp;

  return 0;
}

int checkRatio(u8 num_carrier, u8 num_entropy) {
  if (num_entropy + 1 > num_carrier) {
    return 0;
  } else {
    return -1;
  }
}

struct matryoshka_io *init_matryoshka_io(struct matryoshka_context *mc, struct bio *base_bio) {
  struct matryoshka_io *io;

  io = kmalloc(sizeof(struct matryoshka_io), GFP_KERNEL);
  if (io == NULL) {
    return NULL;
  }
  
  io->mc = mc;
  io->base_bio = base_bio;
  io->base_sector = dm_target_offset(mc->ti, base_bio->bi_iter.bi_sector);

  atomic_set(&(io->entropy_done), 0);
  atomic_set(&(io->carrier_done), 0);
  atomic_set(&(io->erasure_done), 0);

  io->erasures = (int*) kmalloc(sizeof(int) * (1 + mc->num_entropy + mc->num_carrier), GFP_KERNEL);
  io->erasures[0] = 1;
  io->erasures[1] = -1;

  mutex_init(&(io->lock));
  
  return io;
}

void io_update_erasures(struct matryoshka_context *mc, struct matryoshka_io *io, int index) {
  int last = 0;
  int i;
  
  mutex_lock(&io->lock);

  for (i = 0; i < 1 + mc->num_entropy + mc->num_carrier; ++i) {
    if (io->erasures[i] == -1) {
      last = i;
      break;
    }
  }

  if (last != 0) {
    io->erasures[last] = index;
    io->erasures[last + 1] = -1;
  }

  mutex_unlock(&io->lock);
}

void io_accumulate_error(struct matryoshka_io *io, int error) {
  mutex_lock(&io->lock);
  if (!io->error) {
      io->error = error;
  } else if (io->error != error) {
      io->error = -EIO;
  }
  mutex_unlock(&io->lock);
}

inline void bio_map_dev(struct bio *bio, struct matryoshka_device *d) {
  bio->bi_bdev = d->dev->bdev;
}

inline void bio_map_sector(struct bio *bio, sector_t sector) {
  bio->bi_iter.bi_sector = sector;
}

inline void bio_map_operation(struct bio *bio, unsigned int opf) {
  bio->bi_opf = opf;
}

inline void matryoshka_bio_init(struct bio *bio, struct matryoshka_io *io, bio_end_io_t ep) {
  bio->bi_private = io;
  bio->bi_end_io = ep;
}

inline void matryoshka_bio_init_linear(struct matryoshka_context *mc, struct bio *bio, struct matryoshka_device *d, struct matryoshka_io *io) {
  bio_map_dev(bio, d);
  bio_map_sector(bio, io->base_sector + d->start);
}

struct bio *matryoshka_alloc_bio(struct matryoshka_context *mc, unsigned size) {
  struct bio *clone;
  unsigned int nr_iovecs = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
  gfp_t gfp_mask = GFP_NOWAIT | GFP_NOIO;
  unsigned i, len, remaining_size;
  struct page *page;

  if (unlikely(gfp_mask & __GFP_DIRECT_RECLAIM)) {
    mutex_lock(&mc->bio_alloc_lock);
  }

  clone = bio_alloc_bioset(GFP_NOIO, nr_iovecs, mc->bs);
  if (!clone) {
    goto out;
  }

  remaining_size = size;
  for (i = 0; i < nr_iovecs; i++) {
    page = mempool_alloc(mc->page_pool, gfp_mask);
    len = (remaining_size > PAGE_SIZE) ? PAGE_SIZE : remaining_size;

    bio_add_page(clone, page, len, 0);

    remaining_size -= len;
  }

out:
  if (unlikely(gfp_mask & __GFP_DIRECT_RECLAIM)) {
    mutex_unlock(&mc->bio_alloc_lock);
  }

  return clone;
}

void matryoshka_free_buffer_pages(struct matryoshka_context *mc, struct bio *clone) {
  unsigned int i;
  struct bio_vec *bv;

  bio_for_each_segment_all(bv, clone, i)
  {
    BUG_ON(!bv->bv_page);
    mempool_free(bv->bv_page, mc->page_pool);
    bv->bv_page = NULL;
  }
}

// Calculate CRC32 bitwise little-endian
inline u32 crc32le(const char *buf, __kernel_size_t len) {
  return crc32_le(INIT_CRC, buf, len);
}

void init_bvec(struct matryoshka_io *io) {
  struct matryoshka_context *mc = io->mc;

  int i;

  if (io->base_bio) {
    io->iter_base = io->base_bio->bi_iter;
  }
  for (i = 0; i < mc->num_entropy; ++i) {
    if (io->entropy_bios[i]) {
      io->iter_entropy[i] = io->entropy_bios[i]->bi_iter;
    }
  }
  for (i = 0; i < mc->num_carrier; ++i) {
    if (io->carrier_bios[i]) {
      io->iter_carrier[i] = io->carrier_bios[i]->bi_iter;
    }
  }
}

int erasure_decode(struct matryoshka_context *mc, struct matryoshka_io *io) {
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
  int *erasures = io->erasures;

  int i;
  
  matrix = reed_sol_vandermonde_coding_matrix(k, m, w);

  init_bvec(io);

  while (1) {
    if (!io->iter_base.bi_size) {
      break;
    }
    for (i = 0; i < mc->num_entropy; ++i) {
      if (!io->iter_entropy[i].bi_size) {
        break;
      }
    }
    for (i = 0; i < mc->num_carrier; ++i) {
      if (!io->iter_carrier[i].bi_size) {
        break;
      }
    }

    data_vecs[0] = bio_iter_iovec(io->base_bio, io->iter_base);
    data[0] = page_address(data_vecs[0].bv_page);
    blocksize = mc->sector_size;
    for (i = 1; i < k; ++i) {
      data_vecs[i] = bio_iter_iovec(io->entropy_bios[i], io->iter_entropy[i]);
      data[i] = page_address(data_vecs[i].bv_page);
    }
    for (i = 0; i < m; ++i) {
      coding_vecs[i] = bio_iter_iovec(io->entropy_bios[i], io->iter_entropy[i]);
      coding[i] = page_address(coding_vecs[i].bv_page);
    }

    status = jerasure_matrix_decode(k, m, w, matrix, ROW_K_ONES, erasures, data, coding, blocksize);

    bio_advance_iter(io->base_bio, &io->iter_base, mc->sector_size);
    for (i = 0; i < mc->num_entropy; ++i) {
      bio_advance_iter(io->entropy_bios[i], &io->iter_entropy[i], mc->sector_size);
    }
    for (i = 0; i < mc->num_carrier; ++i) {
      bio_advance_iter(io->carrier_bios[i], &io->iter_carrier[i], mc->sector_size);
    }
  }

  return status;
}

void erasure_encode(struct matryoshka_context *mc, struct matryoshka_io *io) {
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

  init_bvec(io);

  while (1) {
    if (!io->iter_base.bi_size) {
      break;
    }
    for (i = 0; i < mc->num_entropy; ++i) {
      if (!io->iter_entropy[i].bi_size) {
        break;
      }
    }
    for (i = 0; i < mc->num_carrier; ++i) {
      if (!io->iter_carrier[i].bi_size) {
        break;
      }
    }

    data_vecs[0] = bio_iter_iovec(io->base_bio, io->iter_base);
    data[0] = page_address(data_vecs[0].bv_page);
    blocksize = mc->sector_size;
    for (i = 1; i < k; ++i) {
      data_vecs[i] = bio_iter_iovec(io->entropy_bios[i], io->iter_entropy[i]);
      data[i] = page_address(data_vecs[i].bv_page);
    }
    for (i = 0; i < m; ++i) {
      coding_vecs[i] = bio_iter_iovec(io->entropy_bios[i], io->iter_entropy[i]);
      coding[i] = page_address(coding_vecs[i].bv_page);
    }

    jerasure_matrix_encode(k, m, w, matrix, data, coding, blocksize);

    bio_advance_iter(io->base_bio, &io->iter_base, mc->sector_size);
    for (i = 0; i < mc->num_entropy; ++i) {
      bio_advance_iter(io->entropy_bios[i], &io->iter_entropy[i], mc->sector_size);
    }
    for (i = 0; i < mc->num_carrier; ++i) {
      bio_advance_iter(io->carrier_bios[i], &io->iter_carrier[i], mc->sector_size);
    }
  }
}

inline void get_32bit_random_number(u32 *unum) {
  get_random_bytes(unum, sizeof(unum));
}

/**
 * do_shash() - Do a synchronous hash operation in software
 * @name:       The name of the hash algorithm
 * @result:     Buffer where digest is to be written
 * @data1:      First part of data to hash. May be NULL.
 * @data1_len:  Length of data1, in bytes
 * @data2:      Second part of data to hash. May be NULL.
 * @data2_len:  Length of data2, in bytes
 * @key:	Key (if keyed hash)
 * @key_len:	Length of key, in bytes (or 0 if non-keyed hash)
 *
 * Note that the crypto API will not select this driver's own transform because
 * this driver only registers asynchronous algos.
 *
 * Return: 0 if hash successfully stored in result
 *         < 0 otherwise
 */
int do_shash(unsigned char *name, unsigned char *result,
             const u8 *data1, unsigned int data1_len,
             const u8 *data2, unsigned int data2_len,
             const u8 *key, unsigned int key_len) {
  int rc;
  unsigned int size;
  struct crypto_shash *hash;
  struct sdesc *sdesc;

  hash = crypto_alloc_shash(name, 0, 0);
  if (IS_ERR(hash)) {
    rc = PTR_ERR(hash);
    pr_err("%s: Crypto %s allocation error %d", __func__, name, rc);
    return rc;
  }

  size = sizeof(struct shash_desc) + crypto_shash_descsize(hash);
  sdesc = kmalloc(size, GFP_KERNEL);
  if (!sdesc) {
    rc = -ENOMEM;
    pr_err("%s: Memory allocation failure", __func__);
    goto do_shash_err;
  }
  sdesc->shash.tfm = hash;
  sdesc->shash.flags = 0x0;

  if (key_len > 0) {
    rc = crypto_shash_setkey(hash, key, key_len);
    if (rc)
    {
      pr_err("%s: Could not setkey %s shash", __func__, name);
      goto do_shash_err;
    }
  }

  rc = crypto_shash_init(&sdesc->shash);
  if (rc) {
    pr_err("%s: Could not init %s shash", __func__, name);
    goto do_shash_err;
  }
  rc = crypto_shash_update(&sdesc->shash, data1, data1_len);
  if (rc) {
    pr_err("%s: Could not update1", __func__);
    goto do_shash_err;
  }
  if (data2 && data2_len) {
    rc = crypto_shash_update(&sdesc->shash, data2, data2_len);
    if (rc)
    {
      pr_err("%s: Could not update2", __func__);
      goto do_shash_err;
    }
  }
  rc = crypto_shash_final(&sdesc->shash, result);
  if (rc)
    pr_err("%s: Could not generate %s hash", __func__, name);

do_shash_err:
  crypto_free_shash(hash);
  kfree(sdesc);

  return rc;
}

sector_t parse_random_sector(const u8 *data, sector_t max) {
  sector_t ret;
  char* bytes = kmalloc(sizeof(sector_t), GFP_KERNEL);

  memcpy(bytes, data, sizeof(sector_t));
  
  ret = *((sector_t*) bytes);

  kfree(bytes);

  return ret % max;
}

sector_t get_sector_in_sequence(char *passphrase, sector_t logical_sector, u64 counter, sector_t max) {
  sector_t ret;

  int plen;
  char hash[20];
  int status;

  char *bytes;

  plen = strlen(passphrase);

  status = do_shash("sha1", hash, (u8*)&logical_sector, sizeof(sector_t), (u8*)&counter, sizeof(u64), passphrase, plen);

  if (status) {
    return logical_sector;
  }

  bytes = kmalloc(sizeof(sector_t), GFP_KERNEL);
  memcpy(bytes, hash, sizeof(sector_t));

  ret = *((sector_t *)bytes);

  kfree(bytes);

  return ret % max;
}
