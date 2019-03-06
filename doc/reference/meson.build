configure_file(
  input : 'version.xml.in',
  output : 'version.xml',
  configuration : conf,
)

images = [
  'overview.png',
]

private_headers = [
  'internal.h',
  'backport.h',
  'hinawa_context.c',
  'hinawa_context.h',
]

gnome.gtkdoc('hinawa',
  src_dir : [
    join_paths(meson.source_root(), 'src'),
    join_paths(meson.build_root(), 'src'),
  ],
  main_xml : 'hinawa-docs.xml',
  html_assets: images,
  scan_args: [
    '--ignore-headers=' + ' '.join(private_headers),
  ],
  gobject_typesfile: 'hinawa.types',
  dependencies: myself_dep,
  install : true,
)
