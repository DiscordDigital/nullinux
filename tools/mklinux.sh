#!/bin/bash
SOURCE=${BASH_SOURCE[0]}
TOOLSDIR=$(dirname $SOURCE)
MAIN=$(realpath "$TOOLSDIR/..")
LINUXDIR=$(realpath "$MAIN/linux")
CWD=$(pwd)

cd "$LINUXDIR"
make KBUILD_BUILD_USER=root KBUILD_BUILD_HOST=localhost -j$(nproc)

cd "$CWD"
