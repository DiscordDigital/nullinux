#!/bin/bash
SOURCE=${BASH_SOURCE[0]}
TOOLSDIR=$(dirname $SOURCE)
ROOTFS=$(realpath "$TOOLSDIR/../rootfs")

RETURNPWD=$(pwd)

EXCLUDE=(./files)

if [ ! -d $ROOTFS ]; then
    echo "rootfs does not exist."
    exit 1
fi

if [ -f "$ROOTFS/usr/sbin/init.cpio" ]; then
    rm "$ROOTFS/usr/sbin/init.cpio"
fi

cd $ROOTFS

find . > files

for file in $EXCLUDE; do
    grep -v "^$file\$" files > tmpfile && mv tmpfile files
done

cat files | cpio -H newc -o > ../temp/init.cpio

cd $RETURNPWD
