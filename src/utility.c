#include <linux/kernel.h>
#include <linux/types.h>

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
