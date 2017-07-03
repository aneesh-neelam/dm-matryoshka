# Modules directory
MODULESDIR = /lib/modules/$(shell uname -r)

# Kernel build diretory, kernel source and headers
KDIR ?= $(MODULESDIR)/build

# Module install diretory
INSTALLDIR = $(MODULESDIR)/

# Signing key pairs for UEFI Secure Boot, modify the following for your own
PRIVATEKEYFILE = /home/aneeshneelam/.uefi-sb/sb.priv
PUBLICKEYFILE = /home/aneeshneelam/.uefi-sb/sb_pub.der

# Module name
module = dm-matryoshka

# SRC and LIB files
SRCOBJS = ./src/integrity.o ./src/utility.o ./src/fs.o ./src/entropy.o ./src/erasure.o ./src/fs_fat.o ./src/workqueue.o ./src/xor.o ./src/target.o ./src/read.o ./src/write.o
LIBOBJS = ./lib/jerasure/cauchy.o ./lib/jerasure/galois.o ./lib/jerasure/jerasure.o ./lib/jerasure/liberation.o ./lib/jerasure/reed_sol.o

# KBUILD Flags
KBUILD_CFLAGS += -Wno-unused-parameter

# Kernel module build config
obj-m += $(module).o
dm-matryoshka-objs := $(SRCOBJS) $(LIBOBJS)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

sign:
	$(KDIR)/scripts/sign-file sha256 $(PRIVATEKEYFILE) $(PUBLICKEYFILE) $(module).ko

install:
	cp $(module).ko $(INSTALLDIR)
	depmod -a

load:
	modprobe $(module)

unload:
	modprobe -r $(module)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
