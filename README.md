Graphical Mixer Interface for the Scarlett series
=================================================

Currently supported models, first generation of
- 18i6
- 18i8
- 18i20
- 6i6 (untested)

This is just a GUI, the device **must** be supported by the ALSA Linux kernel device-driver.

The mixer-elements are numerically indexed and only work with vanilla Linux.
At the time of writing only the 1st generation of Scarlett devices are supported (Linux 4.16, April 2018).

This UI a **quick hack**, it may or may not work and is prepared for other Scarlett devices, but **you** **are** **on** **your** **own**.

Please do **not** package this software as-is, nor make it available to end-users since most will be disappointed.

Setup
-----

Build-dependencies: gnu-make, a c-compiler, pkg-config, libpango, libcairo,
lv2 (SDK), alsa (libasound) and openGL (sometimes called: glu, glx, mesa).

```bash
  git clone git://github.com/x42/scarlett-mixer
  cd scarlett-mixer
  git submodule init
  git submodule update
  make
```

Usage (run from source-dir)
---------------------------

```bash
  ./scarlett-mixer --help
  ./scarlett-mixer hw:2   # change "hw:2" to match your device
```

Screenshot
----------

![screenshot](https://raw.github.com/x42/scarlett-mixer/master/scarlett-mixer-gui.png "Scarlett 18i6 Mixer")

See also
--------

ALSA Mixer in HTLM-5 with ALSA JSON Gateway: http://breizhme.net/alsajson/mixers/ajg#/
