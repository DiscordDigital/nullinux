#!/bin/bash
disk=/dev/disk/by-label/NHOME
if [ -L $disk ]; then
    mountEL=1
    # Create a tmp mount to inspect the partition
    mount $disk /mnt
    if [ $? -eq 0 ]; then
        if [ -f /mnt/.bashrc ]; then
            # In this scenario remount onto /root
            umount /mnt

            # while mountpoint -q /mnt; do sleep 0.2; done
            mount $disk /root -o umask=077
            mountEL=$?

            # replace config files from persistence config folder
            if [[ -f /root/config/group ]]; then
                rm /etc/group
                ln -s /root/config/group /etc/group
            fi

            if [[ -f /root/config/passwd ]]; then
                rm /etc/passwd
                ln -s /root/config/passwd /etc/passwd
            fi

            if [[ -f /root/config/shadow ]]; then
                rm /etc/shadow
                ln -s /root/config/shadow /etc/shadow
            fi

            if [[ -f /root/config/network.sh ]]; then
                rm /etc/network/network.sh
                ln -s /root/config/network.sh /etc/network/network.sh
            fi

            if [[ -d /root/config/ssh ]]; then
                rm -r /etc/ssh
                ln -s /root/config/ssh /etc/ssh
            fi

            if [[ -f /root/config/user.autostart.sync.txt ]]; then
                ln -s /root/config/user.autostart.sync.txt /opt/init.0/user.autostart.sync.txt
            fi
        else
            echo Found unconfigured persistent drive.
            echo Type persistentsetup to configure.
            umount /mnt
            exit 0
        fi

        if [ $mountEL -eq 0 ]; then
            touch /tmp/persistent
            exit 0
        else
            if mountpoint -q /mnt; then
                umount /mnt
            fi
            echo Failed to mount $disk to /root, home is not persistent! > /dev/stderr
        fi
    else
        echo Failed to mount $disk to /mnt > /dev/stderr
    fi
fi
