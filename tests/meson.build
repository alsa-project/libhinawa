tests = [
  'fw-node',
  'fw-req',
  'fw-resp',
  'fw-fcp',
  'cycle-time',
  'snd-unit',
  'snd-dice',
  'snd-efw',
  'snd-dg00x',
  'hinawa-enum',
  'hinawa-functions',
]

envs = environment()
envs.append('LD_LIBRARY_PATH', builddir, separator : ':')
envs.append('GI_TYPELIB_PATH', builddir, separator : ':')

foreach test : tests
    name = test
    script = test
    prog = find_program(script)
    test(name, prog,
      env: envs,
    )
endforeach
