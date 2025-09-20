grub-mkimage -O i386-pc -o core.img     --prefix=/boot/grub     biosdisk iso9660 multiboot normal search search_label gfxterm vbe video

cp /usr/lib/grub/i386-pc/cdboot.img bootdisk-root/eltorito.img
cat core.img >> bootdisk-root/eltorito.img
