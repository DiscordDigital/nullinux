#!/bin/bash
SOURCE=${BASH_SOURCE[0]}
TOOLSDIR=$(dirname $SOURCE)
MAIN=$(realpath "$TOOLSDIR/..")
LINUXFOLDER=$(realpath "$MAIN/linux")
ROOTFS=$(realpath "$MAIN/rootfs")

mkdir -p "$ROOTFS/lib/modules/"

if [ -f "$LINUXFOLDER/drivers/md/dm-mod.ko" ]; then
    cp "$LINUXFOLDER/drivers/md/dm-mod.ko" "$ROOTFS/lib/modules/"
fi

if [ -f "$LINUXFOLDER/drivers/md/dm-crypt.ko" ]; then
    cp "$LINUXFOLDER/drivers/md/dm-crypt.ko" "$ROOTFS/lib/modules/"
fi

if [ -f "$LINUXFOLDER/drivers/md/dm-zero.ko" ]; then
    cp "$LINUXFOLDER/drivers/md/dm-zero.ko" "$ROOTFS/lib/modules/"
fi

exit 0
