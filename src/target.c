#include <linux/device-mapper.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/bio.h>
#include <linux/string.h>
#include <linux/types.h>

#include "../include/carrier.h"
#include "../include/entropy.h"
#include "../include/erasure.h"
#include "../include/fs.h"
#include "../include/target.h"
#include "../include/xor.h"
#include "../include/workqueue.h"


struct bio *mybio_clone(struct bio *sbio)
{
    struct bio *bio = bio_kmalloc(GFP_NOIO, sbio->bi_max_vecs);
    struct bio_vec *bvec, *sbvec;
    unsigned short i;

    if (!bio)
        return NULL;

    memcpy(bio->bi_io_vec, sbio->bi_io_vec,
           sbio->bi_max_vecs * sizeof(struct bio_vec));

    for (i = 0, bvec = bio->bi_io_vec, sbvec = sbio->bi_io_vec;
         i < bio->bi_max_vecs; i++, bvec++, sbvec++) {
        bvec->bv_page = alloc_page(GFP_NOIO);
        if (!bvec->bv_page)
            goto mybio_clone_cleanup;

        bvec->bv_offset = sbvec->bv_offset;
        bvec->bv_len = sbvec->bv_len;
    }

    bio->bi_iter.bi_sector = sbio->bi_iter.bi_sector;
    bio->bi_opf = sbio->bi_opf;
    bio->bi_vcnt = sbio->bi_vcnt;
    bio->bi_iter.bi_size = sbio->bi_iter.bi_size;
    bio->bi_iter.bi_idx = 0;
    return bio;

mybio_clone_cleanup:
    while (i--) {
        bvec--;
        __free_page(bvec->bv_page);
    }
    return NULL;
}

struct bio *custom_bio_clone(struct bio *sbio) {
  struct bio *bio = bio_alloc(GFP_NOIO, bio_segments(sbio));
  struct bio_vec bv;
  struct bvec_iter iter;
  struct page *page;

  bio_for_each_segment(bv, sbio, iter) {
		page = alloc_page(GFP_KERNEL);
    bio_add_page(bio, page, bv.bv_len, bv.bv_offset);
	}

  if (sbio -> bi_next != NULL) {
    bio -> bi_next = custom_bio_clone(sbio -> bi_next);
  }

  return bio;
}

int matryoshka_read(struct dm_target *ti, struct bio *bio) {
  struct matryoshka_context *mc = (struct matryoshka_context*) ti -> private;

  struct bio *newbio = custom_bio_clone(bio);
  newbio -> bi_bdev = mc -> entropy -> dev -> bdev;
  newbio->bi_iter.bi_sector = mc -> entropy -> start + dm_target_offset(ti, bio->bi_iter.bi_sector); // TODO Add regular fs free sector
  int ret = submit_bio_wait(newbio);
  printk(KERN_DEBUG "Successfully read from entropy device: %d", ret);

  bio -> bi_bdev = mc -> carrier -> dev -> bdev;
  if (bio_sectors(bio)) {
    bio->bi_iter.bi_sector = mc -> carrier -> start + dm_target_offset(ti, bio->bi_iter.bi_sector); // TODO Add regular fs free sector
  }


  return DM_MAPIO_REMAPPED;
}

int matryoshka_write(struct dm_target *ti, struct bio *bio) {
  struct matryoshka_context *mc = (struct matryoshka_context*) ti -> private;

  struct bio *newbio = custom_bio_clone(bio);
  newbio -> bi_bdev = mc -> entropy -> dev -> bdev;
  newbio->bi_iter.bi_sector = mc -> entropy -> start + dm_target_offset(ti, bio->bi_iter.bi_sector); // TODO Add regular fs free sector
  int ret = submit_bio_wait(newbio);
  printk(KERN_DEBUG "Successfully read from entropy device: %d", ret);

  bio -> bi_bdev = mc -> carrier -> dev -> bdev;
  if (bio_sectors(bio)) {
    bio->bi_iter.bi_sector = mc -> carrier -> start + dm_target_offset(ti, bio->bi_iter.bi_sector); // TODO Add regular fs free sector
  }

  return DM_MAPIO_REMAPPED;
}

/*
 * Construct a matryoshka mapping: <passphrase> entropy_dev_path> <carrier_dev_path>
 */
static int matryoshka_ctr(struct dm_target *ti, unsigned int argc, char **argv) {
  struct matryoshka_context *context;
  struct matryoshka_device *carrier;
  struct matryoshka_device *entropy;
  struct fs_vfat *vfat;
  int ret1, ret2;
  int passphrase_length;
  unsigned long long tmp;
  char dummy;

  ret1 = ret2 = -EIO;

  if (argc != 6) {
    ti -> error = "dm:matryoshka: Invalid number of arguments for constructor";
    return -EINVAL;
  }

  context = kmalloc(sizeof(*context), GFP_KERNEL);
  carrier = kmalloc(sizeof(*carrier), GFP_KERNEL);
  entropy = kmalloc(sizeof(*entropy), GFP_KERNEL);
  vfat = kmalloc(sizeof(*vfat), GFP_KERNEL);
  if (context == NULL || carrier == NULL || entropy == NULL || vfat == NULL) {
    ti -> error = "dm:matryoshka: Cannot allocate memory for context or device or vfat header";
    return -ENOMEM;
  }

  passphrase_length = strlen(argv[0]);
  context -> passphrase = kmalloc(passphrase_length + 1, GFP_KERNEL);
  if (context -> passphrase == NULL) {
    ti -> error = "dm:matryoshka: Cannot allocate memory for passphrase";
    return -ENOMEM;
  }
  strncpy(context -> passphrase, argv[0], passphrase_length);
  context -> passphrase[passphrase_length] = '\0';

  ret1 = ret2 = -EINVAL;
  ret1 = dm_get_device(ti, argv[1], dm_table_get_mode(ti -> table), &carrier -> dev);
  ret2 = dm_get_device(ti, argv[3], dm_table_get_mode(ti -> table), &entropy -> dev);
  if (ret1) {
    ti -> error = "dm:matryoshka: Carrier device lookup failed";
    goto bad;
  }
  if (ret2) {
    ti -> error = "dm:matryoshka: Entropy device lookup failed";
    goto bad;
  }

  context -> carrier_fs = get_carrier_fs(argv[5]);

  if (sscanf(argv[2], "%llu%c", &tmp, &dummy) != 1) {
    ti->error = "dm-matryoshka: Invalid carrier device sector";
    goto bad;
  }
  carrier -> start = tmp;
  context -> carrier = carrier;

  if (sscanf(argv[4], "%llu%c", &tmp, &dummy) != 1) {
    ti->error = "dm-matryoshka: Invalid entropy device sector";
    goto bad;
  }
  entropy -> start = tmp;
  context -> entropy = entropy;

  vfat_get_header(vfat, carrier -> dev, carrier -> start);
  context -> fs = vfat;

  context -> num_carrier = 1;
  context -> num_entropy = 1;
  context -> num_userdata = 1;

  ti -> num_flush_bios = 1;
  ti -> num_discard_bios = 1;
  ti -> num_write_same_bios = 1;

  ti -> private = context;

  /* Initilize kmatryoshkad thread: */
  context -> matryoshka_wq = create_singlethread_workqueue("kmatryoshkad");
  if (!context -> matryoshka_wq) {
      DMERR("Couldn't start kxord");
      ret1 = ret2 = -ENOMEM;
  }
  INIT_WORK(&context -> matryoshka_work, kmatryoshkad_do);

  printk(KERN_DEBUG "dm-matryoshka passphrase: %s: ", context -> passphrase);
  printk(KERN_DEBUG "dm-matryoshka carrier device: %s: ", argv[1]);
  printk(KERN_DEBUG "dm-matryoshka carrier device starting sector: %lu: ", context -> carrier -> start);
  printk(KERN_DEBUG "dm-matryoshka entropy device: %s: ", argv[3]);
  printk(KERN_DEBUG "dm-matryoshka entropy device starting sector: %lu: ", context -> entropy -> start);
  printk(KERN_DEBUG "dm-matryoshka carrier filesystem type: %x: ", context -> carrier_fs);

  return 0;

  bad:
    destroy_workqueue(context -> matryoshka_wq);
    kfree(context);
    kfree(carrier);
    kfree(entropy);
    kfree(vfat);
    return ret1 && ret2;
}

static void matryoshka_dtr(struct dm_target *ti) {
  struct matryoshka_context *context = (struct matryoshka_context*) ti -> private;
  struct matryoshka_device *carrier = (struct matryoshka_device*) context -> carrier;
  struct matryoshka_device *entropy = (struct matryoshka_device*) context -> entropy;
  struct fs_vfat *vfat = (struct fs_vfat*) context -> fs;

  flush_workqueue(context->matryoshka_wq);
  flush_scheduled_work();
  destroy_workqueue(context->matryoshka_wq);

  dm_put_device(ti, carrier -> dev);
  dm_put_device(ti, entropy -> dev);

  kfree(context);
  kfree(carrier);
  kfree(entropy);
  kfree(vfat);
}

static int matryoshka_map(struct dm_target *ti, struct bio *bio) {
  struct matryoshka_context *mc = (struct matryoshka_context*) ti -> private;

  switch (bio_op(bio)) {
    case REQ_OP_READ:
      // Read Operation
      //kmatryoshkad_queue_bio(mc, bio);
      //return DM_MAPIO_SUBMITTED;
      return matryoshka_read(ti, bio);

    case REQ_OP_WRITE:
      // Write Operation
      //kmatryoshkad_queue_bio(mc, bio);
      //return DM_MAPIO_SUBMITTED;
      return matryoshka_read(ti, bio);

    default:
      bio -> bi_bdev = mc -> carrier -> dev -> bdev;
      if (bio_sectors(bio)) {
        bio->bi_iter.bi_sector = mc -> carrier -> start + dm_target_offset(ti, bio->bi_iter.bi_sector); // TODO Add regular fs free sector
      }
      return DM_MAPIO_REMAPPED;
  }
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
