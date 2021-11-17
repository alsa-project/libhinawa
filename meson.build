project('libhinawa', 'c',
  version: '2.3.0',
  license: 'LGPL-2.1+',
  meson_version: '>= 0.46.0',
)

# Detect support level in Linux sound subsystem.
cc = meson.get_compiler('c')

# For g-i and documentation.
gnome = import('gnome')
inc = include_directories('src')

subdir('src')
subdir('doc')
subdir('tests')
