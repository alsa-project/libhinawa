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
]

gnome.gtkdoc('hinawa',
  src_dir : inc,
  main_xml : 'hinawa-docs.xml',
  html_assets: images,
  scan_args: [
    '--ignore-headers=' + ' '.join(private_headers),
  ],
  gobject_typesfile: 'hinawa.types',
  dependencies: myself_dep,
  install : true,
)
