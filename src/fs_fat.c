#include <linux/gfp.h>

#include "../include/target.h"
#include "../include/fs.h"
#include "../include/fs_fat.h"


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

int fat_get_header(struct fs_fat *fat, struct dm_dev *carrier, sector_t start) {
  int status;

  sector_t fat_header_sector;
  struct page *fat_page;
  char *buffer;
  int buffersize;

  fat_header_sector = start + 0x0;
  buffersize = 128;

  fat_page = alloc_page(GFP_KERNEL);

  status = read_page(carrier -> bdev, fat_header_sector, PAGE_SIZE, fat_page);
  if (status != 0) {
    return status;
  }

  buffer = (char*) page_address(fat_page);

  fat -> dev = carrier;
  status = fat_parse_header(fat, buffer, buffersize);
  status = fat_compute_values(fat);

  return status;
}

int fat_parse_header(struct fs_fat *fat, char *buffer, int buffersize) {
  fat -> bytesPerSector = (FAT_READ_SHORT(buffer, FAT_BYTES_PER_SECTOR)) & (0xffff);
  fat -> sectorsPerCluster = buffer[FAT_SECTORS_PER_CLUSTER] & 0xff;
  fat -> reservedSectors = (FAT_READ_SHORT(buffer, FAT_RESERVED_SECTORS)) & (0xffff);
  fat -> fats = buffer[FAT_FATS];

  fat -> sectorsPerFat = (FAT_READ_SHORT(buffer, FAT16_SECTORS_PER_FAT)) & (0xffff);

  if (fat -> sectorsPerFat != 0) {
    fat -> type = FAT16;
    fat -> bits = 16;
    fat -> rootEntries = (FAT_READ_SHORT(buffer, FAT16_ROOT_ENTRIES)) & (0xffff);
    fat -> rootDirectory = 0;

    fat -> totalSectors = (FAT_READ_SHORT(buffer, FAT16_TOTAL_SECTORS)) & (0xffff);
    if (!fat -> totalSectors) {
      fat -> totalSectors = (FAT_READ_LONG(buffer, FAT_TOTAL_SECTORS)) & (0xffffffff);
    }
  } else {
    fat -> type = FAT32;
    fat -> bits = 32;
    fat -> sectorsPerFat = (FAT_READ_LONG(buffer, FAT_SECTORS_PER_FAT)) & (0xffffffff);
    fat -> totalSectors = (FAT_READ_LONG(buffer, FAT_TOTAL_SECTORS)) & (0xffffffff);
    fat -> rootDirectory = (FAT_READ_LONG(buffer, FAT_ROOT_DIRECTORY)) & (0xffffffff);
  }

  return 0;
}

int fat_compute_values(struct fs_fat *fat) {
  fat -> fatStart = fat -> bytesPerSector * fat -> reservedSectors;
  fat -> dataStart = fat -> fatStart + fat -> fats * fat -> sectorsPerFat * fat -> bytesPerSector;
  fat -> bytesPerCluster = fat -> bytesPerSector * fat -> sectorsPerCluster;
  fat -> totalSize = fat -> totalSectors * fat -> bytesPerSector;
  fat -> fatSize = fat -> sectorsPerFat * fat -> bytesPerSector;
  fat -> totalClusters = (fat -> fatSize * 8) / fat -> bits;
  fat -> dataSize = fat -> totalClusters * fat -> bytesPerCluster;

  if (fat -> type == FAT16) {
    int rootBytes = fat -> rootEntries * 32;
    fat -> rootClusters = rootBytes / fat -> bytesPerCluster + ((rootBytes % fat -> bytesPerCluster) ? 1 : 0);
  }

  return 0;
}

/*
  Usage:
    sector_t s = 512000;
    int b = fat_is_cluster_used(fat, s / sectors_per_cluster);
    printk(KERN_DEBUG "Sector %llu free?: %llu", s, b);
*/
int fat_is_cluster_used(struct fs_fat *fat, unsigned int cluster) {
  struct dm_dev *carrier = (struct dm_dev*) fat -> dev;
  int fat_type = 0;
  char *buffer;
  unsigned int next;
  int status;

  sector_t sector = (fat -> fatStart + fat -> fatSize * fat_type + (fat -> bits * cluster) / 8) >> SECTOR_SHIFT;
  struct page *fat_page = alloc_page(GFP_KERNEL);

  status = read_page(carrier -> bdev, sector, PAGE_SIZE, fat_page);
  if (status != 0) {
    return -EIO;
  }

  buffer = (char*) page_address(fat_page);

  if (fat -> type == FAT32) {
    next = (FAT_READ_LONG(buffer, 0)) & (0x0fffffff);

    if (next >= 0x0ffffff0) {
        return FAT_LAST;
    } else {
        return next;
    }
  } else {
    next = (FAT_READ_SHORT(buffer,0)) & (0xffff);

    if (fat -> bits == 12) {
      int bit = cluster * fat -> bits;
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

char* get_fat_type_name(struct fs_fat *fat) {
  if (fat->bits == 32) {
    return FS_FAT32_NAME;
  } else {
    return FS_FAT16_NAME;
  }
}
