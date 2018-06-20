project('libhinawa', 'c',
  version: '0.9.0',
  license: 'LGPL-2.1+',
  meson_version: '>= 0.32.0',
)

my_version = meson.project_version()
varr = my_version.split('.')
my_major = varr[0]
my_minor = varr[1]
my_micro = varr[2]

my_binary_version = '@0@.@1@.@2@'.format(my_major, my_minor.to_int() * 10, my_micro)

# Detect support level in Linux sound subsystem.
cc = meson.get_compiler('c')

inc = include_directories('src')

if cc.has_header('backport.h', include_directories: inc)
  with_backport = true
  with_snd_dice = true
  with_snd_efw = true
  with_snd_dg00x = true
  with_snd_motu = true
else
  with_backport = false
  with_snd_dice = false
  with_snd_efw = false
  with_snd_dg00x = false
  with_snd_motu = false

  if cc.has_header_symbol('sound/firewire.h',
                          'struct snd_firewire_event_dice_notification')
    with_snd_dice = true
  endif
  if cc.has_header_symbol('sound/firewire.h',
                          'struct snd_firewire_event_efw_response')
    with_snd_efw = true
  endif
  if cc.has_header_symbol('sound/firewire.h',
                          'struct snd_firewire_event_digi00x_message')
    with_snd_dg00x = true
  endif
  if cc.has_header_symbol('sound/firewire.h',
                          'struct snd_firewire_motu_status')
    with_snd_motu = true
  endif
endif

conf = configuration_data()
conf.set10('WITH_BACKPORT',  with_backport)
conf.set10('HAVE_SND_DICE',  with_snd_dice)
conf.set10('HAVE_SND_EFW',   with_snd_efw)
conf.set10('HAVE_SND_DG00X', with_snd_dg00x)
conf.set10('HAVE_SND_MOTU',  with_snd_motu)
conf.set('VERSION', my_version)

# For g-i and documentation.
gnome = import('gnome')

subdir('src')
subdir('doc')
subdir('tests')
