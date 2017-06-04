#ifndef ENTROPY_H
#define ENTROPY_H

#include <linux/device-mapper.h>


int get_entropy_blocks(struct dm_dev *);

#endif /* ENTROPY_H */
