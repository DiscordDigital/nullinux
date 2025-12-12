#!/bin/bash
SOURCE=${BASH_SOURCE[0]}
TOOLSDIR=$(dirname $SOURCE)
MAIN=$(realpath "$TOOLSDIR/..")
ROOTFS=$(realpath "$MAIN/rootfs")

# Create /usr/share if it does not exist inside rootfs
mkdir -p "$ROOTFS/usr/share/"

# Copy zoneinfo folder into rootfs
cp -r /usr/share/zoneinfo "$ROOTFS/usr/share/"
