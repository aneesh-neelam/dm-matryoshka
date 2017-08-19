#include <linux/device-mapper.h>
#include <linux/bio.h>

#include "../include/workqueue.h"
#include "../include/target.h"
#include "../include/fs_fat.h"
#include "../include/integrity.h"
#include "../include/utility.h"


void kmatryoshkad_queue_io(struct matryoshka_io *io) {
  struct matryoshka_context *mc = io->mc;

  INIT_WORK(&io->work, kmatryoshkad_do);
  queue_work(mc->matryoshka_wq, &io->work);
}

static void kmatryoshkad_end_carrier_read(struct bio *bio) {
  struct matryoshka_io *io = (struct matryoshka_io*) bio->bi_private;
  struct matryoshka_context *mc = io->mc;

  int i;

  int index = atomic_read(&(io->carrier_done));

  atomic_inc(&(io->carrier_done));

  io_accumulate_error(io, bio->bi_error);

  if (!io->error) {

    matryoshka_bio_integrity_check(mc, io, bio);

    for (i = index + 1 + mc->num_entropy; i < mc->num_carrier + mc->num_entropy + 1; ++i) {
      io_update_erasure(mc, io, i);
    }
    

  }
}

static void kmatryoshkad_end_carrier_write(struct bio *bio) {
  struct matryoshka_io *io = (struct matryoshka_io*) bio->bi_private;
  struct matryoshka_context *mc = io->mc;
}

static void kmatryoshkad_end_entropy_read(struct bio *bio) {
  struct matryoshka_io *io = (struct matryoshka_io*) bio->bi_private;
  struct matryoshka_context *mc = io->mc;

  int i;
  sector_t generated_sector;
  int skip = -1;

  atomic_inc(&(io->entropy_done));

  io_accumulate_error(io, bio->bi_error);

  if (atomic_read(&(io->entropy_done)) == mc->num_entropy && !io->error) {

    if (bio_data_dir(io->base_bio) == READ) {
      i = 0;

      do {
        if (skip = -1) {
          skip = 0;
        } else {
          skip++;
        }
        if (skip > 10) {
          break;
        }
        generated_sector = get_sector_in_sequence(mc->passphrase, io->base_sector, i + skip, mc->cluster_count * mc->sectors_per_cluster);
      } while (fat_is_cluster_used(mc->fs, generated_sector / mc->sectors_per_cluster));

      if (skip <= 10) {
        matryoshka_bio_init(io->carrier_bios[i], io, kmatryoshkad_end_carrier_read);
        bio_map_operation(io->carrier_bios[i], READ);
        bio_map_dev(io->carrier_bios[i], mc->carrier);
        bio_map_sector(io->carrier_bios[i], generated_sector);

        generic_make_request(io->carrier_bios[i]);
      }
    } else {

      erasure_encode(mc, io);

      for (i = 0; i < mc->num_carrier; ++i) {
        do {
        if (skip = -1) {
          skip = 0;
        } else {
          skip++;
        }
        if (skip > 10) {
          break;
        }
        generated_sector = get_sector_in_sequence(mc->passphrase, io->base_sector, i + skip, mc->cluster_count * mc->sectors_per_cluster);
      } while (fat_is_cluster_used(mc->fs, generated_sector / mc->sectors_per_cluster));

        if (skip <= 10) {

          matryoshka_bio_init(io->carrier_bios[i], io, kmatryoshkad_end_carrier_write);
          bio_map_operation(io->carrier_bios[i], WRITE);
          bio_map_dev(io->carrier_bios[i], mc->carrier);
          bio_map_sector(io->carrier_bios[i], generated_sector);

          generic_make_request(io->carrier_bios[i]);
        }
      }
    }
  }
}

void kmatryoshkad_do(struct work_struct *work) {
  struct matryoshka_io *io = container_of(work, struct matryoshka_io, work);
  struct matryoshka_context *mc = io->mc;

  int i;
  sector_t generated_sector;

  for (i = 0; i < mc->num_carrier; ++i) {
    io->carrier_bios[i] = matryoshka_alloc_bio(mc, io->base_bio->bi_iter.bi_size);
  }

  for (i = 0; i < mc->num_entropy; ++i) {
    io->entropy_bios[i] = matryoshka_alloc_bio(mc, io->base_bio->bi_iter.bi_size);

    generated_sector = get_sector_in_sequence(mc->passphrase, io->base_sector, i, mc->entropy_max_sectors);

    matryoshka_bio_init(io->entropy_bios[i], io, kmatryoshkad_end_entropy_read);
    bio_map_operation(io->entropy_bios[i], READ);
    bio_map_dev(io->entropy_bios[i], mc->entropy);
    bio_map_sector(io->entropy_bios[i], generated_sector);

    generic_make_request(io->entropy_bios[i]);
  }
}
