#include "../include/workqueue.h"
#include "../include/erasure.h"
#include "../include/xor.h"
#include "../include/target.h"


void wakeup_kmatryoshkad(struct matryoshka_context *context) {
    queue_work(context -> matryoshka_wq, &context -> matryoshka_work);
}

void kmatryoshkad_queue_bio(struct matryoshka_context *context, struct bio *bio) {
    int should_wake = 0; // Whether kmatryoshkad should be waken up

    mutex_lock(&(context->lock));

    should_wake = !(context->bios.head);
    bio_list_add(&(context->bios), bio);

    mutex_unlock(&(context->lock));

    if (should_wake) {
      wakeup_kmatryoshkad(context);
    }
}

void kmatryoshkad_init_dev_bio(struct bio *bio, struct matryoshka_device *d, struct io *io, bio_end_io_t ep) {
    bio->bi_bdev = d->dev->bdev;
    bio->bi_iter.bi_sector += d->start;
    bio->bi_private = io;
    bio->bi_end_io = ep;
}

static void kmatryoshkad_end_read(struct bio *bio) {
  bio_endio(bio);
}

static void kmatryoshkad_do_read(struct matryoshka_context *mc, struct bio *bio) {
  kmatryoshkad_init_dev_bio(bio, mc -> carrier, bio -> bi_private, kmatryoshkad_end_read);

  submit_bio(bio);
}

static void kmatryoshkad_end_write(struct bio *bio) {
  bio_endio(bio);
}

static void kmatryoshkad_do_write(struct matryoshka_context *mc, struct bio *bio) {
  kmatryoshkad_init_dev_bio(bio, mc -> carrier, bio -> bi_private, kmatryoshkad_end_write);

  submit_bio(bio);
}

void kmatryoshkad_do(struct work_struct *work) {
  struct matryoshka_context *context = container_of(work, struct matryoshka_context, matryoshka_work);

  struct bio_list bios;
  struct bio *bio;

  mutex_lock(&(context->lock));

  bios = context->bios;
  bio_list_init(&context->bios);

  mutex_unlock(&(context->lock));

  while ((bio = bio_list_pop(&bios))) {
      if (bio_data_dir(bio) == READ)
          kmatryoshkad_do_read(context, bio);
      else
          kmatryoshkad_do_write(context, bio);
  }
}
