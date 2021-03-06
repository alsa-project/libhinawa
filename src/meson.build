configure_file(
  output: 'config.h',
  configuration: conf,
)

# Depends on glib-2.0 and gobject-2.0
gobject = dependency('gobject-2.0',
  version: '>=2.34.0'
)
dependencies = [
  gobject,
]

sources = [
  'fw_node.c',
  'fw_resp.c',
  'fw_req.c',
  'fw_fcp.c',
  'snd_unit.c',
]

headers = [
  'fw_node.h',
  'fw_resp.h',
  'fw_req.h',
  'fw_fcp.h',
  'snd_unit.h',
]

privates = [
  'internal.h',
]

if with_snd_dice
  sources += 'snd_dice.c'
  headers += 'snd_dice.h'
endif
if with_snd_efw
  sources += 'snd_efw.c'
  headers += 'snd_efw.h'
endif
if with_snd_dg00x
  sources += 'snd_dg00x.c'
  headers += 'snd_dg00x.h'
endif
if with_snd_motu
  sources += 'snd_motu.c'
  headers += 'snd_motu.h'
endif
if with_snd_tscm
  sources += 'snd_tscm.c'
  headers += 'snd_tscm.h'
endif

# Generate marshallers for GObject signals.
marshallers = gnome.genmarshal('hinawa_sigs_marshal',
  prefix: 'hinawa_sigs_marshal',
  sources: 'hinawa_sigs_marshal.list',
  stdinc: true,
)
sources += marshallers

enums = gnome.mkenums_simple('hinawa_enums',
  sources: ['hinawa_enum_types.h'],
  identifier_prefix: 'Hinawa',
  symbol_prefix: 'hinawa',
  install_header: true,
  install_dir: join_paths(get_option('includedir'), meson.project_name()),
  header_prefix: '#include <hinawa_enum_types.h>',
)
headers += 'hinawa_enum_types.h'
sources += enums

mapfile = 'hinawa.map'
vflag = '-Wl,--version-script,' + join_paths(meson.current_source_dir(), mapfile)

myself = library('hinawa',
  sources: sources + headers + privates,
  version: meson.project_version(),
  soversion: meson.project_version().split('.')[0],
  install: true,
  dependencies: dependencies,
  link_args : vflag,
  link_depends : mapfile,
)

myself_dep = declare_dependency(
  link_with: myself,
  dependencies: dependencies,
)

install_headers(headers,
  subdir: meson.project_name()
)

pkg = import('pkgconfig')
pkg.generate(myself,
  name: 'hinawa',
  description: 'Input/Output library to audio and music units on IEEE 1394 bus',
  subdirs: meson.project_name(),
)

if get_option('gir')
  gnome.generate_gir(myself,
    sources: sources + headers,
    nsversion: '3.0',
    namespace: 'Hinawa',
    symbol_prefix: 'hinawa_',
    identifier_prefix: 'Hinawa',
    export_packages: 'hinawa',
    dependencies: dependencies,
    includes: [
      'GLib-2.0',
      'GObject-2.0',
    ],
    install: true,
  )
endif
