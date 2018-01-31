Graphical Mixer Interface for the Scarlett series
=================================================

Currently supported models:
- 18i6
- 18i8

This is a quick hack, it's prepared for other Scarlett devices, but you're on your own..


Build-dependencies: gnu-make, a c-compiler, pkg-config, lv2-dev, libasound2-dev,
libpango, libcairo and openGL (sometimes called: glu, glx, mesa).

```bash
  git clone git://github.com/x42/scarlett-mixer
  cd scarlett-mixer
  git submodule init
  git submodule update
  make
```

```bash
  ./scarlett-mixer hw:2
```

Screenshot
----------

![screenshot](https://raw.github.com/x42/scarlett-mixer/master/scarlett-mixer-gui.png "Scarlett 18i6 Mixer")


See also
--------

ALSA Mixer in HTLM-5 with ALSA JSON Gateway: http://breizhme.net/alsajson/mixers/ajg#/
