project('libhinawa', 'c',
  version: '2.0.0',
  license: 'LGPL-2.1+',
  meson_version: '>= 0.46.0',
)

inc = include_directories('src')

conf = configuration_data()
conf.set('VERSION', meson.project_version())

# For g-i and documentation.
gnome = import('gnome')

subdir('src')
subdir('doc')
subdir('tests')
