#include <linux/gfp.h>

#include "../include/target.h"
#include "../include/fs.h"


int read_page(struct block_device *device, sector_t sector, int size, struct page *page) {
  int ret;
  struct bio *bio = bio_alloc(GFP_NOIO, 1);

  bio->bi_bdev = device;
  bio->bi_iter.bi_sector = sector;
  bio->bi_opf = REQ_OP_READ;
  bio->bi_next = NULL;

  bio_add_page(bio, page, size, 0);

  ret = submit_bio_wait(bio);

  return ret;
}

int vfat_get_header(struct dm_dev *carrier, sector_t start) {
  int status, i;

  sector_t vfat_header_sector;
  struct page *vfat_page;
  char *buffer;
  int buffersize;

  vfat_header_sector = start + 0x0;
  buffersize = 128;

  vfat_page = alloc_page(GFP_KERNEL);

  status = read_page(carrier -> bdev, vfat_header_sector, PAGE_SIZE, vfat_page);

  buffer = (char*) page_address(vfat_page);

  for (i = 0; i < buffersize; ++i) {
    printk(KERN_DEBUG "%c ", buffer[i]);
  }

  return status;
}
