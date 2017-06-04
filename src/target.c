#include <linux/device-mapper.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

#include "../include/carrier.h"
#include "../include/entropy.h"
#include "../include/erasure.h"
#include "../include/fs.h"
#include "../include/target.h"
#include "../include/xor.h"


int matryoshka_read(struct dm_target *ti, struct bio *bio) {
  struct matryoshka_c *mc = (struct matryoshka_c*) ti -> private;

  int status;
  int i;
  struct bio **entropy_bios = kmalloc(mc -> num_entropy * sizeof(struct bio*), GFP_NOIO);
  struct bio **carrier_bios = kmalloc(mc -> num_carrier * sizeof(struct bio*), GFP_NOIO);;

  for (i = 0; i < mc -> num_carrier; ++i) {
    carrier_bios[i] = bio_clone(bio, GFP_NOIO);
  }

  for (i = 0; i < mc -> num_entropy; ++i) {
    entropy_bios[i] = bio_clone(bio, GFP_NOIO);

    entropy_bios[i] -> bi_bdev = mc -> entropy -> bdev;
    entropy_bios[i] -> bi_opf = REQ_OP_READ;
    if (bio_sectors(bio)) {
      entropy_bios[i] -> bi_iter.bi_sector = mc -> entropy_start + dm_target_offset(ti, bio->bi_iter.bi_sector);
    }
    status = submit_bio_wait(entropy_bios[i]);
    if (status != 0) {
      printk(KERN_DEBUG "Write: Read entropy at sector %llu failed", entropy_bios[i] -> bi_iter.bi_sector);
    } else {
      printk(KERN_DEBUG "Write: Reading entropy device at sector: %llu", entropy_bios[i] -> bi_iter.bi_sector);
    }
  }

  for (i = 0; i < mc -> num_carrier; ++i) {
  }

  return DM_MAPIO_REMAPPED;
}

int matryoshka_write(struct dm_target *ti, struct bio *bio) {
  struct matryoshka_c *mc = (struct matryoshka_c*) ti -> private;

  int status;
  int i;
  struct bio **entropy_bios = kmalloc(mc -> num_entropy * sizeof(struct bio*), GFP_NOIO);
  struct bio **carrier_bios = kmalloc(mc -> num_carrier * sizeof(struct bio*), GFP_NOIO);;

  for (i = 0; i < mc -> num_carrier; ++i) {
    carrier_bios[i] = bio_clone(bio, GFP_NOIO);
  }

  for (i = 0; i < mc -> num_entropy; ++i) {
    entropy_bios[i] = bio_clone(bio, GFP_NOIO);

    entropy_bios[i] -> bi_bdev = mc -> entropy -> bdev;
    entropy_bios[i] -> bi_opf = REQ_OP_READ;
    if (bio_sectors(bio)) {
      entropy_bios[i] -> bi_iter.bi_sector = mc -> entropy_start + dm_target_offset(ti, bio->bi_iter.bi_sector);
    }
    status = submit_bio_wait(entropy_bios[i]);
    if (status != 0) {
      printk(KERN_DEBUG "Write: Read entropy at sector %llu failed", entropy_bios[i] -> bi_iter.bi_sector);
    } else {
      printk(KERN_DEBUG "Write: Reading entropy device at sector: %llu", entropy_bios[i] -> bi_iter.bi_sector);
    }
  }

  for (i = 0; i < mc -> num_carrier; ++i) {
  }

  return DM_MAPIO_REMAPPED;
}

/*
 * Construct a matryoshka mapping: <passphrase> entropy_dev_path> <carrier_dev_path>
 */
static int matryoshka_ctr(struct dm_target *ti, unsigned int argc, char **argv) {
  struct matryoshka_c *mc;
  struct fs_vfat *vfat;

  int ret1, ret2;
  int passphrase_length;
  unsigned long long tmp;
  char dummy;

  if (argc != 6) {
    ti -> error = "dm:matryoshka: Invalid number of arguments for constructor";
    return -EINVAL;
  }

  mc = kmalloc(sizeof(*mc), GFP_KERNEL);
  if (mc == NULL) {
    ti -> error = "dm:matryoshka: Cannot allocate context";
    return -ENOMEM;
  }

  passphrase_length = strlen(argv[0]);
  mc -> passphrase = kmalloc(passphrase_length + 1, GFP_KERNEL);
  strncpy(mc -> passphrase, argv[0], passphrase_length);
  mc -> passphrase[passphrase_length] = '\0';

  ret1 = ret2 = -EINVAL;

  ret1 = dm_get_device(ti, argv[1], dm_table_get_mode(ti -> table), &mc -> carrier);
  ret2 = dm_get_device(ti, argv[3], dm_table_get_mode(ti -> table), &mc -> entropy);
  if (ret1 || ret2) {
    ti -> error = "dm:matryoshka: Device lookup failed";
    goto bad;
  }

  mc -> carrier_fs = get_carrier_fs(argv[5]);
  vfat = kmalloc(sizeof(struct fs_vfat), GFP_KERNEL);

  if (sscanf(argv[2], "%llu%c", &tmp, &dummy) != 1) {
    ti->error = "dm-matryoshka: Invalid carrier device sector";
    goto bad;
  }
  mc -> carrier_start = tmp;

  if (sscanf(argv[4], "%llu%c", &tmp, &dummy) != 1) {
    ti->error = "dm-matryoshka: Invalid entropy device sector";
    goto bad;
  }
  mc -> entropy_start = tmp;

  vfat_get_header(vfat, mc -> carrier, mc->carrier_start);
  mc -> fs = vfat;

  mc -> num_carrier = 1;
  mc -> num_entropy = 1;
  mc -> num_userdata = 1;

  ti -> num_flush_bios = 1;
  ti -> num_discard_bios = 1;
  ti -> num_write_same_bios = 1;
  ti -> private = mc;

  printk(KERN_DEBUG "dm-matryoshka passphrase: %s: ", mc -> passphrase);
  printk(KERN_DEBUG "dm-matryoshka carrier device: %s: ", argv[1]);
  printk(KERN_DEBUG "dm-matryoshka carrier device starting sector: %lu: ", mc -> carrier_start);
  printk(KERN_DEBUG "dm-matryoshka entropy device: %s: ", argv[3]);
  printk(KERN_DEBUG "dm-matryoshka entropy device starting sector: %lu: ", mc -> entropy_start);
  printk(KERN_DEBUG "dm-matryoshka carrier filesystem type: %x: ", mc -> carrier_fs);

  return 0;

  bad:
    kfree(mc);
    return ret1 && ret2;
}

static void matryoshka_dtr(struct dm_target *ti) {
  struct matryoshka_c *mc = (struct matryoshka_c*) ti -> private;

  dm_put_device(ti, mc -> carrier);
  dm_put_device(ti, mc -> entropy);
  kfree(mc);
}

static int matryoshka_map(struct dm_target *ti, struct bio *bio) {
  int status;

  struct matryoshka_c *mc = (struct matryoshka_c*) ti -> private;

  bio -> bi_bdev = mc -> carrier -> bdev;
  if (bio_sectors(bio)) {
    bio->bi_iter.bi_sector = mc -> carrier_start + dm_target_offset(ti, bio->bi_iter.bi_sector); // TODO Add regular fs free sector
  }
  status = DM_MAPIO_REMAPPED;

  switch (bio_op(bio)) {

    case REQ_OP_READ:
      // Read Operation
      status = matryoshka_read(ti, bio);
      break;

    case REQ_OP_WRITE:
      // Write Operation
      status = matryoshka_write(ti, bio);
      break;

    default:
      printk(KERN_DEBUG "dm-matryoshka bio_op: other");
      break;
  }

  return status;
}

static struct target_type matryoshka_target = {
  .name   = NAME,
  .version = {VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH},
  .module = THIS_MODULE,
  .ctr    = matryoshka_ctr,
  .dtr    = matryoshka_dtr,
  .map    = matryoshka_map,
};

static int __init dm_matryoshka_init(void) {
  int status;
  printk(KERN_INFO "dm-matryoshka: Registering device mapper target, version: %u.%u.%u \n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
  status = dm_register_target(&matryoshka_target);
  if (status < 0) {
    DMERR("dm-matryoshka: Failed to register device mapper target: %d \n", status);
  }
  return status;
}

static void __exit dm_matryoshka_exit(void) {
  printk(KERN_INFO "dm-matryoshka: Unloading device mapper \n");
  dm_unregister_target(&matryoshka_target);
}

module_init(dm_matryoshka_init);
module_exit(dm_matryoshka_exit);

MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_LICENSE(LICENSE);
