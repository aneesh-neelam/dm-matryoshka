KDIR ?= /lib/modules/$(shell uname -r)/build

obj-m += dm-matryoshka.o
dm-matryoshka-objs := ./src/dm-matryoshka.o

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
