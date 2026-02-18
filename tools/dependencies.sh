#!/bin/bash
SOURCE=${BASH_SOURCE[0]}
TOOLSDIR=$(dirname $SOURCE)
MAIN=$(realpath "$TOOLSDIR/..")
DEPENDENCIES="bison cpio cryptsetup-bin dos2unix dosfstools exfatprogs flex gcc grub-efi-amd64 libcap-dev libelf-dev libssl-dev make mtools ntfs-3g openssh-server unzip xorriso zip"
QEMUPACKAGES="ovmf qemu-system-x86"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -s|--skip-qemu-install)
            skip_qemu=1
            ;;
        -q|--quiet)
            quiet=-y
            ;;
    esac
    shift
done

if [ ! -d "$MAIN/linux" ]; then

    sudo apt update
    if [[ -v skip_qemu ]]; then
        echo "Skipping qemu packages.."
        sudo apt install $DEPENDENCIES $quiet
    else
        sudo apt install $DEPENDENCIES $QEMUPACKAGES $quiet
    fi
   
    if [ "$?" -eq "1" ]; then
        echo "Can't continue without these dependencies.. exiting."
        exit 1
    fi

fi

