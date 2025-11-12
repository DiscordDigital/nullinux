#!/bin/bash
SOURCE=${BASH_SOURCE[0]}
TOOLSDIR=$(dirname $SOURCE)
MAIN=$(realpath "$TOOLSDIR/..")

if [ ! -d "$MAIN/linux" ]; then

    sudo apt update
    sudo apt install bison cpio cryptsetup-bin dos2unix dosfstools exfatprogs flex gcc grub-efi-amd64 libcap-dev libelf-dev libssl-dev make mtools ntfs-3g openssh-server ovmf qemu-system-x86 unzip xorriso zip
    if [ "$?" -eq "1" ]; then
        echo "Can't continue without these dependencies.. exiting."
        exit 1
    fi

fi

