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

static void kmatryoshkad_end_entropy_read(struct bio *bio) {
  struct matryoshka_io *io = bio->bi_private;
  struct matryoshka_context *mc = io->mc;

  int i;

  atomic_inc(&(io->entropy_done));

  if (atomic_read(&(io->entropy_done)) == mc->num_entropy) {
    for (i = 0; i < mc->num_entropy; ++i) {
      bio_put(io->entropy_bios[i]);
    }
  }
}

static void kmatryoshkad_end_carrier_read(struct bio *bio) {
  struct matryoshka_io *io = bio->bi_private;
  struct matryoshka_context *mc = io->mc;
}

static void kmatryoshkad_end_carrier_write(struct bio *bio) {
  struct matryoshka_io *io = bio->bi_private;
  struct matryoshka_context *mc = io->mc;
}

static void kmatryoshkad_do_read(struct matryoshka_io *io) {
  struct matryoshka_context *mc = io->mc;
  int i;

  for (i = 0; i < mc->num_entropy; ++i) {
    io->entropy_bios[i] = matryoshka_alloc_bio(io->base_bio->bi_iter.bi_size);

    matryoshka_bio_init(io->entropy_bios[i], io, kmatryoshkad_end_entropy_read, READ);
    matryoshka_bio_init_linear(mc, io->entropy_bios[i], mc->entropy, io);

    generic_make_request(io->entropy_bios[i]);
  }

  for (i = 0; i < mc->num_carrier; ++i) {
    io->carrier_bios[i] = matryoshka_alloc_bio(io->base_bio->bi_iter.bi_size);
  }

  generic_make_request(io->base_bio);
}

static void kmatryoshkad_do_write(struct matryoshka_io *io) {
  struct matryoshka_context *mc = io->mc;
  int i;

  for (i = 0; i < mc->num_entropy; ++i) {
    io->entropy_bios[i] = matryoshka_alloc_bio(io->base_bio->bi_iter.bi_size);

    matryoshka_bio_init(io->entropy_bios[i], io, kmatryoshkad_end_entropy_read, READ);
    matryoshka_bio_init_linear(mc, io->entropy_bios[i], mc->entropy, io);
    
    generic_make_request(io->entropy_bios[i]);
  }

  for (i = 0; i < mc->num_carrier; ++i) {
    io->carrier_bios[i] = matryoshka_alloc_bio(io->base_bio->bi_iter.bi_size);
  }

  generic_make_request(io->base_bio);
}

void kmatryoshkad_do(struct work_struct *work) {
  struct matryoshka_io *io = container_of(work, struct matryoshka_io, work);
  struct matryoshka_context *mc = io->mc;

  matryoshka_bio_init_linear(mc, io->base_bio, mc->carrier, io);

  if (bio_data_dir(io->base_bio) == READ) {
    kmatryoshkad_do_read(io);
  } else {
    kmatryoshkad_do_write(io);
  }
}
