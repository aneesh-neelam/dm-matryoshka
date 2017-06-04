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


struct matryoshka_device {
  struct matryoshka_c *mc;
  struct dm_dev *dev;
	sector_t start;
}

struct matryoshka_context {
  spinlock_t lock;

  char *passphrase;
  u8 carrier_fs;

  struct bio_list entropy_bios;
  struct bio_list carrier_bios;

  struct workqueue_struct *matryoshka_wq;
  struct work_struct matryoshka_work;

  struct fs_vfat *fs;

  u8 num_carrier; // m
  u8 num_entropy; // k
  u8 num_userdata; // d
  struct matryoshka_device carrier;
  struct matryoshka_device entropy;
};


int get_entropy_blocks(struct dm_dev*);

int matryoshka_read(struct dm_target*, struct bio*);
int matryoshka_write(struct dm_target*, struct bio*);
