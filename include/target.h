#include <linux/types.h>
#include <linux/device-mapper.h>

#include "fs.h"

#define DM_MSG_PREFIX "matryoshka"
#define NAME "matryoshka"
#define LICENSE "Dual BSD/GPL"
#define DESCRIPTION "Matryoshka Device Mapper"
#define AUTHOR "Aneesh Neelam <aneelam@ucsc.edu>"

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 0


/*
 * Matryoshka: Maps in the Matryoshka way.
 */
struct matryoshka_c {
  char *passphrase;
  struct dm_dev *entropy;
  struct dm_dev *carrier;
  u8 carrier_fs;
  sector_t carrier_start;
  sector_t entropy_start;

  struct workqueue_struct *matryoshka_wq;
  struct work_struct matryoshka_work;

  struct fs_vfat *fs;
};


int get_entropy_blocks(struct dm_dev*);

int matryoshka_read(struct dm_target*, struct bio*);
int matryoshka_write(struct dm_target*, struct bio*);
