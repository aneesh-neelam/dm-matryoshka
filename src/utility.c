#include <linux/types.h>

#include "../include/utility.h"


int convertStringToU8(u8* res, char* str) {
  // Parse string to unsigned long int
  unsigned long long tmp;
  char dummy;

  if (sscanf(str, "%llu%c", &tmp, &dummy) != 1) {
    ti->error = "dm-matryoshka: Invalid number of entropy blocks";
    return -1;
  }
  *res = tmp;

  return 0;
}
