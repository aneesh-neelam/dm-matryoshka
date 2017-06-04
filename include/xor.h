#ifndef XOR_H
#define XOR_H

#include <linux/types.h>


int xor_assign(const char *, char *, __kernel_size_t);
int xor_copy(const char *, const char *, char *, __kernel_size_t);

#endif /* XOR_H */
