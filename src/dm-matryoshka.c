#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>

#define DRIVER_AUTHOR "Aneesh Neelam <aneelam@ucsc.edu>"
#define DRIVER_DESC   "Device Mapper for Matryoshka"

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

static int __init dm_matryoshka_init(void) {
  printk(KERN_INFO "Init Matryoshka Device Mapper\n");
  return 0;
}

static void __exit dm_matryoshka_exit(void) {
  printk(KERN_INFO "Exit Matroyshka Kernel Module\n");
}

module_init(dm_matryoshka_init);
module_exit(dm_matryoshka_exit);
