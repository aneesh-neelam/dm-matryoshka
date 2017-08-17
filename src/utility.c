#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/atomic.h>
#include <linux/crc32.h>
#include <linux/random.h>
#include <crypto/hash.h>

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

int erasure_encode(struct matryoshka_context *mc, struct matryoshka_io *io) {
  int *matrix = NULL;

  int k = mc->num_entropy + 1;
  int m = mc->num_carrier;

  int w = WORD_SIZE;
  
  matrix = reed_sol_vandermonde_coding_matrix(k, m, w);

  // jerasure_matrix_encode()

  return 0;
}

int erasure_decode(struct bio_vec *userdata, struct bio_vec *carrier, struct bio_vec *entropy) {
  int *matrix = NULL;
  
  int k = mc->num_entropy + 1;
  int m = mc->num_carrier;
  
  int w = WORD_SIZE;
  int numerased = 0;
  int *erasures;
    
  matrix = reed_sol_vandermonde_coding_matrix(k, m, w);


  return 0;
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
