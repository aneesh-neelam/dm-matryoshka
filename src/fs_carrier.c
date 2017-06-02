#include <linux/string.h>
#include <linux/types.h>

#include "../include/carrier.h"


u8 get_carrier_fs(char *fs) {
  if (strncmp("vfat", fs, 4) == 0) {
    return FS_VFAT;
  } else if (strncmp("exfat", fs, 5) == 0) {
    return FS_EXFAT;
  } else if (strncmp("ext4", fs, 4) == 0) {
    return FS_EXT4;
  } else if (strncmp("btrfs", fs, 5) == 0) {
    return FS_BTRFS;
  } else if (strncmp("ntfs", fs, 4) == 0) {
    return FS_NTFS;
  } else if (strncmp("zfs", fs, 3) == 0) {
    return FS_ZFS;
  } else {
    return FS_UNKNOWN;
  }
}
