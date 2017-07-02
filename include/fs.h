#ifndef CARRIER_H
#define CARRIER_H

#include <linux/types.h>

#define FS_FAT 0x01
#define FS_EXFAT 0x02
#define FS_EXT4 0x03
#define FS_BTRFS 0x04
#define FS_NTFS 0x05
#define FS_ZFS 0x06
#define FS_UNKNOWN 0x00

#define FS_FAT_NAME "fat"
#define FS_EXFAT_NAME "exfat"
#define FS_EXT4_NAME "ext4"
#define FS_BTRFS_NAME "btrfs"
#define FS_NTFS_NAME "ntfs"
#define FS_ZFS_NAME "zfs"
#define FS_UNKNOWN_NAME "unknown"

#define FS_FAT16_NAME "fat16"
#define FS_FAT32_NAME "fat32"

u8 get_carrier_fs(char*);
char* get_fs_name(u8 fs_type);

#endif /* CARRIER_H */
