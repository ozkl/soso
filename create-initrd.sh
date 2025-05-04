#!/bin/bash
set -euo pipefail

IMG=initrd.fat
MNT=mnt
SIZE_MB=8

mkdir -p "$MNT"
dd if=/dev/zero of=$IMG bs=1M count=$SIZE_MB

LOOP=$(sudo losetup --find --show $IMG)

cleanup() {
    if mountpoint -q "$MNT"; then
        sudo umount "$MNT"
    fi
    if losetup -a | grep -q "$LOOP"; then
        sudo losetup -d "$LOOP"
    fi
}
trap cleanup EXIT

sudo mkfs.vfat "$LOOP"
sudo mount "$LOOP" "$MNT"
sudo cp -r --no-preserve=ownership userspace/bin/* "$MNT/"
sync
