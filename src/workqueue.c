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

static void kmatryoshkad_cleanup(struct matryoshka_io *io) {
  struct matryoshka_context *mc = io->mc;

  int i;

  for (i = 0; i < mc->num_carrier; ++i) {
    matryoshka_free_buffer_pages(mc, io->carrier_bios[i]);
    bio_put(io->carrier_bios[i]);
  }

  for (i = 0; i < mc->num_entropy; ++i) {
    matryoshka_free_buffer_pages(mc, io->entropy_bios[i]);
    bio_put(io->entropy_bios[i]);
  }

  kfree(io);
}

static void kmatryoshkad_end_carrier_read(struct bio *bio) {
  struct matryoshka_io *io = (struct matryoshka_io*) bio->bi_private;
  struct matryoshka_context *mc = io->mc;

  int i;
  int index;
  int skip;
  int integrity = 0;
  int status;

  sector_t generated_sector = 0;

  skip = atomic_read(&(io->carrier_done));
  index = skip / 10;

  atomic_inc(&(io->carrier_done));

  io_accumulate_error(io, bio->bi_error);

  if (!io->error) {
    status = erasure_decode(mc, io);
    if (!status) {
      integrity = matryoshka_bio_integrity_check(mc, io, bio);
    }
    if (status || integrity) {
      do {
        if (skip > (10 * mc->num_carrier)) {
          io_accumulate_error(io, -EIO);
          break;
        }
        generated_sector = get_sector_in_sequence(mc->passphrase, io->base_sector, index + (skip % 10), mc->cluster_count * mc->sectors_per_cluster);
        skip++;
      } while (fat_is_cluster_used(mc->fs, generated_sector / mc->sectors_per_cluster));

      if (skip <= (10 * mc->num_carrier)) {
        atomic_set(&(io->carrier_done), skip);
        index = skip / 10;
        for (i = index + mc->num_entropy; i < mc->num_carrier + mc->num_entropy + 1; ++i) {
          io_update_erasures(mc, io, i);
        }

        matryoshka_bio_init(io->carrier_bios[index], io, kmatryoshkad_end_carrier_read);
        bio_map_operation(io->carrier_bios[index], READ);
        bio_map_dev(io->carrier_bios[index], mc->carrier);
        bio_map_sector(io->carrier_bios[index], generated_sector);

        generic_make_request(io->carrier_bios[index]);
      } else {

        io->base_bio->bi_error = io->error;
        bio = io->base_bio;

        kmatryoshkad_cleanup(io);
        bio_endio(bio);
      }
    } else {

      io->base_bio->bi_error = io->error;
      bio = io->base_bio;

      kmatryoshkad_cleanup(io);
      bio_endio(bio);
    }
  } else {

    io->base_bio->bi_error = io->error;
    bio = io->base_bio;

    kmatryoshkad_cleanup(io);
    bio_endio(bio);
  }
}

static void kmatryoshkad_end_carrier_write(struct bio *bio) {
  struct matryoshka_io *io = (struct matryoshka_io*) bio->bi_private;
  struct matryoshka_context *mc = io->mc;

  atomic_inc(&(io->carrier_done));

  io_accumulate_error(io, bio->bi_error);

  if (atomic_read(&(io->carrier_done)) == mc->num_carrier) {
    io->base_bio->bi_error = io->error;
    bio = io->base_bio;

    kmatryoshkad_cleanup(io);
    bio_endio(bio);
  }
}

static void kmatryoshkad_end_entropy_read(struct bio *bio) {
  struct matryoshka_io *io = (struct matryoshka_io*) bio->bi_private;
  struct matryoshka_context *mc = io->mc;

  int i;
  sector_t generated_sector;
  int skip;
  int status;

  atomic_inc(&(io->entropy_done));

  io_accumulate_error(io, bio->bi_error);

  if (io->error) {
    io->base_bio->bi_error = io->error;
    bio = io->base_bio;

    kmatryoshkad_cleanup(io);
    bio_endio(bio);
  } else if (atomic_read(&(io->entropy_done)) == mc->num_entropy) {

    if (bio_data_dir(io->base_bio) == READ) {
      i = 0;
      skip = -1;

      do {
        if (skip == -1) {
          skip = 0;
        } else {
          skip++;
        }
        if (skip > 10) {
          io_accumulate_error(io, -EIO);
          break;
        }
        generated_sector = get_sector_in_sequence(mc->passphrase, io->base_sector, i + skip, mc->cluster_count * mc->sectors_per_cluster);
      } while (fat_is_cluster_used(mc->fs, generated_sector / mc->sectors_per_cluster));

      if (skip <= 10) {
        for (i = 1 + mc->num_entropy; j < mc->num_carrier + mc->num_entropy + 1; ++j) {
          io_update_erasures(mc, io, j);
        }
        atomic_set(&(io->carrier_done), skip);

        matryoshka_bio_init(io->carrier_bios[i], io, kmatryoshkad_end_carrier_read);
        bio_map_operation(io->carrier_bios[i], READ);
        bio_map_dev(io->carrier_bios[i], mc->carrier);
        bio_map_sector(io->carrier_bios[i], generated_sector);

        generic_make_request(io->carrier_bios[i]);
      } else {
        io->base_bio->bi_error = io->error;
        bio = io->base_bio;

        kmatryoshkad_cleanup(io);
        bio_endio(bio);
      }
    } else {

      erasure_encode(mc, io);

      skip = -1;

      for (i = 0; i < mc->num_carrier; ++i) {
        do {
          if (skip == -1) {
            skip = 0;
          } else {
            skip++;
          }
          if (skip > 10) {
            io_accumulate_error(io, -EIO);
            break;
          }
          generated_sector = get_sector_in_sequence(mc->passphrase, io->base_sector, i + skip, mc->cluster_count * mc->sectors_per_cluster);
        } while (fat_is_cluster_used(mc->fs, generated_sector / mc->sectors_per_cluster));

        if (skip <= 10) {

          status = matryoshka_bio_integrity_update(mc, io, bio);

          if (status) {
            io_accumulate_error(io, status);

            io->base_bio->bi_error = io->error;
            bio = io->base_bio;

            kmatryoshkad_cleanup(io);
            bio_endio(bio);
          } else {

            matryoshka_bio_init(io->carrier_bios[i], io, kmatryoshkad_end_carrier_write);
            bio_map_operation(io->carrier_bios[i], WRITE);
            bio_map_dev(io->carrier_bios[i], mc->carrier);
            bio_map_sector(io->carrier_bios[i], generated_sector);

            generic_make_request(io->carrier_bios[i]);
          }
        } else {
          io->base_bio->bi_error = io->error;
          bio = io->base_bio;

          kmatryoshkad_cleanup(io);
          bio_endio(bio);
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
