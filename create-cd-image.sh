#!/bin/bash
cp kernel.bin bootdisk-root/boot/
cp initrd.img bootdisk-root/boot/
grub-mkrescue -o soso.iso bootdisk-root
