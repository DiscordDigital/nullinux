#!/bin/bash
SOURCE=${BASH_SOURCE[0]}
TOOLSDIR=$(dirname $SOURCE)
ROOTFS=$(realpath "$TOOLSDIR/../rootfs")

rm -rf $ROOTFS/usr
rm -rf $ROOTFS/lib
rm -rf $ROOTFS/lib64
