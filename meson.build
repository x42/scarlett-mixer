project('scarlett-mixer', 'c')

cc = meson.get_compiler('c')

deps = [
  dependency('cairo'),
  dependency('pango'),
  dependency('pangocairo'),
  dependency('gl'),
  dependency('glu'),
  dependency('alsa'),
  dependency('lv2'),
  dependency('threads'),
  cc.find_library('m'),
  cc.find_library('X11'),
]

executable('scarlett-mixer',
  sources: [
    'robtk/robtkapp.c',
    'robtk/ui_gl.c',
    'robtk/pugl/pugl_x11.c',
  ],
  dependencies: deps,
  include_directories: include_directories('robtk'),
  c_args: [
    '-DAPPTITLE="Scarlett 18i6/18i8 Mixer"',
    '-DDEFAULT_NOT_ONTOP',
    '-DXTERNAL_UI',
    '-DHAVE_IDLE_IFACE',
    '-DRTK_DESCRIPTOR=lv2ui_descriptor',
    '-DPLUGIN_SOURCE="src/scarlett_mixer.c"',
    '-Wno-unused-function',
  ],
)
