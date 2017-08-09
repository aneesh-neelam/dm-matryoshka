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
  bio_endio(bio);
}

static void kmatryoshkad_do_read(struct matryoshka_io *io) {
  struct matryoshka_context *mc = io->mc;
  mybio_init_dev(mc, io->base_bio, mc -> carrier, io, kmatryoshkad_end_read);

  generic_make_request(io->base_bio);
}

static void kmatryoshkad_end_write(struct bio *bio) {
  bio_endio(bio);
}

static void kmatryoshkad_do_write(struct matryoshka_io *io) {
  struct matryoshka_context *mc = io->mc;
  mybio_init_dev(mc, io->base_bio, mc->carrier, io, kmatryoshkad_end_write);

  generic_make_request(io->base_bio);
}

void kmatryoshkad_do(struct work_struct *work) {
  struct matryoshka_io *io = container_of(work, struct matryoshka_io, work);

  if (bio_data_dir(io->base_bio) == READ) {
    kmatryoshkad_do_read(io);
  } else {
    kmatryoshkad_do_write(io);
  }
}
