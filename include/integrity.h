#ifndef INTEGRITY_H
#define INTEGRITY_H

#include <linux/device-mapper.h>

#include "../include/target.h"

int matryoshka_bio_integrity_check(struct matryoshka_context*, struct matryoshka_io*, struct bio*);

#endif /* INTEGRITY_H */
