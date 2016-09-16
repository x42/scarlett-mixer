CFLAGS=-g -Wall -Wno-unused-function
RW?=robtk/

APPTITLE=Scarlett 18i6 Mixer
APP_SRC=src/scarlett_mixer.c

PUGL_SRC=$(RW)pugl/pugl_x11.c

GLUICFLAGS=-I. -I$(RW)
GLUICFLAGS+=`pkg-config --cflags cairo pango lv2 glu alsa` -pthread
GLUICFLAGS+=-DDEFAULT_NOT_ONTOP

LOADLIBES=`pkg-config --libs $(PKG_UI_FLAGS) cairo pangocairo pango glu gl alsa` -lX11

all: scarlett-mixer

# TODO source $(RW)robtk.mk, add dependencies

scarlett-mixer: $(APP_SRC) $(RW)robtkapp.c $(RW)ui_gl.c $(PUGL_SRC) Makefile
	$(CXX) $(CPPFLAGS) \
		-o $@ \
		$(CFLAGS) $(GLUICFLAGS) \
		-DXTERNAL_UI -DHAVE_IDLE_IFACE -DRTK_DESCRIPTOR=lv2ui_descriptor \
		-DPLUGIN_SOURCE=\"$(APP_SRC)\" \
		-DAPPTITLE="\"$(APPTITLE)\"" \
		$(RW)robtkapp.c $(RW)ui_gl.c $(PUGL_SRC) \
		$(LDFLAGS) $(LOADLIBES)

clean:
	rm -f scarlett-mixer

.PHONY: clean all
