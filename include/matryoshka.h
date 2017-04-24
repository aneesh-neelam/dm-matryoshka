#include <linux/types.h>
#include <linux/device-mapper.h>

#define DM_MSG_PREFIX "matryoshka"
#define NAME "matryoshka"
#define LICENSE "Dual BSD/GPL"
#define DESCRIPTION "Matryoshka Device Mapper"
#define AUTHOR "Aneesh Neelam <aneelam@ucsc.edu>"

#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 0

/*
 * Matryoshka: Maps in the Matryoshka way.
 */
struct matryoshka_c {
  u8 carrier_fs;
  char *passphrase;
  struct dm_dev *entropy;
  struct dm_dev *carrier;
};

#define FS_VFAT 0x01
#define FS_EXFAT 0x02
#define FS_EXT4 0x03
#define FS_BTRFS 0x04
#define FS_NTFS 0x05
#define FS_ZFS 0x06
#define FS_UNKNOWN 0x00

u8 get_carrier_fs(char*);
int get_entropy_blocks(struct dm_dev*);
int erasure_encode(struct bio_vec*, struct bio_vec*, struct bio_vec*);
int erasure_deconstruct(struct bio_vec*, struct bio_vec*, struct bio_vec*);
int matryoshka_read(struct dm_target*, struct bio*);
int matryoshka_write(struct dm_target*, struct bio*);
