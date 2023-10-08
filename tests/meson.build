tests = [
  'fw-node',
  'fw-req',
  'fw-resp',
  'fw-fcp',
  'cycle-time',
  'hinawa-enum',
  'hinawa-functions',
]


builddir = join_paths(meson.project_build_root(), 'src')

envs = environment()
envs.append('LD_LIBRARY_PATH', builddir, separator : ':')
envs.append('GI_TYPELIB_PATH', builddir, separator : ':')

foreach test : tests
    name = test
    script = test
    prog = find_program(script)
    test(name, prog,
      env: envs,
      depends: hinawa_gir,
    )
endforeach
