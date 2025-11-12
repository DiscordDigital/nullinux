#!/bin/bash

export PATH=/opt/tools:$PATH

if [ ! -f /tmp/init0 ]; then
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

    # Seal init0 execution
    touch /tmp/init0

    # cd into /root if persistent mode and source .bashrc again
    if [ -f /tmp/persistent ]; then
        cd $HOME
        source $HOME/.bashrc
    fi
fi

splash() {
    /opt/splash.sh
}

help() {
    /opt/help.sh
}

PS1='(\[\e[32;1m\]\W\[\e[0m\])> '
