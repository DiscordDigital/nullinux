#!/bin/bash
kernel_version=$(uname -r)
cpu=$(cat /proc/cpuinfo | grep "model name" | awk -F': ' '{print $2}')
network_status=$(cat /tmp/netstat)

if [ ! -f /tmp/persistent ]; then
    persistent_mode="ramdisk mode"
else
    persistent_mode="persistent mode"
fi

cat /opt/nulsplash.txt | 
sed "s/{kernel_version}/$kernel_version/g" | 
sed "s/{cpu}/$cpu/g" | 
sed "s/{network_status}/$network_status/g" | 
sed "s/{persistent_mode}/$persistent_mode/g"
exit 0