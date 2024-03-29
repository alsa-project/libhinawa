# Depends on glib-2.0 and gobject-2.0
gobject = dependency('gobject-2.0',
  version: '>=2.44.0'
)
dependencies = [
  gobject,
]

sources = [
  'fw_node.c',
  'fw_resp.c',
  'fw_req.c',
  'fw_fcp.c',
  'cycle_time.c',
]

headers = [
  'hinawa.h',
  'fw_node.h',
  'fw_resp.h',
  'fw_req.h',
  'fw_fcp.h',
  'cycle_time.h',
  'hinawa_enum_types.h',
]

privates = [
  'internal.h',
]

inc_dir = meson.project_name()

# Generate marshallers for GObject signals.
marshallers = gnome.genmarshal('hinawa_sigs_marshal',
  prefix: 'hinawa_sigs_marshal',
  sources: 'hinawa_sigs_marshal.list',
  install_header: true,
  install_dir: join_paths(get_option('includedir'), inc_dir),
  stdinc: true,
)

enums = gnome.mkenums_simple('hinawa_enums',
  sources: ['hinawa_enum_types.h'],
  identifier_prefix: 'Hinawa',
  symbol_prefix: 'hinawa',
  install_header: true,
  install_dir: join_paths(get_option('includedir'), inc_dir),
  header_prefix: '#include <hinawa.h>',
)

mapfile = 'hinawa.map'
vflag = '-Wl,--version-script,' + join_paths(meson.current_source_dir(), mapfile)

myself = library('hinawa',
  sources: sources + headers + privates + marshallers + enums,
  version: meson.project_version(),
  soversion: major_version,
  install: true,
  include_directories: backport_header_dir + include_directories('.'),
  dependencies: dependencies,
  link_args : vflag,
  link_depends : mapfile,
)

install_headers(headers,
  subdir: inc_dir,
)

pkg = import('pkgconfig')
pkg.generate(myself,
  description: 'The library to operate 1394 OHCI hardware for asynchronous communication in IEEE 1394 bus',
  subdirs: inc_dir,
)

hinawa_gir = gnome.generate_gir(myself,
  sources: enums + headers + sources,
  nsversion: '@0@.0'.format(major_version),
  namespace: 'Hinawa',
  symbol_prefix: 'hinawa_',
  identifier_prefix: 'Hinawa',
  export_packages: 'hinawa',
  dependencies: dependencies,
  includes: [
    'GLib-2.0',
    'GObject-2.0',
  ],
  header: 'hinawa.h',
  install: true,
)

# For wrap dependency system.
hinawa_dep = declare_dependency(
  link_with: myself,
  dependencies: dependencies,
  sources: headers + marshallers + enums + hinawa_gir,
  include_directories: include_directories('.'),
)
meson.override_dependency('hinawa', hinawa_dep)
