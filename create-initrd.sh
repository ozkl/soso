#!/bin/bash -x

dd if=/dev/zero of=initrd.fat bs=8M count=1
LOOP=`losetup -f`
sudo losetup $LOOP initrd.fat
sudo mkfs.vfat $LOOP
sudo mount $LOOP mnt
cp -r userspace/bin/* mnt/
sudo umount mnt
sudo losetup -d $LOOP
sync
sudo chown $SUDO_USER:$SUDO_USER initrd.fat
