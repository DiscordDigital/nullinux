#!/bin/bash

export PATH=/opt/tools:$PATH

splash() {
    /opt/splash.sh
}

help() {
    /opt/help.sh
}

PS1='(\[\e[32;1m\]\W\[\e[0m\])> '
