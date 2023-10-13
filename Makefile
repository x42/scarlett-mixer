#!/usr/bin/make -f

PREFIX ?= /usr/local
bindir = $(PREFIX)/bin
mandir = $(PREFIX)/share/man/man1

CFLAGS  ?= -g -Wall -Wno-unused-function

PKG_CONFIG ?= pkg-config

VERSION ?= $(shell (git describe --tags HEAD 2>/dev/null || echo "v0.1") | sed 's/-g.*$$//;s/^v//')
RW      ?= robtk/

APP_SRC  = src/scarlett_mixer.c
PUGL_SRC = $(RW)pugl/pugl_x11.c

ifeq ($(shell $(PKG_CONFIG) --exists cairo pangocairo pango glu gl alsa || echo no), no)
  $(error "build dependencies are not satisfied")
endif

ifeq ($(shell $(PKG_CONFIG) --atleast-version=1.18.6 lv2 && echo yes), yes)
  override CFLAGS += -DHAVE_LV2_1_18_6
endif

GLUICFLAGS=-I. -I$(RW)
GLUICFLAGS+=`$(PKG_CONFIG) --cflags cairo pango lv2 glu alsa` -pthread
GLUICFLAGS+=-DDEFAULT_NOT_ONTOP

LOADLIBES=`$(PKG_CONFIG) --libs $(PKG_UI_FLAGS) cairo pangocairo pango glu gl alsa` -lX11 -lm

###############################################################################
all: scarlett-mixer

man: scarlett-mixer.1

# TODO source $(RW)robtk.mk, add dependencies

scarlett-mixer: $(APP_SRC) $(RW)robtkapp.c $(RW)ui_gl.c $(PUGL_SRC) Makefile
	$(CC) $(CPPFLAGS) \
		-o $@ \
		-DVERSION=\"$(VERSION)\" \
		$(CFLAGS) $(GLUICFLAGS) -std=c99 \
		-DXTERNAL_UI -DHAVE_IDLE_IFACE -DRTK_DESCRIPTOR=lv2ui_descriptor \
		-DPLUGIN_SOURCE=\"$(APP_SRC)\" \
		-DAPPTITLE="\"Scarlett Mixer\"" \
		$(RW)robtkapp.c $(RW)ui_gl.c $(PUGL_SRC) \
		$(LDFLAGS) $(LOADLIBES)

clean:
	rm -f scarlett-mixer

scarlett-mixer.1: scarlett-mixer
	help2man -N -n 'Mixer GUI for Focusrite Scarlett USB Devices' -o scarlett-mixer.1 ./scarlett-mixer


install: install-bin install-man

uninstall: uninstall-bin uninstall-man

install-bin: scarlett-mixer
	install -d $(DESTDIR)$(bindir)
	install -m755 scarlett-mixer $(DESTDIR)$(bindir)

uninstall-bin:
	rm -f $(DESTDIR)$(bindir)/scarlett-mixer
	-rmdir $(DESTDIR)$(bindir)

install-man:
	install -d $(DESTDIR)$(mandir)
	install -m644 scarlett-mixer.1 $(DESTDIR)$(mandir)

uninstall-man:
	rm -f $(DESTDIR)$(mandir)/scarlett-mixer.1
	-rmdir $(DESTDIR)$(mandir)


.PHONY: all clean install uninstall man install-man install-bin uninstall-man uninstall-bin
