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

mapfile = 'hinawa.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)

myself = library('hinawa',
  sources: sources,
  version: my_binary_version,
  soversion: my_major,
  install: true,
  dependencies: dependencies,
  link_args : vflag,
  link_depends : mapfile,
)

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
install_headers(headers,
  subdir: meson.project_name()
)

pkg = import('pkgconfig')
pkg.generate(
  version: my_version,
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
    sources: sources,
    nsversion: '1.0',
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
