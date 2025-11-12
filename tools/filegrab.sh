#!/bin/bash
SOURCE=${BASH_SOURCE[0]}
TOOLSDIR=$(dirname $SOURCE)
ROOTFS=$(realpath "$TOOLSDIR/../rootfs")
CONFIGDIR=$(realpath "$TOOLSDIR/../config")

if [ ! -d $ROOTFS ]; then
    echo "rootfs does not exist."
    exit 1
fi

EMBEDDED_BINS=$CONFIGDIR/embed.txt

while IFS= read -r line; do
    if [[ $line == /* ]]; then
        virtfile=${line:1}
        virtdir=$ROOTFS/$(dirname $virtfile)
        realfile=$line

        if [ ! -d $virtdir ]; then
            mkdir -p $virtdir
        fi

        cp $realfile $virtdir
        echo "Copying $realfile to $virtdir"

        ldd $realfile | while IFS= read -r line; do
           l1=$(echo "$line" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
           valid=false
           if [[ $l1 == *"=>"* ]]; then
               valid=true
               realfile=$(echo "$l1" | awk '{print $3}')
           fi
           if [[ $l1 == /* ]]; then
               valid=true
               read -r realfile _ <<< "$l1"
           fi
           if [ "$valid" = true ]; then
               virtfile=${realfile:1}
               virtdir=$ROOTFS/$(dirname $virtfile)

               if [ ! -d $virtdir ]; then
                   mkdir -p $virtdir
               fi

               cp $realfile $virtdir
               echo "Copying $realfile to $virtdir"
           fi
        done
    else
        echo "Ignoring $line because it is not an absolute path"
    fi
done < "$EMBEDDED_BINS"
