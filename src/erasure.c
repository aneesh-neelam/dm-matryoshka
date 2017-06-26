#include <linux/device-mapper.h>

#include "../include/erasure.h"

#include "../lib/jerasure/jerasure.h"
#include "../lib/jerasure/reed_sol.h"
#include "../lib/jerasure/galois.h"
#include "../lib/jerasure/cauchy.h"
#include "../lib/jerasure/liberation.h"

int erasure_encode(struct bio_vec *carrier, struct bio_vec *userdata, struct bio_vec *entropy) {
  // TODO link with a C erasure library

  return 0;
}

int erasure_decode(struct bio_vec *userdata, struct bio_vec *carrier, struct bio_vec *entropy) {
  // TODO link with a C erasure library

  return 0;
}
