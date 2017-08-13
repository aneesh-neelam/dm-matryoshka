#ifndef UTILITY_H
#define UTILITY_H

#include <linux/types.h>

#include "target.h"

int convertStringToU8(u8* res, char* str);
int convertStringToSector_t(sector_t* res, char* str);
int checkRatio(u8 num_carrier, u8 num_entropy);

struct matryoshka_io *init_matryoshka_io(struct matryoshka_context*, struct bio*);
void io_accumulate_error(struct matryoshka_io*, int);

inline struct bio *mybio_clone(struct bio*);
inline void mybio_free(struct bio*);
inline void mybio_copy_data(struct bio*, struct bio*);
struct bio **init_bios(struct bio*, unsigned int);
inline void bio_map_dev(struct bio*, struct matryoshka_device*);
inline void bio_map_sector(struct bio*, struct matryoshka_context*, struct matryoshka_device*);
inline void mybio_init_dev(struct matryoshka_context*, struct bio*, struct matryoshka_device*, struct matryoshka_io*, bio_end_io_t);
void mybio_xor_assign(struct bio*, struct bio*);
void mybio_xor_copy(struct bio*, struct bio*, struct bio*);

#endif /* UTILITY_H */
