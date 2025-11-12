#!/bin/bash

# Methods: disabled, static, dhcpv4
export method=dhcpv4
export interface=auto

# Static ip configuration, set method to static first
export v4address=192.168.0.10
export v4cidr=24
export v4gateway=192.168.0.1
export v4dns1=192.168.0.1
export v4dns2=
