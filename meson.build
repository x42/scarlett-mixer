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

name = 'Scarlett ' + get_option('num_inputs') + 'i' + get_option('num_outputs')
apptitle = '-DAPPTITLE="' + name + ' Mixer"'

conf = configuration_data()
conf.set('SMI', get_option('num_inputs'))
conf.set('SMO', get_option('num_outputs'))
conf.set('SIN', get_option('num_inputs'))
conf.set('SOUT', get_option('num_outputs'))
conf.set_quoted('DEVICE_NAME', name + ' USB')

configure_file(
  output: 'config.h',
  configuration: conf
)

executable('scarlett-mixer',
  sources: [
    'robtk/robtkapp.c',
    'robtk/ui_gl.c',
    'robtk/pugl/pugl_x11.c',
  ],
  dependencies: deps,
  include_directories: include_directories('robtk'),
  c_args: [
    apptitle,
    '-xc++',  # temporary fix to make it compile with g++
    '-DHAVE_CONFIG_H',
    '-DDEFAULT_NOT_ONTOP',
    '-DXTERNAL_UI',
    '-DHAVE_IDLE_IFACE',
    '-DRTK_DESCRIPTOR=lv2ui_descriptor',
    '-DPLUGIN_SOURCE="src/scarlett_mixer.c"',
    '-Wno-unused-function',
  ],
)
