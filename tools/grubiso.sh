#!/bin/bash
SOURCE=${BASH_SOURCE[0]}
TOOLSDIR=$(dirname $SOURCE)
ROOTFS=$(realpath "$TOOLSDIR/../rootfs")
TEMPLATES=$(realpath "$TOOLSDIR/../templates")
ISODIR=$(realpath "$TOOLSDIR/../iso")
TEMPDIR=$(realpath "$TOOLSDIR/../temp")
MAIN=$(realpath "$TOOLSDIR/../")

if [ -d "$ISODIR" ]; then
    rm -rf "$ISODIR"
fi

# Get unicode.pf2 from host system
mkdir -p "$ISODIR/boot/grub/fonts"
cp /boot/grub/unicode.pf2 "$ISODIR/boot/grub/fonts/"

# Get grub.cfg from templates
cp "$TEMPLATES/grub.cfg" "$ISODIR/boot/grub/"

# Obtain init.cpio from temp folder
cp "$TEMPDIR/init.cpio" "$ISODIR/boot/initramfs.cpio"

# gzip the initramfs.cpio
gzip -9 "$ISODIR/boot/initramfs.cpio"

# Copy bzImage into iso/boot/
cp "$MAIN/linux/arch/x86/boot/bzImage" "$ISODIR/boot/"

# Create ISO file
grub-mkrescue -volid NULLINUX -o "$MAIN/nullinux.iso" "$ISODIR"
