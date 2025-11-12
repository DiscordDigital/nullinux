#!/bin/bash
SOURCE=${BASH_SOURCE[0]}
TOOLSDIR=$(dirname $SOURCE)
ROOTFS=$(realpath "$TOOLSDIR/../rootfs")
TEMPDIR=$(realpath "$TOOLSDIR/../temp")
MAIN=$(realpath "$TOOLSDIR/../")

# Copy OVMF file from host to temp
cp /usr/share/OVMF/OVMF_CODE_4M.fd "$TEMPDIR/"

# Create persistent drive if not exist
if [ ! -f "$TEMPDIR/persistent.img" ]; then
    dd if=/dev/zero of="$TEMPDIR/persistent.img" bs=1M count=1000
fi

# Start qemu
qemu-system-x86_64 -cdrom "$MAIN/nullinux.iso" -m 1024 -drive if=pflash,format=raw,readonly=on,file="$TEMPDIR/OVMF_CODE_4M.fd" -netdev user,id=net0 -device virtio-net-pci,netdev=net0 -drive file="$TEMPDIR/persistent.img",format=raw
