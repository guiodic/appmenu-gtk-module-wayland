gtk3_module = shared_module(
    'appmenu-gtk-module-wayland', [module_sources, wayland_sources, lib_sources],
    dependencies: [gtk3, dbusmenu_glib, dbusmenu_gtk3, wayland_client, x11],
    include_directories: [parser_inc, include_directories('../../wayland/generated')],
    install: true,
    install_dir: join_paths(get_option('libdir'), 'gtk-3.0', 'modules')
)
