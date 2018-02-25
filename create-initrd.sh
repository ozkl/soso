#!/bin/bash

dd if=/dev/zero of=initrd.fat bs=8M count=1
LOOP=`losetup -f`
sudo losetup $LOOP initrd.fat
sudo mkfs.vfat $LOOP
sudo mount $LOOP /mnt
cp -r userspace/bin/* /mnt/
umount /mnt
losetup -d $LOOP
sync
chown $SUDO_USER:$SUDO_USER initrd.fat