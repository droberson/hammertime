KROOT ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

obj-m += hammertime.o canttouchthis.o breakitdown.o

allofit: modules

modules:
	@$(MAKE) -C $(KROOT) M=$(PWD) CC=$(CC) modules

modules_install:
	@$(MAKE) -C $(KROOT) M=$(PWD) modules_install

kernel_clean:
	@$(MAKE) -C $(KROOT) M=$(PWD) clean

clean: kernel_clean
	rm -rf Module.symvers modules.order


.PHONY: modules modules_install clean
