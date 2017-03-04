MODULESDIR = /lib/modules/$(shell uname -r)
KDIR ?= $(MODULESDIR)/build
INSTALLDIR = $(MODULESDIR)/
module = dm-matryoshka
obj-m += $(module).o
dm-matryoshka-objs := ./src/$(module).o

all:
	make -C $(KDIR) M=$(PWD) modules

install:
	cp $(module).ko $(INSTALLDIR)
	depmod -a

load:
	modprobe $(module)

unload:
	modprobe -r $(module)

clean:
	make -C $(KDIR) M=$(PWD) clean
