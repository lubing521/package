#
# Copyright (C) 2013 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=ismart_alsa
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/ismart_alsa
	SUBMENU:=i2s alsa modules
	TITLE:=Atheros 933X I2S module
	DEPENDS:=
	FILES:=$(PKG_BUILD_DIR)/ar7240_alsa.ko $(PKG_BUILD_DIR)/i2c_gpio_bus.ko
#	FILES:=$(PKG_BUILD_DIR)/ar7240_i2s.ko
	KCONFIG:=
endef

define KernelPackage/ismart_alsa/description
 Kernel module for atheros ar933x i2s support
endef

EXTRA_KCONFIG:= \
	CONFIG_AR9331_I2S=m

EXTRA_CFLAGS:= \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=m,%,$(filter %=m,$(EXTRA_KCONFIG)))) \
	$(patsubst CONFIG_%, -DCONFIG_%=1, $(patsubst %=y,%,$(filter %=y,$(EXTRA_KCONFIG)))) \

MAKE_OPTS:= \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)" \
	SUBDIRS="$(PKG_BUILD_DIR)" \
	EXTRA_CFLAGS="$(EXTRA_CFLAGS)" \
	$(EXTRA_KCONFIG)

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
	$(MAKE_OPTS) \
	modules
endef

$(eval $(call KernelPackage,ismart_alsa))
