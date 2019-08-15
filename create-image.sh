#!/bin/bash

dd if=/dev/zero of=soso.img bs=26M count=1
fdisk soso.img << EOF
n
p
1


a
w
EOF

LOOPDISK=`losetup -f`
sudo losetup $LOOPDISK soso.img
LOOPPARTITION=`losetup -f`
sudo losetup $LOOPPARTITION soso.img -o 1048576
sudo mke2fs $LOOPPARTITION
sudo mount $LOOPPARTITION /mnt
cp kernel.bin bootdisk-root/boot/
cp initrd.fat bootdisk-root/boot/
cp -r bootdisk-root/* /mnt/
sudo grub-install --root-directory=/mnt --no-floppy --modules="normal part_msdos ext2 multiboot" $LOOPDISK
umount /mnt
losetup -d $LOOPPARTITION
losetup -d $LOOPDISK
sync

chown $SUDO_USER:$SUDO_USER soso.img
