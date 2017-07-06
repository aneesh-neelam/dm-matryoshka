#include <linux/crc32.h>
#include <linux/types.h>

#include "../include/integrity.h"

// Calculate CRC32 bitwise little-endian
u32 crc32le(const char *buf, __kernel_size_t len) {
  return crc32_le(INIT_CRC, buf, len);
}
