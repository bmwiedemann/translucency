K=/usr/src/linux
UM=/usr/src/linux-2.4.18-um27
CFLAGS=-O2 -Werror -Wall -Wstrict-prototypes -fomit-frame-pointer -pipe -fno-strength-reduce
F=/tmp/fromdir/
T=/tmp/todir/
M=translucency
D=$M
O=base.o extension.o sysctl.o staticext.o
E=ext

#KERNELINCLUDE = `if [ "${ARCH}" = "um" ] ; then echo -I${UM}/include -I${UM}/arch/um/include ; else echo -I$K/include ; fi `
KERNELINCLUDE = $(shell if [ "${ARCH}" = "um" ] ; then echo -I${UM}/include -I${UM}/arch/um/include ; else echo -I$K/include ; fi )
ALLINCLUDE = -nostdinc $(KERNELINCLUDE) $(INCLUDE) -I. -I/usr/include

all: $M.o
$M.o: $O
	ld -i $O -o $M.o

$E.c: redirect.txt makeext.pl
	perl makeext.pl $E <redirect.txt
	patch -p0 $E.c < extdiff.txt

ok:
	cp -a $E.c extension.c
	mv $E.h extension.h

extdiff:
	cp -a extension.c $En.c
	perl makeext.pl $E <redirect.txt
	-diff -u $E.c $En.c >extdiff.txt
	mv $En.c $E.c

%.o: %.c base.h compatibility.h
	gcc -c $(CFLAGS) $(ALLINCLUDE) $<
%.s: %.c Makefile
	gcc -S $(CFLAGS) $(ALLINCLUDE) $<

tar: tgz
tgz: clean
	tar czf ../$D.tar.gz -C .. --owner=0 --group=0 $D/

ramdsk: $T/is_ramdsk

$T/is_ramdsk:
	dd if=/dev/zero of=/dev/ram1 count=4096
	mke2fs /dev/ram1
	mkdir -p $T
	mount /dev/ram1 $T
	touch $T/is_ramdsk

testfiles: $T/have_testfiles

$T/have_testfiles:
	mkdir -p $F/sub
	mkdir -p $F/sub2
	echo 'this is file "static"' >$F/static
	echo 'this is file "sub/static"' >$F/sub/static
	echo 'this is file "sub2/static"' >$F/sub2/static
	echo 'this is static file "sub/test"' >$F/sub/test
	echo 'this is static file "sub2/test"' >$F/sub2/test
	mkdir -p $T/sub2
	mkdir -p $T/sub3
	echo 'this is file "test"' > $T/test
	echo 'this is overlayed file "sub2/test"' > $T/sub2/test
	echo 'this is file "sub2/dynamic"' > $T/sub2/dynamic
	echo 'this is file "sub3/test"' > $T/sub3/test
	echo 'this is file "/tmp/linktest"' > /tmp/linktest
	echo -e \#\!/bin/sh\\necho static > $F/sub2/exectest
	chmod 755 $F/sub2/exectest
	-ln -s "static" $F/link
	-ln -s "../linktest" $F/link2
	-ln -s "../linktest" $T/link2
	-ln -s "$Tsub3/test" $F/link3
	-ln -s "$Fsub/static" $T/link4
	-ln -s "$F/sub" $T/link5
	-ln -s "$F/sub2" $T/link6
	-ln -s "$F/sub3" $F/link7
	-ln -s "$T/sub3" $F/link8
	-chmod -R a-w /tmp/fromdir/ 
	touch $T/have_testfiles

test: $M.o testfiles
	sync
	insmod -f $M.o from=$F to=$T
	-ls -RF $T
	-ls -RF $F
	-ls -F $F/link5/
	-ls -F $F/link6/
	-ls -F $F/link8/
	-sleep 1
	-cat $F/test
	-cat $F/static
	-cat $F/sub/test
	-cat $F/sub2/test
	-cat $F/sub3/test
	-cat $F/sub2/static
	-cat $F/link
	-cat $F/link2
	-cat $F/link3
	-cat $F/link4
	-cat $F/link5/test
	-cat $F/link6/test
	-cat $F/link7/test
	-cat $F/link8/test
	-cat $F/sub/../sub3/test
	-cat $F/sub3/../sub/test
	-$F/sub2/exectest
	-echo echo appended >>$F/sub2/exectest
	-$F/sub2/exectest
	-rm $F/sub2/exectest
	-echo -e \#\!/bin/sh\\necho overridden > $F/sub2/exectest
	-$F/sub2/exectest
	-rm $F/sub2/exectest
	-$F/sub2/exectest
	echo 2 > /proc/sys/translucency/flags
	-echo echo appended >>$F/sub2/exectest
	-$F/sub2/exectest
	echo 8 > /proc/sys/translucency/flags
	-ls -RF $F
	echo 4 > /proc/sys/translucency/flags
	-ls -RF $F
	-ls -RF $T
	-cat $F/sub2/test
	-$F/sub2/exectest
	echo 0 > /proc/sys/translucency/flags
	-$F/sub2/exectest
	-rm $F/sub2/exectest
	rmmod $M

ktest: $M.o
	#-rm -rf $T
	mkdir -p $T/include
	cp -a $K/include/asm-i386 $T/include/
	chown -R nobody. $T
	sync
	insmod -f $M.o uid=65534 from=$K to=$T
	-su nobody -c "cd $K; make menuconfig"
	echo 8 >/proc/sys/translucency/flags	#turn off merged dir listings
	-su nobody -c "cd $K; make dep"
	echo 0 >/proc/sys/translucency/flags
	rm $T/include/asm; ln -s asm-i386 $T/include/asm
	-su nobody -c "cd $K; make bzImage modules"
	#rmmod $M

testrun:
	-rm -rf $F $T /tmp/linktest
	make testfiles all > /dev/null
	make test | diff -u testrun2.txt -
clean:
	-rm *.o *.rej *.[ch].orig $E.[ch]
mrproper: clean
	-rm -rf $F $T /tmp/linktest *.s
