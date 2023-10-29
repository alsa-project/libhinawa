ext_contents = [
  'overview.md',
  'overview.png',
]

conf = configuration_data()
conf.set('VERSION', meson.project_version())

doc_toml = configure_file(
  input: 'hinawa.toml.in',
  output: 'hinawa.toml',
  configuration: conf,
)

dependency('gi-docgen',
  version: '>= 2023.1',
  fallback: ['gi-docgen', 'dummy_dep'],
)
gidocgen = find_program('gi-docgen')

doc_dir = meson.project_name()

custom_target('hinawa-doc',
  input: [ doc_toml, hinawa_gir[0] ],
  output: doc_dir,
  command: [
    gidocgen,
    'generate',
    '--no-namespace-dir',
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  depend_files: ext_contents,
  build_by_default: true,
  install: true,
  install_dir: join_paths(get_option('datadir'), 'doc'),
)
