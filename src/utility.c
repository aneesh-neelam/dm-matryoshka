#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/atomic.h>

#include "../include/target.h"
#include "../include/utility.h"
#include "../include/xor.h"


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

// io Utility functions

struct matryoshka_io *init_matryoshka_io(struct matryoshka_context *mc, struct bio *base_bio) {
  struct matryoshka_io *io;

  io = kmalloc(sizeof(struct matryoshka_io), GFP_KERNEL);
  if (io == NULL) {
    return NULL;
  }
  
  io->mc = mc;
  io->base_bio = bio;

  atomic_set(&(io->entropy_done), 0);
  atomic_set(&(io->carrier_done), 0);
  atomic_set(&(io->erasure_done), 0);

  mutex_init(&(io->lock));

  carrier_bios = init_bios(bio, 2);
  entropy_bios = init_bios(bio, 1);

  return io;
}

void io_accumulate_error(struct matryoshka_io *io, int error) {
    if (!io->error) {
        io->error = error;
    } else if (io->error != error) {
        io->error = -EIO;
    }
}

// bio Utility functions
/**
  Clones the given bio and its bio_vec structures. The new bio is not flagged as
  cloned, and new pages will be allocated for all of its bio_vec's.
  \param[in] sbio The bio structure to clone.
  \retval NULL if no memory could be allocated.
*/
struct bio *mybio_clone(struct bio *sbio) {
    struct bio *bio = bio_kmalloc(GFP_NOIO, sbio->bi_max_vecs);
    struct bio_vec *bvec, *sbvec;
    unsigned short i;

    if (!bio)
        return NULL;

    memcpy(bio->bi_io_vec, sbio->bi_io_vec,
           sbio->bi_max_vecs * sizeof(struct bio_vec));

    for (i = 0, bvec = bio->bi_io_vec, sbvec = sbio->bi_io_vec;
         i < bio->bi_max_vecs; i++, bvec++, sbvec++) {
        bvec->bv_page = alloc_page(GFP_NOIO);
        if (!bvec->bv_page)
            goto mybio_clone_cleanup;

        bvec->bv_offset = sbvec->bv_offset;
        bvec->bv_len = sbvec->bv_len;
    }

    bio->bi_sector = sbio->bi_sector;
    bio->bi_rw = sbio->bi_rw;
    bio->bi_vcnt = sbio->bi_vcnt;
    bio->bi_size = sbio->bi_size;
    bio->bi_idx = 0;
    return bio;

mybio_clone_cleanup:
    while (i--) {
        bvec--;
        __free_page(bvec->bv_page);
    }
    return NULL;
}

/**
  Deallocates a bio structure and all of the pages associated to its bio_vec's.
  \param[in] bio the bio to free.
*/
void mybio_free(struct bio *bio) {
    unsigned short i;
    struct bio_vec *bvec = bio->bi_io_vec;
    for (i = 0; i < bio->bi_max_vecs; i++, bvec++)
        __free_page(bvec->bv_page);

    (*bio->bi_destructor)(bio);
}

/**
  Copies all data from one bio to another.
  \pre Both bio structures must hold the same amount of data.
  \pre Both bio structures must have the same number of bio_vec's.
  \pre The bio_vec structures of both bios at the same index must be of same
       length.
  \param[in] src The source bio of the data.
  \param[out] dst The destination bio for the data.
*/
void mybio_copy_data(struct bio *src, struct bio *dst) {
    struct bio_vec *src_bvec, *dst_bvec;
    char *src_buf, *dst_buf;
    unsigned short i;

    BUG_ON(src->bi_size != dst->bi_size);
    BUG_ON(src->bi_vcnt != dst->bi_vcnt);

    for (i = 0, src_bvec = src->bi_io_vec, dst_bvec = dst->bi_io_vec; i < src->bi_vcnt; i++, src_bvec++, dst_bvec++) {
        BUG_ON(src_bvec->bv_len != dst_bvec->bv_len);

        src_buf = __bio_kmap_atomic(src, i);
        dst_buf = __bio_kmap_atomic(dst, i);
        memcpy(dst_buf + dst_bvec->bv_offset,
               src_buf + src_bvec->bv_offset,
               src_bvec->bv_len);
        __bio_kunmap_atomic(dst_buf);
        __bio_kunmap_atomic(src_buf);
    }
}

/**
  Clones the given number of bios and an array of pointers to the cloned
  vectors.
  \param[in] src The BIO to clone.
  \param[in] count Number of clones to create.
  \returns The pointer to the array of cloned vectors.
*/
struct bio **init_bios(struct bio *src, unsigned count) {
    struct bio **bios = kmalloc(count * sizeof(struct bio*), GFP_NOIO);
    unsigned i;

    if (!bios)
        goto init_bios_cleanup;

    for (i = 0; i < count; i++) {
        /* Clone the BIO for a specific device: */
        bios[i] = mybio_clone(src);
        if (!bios[i])
            goto init_bios_cleanup_bios;
    }
    return bios;

init_bios_cleanup_bios:
    while (i--)
        mybio_free(bios[i]);

    kfree(bios);
init_bios_cleanup:
    return NULL;
}

void bio_map_dev(struct bio *bio, struct matryoshka_device *d) {
  bio->bi_bdev = d->dev->bdev;
}

void bio_map_sector(struct bio *bio, struct matryoshka_context *mc, struct matryoshka_device *d) {
  if (bio_sectors(bio)) {
    bio->bi_iter.bi_sector = d->start + dm_target_offset(mc->ti, bio->bi_iter.bi_sector);
  }
}

void mybio_init_dev(struct matryoshka_context *mc, struct bio *bio, struct matryoshka_device *d, struct matryoshka_io *io, bio_end_io_t ep) {
  bio_map_dev(bio, d);
  bio_map_sector(bio, mc, d);
  bio->bi_private = io;
  bio->bi_end_io = ep;
}

/**
  Xors the data of one bio with the data another bio.
  \pre Both bio structures must hold the same amount of data.
  \pre Both bio structures must have the same number of bio_vec's.
  \pre The bio_vec structures of both bios at the same index must be of same
       length.
  \param[in] src The bio to xor the data from.
  \param[out] dst The bio to xor the data into.
*/
void mybio_xor_assign(struct bio *src, struct bio *dst) {
    struct bio_vec *src_bvec, *dst_bvec;
    char *src_buf, *dst_buf;
    unsigned short i;

    BUG_ON(src->bi_size != dst->bi_size);
    BUG_ON(src->bi_vcnt != dst->bi_vcnt);

    for (i = 0, src_bvec = src->bi_io_vec, dst_bvec = dst->bi_io_vec;
         i < src->bi_vcnt; i++, src_bvec++, dst_bvec++)
    {
        BUG_ON(src_bvec->bv_len != dst_bvec->bv_len);

        src_buf = __bio_kmap_atomic(src, i);
        dst_buf = __bio_kmap_atomic(dst, i);
        xor_assign(src_buf + src_bvec->bv_offset,
                   dst_buf + dst_bvec->bv_offset,
                   src_bvec->bv_len);
        __bio_kunmap_atomic(dst_buf);
        __bio_kunmap_atomic(src_buf);
    }
}

/**
  Xors the data of two bio structures into a third bio structure.
  \pre All three bio structures must hold the same amount of data.
  \pre All three structures must have the same number of bio_vec's.
  \pre The bio_vec structures of all three bios at the same index must be of
       same length.
  \param[in] src The first bio to xor the data from.
  \param[in] src2 The second bio to xor the data from.
  \param[out] dst The bio to store the data in.
*/
void mybio_xor_copy(struct bio *src, struct bio *src2, struct bio *dst) {
    struct bio_vec *src_bvec = src->bi_io_vec;
    struct bio_vec *src2_bvec = src2->bi_io_vec;
    struct bio_vec *dst_bvec = dst->bi_io_vec;
    char *src_buf, *src2_buf, *dst_buf;
    unsigned short i;

    BUG_ON(src->bi_size != dst->bi_size);
    BUG_ON(src->bi_vcnt != dst->bi_vcnt);
    BUG_ON(src2->bi_size != dst->bi_size);
    BUG_ON(src2->bi_vcnt != dst->bi_vcnt);

    for (i = 0; i < src->bi_vcnt; i++, src_bvec++, src2_bvec++, dst_bvec++)
    {
        BUG_ON(src_bvec->bv_len != dst_bvec->bv_len);
        BUG_ON(src2_bvec->bv_len != dst_bvec->bv_len);

        src_buf = __bio_kmap_atomic(src, i);
        src2_buf = __bio_kmap_atomic(src2, i);
        dst_buf = __bio_kmap_atomic(dst, i);
        xor_copy(src_buf + src_bvec->bv_offset,
                 src2_buf + src2_bvec->bv_offset,
                 dst_buf + dst_bvec->bv_offset,
                 src_bvec->bv_len);
        __bio_kunmap_atomic(dst_buf);
        __bio_kunmap_atomic(src2_buf);
        __bio_kunmap_atomic(src_buf);
    }
}
