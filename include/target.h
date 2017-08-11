#ifndef TARGET_H
#define TARGET_H

#include <linux/types.h>
#include <linux/device-mapper.h>
#include <linux/mutex.h>
#include <<asm/atomic.h>>

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
};

struct matryoshka_context {
  struct mutex lock;

  struct dm_target *ti;

  char *passphrase;
  u8 carrier_fs;
  char *carrier_fs_name;

  struct workqueue_struct *matryoshka_wq;

  struct fs_fat *fs;

  __kernel_size_t cluster_size;
  __kernel_size_t sector_size;
  u32 cluster_count;
  u32 sectors_per_cluster;

  u8 num_carrier; // m
  u8 num_entropy; // k
  struct matryoshka_device *carrier;
  struct matryoshka_device *entropy;

  struct bio_list bios;
};

struct matryoshka_io {
  struct matryoshka_context *mc;

  struct bio *base_bio;

  struct work_struct work;

  struct mutex lock;

  struct bio **entropy_bios;
  struct bio **carrier_bios;

  atomic_t carrier_done;
  atomic_t entropy_done;
  atomic_t erasure_done;

  int error;
};

#endif /* TARGET_H */
