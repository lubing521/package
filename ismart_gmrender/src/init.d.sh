#!/bin/sh /etc/rc.common
# (C) 2008 openwrt.org

START=99
STOP=99

start() {
        insmod /root/i2c_gpio_bus.ko
        insmod /root/ar7240_alsa.ko
        sleep 20s && LANIP=`ifconfig eth0 | grep "inet addr" | awk -F: '{print $2}' | awk '{printf $1}'` && [ -x /usr/sbin/gmediarender ] && gmediarender -I $LANIP -f guogee -d &
}

stop() {
        killall gmediarender
}
