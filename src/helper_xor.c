#include <linux/types.h>

#include "../include/xor.h"


int xor_assign(const char *src, char *dst, __kernel_size_t size) {
  while (size--) {
    *(dst++) ^= *(src++);
  }

  return 0;
}

int xor_copy(const char *src1, const char *src2, char *dst, __kernel_size_t size) {
  while (size--) {
    *(dst++) = *(src1++) ^ *(src2++);
  }

  return 0;
}
