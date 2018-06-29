tests = [
  ['FwUnit',    'fw-unit'],
  ['FwReq',     'fw-req'],
  ['FwResp',    'fw-resp'],
  ['FwFcp',     'fw-fcp'],
  ['SndUnit',   'snd-unit'],
  ['SndEfw',    'snd-efw'],
  ['SndDice',   'snd-dice'],
  ['SndDg00x',  'snd-dg00x'],
  ['SndMotu',   'snd-motu'],
  ['HinawaEnum','hinawa-enum'],
]

objdir = '@0@/src'.format(meson.build_root())

envs = environment()
envs.append('LD_LIBRARY_PATH', objdir, separator : ':')
envs.append('GI_TYPELIB_PATH', objdir, separator : ':')

foreach test : tests
    name = test[0]
    script = test[1]
    prog = find_program(script)
    test(name, prog,
      env: envs,
    )
endforeach
