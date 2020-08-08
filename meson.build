project('libhinawa', 'c',
  version: '2.0.0',
  license: 'LGPL-2.1+',
  meson_version: '>= 0.46.0',
)

# Detect support level in Linux sound subsystem.
cc = meson.get_compiler('c')

inc = include_directories('src')

if cc.has_header('backport.h', include_directories: inc)
  with_backport = true
else
  with_backport = false
endif

conf = configuration_data()
conf.set10('WITH_BACKPORT',  with_backport)
conf.set('VERSION', meson.project_version())

# For g-i and documentation.
gnome = import('gnome')

subdir('src')
subdir('doc')
subdir('tests')
