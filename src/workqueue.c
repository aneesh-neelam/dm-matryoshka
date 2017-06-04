#include "../include/workqueue.h"
#include "../include/erasure.h"
#include "../include/xor.h"
#include "../include/target.h"

static void wakeup_kmatryoshkad(struct matryoshka_context *context) {
    queue_work(context -> matryoshka_wq, &context -> matryoshka_work);
}

static void kmatryoshkad_queue_entropy_bio(struct matryoshka_context *context, struct bio *bio) {
    unsigned long flags;
    int should_wake = 0; /* Whether kmatryoshkad should be waken up. */

    spin_lock_irqsave(&context->lock, flags);
    should_wake = !(context->entropy_bios.head);
    bio_list_add(&context->entropy_bios, bio);
    spin_unlock_irqrestore(&context->lock, flags);

    if (should_wake)
        kmatryoshkad(context);
}

static struct bio **kmatryoshkad_init_bios(struct bio *src, unsigned int count) {
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

inline void kmatryoshkad_init_dev_bio(struct bio *bio, struct matryoshka_device *d, struct io *io, bio_end_io_t ep) {
    bio->bi_bdev = d->dev->bdev;
    bio->bi_iter.bi_sector += d->start;
    bio->bi_private = io;
    bio->bi_end_io = ep;
}
