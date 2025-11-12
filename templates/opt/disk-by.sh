#!/bin/bash
mkdir -p /dev/disk/by-uuid
mkdir -p /dev/disk/by-label

cdboot=no
if [ -e /dev/sr0 ]; then
     cdboot=yes
     sr0label=$(blkid -s LABEL -o value "/dev/sr0" 2>/dev/null)
     if [[ $sr0label != "NULLINUX" ]]; then
         # Wait for removable disk to exist
         cdboot=no
     fi
fi

waiting=yes
if [[ $cdboot == "no" ]]; then
    i=0
    while [[ $waiting == "yes" ]]; do
        if [ $i -eq "10" ]; then
            echo "Timed out waiting for removable devices." > /dev/stderr
            break
        fi
        for dev in /sys/block/sd*; do
            if [ -e "$dev" ] && [ "$(cat $dev/removable)" -eq 1 ]; then
                waiting=no
            fi
        done
        sleep 0.2
        ((i++))
    done
fi

for dev in /dev/sd?* /dev/nvme*n*; do
    # [ -e "$dev" ] || continue

    label=$(blkid -s LABEL -o value "$dev" 2>/dev/null)
    uuid=$(blkid -s UUID -o value $dev 2>/dev/null)

    [ -n "$uuid" ] && ln -s $dev /dev/disk/by-uuid/$uuid

    if [ -n "$label" ]; then
        link="/dev/disk/by-label/$label"
        if [ -e "$link" ]; then
            count=1
            while [ -e "${link}-$count" ]; do
                count=$((count + 1))
            done
            link="${link}-$count"
        fi
        ln -s "$dev" "$link"
    fi
done
