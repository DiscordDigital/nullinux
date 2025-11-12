#!/bin/bash
VERSION=v6.17
SOURCE=${BASH_SOURCE[0]}
TOOLSDIR=$(dirname $SOURCE)
NULFOLDER=$(realpath "$TOOLSDIR/..")
CWD=$(pwd)

if [ ! -d "$NULFOLDER/linux" ]; then
    # go to nul folder
    cd "$NULFOLDER"

    # obtain linux
    git clone --depth 1 --branch $VERSION https://github.com/torvalds/linux

    # insert kernel configurations
    cp templates/kernel linux/.config

    # run make oldconfig
    cd linux
    make oldconfig

    # compile
    "$TOOLSDIR/mklinux.sh"

    # return
    cd "$CWD"
fi
