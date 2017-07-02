#include <linux/string.h>
#include <linux/types.h>

#include "../include/fs.h"


u8 get_carrier_fs(char *fs) {
  if (strncmp(FS_FAT_NAME, fs, 3) == 0) {
    return FS_FAT;
  } else if (strncmp(FS_EXFAT_NAME, fs, 5) == 0) {
    return FS_EXFAT;
  } else if (strncmp(FS_EXT4_NAME, fs, 4) == 0) {
    return FS_EXT4;
  } else if (strncmp(FS_BTRFS_NAME, fs, 5) == 0) {
    return FS_BTRFS;
  } else if (strncmp(FS_NTFS_NAME, fs, 4) == 0) {
    return FS_NTFS;
  } else if (strncmp(FS_ZFS_NAME, fs, 3) == 0) {
    return FS_ZFS;
  } else {
    return FS_UNKNOWN;
  }
}

char* get_fs_name(u8 fs_type) {
  switch(fs_type) {
    case FS_FAT: 
      return FS_FAT_NAME;
    case FS_EXFAT:
      return FS_NAME_EXFAT;
    case FS_EXT4: 
      return FS_NAME_EXT4;
    case FS_BTRFS: 
      return FS_NAME_BTRFS;
    case FS_NTFS: 
      return FS_NAME_NTFS;
    case FS_ZFS: 
      return FS_NAME_ZFS;
    default: 
      return FS_UNKNOWN_NAME;
  }
}
