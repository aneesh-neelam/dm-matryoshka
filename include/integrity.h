#ifndef INTEGRITY_H
#define INTEGRITY_H

#include <linux/types.h>
#include <linux/device-mapper.h>
#include <linux/mutex.h>
#include <asm/atomic.h>

#include "../include/target.h"

struct metadata_io {
  struct mutex lock;

  struct matryoshka_io *io;

  char *metadata;
  struct bio *entropy_bios[10];
  struct bio *carrier_bios[10];

  __kernel_size_t cluster_size;

  sector_t logical_sector;

  struct bvec_iter iter_base;
  struct bvec_iter iter_entropy[10];
  struct bvec_iter iter_carrier[10];

  int *erasures;

  atomic_t carrier_done;
  atomic_t entropy_done;
  atomic_t erasure_done;

  int error;
};

int matryoshka_bio_integrity_check(struct matryoshka_context*, struct matryoshka_io*, struct bio*);

#endif /* INTEGRITY_H */
