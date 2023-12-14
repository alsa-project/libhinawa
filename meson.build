project('hinawa', 'c',
  version: '4.0.1',
  license: 'LGPL-2.1+',
  meson_version: '>= 0.60.0',
)

version = meson.project_version().split('.')
major_version = version[0].to_int()
minor_version = version[1].to_int()
micro_version = version[2].to_int()

# Detect support level in Linux sound subsystem.
cc = meson.get_compiler('c')

backport_header_dir = []
if not cc.has_header_symbol('include/linux/firewire-cdev.h', 'FW_CDEV_EVENT_REQUEST3')
  # Use backport header from Linux kernel v5.16 prepatch.
  backport_header_dir += include_directories('include')
endif

# For g-i and documentation.
gnome = import('gnome')

subdir('src')
subdir('tests')

if get_option('doc')
  subdir('doc')
endif
