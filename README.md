Graphical Mixer Interface for the Scarlett series
=================================================

Currently supported models:
- 18i6
- 18i8
- 18i20
- 6i6 (untested)

This is a **quick hack**, it's prepared for other Scarlett devices, but you are on your own...

Please do not package it as-is, or make it available to end-users since most will be disappointed.

The mixer-elements are numerically indexed and only work with vanilla Linux. Also note
that the device must be supported by the ALSA Liunux kernel device-driver. At the time of writing
only the 1st generation of Scarlett devices are supported (Linux 4.16, April 2018).

Install
-------

Build-dependencies: gnu-make, a c-compiler, pkg-config, libpango, libcairo,
lv2 (SDK), alsa (libasound) and openGL (sometimes called: glu, glx, mesa).

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
