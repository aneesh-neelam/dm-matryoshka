#include <linux/device-mapper.h>


int erasure_encode(struct bio_vec *, struct bio_vec *, struct bio_vec *);
int erasure_decode(struct bio_vec *, struct bio_vec *, struct bio_vec *);
