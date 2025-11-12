#!/bin/bash
SOURCE=${BASH_SOURCE[0]}
TOOLSDIR=$(dirname $SOURCE)
NULFOLDER=$(realpath "$TOOLSDIR/..")
TMPFOLDER=$(realpath "$NULFOLDER/temp")
ROOTFS=$(realpath "$NULFOLDER/rootfs")
KBDFILE=$(realpath "$TMPFOLDER/kbd.tar.gz")

KBDPACKAGEURL=http://deb.debian.org/debian/pool/main/k/kbd/kbd_2.7.1.orig.tar.gz

# Download if deb file does not exist in temp
if [ ! -f "$KBDFILE" ]; then
    wget $KBDPACKAGEURL -O "$KBDFILE"
fi

# Only extract, if kbd folder is missing
if [ ! -d "$ROOTFS/opt/kbd" ]; then
    mkdir -p "$TMPFOLDER/kbd_data"
    tar xf "$KBDFILE" -C "$TMPFOLDER/kbd_data/"
    mkdir -p "$ROOTFS/usr/share/kbd"
    keymaps=$(find "$TMPFOLDER/kbd_data/" -type d -name "keymaps")
    keymapsfolder=$(find "$TMPFOLDER/kbd_data/" -type d -name "keymaps" | head -n 1)
    if [ -d "$keymapsfolder" ]; then
        cp -r "$keymapsfolder" "$ROOTFS/usr/share/kbd/"
    else
        echo Failed to extract keymaps folder from $KBDFILE
    fi
    rm -r "$TMPFOLDER/kbd_data"
fi
