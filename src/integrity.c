#include <linux/device-mapper.h>
#include <linux/bio.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include "../include/integrity.h"
#include "../include/target.h"
#include "../include/utility.h"


int matryoshka_bio_integrity_check(struct matryoshka_context *mc, struct matryoshka_io *io, struct bio *bio) {
    return 0;
}
