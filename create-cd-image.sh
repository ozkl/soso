#!/bin/bash -x
cp kernel.bin bootdisk-root/boot/
cp initrd.fat bootdisk-root/boot/

mkisofs -R -b eltorito.img -no-emul-boot -boot-info-table -o soso.iso bootdisk-root


