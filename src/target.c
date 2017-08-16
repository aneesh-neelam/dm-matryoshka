#include <linux/device-mapper.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/bio.h>
#include <linux/string.h>
#include <linux/types.h>

#include "../include/utility.h"
#include "../include/fs.h"
#include "../include/fs_fat.h"
#include "../include/target.h"
#include "../include/workqueue.h"


/*
 * Construct a matryoshka mapping: <passphrase> entropy_dev_path> <carrier_dev_path>
 */
static int matryoshka_ctr(struct dm_target *ti, unsigned int argc, char **argv) {
  struct matryoshka_context *context;
  struct matryoshka_device *carrier;
  struct matryoshka_device *entropy;
  struct fs_fat *fat;

  int str_len;

  int status = -EIO;

  // Check number of arguments
  if (argc != 8) {
    ti->error = "dm:matryoshka: Invalid number of arguments for constructor";
    return -EINVAL;
  }

  // Allocate memory for data structures
  context = kmalloc(sizeof(struct matryoshka_context), GFP_KERNEL);
  carrier = kmalloc(sizeof(struct matryoshka_device), GFP_KERNEL);
  entropy = kmalloc(sizeof(struct matryoshka_device), GFP_KERNEL);
  fat = kmalloc(sizeof(struct fs_fat), GFP_KERNEL);
  if (context == NULL || carrier == NULL || entropy == NULL || fat == NULL) {
    ti->error = "dm:matryoshka: Cannot allocate memory for context or device or fs header";
    return -ENOMEM;
  }

  // Initilize the mutex(es)
  mutex_init(&(context->lock));

  // Initilize kmatryoshkad thread
  context->matryoshka_wq = alloc_workqueue("kmatryoshkad", WQ_HIGHPRI | WQ_CPU_INTENSIVE | WQ_MEM_RECLAIM, 1);
  if (!context->matryoshka_wq) {
      DMERR("Couldn't start kmatryoshkad");
      status = -ENOMEM;
      goto bad;
  }

  // Parse passphrase from arguments
  str_len = strlen(argv[0]);
  context->passphrase = kmalloc(str_len + 1, GFP_KERNEL);
  if (context->passphrase == NULL) {
    ti->error = "dm:matryoshka: Cannot allocate memory for passphrase in context";
    status = -ENOMEM;
    goto bad;
  }
  strncpy(context->passphrase, argv[0], str_len);
  context->passphrase[str_len] = '\0';

  // Parse num_carrier (m) from arguments
  status = convertStringToU8(&(context->num_carrier), argv[1]);
  if (status) {
    ti->error = "dm-matryoshka: Invalid number of carrier blocks";
    status = -EINVAL;
    goto bad;
  }

  // Parse num_entropy (k) from arguments
  status = convertStringToU8(&(context->num_entropy), argv[2]);
  if (status) {
    ti->error = "dm-matryoshka: Invalid number of entropy blocks";
    status = -EINVAL;
    goto bad;
  }

  // num_carrier must be less than num_entropy + 1
  status = checkRatio(context->num_carrier, context->num_entropy);
  if (status) {
    ti->error = "dm-matryoshka: Invalid ratio of entropy blocks to carrier blocks";
    status = -EINVAL;
    goto bad;
  }

  // Open carrier device
  status = dm_get_device(ti, argv[3], dm_table_get_mode(ti->table), &carrier->dev);
  if (status) {
    ti->error = "dm:matryoshka: Carrier device lookup failed";
    status = -EIO;
    goto bad;
  }

  // Open entropy device
  status = dm_get_device(ti, argv[5], dm_table_get_mode(ti->table), &entropy->dev);
  if (status) {
    ti->error = "dm:matryoshka: Entropy device lookup failed";
    status = -EIO;
    goto bad;
  }

  // Starting sector for carrier device
  status = convertStringToSector_t(&(carrier->start), argv[4]);
  if (status) {
    ti->error = "dm-matryoshka: Invalid carrier device sector";
    status = -EINVAL;
    goto bad;
  }

  // Starting sector for entropy device
  status = convertStringToSector_t(&(entropy->start), argv[6]);
  if (status) {
    ti->error = "dm-matryoshka: Invalid entropy device sector";
    status = -EINVAL;
    goto bad;
  }

  // Assign devices to context
  context->carrier = carrier;
  context->entropy = entropy;

  // Parse FS type
  context->carrier_fs = get_carrier_fs(argv[7]);
  if (context->carrier_fs == FS_FAT) {
    status = fat_get_header(fat, carrier->dev, carrier->start);
    if (status) {
      ti->error = "dm-matryoshka: Error parsing FAT file system on carrier device";
      status = -EIO;
      goto bad;
    }
    context->cluster_count = fat->totalClusters;
    context->cluster_size = fat->bytesPerCluster;
    context->sector_size = fat->bytesPerSector;
    context->sectors_per_cluster = fat->sectorsPerCluster;
    context->fs = fat;
  } else {
    status = -EINVAL;
    ti->error = "dm-matryoshka: Specified file system not supported on carrier device";
    goto bad;
  }

  // Get FS name from type
  if (context->carrier_fs == FS_FAT) {
    context->carrier_fs_name = get_fat_type_name(fat);
  } else {
    context->carrier_fs_name = get_fs_name(context->carrier_fs);
  }

  // Target config
  ti->num_flush_bios = 1;
  ti->num_discard_bios = 0;
  ti->num_write_same_bios = 1;

  // Target private data has context
  context->ti = ti;
  ti->private = context;

  printk(KERN_DEBUG "dm-matryoshka: Passphrase: %s: ", context->passphrase);
  printk(KERN_DEBUG "dm-matryoshka: Number of carrier blocks per IO: %u: ", context->num_carrier);
  printk(KERN_DEBUG "dm-matryoshka: Number of entropy blocks per IO: %u: ", context->num_entropy);
  printk(KERN_DEBUG "dm-matryoshka: Carrier device: %s: ", argv[3]);
  printk(KERN_DEBUG "dm-matryoshka: Carrier device starting sector: %lu: ", context->carrier->start);
  printk(KERN_DEBUG "dm-matryoshka: entropy device: %s: ", argv[5]);
  printk(KERN_DEBUG "dm-matryoshka: Entropy device starting sector: %lu: ", context->entropy->start);
  printk(KERN_DEBUG "dm-matryoshka: Carrier filesystem type: %s: ", context->carrier_fs_name);

  return 0;

  bad:
    destroy_workqueue(context->matryoshka_wq);
    kfree(context);
    kfree(carrier);
    kfree(entropy);
    kfree(fat);
    return status;
}

static void matryoshka_dtr(struct dm_target *ti) {
  struct matryoshka_context *context = (struct matryoshka_context*) ti->private;
  struct matryoshka_device *carrier = (struct matryoshka_device*) context->carrier;
  struct matryoshka_device *entropy = (struct matryoshka_device*) context->entropy;
  struct fs_fat *fat = (struct fs_fat*) context->fs;

  flush_workqueue(context->matryoshka_wq);
  flush_scheduled_work();
  destroy_workqueue(context->matryoshka_wq);

  dm_put_device(ti, carrier->dev);
  dm_put_device(ti, entropy->dev);

  kfree(context);
  kfree(carrier);
  kfree(entropy);
  kfree(fat);
}

static int matryoshka_map(struct dm_target *ti, struct bio *bio) {
  struct matryoshka_context *mc = (struct matryoshka_context*) ti->private;
  struct matryoshka_io *io;

  // Split bio if larger than size of cluster
  if (bio_sectors(bio) > mc->sectors_per_cluster) {
    dm_accept_partial_bio(bio, mc->sectors_per_cluster);
  }

  io = init_matryoshka_io(mc, bio);
  if (io == NULL) {
    return -ENOMEM;
  }

  kmatryoshkad_queue_io(io);

  return DM_MAPIO_SUBMITTED;
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
