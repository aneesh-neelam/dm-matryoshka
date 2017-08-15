#include <linux/device-mapper.h>

#include "../include/workqueue.h"
#include "../include/erasure.h"
#include "../include/xor.h"
#include "../include/target.h"
#include "../include/utility.h"


void kmatryoshkad_queue_io(struct matryoshka_io *io) {
  struct matryoshka_context *mc = io->mc;

  INIT_WORK(&io->work, kmatryoshkad_do);
  queue_work(mc->matryoshka_wq, &io->work);
}

static void kmatryoshkad_end_read(struct bio *bio) {
  struct matryoshka_io *io = bio->bi_private;
  struct matryoshka_context *mc = io->mc;

  mutex_lock(&(io->lock));
  if (bio->bi_error) {
    io_accumulate_error(io, bio->bi_error);
    mutex_unlock(&(io->lock));
  } else if (!io->error) {
      if (1 == atomic_read(&(io->erasure_done))) {
        mutex_unlock(&(io->lock));
  
        mybio_free(bio);
  
        bio = io->base_bio;
        bio->bi_error = io->error;
  
        kfree(io->carrier_bios);
        kfree(io->entropy_bios);
        kfree(io);
  
        bio_endio(bio);
      } else if (1 > atomic_read(&(io->carrier_done))) {
        mybio_copy_data(bio, io->carrier_bios[atomic_read(&(io->carrier_done))]);
        atomic_inc(&(io->carrier_done));

        mutex_unlock(&(io->lock));
      } else {
      mybio_copy_data(bio, io->carrier_bios[atomic_read(&(io->carrier_done))]);
      atomic_inc(&(io->carrier_done));

      mybio_xor_copy(io->carrier_bios[0], io->carrier_bios[1], io->base_bio);
      atomic_inc(&(io->erasure_done));

      mutex_unlock(&(io->lock));
    } else {
      mutex_unlock(&(io->lock));

      mybio_free(bio);

      bio = io->base_bio;
      bio->bi_error = io->error;

      kfree(io->carrier_bios);
      kfree(io->entropy_bios);
      kfree(io);

      endio(io->base_bio);
    }
  }
}

static void kmatryoshkad_do_read(struct matryoshka_io *io) {
  struct matryoshka_context *mc = io->mc;

  struct bio *carrier = mybio_clone(io->base_bio);
  struct bio *entropy = mybio_clone(io->base_bio);

  mybio_init_dev(mc, carrier, mc->carrier, io, kmatryoshkad_end_read);
  mybio_init_dev(mc, entropy, mc->entropy, io, kmatryoshkad_end_read);

  generic_make_request(carrier);
  generic_make_request(entropy);
}

static void kmatryoshkad_end_write(struct bio *bio) {
  struct matryoshka_io *io = bio->bi_private;
  struct matryoshka_context *mc = io->mc;
  
  mutex_lock(&(io->lock));
  if (bio->bi_error) {
    io_accumulate_error(io, bio->bi_error);
    mutex_unlock(&(io->lock));
  } else if (!io->error) {
    if (1 == atomic_read(&(io->erasure_done))) {
      mutex_unlock(&(io->lock));

      mybio_free(bio);

      bio = io->base_bio;
      bio->bi_error = io->error;

      kfree(io->carrier_bios);
      kfree(io->entropy_bios);
      kfree(io);
  
      bio_endio(bio);
    } else if (1 > atomic_read(&(io->entropy_done))) {
      mybio_copy_data(bio, io->entropy_bios[atomic_read(&(io->entropy_done))]);
      atomic_inc(&(io->entropy_done));

      mybio_xor_assign(io->entropy_bios[0], io->base_bio);
      atomic_inc(&(io->erasure_done));
      
      mutex_unlock(&(io->lock));

      mybio_init_dev(mc, io->base_bio, mc->carrier, io, kmatryoshkad_end_write);
      
      generic_make_request(io->base_bio);
    }
  } else {
    mutex_unlock(&(io->lock));

    mybio_free(bio);

    bio = io->base_bio;
    bio->bi_error = io->error;

    kfree(io->carrier_bios);
    kfree(io->entropy_bios);
    kfree(io);

    bio_endio(bio);
  }
}

static void kmatryoshkad_do_write(struct matryoshka_io *io) {
  struct matryoshka_context *mc = io->mc;
  
  // struct bio *carrier = mybio_clone(io->bsse_bio);
  struct bio *entropy = mybio_clone(io->base_bio);
  entropy->bi_opf = REQ_OP_READ;

  // mybio_init_dev(mc, carrier, mc -> carrier, io, kmatryoshkad_end_read);
  mybio_init_dev(mc, entropy, mc->entropy, io, kmatryoshkad_end_write);

  generic_make_request(entropy);
}

void kmatryoshkad_do(struct work_struct *work) {
  struct matryoshka_io *io = container_of(work, struct matryoshka_io, work);

  if (bio_data_dir(io->base_bio) == READ) {
    kmatryoshkad_do_read(io);
  } else {
    kmatryoshkad_do_write(io);
  }
}
