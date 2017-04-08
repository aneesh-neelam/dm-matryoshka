#include <linux/device-mapper.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>

#include "../include/dm-matryoshka.h"


/*
 * Helper functions for dm-matryoshka
 */
static int matryoshka_read(struct dm_target *ti, struct bio *bio) {
  // TODO
  return 0;
}

static int matryoshka_write(struct dm_target *ti, struct bio *bio) {
  // TODO
  return 0;
}

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
  dm_put_device(ti, mc -> entropy);
  kfree(mc);
}

static int matryoshka_map(struct dm_target *ti, struct bio *bio) {

  switch (bio_op(bio)) {

    case REQ_OP_READ:
      // Read Operation
      status = matryoshka_read(ti, bio);
      break;

    case REQ_OP_WRITE:
      // Write Operation
      status = matryoshka_read(ti, bio);
      break;

    default:
      status = -EIO;
  }

  if (status != 0) {
    return status;
  }

  bio_endio(bio);
  return DM_MAPIO_SUBMITTED;
}

static void matryoshka_status(struct dm_target *ti, status_type_t type, unsigned status_flags, char *result, unsigned maxlen) {

}

static int matryoshka_iterate_devices(struct dm_target *ti, iterate_devices_callout_fn fn, void *data) {
  return 0;
}

static int matryoshka_prepare_ioctl(struct dm_target *ti, struct block_device **bdev, fmode_t *mode) {
	return 0;
}

static long matryoshka_direct_access(struct dm_target *ti, sector_t sector, void **kaddr, pfn_t *pfn, long size) {
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
