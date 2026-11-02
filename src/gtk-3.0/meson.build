gtk_module_install_dir = get_option('gtk_module_install_dir')
if gtk_module_install_dir == ''
    gtk_libdir = gtk3.get_variable(pkgconfig: 'libdir')
    if sys_root != '' and sys_root != '/'
        if gtk_libdir == sys_root or gtk_libdir.startswith(sys_root + '/')
            target_parts = gtk_libdir.split('/')
            gtk_libdir = '/'
            foreach part_index : range(sys_root.split('/').length(), target_parts.length())
                gtk_libdir = join_paths(gtk_libdir, target_parts[part_index])
            endforeach
        endif
    endif
    gtk_module_install_dir = join_paths(gtk_libdir, 'gtk-3.0', 'modules')
endif

gtk3_module = shared_module(
    'appmenu-gtk-module-wayland', [module_sources, wayland_sources, lib_sources],
    dependencies: [gtk3, dbusmenu_glib, dbusmenu_gtk3, wayland_client, x11],
    include_directories: [parser_inc, include_directories('../../wayland/generated')],
    install: true,
    install_dir: gtk_module_install_dir
)
