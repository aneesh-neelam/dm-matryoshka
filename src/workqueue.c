#include <linux/device-mapper.h>

#include "../include/workqueue.h"
#include "../include/erasure.h"
#include "../include/xor.h"
#include "../include/target.h"


void kmatryoshkad_queue_io(struct matryoshka_io *io) {
  struct matryoshka_context *mc = io->mc;

  INIT_WORK(&io->work, kmatryoshkad_do);
  queue_work(mc->matryoshka_wq, &io->work);
}

void kmatryoshkad_init_dev_bio(struct matryoshka_context *mc, struct bio *bio, struct matryoshka_device *d, struct matryoshka_io *io, bio_end_io_t ep) {
  bio->bi_bdev = d->dev->bdev;
  if (bio_sectors(bio)) {
    bio->bi_iter.bi_sector = d->start + dm_target_offset(mc->ti, bio->bi_iter.bi_sector);
  }
  // bio->bi_private = io;
  // bio->bi_end_io = ep;
}

static void kmatryoshkad_end_read(struct bio *bio) {
  bio_endio(bio);
}

static void kmatryoshkad_do_read(struct matryoshka_io *io) {
  struct matryoshka_context *mc = io->mc;
  kmatryoshkad_init_dev_bio(mc, io->base_bio, mc -> carrier, io, kmatryoshkad_end_read);

  generic_make_request(io->base_bio);

  printk(KERN_DEBUG "Submitted read bio in kmatryoshkad");
}

static void kmatryoshkad_end_write(struct bio *bio) {
  bio_endio(bio);
}

static void kmatryoshkad_do_write(struct matryoshka_io *io) {
  struct matryoshka_context *mc = io->mc;
  kmatryoshkad_init_dev_bio(mc, io->base_bio, mc->carrier, io, kmatryoshkad_end_write);

  generic_make_request(io->base_bio);

  printk(KERN_DEBUG "Submitted write bio in kmatryoshkad");
}

void kmatryoshkad_do(struct work_struct *work) {
  struct matryoshka_io *io = container_of(work, struct matryoshka_io, work);

  if (bio_data_dir(io->base_bio) == READ) {
    kmatryoshkad_do_read(io);
  } else {
    kmatryoshkad_do_write(io);
  }
}
