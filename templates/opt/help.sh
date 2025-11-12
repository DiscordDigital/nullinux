#!/bin/bash

NULVERSION=$(cat /etc/os-release)

echo $NULVERSION
echo
echo "bioskey         - Shows embedded Windows key from bios"
echo "letmein         - Enables a backdoor in Windows to bypass logon screen"
echo "map_bitlocker   - Tool to manage BitLocker related drives"
echo "winfind         - Tool to find Windows on the system"
echo "persistentsetup - Tool to setup persistent drive"
echo "loadkeys        - Load keyboard layouts"
echo "passwd          - Change user password"
echo "sshserver       - Starts ssh server"
echo "webserver       - Starts a minimal http server"
echo "poweroff        - Turn system off"
echo "reboot          - Reboot system"
exit 0
