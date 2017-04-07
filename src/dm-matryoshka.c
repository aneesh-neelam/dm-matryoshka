#include <linux/device-mapper.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>

#include "../include/dm-matryoshka.h"


/*
 * Helper functions for dm-matryoshka
 */


/*
 * Construct a matryoshka mapping: <passphrase> entropy_dev_path> <carrier_dev_path>
 */
static int matryoshka_ctr(struct dm_target *ti, unsigned int argc, char **argv) {
  struct matryoshka_c *mc;
  int ret1, ret2;
  int passphrase_length;

  if (argc != 2) {
    ti -> error = "Invalid number of arguments for dm-matryoshka constructor";
    return -EINVAL;
  }

  mc = kmalloc(sizeof(*mc), GFP_KERNEL);
  if (mc == NULL) {
    ti -> error = "Cannot allocate dm-matryoshka context";
    return -ENOMEM;
  }

  passphrase_length = strlen(argv[0]);
  mc -> passphrase = kmalloc(passphrase_length, GFP_KERNEL);
  strncpy(mc -> passphrase, argv[0], passphrase_length);
  mc -> passphrase[passphrase_length] = '\0';

  ret1 = ret2 = -EINVAL;

  ret1 = dm_get_device(ti, argv[1], dm_table_get_mode(ti -> table), &mc -> entropy);
  ret2 = dm_get_device(ti, argv[2], dm_table_get_mode(ti -> table), &mc -> carrier);
  if (ret1 || ret2) {
    ti -> error = "Device lookup failed";
    goto bad;
  }

  ti -> num_flush_bios = 1;
  ti -> num_discard_bios = 1;
  ti -> num_write_same_bios = 1;
  ti -> private = mc;

  return 0;

  bad:
    kfree(mc);
    return ret1 && ret2;
}

static void matryoshka_dtr(struct dm_target *ti) {
  struct matryoshka_c *mc = (struct matryoshka_c*) ti -> private;

  dm_put_device(ti, mc -> carrier);
  kfree(mc);
}

static int matryoshka_map(struct dm_target *ti, struct bio *bio) {

  switch (bio_op(bio)) {

    case REQ_OP_READ:
      // Read Operation
      break;

    case REQ_OP_WRITE:
      // Write Operation
      break;

    default:
      return -EIO;
  }

  bio_endio(bio);

  return DM_MAPIO_SUBMITTED;

  /*
  linear_map_bio(ti, bio);

	return DM_MAPIO_REMAPPED;
  */
}

static void matryoshka_status(struct dm_target *ti, status_type_t type, unsigned status_flags, char *result, unsigned maxlen) {
  /*
  struct matryoshka_c *mc = ti -> private;

  switch (type) {
    case STATUSTYPE_INFO:
      result[0] = '\0';
      break;

    case STATUSTYPE_TABLE:
      snprintf(result, maxlen, "%s %llu", mc -> dev -> name, (unsigned long long) mc -> start);
      break;
  }
  */
}

static int matryoshka_iterate_devices(struct dm_target *ti, iterate_devices_callout_fn fn, void *data) {
  /*
  struct matryoshka_c *mc = ti -> private;

  return fn(ti, mc -> dev, mc -> start, ti-> len, data);
  */
  return 0;
}

static int matryoshka_prepare_ioctl(struct dm_target *ti, struct block_device **bdev, fmode_t *mode) {
  /*
  struct matryoshka_c *mc = ti -> private;
  struct dm_dev *dev = mc -> dev;

  *bdev = dev -> bdev;


	  Only pass ioctls through if the device sizes match exactly.

	if (mc -> start || ti -> len != i_size_read(dev -> bdev -> bd_inode) >> SECTOR_SHIFT) {
		return 1;
  }
  */
	return 0;
}

static long matryoshka_direct_access(struct dm_target *ti, sector_t sector, void **kaddr, pfn_t *pfn, long size) {
  /*
  struct matryoshka_c *mc = ti -> private;
  struct block_device *bdev = mc -> dev -> bdev;
  struct blk_dax_ctl dax = {
    .sector = linear_map_sector(ti, sector),
    .size = size,
  };
  long ret;

  ret = bdev_direct_access(bdev, &dax);
  *kaddr = dax.addr;
  *pfn = dax.pfn;

  return ret;
  */
  return 0;
}

static struct target_type matryoshka_target = {
  .name   = NAME,
  .version = {VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH},
  .module = THIS_MODULE,
  .ctr    = matryoshka_ctr,
  .dtr    = matryoshka_dtr,
  .map    = matryoshka_map,
  .status = matryoshka_status,
  .iterate_devices = matryoshka_iterate_devices,
  .prepare_ioctl = matryoshka_prepare_ioctl,
  .direct_access = matryoshka_direct_access,
};

static int __init dm_matryoshka_init(void) {
  int status;
  printk(KERN_INFO "Loading Matryoshka Device Mapper, Version: %u.%u.%u \n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
  status = dm_register_target(&matryoshka_target);
  if (status < 0) {
    DMERR("Failed to Register Matryoshka Device Mapper: %d", status);
  }
  return status;
}

static void __exit dm_matryoshka_exit(void) {
  printk(KERN_INFO "Unloading Matryoshka Device Mapper \n");
  dm_unregister_target(&matryoshka_target);
}

module_init(dm_matryoshka_init);
module_exit(dm_matryoshka_exit);

MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_LICENSE(LICENSE);
