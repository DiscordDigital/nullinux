#!/bin/bash
SOURCE=${BASH_SOURCE[0]}
TOOLSDIR=$(dirname $SOURCE)
ROOTFS=$(realpath "$TOOLSDIR/../rootfs")
TEMPLATES=$(realpath "$TOOLSDIR/../templates")

if [ -d "$ROOTFS" ]; then
    rm -rf "$ROOTFS"
fi

# Create folder structure
mkdir -p "$ROOTFS/proc"
mkdir -p "$ROOTFS/sys"
mkdir -p "$ROOTFS/mnt"
mkdir -p "$ROOTFS/lib"
mkdir -p "$ROOTFS/lib/x86_64-linux-gnu"
mkdir -p "$ROOTFS/lib/modules"
mkdir -p "$ROOTFS/usr"
mkdir -p "$ROOTFS/usr/sbin"
mkdir -p "$ROOTFS/usr/bin"
mkdir -p "$ROOTFS/usr/lib"
mkdir -p "$ROOTFS/usr/lib/openssh"
mkdir -p "$ROOTFS/usr/share"
mkdir -p "$ROOTFS/usr/share/kbd"
mkdir -p "$ROOTFS/usr/share/misc"
mkdir -p "$ROOTFS/usr/share/terminfo"
mkdir -p "$ROOTFS/usr/share/consolefonts"
mkdir -p "$ROOTFS/root"
mkdir -p "$ROOTFS/tmp"
mkdir -p "$ROOTFS/dev"

# Copy opt folder
cp -r "$TEMPLATES/opt" "$ROOTFS/"

# Copy etc folder
cp -r "$TEMPLATES/etc" "$ROOTFS/"

# Copy root home directory
cp -r "$TEMPLATES/root" "$ROOTFS/"

# Create os-release file
head=$(git rev-parse --short HEAD 2> /dev/null)
if [ $? -eq 0 ]; then
    echo "nullinux ($head)" > "$ROOTFS/etc/os-release"
else
    echo "nullinux (unknown)" > "$ROOTFS/etc/os-release"
fi

exit 0
