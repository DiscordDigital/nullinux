#!/bin/bash
SOURCE=${BASH_SOURCE[0]}
TOOLSDIR=$(dirname $SOURCE)
ROOTFS=$(realpath "$TOOLSDIR/../rootfs")
SRCDIR=$(realpath "$TOOLSDIR/../src")

# Target folders
mkdir -p "$ROOTFS/usr/bin/"
mkdir -p "$ROOTFS/usr/sbin"

# DHCP Client
gcc -static -Os -o "$ROOTFS/usr/bin/dhcp" "$SRCDIR/dhcp.c"

# powerctl
gcc -static -Os -o "$ROOTFS/usr/bin/powerctl" "$SRCDIR/powerctl.c"

# init
gcc -static -Os -o "$ROOTFS/usr/sbin/init" "$SRCDIR/init.c"

# webserver
gcc -static -Os -o "$ROOTFS/usr/sbin/webserver" "$SRCDIR/webserver.c" -lcrypto
