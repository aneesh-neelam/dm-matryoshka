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

# Kernel module build config
obj-m += $(module).o
dm-matryoshka-objs := ./src/$(module).o

all:
	make -C $(KDIR) M=$(PWD) modules

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
	make -C $(KDIR) M=$(PWD) clean
