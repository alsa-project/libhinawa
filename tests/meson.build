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
  'snd-motu',
  'snd-tscm',
  'hinawa-enum',
  'snd-motu-register-dsp-parameter',
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
      depends: hinawa_gir,
    )
endforeach
