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
  'hinawa_context.h',
  'hinawa_context.c',
  'fw_unit.h',
  'fw_unit.c',
  'fw_resp.h',
  'fw_resp.c',
  'fw_req.h',
  'fw_req.c',
  'fw_fcp.h',
  'fw_fcp.c',
  'internal.h',
  'snd_unit.h',
  'snd_unit.c',
]

headers = [
  'fw_unit.h',
  'fw_resp.h',
  'fw_req.h',
  'fw_fcp.h',
  'snd_unit.h',
  'snd_dice.h',
  'snd_efw.h',
  'snd_dg00x.h',
  'snd_motu.h',
]

if with_snd_dice
  sources += ['snd_dice.h', 'snd_dice.c']
endif
if with_snd_efw
  sources += ['snd_efw.h', 'snd_efw.c']
endif
if with_snd_dg00x
  sources += ['snd_dg00x.h', 'snd_dg00x.c']
endif
if with_snd_motu
  sources += ['snd_motu.h', 'snd_motu.c']
endif

# Generate marshallers for GObject signals.
marshallers = gnome.genmarshal('hinawa_sigs_marshal',
  prefix: 'hinawa_sigs_marshal',
  sources: 'hinawa_sigs_marshal.list',
  stdinc: true,
)
sources += marshallers

enums = gnome.mkenums('hinawa_enums',
  sources: ['hinawa_enum_types.h'],
  identifier_prefix: 'Hinawa',
  symbol_prefix: 'hinawa',
  c_template: 'hinawa_enums.c.template',
  h_template: 'hinawa_enums.h.template',
  install_header: true)
headers += 'hinawa_enum_types.h'
sources += enums

mapfile = 'hinawa.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)

myself = library('hinawa',
  sources: sources,
  version: meson.project_version(),
  soversion: meson.project_version().split('.')[0],
  install: true,
  dependencies: dependencies,
  link_args : vflag,
  link_depends : mapfile,
)

install_headers(headers,
  subdir: meson.project_name()
)

pkg = import('pkgconfig')
pkg.generate(
  version: meson.project_version(),
  libraries: myself,
  requires: [
    'glib-2.0',
    'gobject-2.0',
  ],
  name: meson.project_name(),
  description: 'Input/Output library to audio and music units on IEEE 1394 bus',
  filebase: meson.project_name(),
  subdirs: meson.project_name(),
)

if get_option('gir')
  gnome.generate_gir(myself,
    sources: sources + headers,
    nsversion: '2.0',
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
