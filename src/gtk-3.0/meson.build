gtk3_module = shared_module(
    'appmenu-gtk-module', [module_sources, wayland_sources],
    dependencies: [gtk3_parser_dep, dbusmenu_glib, dbusmenu_gtk3, wayland_client],
    include_directories: include_directories('../../wayland/generated'),
    install: true,
    install_dir: join_paths(gtk3.get_variable(pkgconfig:'libdir'),'gtk-3.0','modules')
)
