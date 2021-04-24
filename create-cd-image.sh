#!/bin/bash -x
cp kernel.bin bootdisk-root/boot/
cp initrd.fat bootdisk-root/boot/
grub-mkrescue -o soso.iso bootdisk-root

#grub-pc, xorriso, mtools should have been installed
