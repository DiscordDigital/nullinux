#!/bin/bash
MAIN=$(realpath $(dirname ${BASH_SOURCE[0]}))
TOOLSDIR=$(realpath "$MAIN/tools")

declare -a tools=( "dependencies" "getlinux" "mkrootfs" "compiletools" "filegrab" "copymodules" "getkbd" "createinitcpio" "grubiso" "qemutest" )

for tool in "${tools[@]}"; do
    echo "Running $tool.sh"
    "$TOOLSDIR/$tool.sh"
    if [ "$?" -eq "1" ]; then
        echo "Tool exited with 1, aborting."
        exit 1
    fi
done

exit 0

