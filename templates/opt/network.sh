#!/bin/bash

# Import global configs to figure out what to do
eval "$(cat /etc/network/network.sh)"

check_interface() {
    if [ ! -d /sys/class/net/$1 ]; then
        return 1
    fi
}

enable_interface() {
    check_interface $1
    if [ $? -eq 1 ]; then
        echo Failed to enable interface $1, does not exist > /dev/stderr
        return 1
    fi

    ip link set $1 up

    return $?
}

disable_interface() {
    check_interface $1
    if [ $? -eq 1 ]; then
        echo Failed to disable interface $1, does not exist > /dev/stderr
        return 1
    fi

    ip link set $1 down

    return $?
}

set_default() {
    check_interface $1
    if [ $? -eq 1 ]; then
        echo Failed to set $1 as default route, interface does not exist > /dev/stderr
        return 1
    fi

    ip route replace default dev $1
    if [ $? -ne 0 ]; then
        echo Failed to set $1 as default route, ip command failed > /dev/stderr
        return 1
    fi
    return 0
}

initialize_interfaces() {
    # Check if interface exists and enable
    enable_interface lo
    enable_interface $1
    if [ $? -eq 1 ];then
        echo "Failed to enable $1 on method $2" > /dev/stderr
        return 1
    fi

    set_default $1
    if [ $? -eq 1 ];then
        echo "Failed to make $1 default on method $2" > /dev/stderr
        return 1
    fi
    return 0
}

if [[ $method == "disabled" ]]; then
    echo "Network is disabled by /etc/network/network.sh" > /dev/stderr
    echo "network disabled" > /tmp/netstat
    exit 0
fi

if [[ $interface == "auto" ]]; then
    # Fallback
    interface=eth0
    for net in /sys/class/net/eth*; do
        enable_interface $(basename $net)
        link=$(cat "$net/carrier")
        if [ "$link" -eq "1" ]; then
            interface=$(basename $net)
            break
        fi
        disable_interface $(basename $net)
    done
fi

if [[ $method == "dhcpv4" ]]; then
    # init interfaces
    initialize_interfaces $interface dhcpv4
    if [ $? -ne 0 ]; then
        echo "Failed to configure network using method dhcpv4" > /dev/stderr
        echo "offline" > /tmp/netstat
        exit 1
    fi

    error=wait
    dhcpcount=0
    while [[ -n $error ]]; do
        ((dhcpcount++))

        # Obtain DHCP information
        eval "$(dhcp $interface)"

        if [[ $dhcpcount -ge 3 ]]; then
            echo "Giving up sending dhcp requests to $interface after 3rd time" > /dev/stderr
            break
        fi

        sleep 0.2
    done

    # Only configure ip, if no error happened
    if [ ! -z "${error}" ]; then
        echo $error > /dev/stderr
        echo "offline" > /tmp/netstat
        echo "Failed to configure network using method dhcpv4" > /dev/stderr
        exit 1
    else
        ip addr flush dev $interface
        ip addr add $address/$cidr dev $interface

        if [ ! -z "${gateway}" ]; then
            ip route replace default via $gateway
        fi

        if [ ! -z "${dns1}" ]; then
            echo nameserver $dns1 > /etc/resolv.conf
        fi

        if [ ! -z "${dns2}" ]; then
            echo nameserver $dns2 >> /etc/resolv.conf
        fi

        echo "$address\/$cidr" > /tmp/netstat
        exit 0
    fi
fi

if [[ $method == "static" ]]; then
    # init interfaces
    initialize_interfaces $interface static
    if [ $? -ne 0 ]; then
        echo "Failed to configure network using method static" > /dev/stderr
        echo "offline" > /tmp/netstat
        exit 1
    fi

    ip addr flush dev $interface
    ip addr add $v4address/$v4cidr dev $interface

    if [ ! -z "${v4gateway}" ]; then
       ip route replace default via $v4gateway
    fi

    if [ ! -z "${v4dns1}" ]; then
       echo nameserver $v4dns1 > /etc/resolv.conf
    fi

    if [ ! -z "${v4dns2}" ]; then
       echo nameserver $v4dns2 >> /etc/resolv.conf
    fi

    echo "$v4address\/$v4cidr" > /tmp/netstat
    exit 0
fi
