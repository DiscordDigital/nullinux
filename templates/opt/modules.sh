#!/bin/bash
modprobe /lib/modules/dm-mod.ko > /dev/null
modprobe /lib/modules/dm-crypt.ko > /dev/null
modprobe /lib/modules/dm-zero.ko > /dev/null
exit 0
