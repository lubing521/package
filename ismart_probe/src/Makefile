SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
#CC = gcc
#INCLUDES = -I ./lib -I /usr/include/libnl3
CCFLAGS = -Wall -O0
#CCFLAGS = -Wall -O0
#LDFLAGS = -L
LIBS += -lpthread -lpcap
PKG_CONFIG ?= pkg-config

ifeq ($(NO_PKG_CONFIG),)
NL3xFOUND := $(shell $(PKG_CONFIG) --atleast-version=3.2 libnl-3.0 && echo Y)
ifneq ($(NL3xFOUND),Y)
NL31FOUND := $(shell $(PKG_CONFIG) --exact-version=3.1 libnl-3.1 && echo Y)
ifneq ($(NL31FOUND),Y)
NL3FOUND := $(shell $(PKG_CONFIG) --atleast-version=3 libnl-3.0 && echo Y)
ifneq ($(NL3FOUND),Y)
NL2FOUND := $(shell $(PKG_CONFIG) --atleast-version=2 libnl-2.0 && echo Y)
ifneq ($(NL2FOUND),Y)
NL1FOUND := $(shell $(PKG_CONFIG) --atleast-version=1 libnl-1 && echo Y)
endif
endif
endif
endif

ifeq ($(NL1FOUND),Y)
NLLIBNAME = libnl-1
endif

ifeq ($(NL2FOUND),Y)
CCFLAGS += -DCONFIG_LIBNL20
LIBS += -lnl-genl
NLLIBNAME = libnl-2.0
endif

ifeq ($(NL3xFOUND),Y)
# libnl 3.2 might be found as 3.2 and 3.0
NL3FOUND = N
CCFLAGS += -DCONFIG_LIBNL30
LIBS += -lnl-genl-3
NLLIBNAME = libnl-3.0
endif

ifeq ($(NL3FOUND),Y)
CCFLAGS += -DCONFIG_LIBNL30
LIBS += -lnl-genl
NLLIBNAME = libnl-3.0
endif

# nl-3.1 has a broken libnl-gnl-3.1.pc file
# as show by pkg-config --debug --libs --cflags --exact-version=3.1 libnl-genl-3.1;echo $?
ifeq ($(NL31FOUND),Y)
CCFLAGS += -DCONFIG_LIBNL30
LIBS += -lnl-genl
NLLIBNAME = libnl-3.1
endif

ifeq ($(NLLIBNAME),)
$(error Cannot find development files for any supported version of libnl)
endif

LIBS += $(shell $(PKG_CONFIG) --libs $(NLLIBNAME))
CCFLAGS += $(shell $(PKG_CONFIG) --cflags $(NLLIBNAME))
endif # NO_PKG_CONFIG

ismart_probe : $(OBJS)
	$(CC) ismart_probe.o List.o radiotap.o wireless.o sections.o phy.o util.o -o $@  $(LIBS) -lpthread -lpcap

	cat tp-link.txt d-link.txt mercury.txt hiwifi.txt netcore.txt netgear.txt tenda.txt h3c.txt volans.txt b-link.txt fast.txt ruijie.txt UTT.txt > router.txt 
	$(CC) probectl.o -o probectl

#getline:
#	$(CC)  -o getline getline.c

%.o : %.c
	$(CC) -c $< $(CCFLAGS) $(INCLUDES)
clean:
	rm -rf *.o
	rm -rf ismart_probe probectl
.PHONY:clean


