#ifndef FS_FAT_H
#define FS_FAT_H

#include <linux/device-mapper.h>

// Last cluster
#define FAT_LAST (-1)

// Header offsets
#define FAT_BYTES_PER_SECTOR        0x0b
#define FAT_SECTORS_PER_CLUSTER     0x0d
#define FAT_RESERVED_SECTORS        0x0e
#define FAT_FATS                    0x10
#define FAT_TOTAL_SECTORS           0x20
#define FAT_SECTORS_PER_FAT         0x24
#define FAT_ROOT_DIRECTORY          0x2c
#define FAT_DISK_LABEL              0x47
#define FAT_DISK_LABEL_SIZE         11
#define FAT_DISK_OEM                0x3
#define FAT_DISK_OEM_SIZE           8
#define FAT_DISK_FS                 0x52
#define FAT_DISK_FS_SIZE            8
#define FAT_CREATION_DATE           0x10
#define FAT_CHANGE_DATE             0x16

#define FAT16_SECTORS_PER_FAT       0x16
#define FAT16_DISK_FS               0x36
#define FAT16_DISK_FS_SIZE          8
#define FAT16_DISK_LABEL            0x2b
#define FAT16_DISK_LABEL_SIZE       11
#define FAT16_TOTAL_SECTORS         0x13
#define FAT16_ROOT_ENTRIES          0x11

#define FAT32 0
#define FAT16 1

// utils
#define FAT_READ_SHORT(buffer,x) ((buffer[x]&0xff)|((buffer[x+1]&0xff)<<8))
#define FAT_READ_LONG(buffer,x) \
        ((buffer[x]&0xff)|((buffer[x+1]&0xff)<<8))| \
        (((buffer[x+2]&0xff)<<16)|((buffer[x+3]&0xff)<<24))

#define FAT_WRITE_SHORT(buffer,x,s) \
        buffer[x] = (s)&0xff; \
        buffer[x+1] = ((s)>>8)&0xff; \

#define FAT_WRITE_LONG(buffer,x,l) \
        buffer[x] = (l)&0xff; \
        buffer[x+1] = ((l)>>8)&0xff; \
        buffer[x+2] = ((l)>>16)&0xff; \
buffer[x+3] = ((l)>>24)&0xff;


struct fs_fat {
  struct dm_dev *dev;
  // Header values
  int type;
  unsigned long long totalSectors;
  unsigned long long bytesPerSector;
  unsigned long long sectorsPerCluster;
  unsigned long long reservedSectors;
  unsigned long long fats;
  unsigned long long sectorsPerFat;
  unsigned long long rootDirectory;
  unsigned long long reserved;
  unsigned long long strange;
  unsigned int bits;

  // Specific to FAT16
  unsigned long long rootEntries;
  unsigned long long rootClusters;

  // Computed values
  unsigned long long fatStart;
  unsigned long long dataStart;
  unsigned long long bytesPerCluster;
  unsigned long long totalSize;
  unsigned long long dataSize;
  unsigned long long fatSize;
  unsigned long long totalClusters;
};


int fat_get_header(struct fs_fat*, struct dm_dev*, sector_t);
int fat_parse_header(struct fs_fat*, char*, int);
int fat_compute_values(struct fs_fat*);
int fat_is_cluster_used(struct fs_fat *,unsigned int);
int read_page(struct block_device *, sector_t, int, struct page *);
char* get_fat_type_name(struct fs_fat*);

#endif /* FS_FAT_H */
