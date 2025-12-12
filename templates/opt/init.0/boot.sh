#!/bin/bash

# Autostart all init0 sync entries in /opt/init.0/autostart.sync.txt
while read line; do
    bash -c "$line" 2> >( /opt/tools/log "$(basename $line)" )
done </opt/init.0/autostart.sync.txt

# Autostart all init0 sync entries in /opt/init.0/user.autostart.sync.txt
if [[ -f /opt/init.0/user.autostart.sync.txt ]]; then
    while read line; do
        bash -c "$line" 2> >( /opt/tools/log "$(basename $line)" )
    done </opt/init.0/user.autostart.sync.txt
fi

# Splash
eval "/opt/splash.sh"