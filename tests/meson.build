tests = [
  'fw-node',
  'fw-req',
  'fw-resp',
  'fw-fcp',
  'snd-unit',
  'snd-dice',
  'snd-efw',
  'snd-dg00x',
  'snd-motu',
  'snd-tscm',
  'hinawa-enum',
]

objdir = join_paths(meson.build_root(), 'src')

envs = environment()
envs.append('LD_LIBRARY_PATH', objdir, separator : ':')
envs.append('GI_TYPELIB_PATH', objdir, separator : ':')

foreach test : tests
    name = test
    script = test
    prog = find_program(script)
    test(name, prog,
      env: envs,
    )
endforeach
