project('libhinawa', 'c',
  version: '2.5.1',
  license: 'LGPL-2.1+',
  meson_version: '>= 0.46.0',
)

# Detect support level in Linux sound subsystem.
cc = meson.get_compiler('c')

backport = []
if not cc.has_header_symbol('include/sound/firewire.h',
			    'SNDRV_FIREWIRE_MOTU_REGISTER_DSP_METER_INPUT_COUNT') or
   not cc.has_header_symbol('include/linux/firewire-cdev.h', 'FW_CDEV_EVENT_REQUEST3')
  # Use backport header from Linux kernel v5.16 prepatch.
  backport += include_directories('include')
endif

# For g-i and documentation.
gnome = import('gnome')
inc = include_directories('src')

subdir('src')
subdir('tests')

if get_option('doc')
  subdir('doc')
endif
