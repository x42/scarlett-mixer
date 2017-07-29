Graphical Mixer Interface for the Scarlett 18i6
===============================================

This is a quick hack, you're on your own..


Build-dependencies: gnu-make, a c-compiler, libpango, libcairo
and openGL (sometimes called: glu, glx, mesa).

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
