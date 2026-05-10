obj-m += vmmc.o

KERNELDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

SPARSE_FLAGS := -Waddress-space \
					-Wcontext \
					-Wbitwise \
					-Wcast-to-as \
					-Wcast-truncate \
					-Wenum-mismatch \
					-Wshadow \
					-Wtypesign \
					-Wdecl \
					-Wnon-pointer-null \
					-Wreturn-void \
					-Wshift-count-overflow

all:
	make -C $(KERNELDIR) M=$(PWD) modules

sparse:
	make C=1 CHECK="sparse $(SPARSE_FLAGS)" \
		-C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
