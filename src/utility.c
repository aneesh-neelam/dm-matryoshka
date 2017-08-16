#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/atomic.h>

#include "../include/target.h"
#include "../include/utility.h"


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

  mutex_init(&(io->lock));
  
  return io;
}

void io_accumulate_error(struct matryoshka_io *io, int error) {
    if (!io->error) {
        io->error = error;
    } else if (io->error != error) {
        io->error = -EIO;
    }
}

inline void bio_map_dev(struct bio *bio, struct matryoshka_device *d) {
  bio->bi_bdev = d->dev->bdev;
}

inline void bio_map_sector(struct bio *bio, sector_t sector) {
  bio->bi_iter.bi_sector = sector;
}

void matryoshka_bio_init(struct bio *bio, struct matryoshka_io *io, bio_end_io_t ep, unsigned int opf) {
  bio->bi_private = io;
  bio->bi_end_io = ep;
  bio->bi_opf = opf;
}

void matryoshka_bio_init_linear(struct matryoshka_context *mc, struct bio *bio, struct matryoshka_device *d, struct matryoshka_io *io) {
  bio_map_dev(bio, d);
  bio_map_sector(bio, io->base_sector + d->start);
}

struct bio *matryoshka_alloc_bio(unsigned size) {
  struct bio *clone;
  unsigned int nr_iovecs = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
  gfp_t gfp_mask = GFP_NOWAIT | GFP_NOIO;
  unsigned i, len, remaining_size;
  struct page *page;

  clone = bio_alloc(gfp_mask, nr_iovecs);

  remaining_size = size;
  for (i = 0; i < nr_iovecs; i++) {
    page = alloc_page(gfp_mask);
    len = (remaining_size > PAGE_SIZE) ? PAGE_SIZE : remaining_size;

    bio_add_page(clone, page, len, 0);

    remaining_size -= len;
  }

  return clone;
}

// Calculate CRC32 bitwise little-endian
inline u32 crc32le(const char *buf, __kernel_size_t len) {
  return crc32_le(INIT_CRC, buf, len);
}

int erasure_encode(struct bio_vec *carrier, struct bio_vec *userdata, struct bio_vec *entropy) {
  // TODO link with a C erasure library

  return 0;
}

int erasure_decode(struct bio_vec *userdata, struct bio_vec *carrier, struct bio_vec *entropy) {
  // TODO link with a C erasure library

  return 0;
}
