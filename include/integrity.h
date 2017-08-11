#ifndef INTEGRITY_H
#define INTEGRITY_H

#include <linux/types.h>

#define INIT_CRC 0

inline u32 crc32le(const char *, __kernel_size_t);

#endif /* INTEGRITY_H */
