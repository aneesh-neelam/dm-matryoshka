#include <linux/device-mapper.h>

#include "../include/erasure.h"


int erasure_encode(struct bio_vec *carrier, struct bio_vec *userdata, struct bio_vec *entropy) {
  // TODO link with a C erasure library

  return 0;
}

int erasure_decode(struct bio_vec *userdata, struct bio_vec *carrier, struct bio_vec *entropy) {
  // TODO link with a C erasure library

  return 0;
}
