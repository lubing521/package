
include $(TOPDIR)/rules.mk
PKG_NAME:=ismart_gmrender
PKG_VERSION:=1.12.9
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

PKG_FIXUP:=autoreconf

TARGET_CFLAGS += -std=c99

include $(INCLUDE_DIR)/package.mk
 
define Package/$(PKG_NAME)
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=dlna render
	MAINTAINER:=dengjunyong
	DEPENDS:=+libpthread +glib2 +gstreamer +libupnp +libxml2
endef

define Package/$(PKG_NAME)/description
	If you can't figure out what this program does, you're probably
	brain-dead and need immediate medical attention.
endef
 
define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/gmediarender $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/usr/share/gmediarender
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/data/grender-64x64.png $(1)/usr/share/gmediarender/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/data/grender-128x128.png $(1)/usr/share/gmediarender/
endef

$(eval $(call BuildPackage,ismart_gmrender))
