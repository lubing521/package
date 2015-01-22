#!/bin/sh /etc/rc.common
# (C) 2008 openwrt.org

START=99
STOP=99

start() {
    [ -x /usr/bin/ismart_probe ] && sleep 50s && ismart_probe -i wlan0 -d &
    [ -x /usr/bin/wget_unblock.sh ] && wget_unblock.sh &
}

stop() {
    killall ismart_probe
}

