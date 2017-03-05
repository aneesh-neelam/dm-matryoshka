#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/device-mapper.h>

#include "../include/dm-matryoshka.h"


/*
 * Construct a matryoshka mapping: <dev_path> <offset>
 */
static int matryoshka_ctr(struct dm_target *ti, unsigned int argc, char **argv) {
  struct matryoshka_c *mc;
  unsigned long long tmp;
  char dummy;
  int ret;

  if (argc != 2) {
    ti -> error = "Invalid number of arguments";
    return -EINVAL;
  }
  
  mc = kmalloc(sizeof(*mc), GFP_KERNEL);
  if (mc == NULL) {
    ti -> error = "Cannot allocate matryoshka context";
    return -ENOMEM;
  }

  ret = -EINVAL;
  if (sscanf(argv[1], "%llu%c", &tmp, &dummy) != 1) {
    ti -> error = "Invalid device sector";
    goto bad;
  }
  mc -> start = tmp;

  ret = dm_get_device(ti, argv[0], dm_table_get_mode(ti -> table), &mc -> dev);
  if (ret) {
    ti->error = "Device lookup failed";
    goto bad;
  }

  ti->num_flush_bios = 1;
  ti->num_discard_bios = 1;
  ti->num_write_same_bios = 1;
  ti->private = mc;
  
  return 0;

  bad: 
    kfree(mc);
    return ret;
}

static void matryoshka_dtr(struct dm_target *ti) {
  struct matryoshka_c *mc = (struct matryoshka_c*) ti -> private;

  dm_put_device(ti, mc -> dev);
  kfree(mc);
}

static int matryoshka_map(struct dm_target *ti, struct bio *bio) {
  return 0;
}

static void matryoshka_status(struct dm_target *ti, status_type_t type, unsigned status_flags, char *result, unsigned maxlen) {

}

static void matryoshka_postsuspend(struct dm_target *ti) {

}

static int matryoshka_preresume(struct dm_target *ti) {
  return 0;
}

static void matryoshka_resume(struct dm_target *ti) {

}

static int matryoshka_message(struct dm_target *ti, unsigned argc, char **argv) {
  return 0;
}

static int matryoshka_iterate_devices(struct dm_target *ti, iterate_devices_callout_fn fn, void *data) {
  return 0;
}

static void matryoshka_io_hints(struct dm_target *ti, struct queue_limits *limits) {

}


static struct target_type matryoshka_target = {
  .name   = NAME,
  .version = {1, 0, 0},
  .module = THIS_MODULE,
  .ctr    = matryoshka_ctr,
  .dtr    = matryoshka_dtr,
  .map    = matryoshka_map,
  .status = matryoshka_status,
  .postsuspend = matryoshka_postsuspend,
  .preresume = matryoshka_preresume,
  .resume = matryoshka_resume,
  .message = matryoshka_message,
  .iterate_devices = matryoshka_iterate_devices,
  .io_hints = matryoshka_io_hints,
};

static int __init dm_matryoshka_init(void) {
  int status;
  printk(KERN_INFO "Init Matryoshka Device Mapper\n");
  status = dm_register_target(&matryoshka_target);
  if (status < 0) {
    DMERR("Failed to Register Matryoshka Device Mapper: %d", status);
  }
  return status;
}

static void __exit dm_matryoshka_exit(void) {
  printk(KERN_INFO "Exit Matryoshka Device Mapper\n");
  dm_unregister_target(&matryoshka_target);
}

module_init(dm_matryoshka_init);
module_exit(dm_matryoshka_exit);

MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_LICENSE(LICENSE);
