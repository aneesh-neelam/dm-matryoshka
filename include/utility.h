#ifndef UTILITY_H
#define UTILITY_H

#include <linux/types.h>

int convertStringToU8(u8* res, char* str);
int convertStringToSector_t(sector_t* res, char* str);
int checkRatio(u8 num_carrier, u8 num_entropy);

struct bio *mybio_clone(struct bio*);
void mybio_free(struct bio*);
void mybio_copy_data(struct bio*, struct bio*);
struct bio **init_bios(struct bio*, unsigned int);
void bio_map_dev(struct bio*, struct matryoshka_device*);
void bio_map_sector(struct bio*, struct matryoshka_context*, struct matryoshka_device*);
void init_bio_dev(struct matryoshka_context*, struct bio*, struct matryoshka_device*, struct matryoshka_io*, bio_end_io_t);

#endif /* UTILITY_H */
