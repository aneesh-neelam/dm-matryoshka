#ifndef UTILITY_H
#define UTILITY_H

#include <linux/types.h>

#include "target.h"

#define INIT_CRC 0

inline u32 crc32le(const char *, __kernel_size_t);

int convertStringToU8(u8* res, char* str);
int convertStringToSector_t(sector_t* res, char* str);
int checkRatio(u8 num_carrier, u8 num_entropy);

struct matryoshka_io *init_matryoshka_io(struct matryoshka_context*, struct bio*);
void io_accumulate_error(struct matryoshka_io*, int);

struct bio *matryoshka_alloc_bio(unsigned size);

inline void bio_map_dev(struct bio*, struct matryoshka_device*);
inline void bio_map_sector(struct bio*, sector_t);

void matryoshka_bio_init(struct bio *, struct matryoshka_io *, bio_end_io_t, unsigned int);
void matryoshka_bio_init_linear(struct matryoshka_context *mc, struct bio *bio, struct matryoshka_device*, struct matryoshka_io *io);

int erasure_encode(struct bio_vec *, struct bio_vec *, struct bio_vec *);
int erasure_decode(struct bio_vec *, struct bio_vec *, struct bio_vec *);

#endif /* UTILITY_H */
