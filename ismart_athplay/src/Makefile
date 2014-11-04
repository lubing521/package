#
# Makefile for Atheros tools.
#

#ifneq ($(strip $(TOOLPREFIX)),)
#CROSS:=$(TOOLPREFIX)
#endif

#CC=$(CROSS)gcc
#STRIP           =$(CROSS)strip

ifeq ($(strip ${ATH_WASP_I2S}),1)
EXTRA_CFLAGS += -DWASP
endif

ALL=	athplay i2sconf ioctl alsa

all:	${ALL}

athplay: athplay.c
	${CC} ${EXTRA_CFLAGS} -o athplay athplay.c

i2sconf: i2sconf.c
	${CC} -o i2sconf i2sconf.c

ioctl: ioctl.c
	${CC} -g -o ioctl ioctl.c

alsa:alsa.c
	${CC} -g -o alsa alsa.c -lasound

clean: 
	rm -f ${ALL} 
