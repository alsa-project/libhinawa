tests = [
  'fw-node',
  'fw-req',
  'fw-resp',
  'fw-fcp',
  'snd-unit',
  'hinawa-enum',
]

if with_snd_dice == true
  tests += 'snd-dice'
endif

if with_snd_efw == true
  tests += 'snd-efw'
endif

if with_snd_dg00x == true
  tests += 'snd-dg00x'
endif

if with_snd_motu == true
  tests += 'snd-motu'
endif

if with_snd_tscm == true
  tests += 'snd-tscm'
endif

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
