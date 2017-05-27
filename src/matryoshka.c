#include <linux/device-mapper.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>

#include "../include/matryoshka.h"
#include "../include/matryoshka_vfat.h"


/*
 * Helper functions for dm-matryoshka
 */
u8 get_carrier_fs(char *fs) {
  if (strncmp("vfat", fs, 4) == 0) {
    return FS_VFAT;
  } else if (strncmp("exfat", fs, 5) == 0) {
    return FS_EXFAT;
  } else if (strncmp("ext4", fs, 4) == 0) {
    return FS_EXT4;
  } else if (strncmp("btrfs", fs, 5) == 0) {
    return FS_BTRFS;
  } else if (strncmp("ntfs", fs, 4) == 0) {
    return FS_NTFS;
  } else if (strncmp("zfs", fs, 3) == 0) {
    return FS_ZFS;
  } else {
    return FS_UNKNOWN;
  }
}

int get_entropy_blocks(struct dm_dev *entropy) {
  // TODO in the context of bio, this function may not be necessary

  return 0;
}


int erasure_encode(struct bio_vec *carrier, struct bio_vec *userdata, struct bio_vec *entropy) {
  // TODO link with a C erasure library

  return 0;
}

int erasure_reconstruct(struct bio_vec *userdata, struct bio_vec *carrier, struct bio_vec *entropy) {
  // TODO link with a C erasure library

  return 0;
}


int xor_data(char *output, const char *input1, const char *input2, u64 length) {
  u64 i;

  if (output == NULL || input1 == NULL || input2 == NULL) {
    return -EIO;
  }

  for (i = 0; i < length; ++i) {
        output[i] = input1[i] ^ input2[i];
  }

  return 0;
}

int matryoshka_read(struct dm_target *ti, struct bio *bio) {
  int entropy_status;
  int freelist_status;

  struct matryoshka_c *mc = (struct matryoshka_c*) ti -> private;

  entropy_status = get_entropy_blocks(mc -> entropy);
  // TODO corresponding entropy block in callback

  if (mc -> carrier_fs == FS_VFAT) {
    freelist_status = vfat_get_free_blocks(mc -> carrier);
    // TODO Find free sector from underlying file system
  } else {
    return -EIO;
  }

  // TODO erasure code entropy block and carrier block, modify bio and submit

  return DM_MAPIO_REMAPPED;
}

int matryoshka_write(struct dm_target *ti, struct bio *bio) {
  int entropy_status;
  int freelist_status;

  struct matryoshka_c *mc = (struct matryoshka_c*) ti -> private;

  entropy_status = get_entropy_blocks(mc -> entropy);
  // TODO corresponding entropy block in callback

  if (mc -> carrier_fs == FS_VFAT) {
    freelist_status = vfat_get_free_blocks(mc -> carrier);
    // TODO Find free sector from underlying file system
  } else {
    return -EIO;
  }

  // TODO erasure code entropy block and carrier block, modify bio and submit

  return DM_MAPIO_REMAPPED;
}

/*
 * Construct a matryoshka mapping: <passphrase> entropy_dev_path> <carrier_dev_path>
 */
static int matryoshka_ctr(struct dm_target *ti, unsigned int argc, char **argv) {
  struct matryoshka_c *mc;

  int ret1, ret2;
  int passphrase_length;
  unsigned long long tmp;
  char dummy;

  if (argc != 5) {
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

  ti -> num_flush_bios = 1;
  ti -> num_discard_bios = 1;
  ti -> num_write_same_bios = 1;
  ti -> private = mc;

  printk(KERN_DEBUG "dm-matryoshka Passphrase: %s: ", mc -> passphrase);
  printk(KERN_DEBUG "dm-matryoshka Carrier Device: %s: ", argv[1]);
  printk(KERN_DEBUG "dm-matryoshka Entropy Device: %s: ", argv[2]);
  printk(KERN_DEBUG "dm-matryoshka Carrier Filesystem Type: %x: ", mc -> carrier_fs);

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
    bio->bi_iter.bi_sector = mc -> start + dm_target_offset(ti, bio->bi_iter.bi_sector); // TODO Add regular fs free sector
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
