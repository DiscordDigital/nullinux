#!/bin/bash
export tzfile="$HOME/config/timezone"
if [ -f $tzfile ]; then
    export timezone=$(<$tzfile)
    if [ -f "/usr/share/zoneinfo/$timezone" ]; then
        ln -s "/usr/share/zoneinfo/$timezone" "/etc/localtime"
    fi
fi
