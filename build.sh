#!/bin/bash
SOURCE=${BASH_SOURCE[0]}
MAIN=$(dirname $SOURCE)

show_help() {
cat << EOF
Usage: $0 [options]

Installs required dependencies, and crafts an ISO file in the same directory as $(basename $0).

Options:
  -h, --help                   Shows the help of the build tool.
  -q, --quiet                  Skips running qemu, useful if you just want to obtain the ISO file.
  -s, --skip-qemu-install      Skips the installation of qemu.
  -i, --install-qemu           Useful if you initially skipped the installation of qemu.
  -x, --extract                Extracts the generated ISO file to an output directory.
EOF
}

install_qemu() {
    sudo apt update
    sudo apt install ovmf qemu-system-x86
    return $?
}

args="$@"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -q|--quiet)
            quiet=1
            ;;
        -s|--skip-qemu-install)
            skip_qemu=1
            ;;
        -i|--install-qemu)
            install_qemu
            exit $?
            ;;
        -x|--extract)
            shift
            extract="$1"
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 10
            ;;
    esac
    shift
done

MAIN=$(realpath $(dirname ${BASH_SOURCE[0]}))
TOOLSDIR=$(realpath "$MAIN/tools")

declare -a tools=( "dependencies" "getlinux" "mkrootfs" "compiletools" "filegrab" "copymodules" "getkbd" "timezone" "createinitcpio" "grubiso" "qemutest" )

for tool in "${tools[@]}"; do

    echo "Running $tool.sh"
    "$TOOLSDIR/$tool.sh" $args

    if [ "$?" -eq "1" ]; then
        echo "Tool exited with 1, aborting."
        exit 1
    fi
done

if [[ -v extract ]]; then
    if [ -d "$extract" ]; then
        echo "Extracting nullinux.iso to $extract"
        mv "$MAIN/nullinux.iso" "$extract"
        exit $?
    else
        echo "Extract was specified, but the directory \"$extract\" was not found."
        exit -1
    fi
fi

exit 0

