#ifndef TARGET_H
#define TARGET_H

#include <linux/types.h>
#include <linux/device-mapper.h>
#include <linux/mutex.h>
#include <asm/atomic.h>

#include "fs.h"

#define DM_MSG_PREFIX "matryoshka"
#define NAME "matryoshka"
#define LICENSE "Dual BSD/GPL"
#define DESCRIPTION "Matryoshka Device Mapper"
#define AUTHOR "Aneesh Neelam <aneelam@ucsc.edu>"

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 0

#define MIN_IOS 64

struct matryoshka_device {
  struct matryoshka_c *mc;
  struct dm_dev *dev;
	sector_t start;
};

struct matryoshka_context {
  struct mutex lock;

  mempool_t *page_pool;
  struct bio_set *bs;
  struct mutex bio_alloc_lock;

  struct dm_target *ti;

  char *passphrase;
  u8 carrier_fs;
  char *carrier_fs_name;

  sector_t metadata_logical_sector;

  sector_t entropy_max_sectors;

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
  struct mutex lock;

  struct matryoshka_context *mc;

  struct bio *base_bio;
  sector_t base_sector;

  struct work_struct work;

  struct bio *entropy_bios[10];
  struct bio *carrier_bios[10];

  struct bvec_iter iter_base;
  struct bvec_iter iter_entropy[10];
  struct bvec_iter iter_carrier[10];

  int *erasures;

  atomic_t carrier_done;
  atomic_t entropy_done;
  atomic_t erasure_done;

  int error;
};

struct metadata_io {
  struct mutex lock;

  struct matryoshka_io *io;
  
  struct bio *metadata_bios[10];
  struct bio *carrier_bios[10];

  sector_t logical_sector;

  struct bvec_iter iter_base;
  struct bvec_iter iter_entropy[10];
  struct bvec_iter iter_carrier[10];

  int *erasures;

  atomic_t carrier_done;
  atomic_t entropy_done;
  atomic_t erasure_done;

  int error;
};

#endif /* TARGET_H */
