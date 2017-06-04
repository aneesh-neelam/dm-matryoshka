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

int vfat_get_header(struct fs_vfat *vfat, struct dm_dev *carrier, sector_t start) {
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

  vfat -> dev = carrier;
  status = vfat_parse_header(vfat, buffer, buffersize);
  status = vfat_compute_values(vfat);

  return status;
}

int vfat_parse_header(struct fs_vfat *vfat, char *buffer, int buffersize) {
  vfat -> bytesPerSector = FAT_READ_SHORT(buffer, FAT_BYTES_PER_SECTOR) & 0xffff;
  vfat -> sectorsPerCluster = buffer[FAT_SECTORS_PER_CLUSTER] & 0xff;
  vfat -> reservedSectors = FAT_READ_SHORT(buffer, FAT_RESERVED_SECTORS) & 0xffff;
  vfat -> fats = buffer[FAT_FATS];

  vfat -> sectorsPerFat = FAT_READ_SHORT(buffer, FAT16_SECTORS_PER_FAT) & 0xffff;

  if (vfat -> sectorsPerFat != 0) {
    vfat -> type = FAT16;
    vfat -> bits = 16;
    vfat -> rootEntries = FAT_READ_SHORT(buffer, FAT16_ROOT_ENTRIES) & 0xffff;
    vfat -> rootDirectory = 0;

    vfat -> totalSectors = FAT_READ_SHORT(buffer, FAT16_TOTAL_SECTORS) & 0xffff;
    if (!vfat -> totalSectors) {
      vfat -> totalSectors = FAT_READ_LONG(buffer, FAT_TOTAL_SECTORS) & 0xffffffff;
    }
  } else {
    vfat -> type = FAT32;
    vfat -> bits = 32;
    vfat -> sectorsPerFat = FAT_READ_LONG(buffer, FAT_SECTORS_PER_FAT) & 0xffffffff;
    vfat -> totalSectors = FAT_READ_LONG(buffer, FAT_TOTAL_SECTORS) & 0xffffffff;
    vfat -> rootDirectory = FAT_READ_LONG(buffer, FAT_ROOT_DIRECTORY) & 0xffffffff;
  }

  return 0;
}

int vfat_compute_values(struct fs_vfat *vfat) {
  vfat -> fatStart = vfat -> bytesPerSector * vfat -> reservedSectors;
  vfat -> dataStart = vfat -> fatStart + vfat -> fats * vfat -> sectorsPerFat * vfat -> bytesPerSector;
  vfat -> bytesPerCluster = vfat -> bytesPerSector * vfat -> sectorsPerCluster;
  vfat -> totalSize = vfat -> totalSectors * vfat -> bytesPerSector;
  vfat -> fatSize = vfat -> sectorsPerFat * vfat -> bytesPerSector;
  vfat -> totalClusters = (vfat -> fatSize * 8) / vfat -> bits;
  vfat -> dataSize = vfat -> totalClusters * vfat -> bytesPerCluster;

  if (vfat -> type == FAT16) {
    int rootBytes = vfat -> rootEntries * 32;
    vfat -> rootClusters = rootBytes / vfat -> bytesPerCluster + ((rootBytes % vfat -> bytesPerCluster) ? 1 : 0);
  }

  return 0;
}

/*
  Usage:
    sector_t s = 512000;
    int b = vfat_is_cluster_used(vfat, s / 16);
    printk(KERN_DEBUG "Sector %llu free?: %llu", s, b);
*/

int vfat_is_cluster_used(struct fs_vfat *vfat, unsigned int cluster) {
  struct dm_dev *carrier = (struct dm_dev*) vfat -> dev;
  int fat = 0;
  char *buffer;
  unsigned int next;
  int bytes = (vfat -> bits == 32 ? 4 : 2);

  sector_t sector = (vfat -> fatStart + vfat -> fatSize * fat + (vfat -> bits * cluster) / 8) >> SECTOR_SHIFT;
  struct page *vfat_page = alloc_page(GFP_KERNEL);

  read_page(carrier -> bdev, sector, PAGE_SIZE, vfat_page);

  buffer = (char*) page_address(vfat_page);

  if (vfat -> type == FAT32) {
    next = FAT_READ_LONG(buffer, 0)&0x0fffffff;

    if (next >= 0x0ffffff0) {
        return FAT_LAST;
    } else {
        return next;
    }
  } else {
    next = FAT_READ_SHORT(buffer,0)&0xffff;

    if (vfat -> bits == 12) {
      int bit = cluster * vfat -> bits;
      if (bit % 8 != 0) {
        next = next >> 4;
      }
      next &= 0xfff;
      if (next >= 0xff0) {
        return FAT_LAST;
      } else {
        return next;
      }
    } else {
      if (next >= 0xfff0) {
        return FAT_LAST;
      } else {
        return next;
      }
    }
  }
}
