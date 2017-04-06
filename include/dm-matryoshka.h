#define DM_MSG_PREFIX "matryoshka"
#define NAME "matryoshka"
#define LICENSE "Dual BSD/GPL"
#define DESCRIPTION "Matryoshka Device Mapper"
#define AUTHOR "Aneesh Neelam <aneelam@ucsc.edu>"

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0

/*
 * Matryoshka: Maps in the Matryoshka way.
 */
struct matryoshka_c {

  char *passphrase;

  struct dm_dev *entropy;
  struct dm_dev *carrier;
};
