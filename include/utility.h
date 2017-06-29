#ifndef UTILITY_H
#define UTILITY_H

#include <linux/types.h>

int convertStringToU8(u8* res, char* str);
int convertStringToSector_t(sector_t* res, char* str);
int checkRatio(u8 num_carrier, u8 num_entropy);

#endif /* UTILITY_H */
