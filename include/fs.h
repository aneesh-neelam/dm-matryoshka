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


u8 get_carrier_fs(char*);

#endif /* CARRIER_H */
