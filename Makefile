D=translucency-0.6.0-mips-port
ifneq ($(KERNELRELEASE),)

obj-m := translucency.o

translucency-objs := extension.o staticext.o base.o execve-$(ARCH).o
modules: $(obj-m)
$(obj-m): $(translucency-objs)
	$(LD) -r -o $@ $^

else
ifndef KDIR
KDIR := /lib/modules/$(shell uname -r)/build
endif
PWD := $(shell pwd)
export ARCH

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
all: default
install:
	# As we want our kernel module to also be debianified...
	mkdir -p $(DESTDIR)/lib/modules/$(shell uname -r)/misc
	install $(CURDIR)/translucency.o $(DESTDIR)/lib/modules/$(shell uname -r)/misc/
	#$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules_install

endif
clean:
	rm -f .*.cmd *.o *.mod.* translucency.ko

extension.c: redirect.txt makeext.pl
	perl makeext.pl extension redirect.txt

tgz: clean extension.c
	chmod a+rX . -R
	chmod go-w . -R
	tar czf ../$D.tar.gz -C .. --owner=0 --group=0 $D/
			
