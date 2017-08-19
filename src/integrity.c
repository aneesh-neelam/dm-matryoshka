#include <linux/device-mapper.h>
#include <linux/bio.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include "../include/integrity.h"
#include "../include/target.h"
#include "../include/utility.h"


struct metadata_io* init_metadata_io(struct matryoshka_context *mc, struct matryoshka_io *io) {
  struct metadata_io *mio;

  mio = kmalloc(sizeof(struct metadata_io), GFP_NOIO);

  mio->io = io;

  atomic_set(&(mio->entropy_done), 0);
  atomic_set(&(mio->carrier_done), 0);
  atomic_set(&(mio->erasure_done), 0);
  
  mio->erasures = (int *)kmalloc(sizeof(int) * (1 + mc->num_entropy + mc->num_carrier), GFP_KERNEL);
  mio->erasures[0] = 1;
  mio->erasures[1] = -1;

  mutex_init(&(mio->lock));

  return mio;
}

void init_metadata_bios(struct matryoshka_context *mc, struct metadata_io *mio) {
  sector_t generated_sector;
  int i;
  int skip;

  for (i = 0; i < mc->num_carrier; ++i) {

    mio->carrier_bios[i] = matryoshka_alloc_bio(mc, mc->cluster_size);
    bio_map_dev(mio->carrier_bios[i], mc->carrier);
    
  }

  for (i = 0; i < mc->num_entropy; ++i) {

    mio->entropy_bios[i] = matryoshka_alloc_bio(mc, mc->cluster_size);
    bio_map_dev(mio->entropy_bios[i], mc->entropy);
  }
}


int matryoshka_bio_integrity_check(struct matryoshka_context *mc, struct matryoshka_io *io, struct bio *bio) {
  int i;

  struct metadata_io *mio = init_metadata_io(mc, io);

  return 0;
}
