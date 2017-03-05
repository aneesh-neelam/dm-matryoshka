#define DM_MSG_PREFIX "matryoshka"
#define NAME "Matryoshka"
#define LICENSE "Dual BSD/GPL"
#define DESCRIPTION "Matryoshka Device Mapper"
#define AUTHOR "Aneesh Neelam <aneelam@ucsc.edu>"

/*
 * Matryoshka: Maps in the Matryoshka way. 
 */
struct matryoshka_c {
        struct dm_dev *dev;
        sector_t start;
};
