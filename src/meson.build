module_sources = files(
    'appmenu-gtk-module.c',
    'datastructs.c',
    'datastructs.h',
    'datastructs-private.h',
    'hijack.c',
    'hijack.h',
    'support.c',
    'support.h',
    'blacklist.c',
    'blacklist.h',
    'platform.c',
    'platform.h',
    'consts.h'
)

wayland_sources = files(
    '../wayland/generated/appmenu.c'
)

module_subdirs = ['gtk-3.0']

if build_gtk3
    subdir('gtk-3.0')
endif
