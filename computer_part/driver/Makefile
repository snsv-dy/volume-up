# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	obj-m := scull.o usb_temp.o
	scull-objs := scull_main.o scull_p.o
	# Otherwise we were called directly from the command
	# line; invoke the kernel build system.
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	MOD_DIR ?= $(shell pwd)
# 	PWD := $(shell pwd)
# 	BUILD_DIR ?= $(PWD)/build
# 	export KBUILD_OUTPUT=$(BUILD_DIR)
default:
	@echo pwd $(PWD)
	@echo build_dir $(BUILD_DIR)
	@echo KERNELDIR $(KERNELDIR)
	$(MAKE) -C $(KERNELDIR) M=$(MOD_DIR) MO=$(shell pwd)/build modules
# 	$(MAKE) -C $(KERNELDIR) M=$(MOD_DIR) modules
endif
# make -C /lib/modules/`uname -r`/build M=$PWD MO=$PWD/build1

clean:
	rm -r *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.mod modules.order *.symvers