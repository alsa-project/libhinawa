configure_file(
  input : 'version.xml.in',
  output : 'version.xml',
  configuration : conf
)

gnome.gtkdoc('hinawa',
  src_dir : join_paths(meson.source_root(), 'src'),
  main_xml : 'hinawa-docs.sgml',
  install : true
)
