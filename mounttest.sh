#!/bin/sh
dd if=/dev/zero of=/dev/ram4 count=100 &>/dev/null
mke2fs -q /dev/ram4
mount /dev/ram4 /var/mnt
mkdir -p /var/mnt/f/f /var/mnt/t
echo echo teststatic> /var/mnt/f/f/test
chmod +x /var/mnt/f/f/test
insmod translucency.o from=/var/mnt/f to=/var/mnt/t
echo echo test>> /var/mnt/f/f/test
cat /var/mnt/f/f/test
/var/mnt/f/f/test
chmod 0 /var/mnt/f/f/test
ls -la cat /var/mnt/f/f/
/var/mnt/f/f/test	#is not allowed (only for root)
#rmmod translucency
#umount /var/mnt

#runs through fine -
#but find /var/mnt/ and ls -l /var/mnt/t triggers device busy bug
