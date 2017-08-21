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

  struct bio *base_bio;
  struct bio *entropy_bios[10];
  struct bio *carrier_bios[10];

  sector_t logical_sector;

  struct bvec_iter iter_base;
  struct bvec_iter iter_entropy[10];
  struct bvec_iter iter_carrier[10];

  int *erasures;

  int error;
};

struct metadata_io *alloc_metadata_io(struct matryoshka_context*, struct matryoshka_io *io);
void free_metadata_io(struct matryoshka_context *mc, struct metadata_io *mio);
void init_metadata_bios(struct matryoshka_context *mc, struct metadata_io *mio);
void mio_update_erasures(struct matryoshka_context *mc, struct metadata_io *io, int index);

void init_metadata_bvec(struct matryoshka_context*, struct metadata_io*);
char* metadata_parse_bio(struct matryoshka_context*, struct bio*);

void metadata_erasure_encode(struct matryoshka_context*, struct metadata_io*);
int metadata_erasure_decode(struct matryoshka_context*, struct metadata_io*);

int matryoshka_bio_integrity_check(struct matryoshka_context*, struct matryoshka_io*, struct bio*);
int matryoshka_bio_integrity_update(struct matryoshka_context*, struct matryoshka_io*, struct bio*);

int metadata_verify(char*, __kernel_size_t);
char *metadata_init(__kernel_size_t);
int metadata_update(struct matryoshka_context*, char*, __kernel_size_t, struct bio*);
int metadata_check(struct matryoshka_context*, char*, __kernel_size_t, struct bio*);
sector_t metadata_next_in_list(char*, __kernel_size_t);

#endif /* INTEGRITY_H */
