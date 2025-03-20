PWD := $(shell pwd)
KERNELRELEASE=$(shell uname -r)
MODNAME=dynamic_jump_example
KERNEL_BUILD_PATH := /usr/src/kernels/$(KERNELRELEASE)/
obj-m += $(MODNAME).o
ccflags-y += -I$(PWD)/../include
all:
	make -C $(KERNEL_BUILD_PATH) M=$(PWD) modules
clean:
	make -C $(KERNEL_BUILD_PATH) M=$(PWD) clean
