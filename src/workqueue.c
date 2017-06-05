#include "../include/workqueue.h"
#include "../include/erasure.h"
#include "../include/xor.h"
#include "../include/target.h"


void wakeup_kmatryoshkad(struct matryoshka_context *context) {
    queue_work(context -> matryoshka_wq, &context -> matryoshka_work);
}

void kmatryoshkad_queue_bio(struct matryoshka_context *context, struct bio *bio) {
    unsigned long flags;
    int should_wake = 0; /* Whether kmatryoshkad should be waken up. */

    spin_lock_irqsave(&context->lock, flags);
    should_wake = !(context->bios.head);
    bio_list_add(&context->bios, bio);
    spin_unlock_irqrestore(&context->lock, flags);

    if (should_wake)
        wakeup_kmatryoshkad(context);
}

struct bio **kmatryoshkad_init_bios(struct bio *src, unsigned int count) {
    struct bio **bios = kmalloc(count * sizeof(struct bio*), GFP_NOIO);
    unsigned i;

    if (!bios)
        goto kmatryoshkad_init_bios_cleanup;

    for (i = 0; i < count; i++) {
        /* Clone the BIO for a specific xor device: */
        bios[i] = mybio_clone(src);
        if (!bios[i])
            goto kmatryoshkad_init_bios_cleanup_bios;
    }
    return bios;

    kmatryoshkad_init_bios_cleanup_bios:
      while (i--)
        bio_put(bios[i]);

    kfree(bios);
    kmatryoshkad_init_bios_cleanup:
      return NULL;
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
  kmatryoshkad_init_dev_bio(bio, mc -> carrier, NULL, kmatryoshkad_end_read);
  bio -> bi_end_io = kmatryoshkad_end_read;

  submit_bio(bio);
}

static void kmatryoshkad_end_write(struct bio *bio) {
  bio_endio(bio);
}

static void kmatryoshkad_do_write(struct matryoshka_context *mc, struct bio *bio) {
  kmatryoshkad_init_dev_bio(bio, mc -> carrier, NULL);
  bio -> bi_end_io = kmatryoshkad_end_write;

  submit_bio(bio);
}

void kmatryoshkad_do(struct work_struct *work) {
  struct matryoshka_context *mc = container_of(work, struct matryoshka_context, matryoshka_work);

  struct bio_list bios;
  struct bio *bio;
  unsigned long flags;

  spin_lock_irqsave(&mc->lock, flags);
  bios = mc->bios;
  bio_list_init(&mc->bios);
  spin_unlock_irqrestore(&mc->lock, flags);

  while ((bio = bio_list_pop(&bios))) {
      if (bio_data_dir(bio) == READ)
          kmatryoshkad_do_read(mc, bio);
      else
          kmatryoshkad_do_write(mc, bio);
  }
}
