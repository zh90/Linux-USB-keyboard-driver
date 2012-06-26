##############################
#usbkdb Makefile for linux
##############################

obj-m:=usbkbd.o

#KERNELDIR ?= /lib/modules/$(shell uname -r)/build

#PWD:=$(shell pwd)

#default:
#$(MAKE) -C $(KERNELDIR) M=$(PWD) modules 
