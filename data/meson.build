install_data([
    'org.appmenu.gtk-module.wayland.gschema.xml'
], install_dir: schema_dir)

systemd = dependency('systemd', required: false)

if systemd.found()
    userunitdir = systemd.get_variable(pkgconfig:'systemduserunitdir')
    if sys_root != '' and sys_root != '/'
        if userunitdir == sys_root or userunitdir.startswith(sys_root + '/')
            target_parts = userunitdir.split('/')
            userunitdir = '/'
            foreach part_index : range(sys_root.split('/').length(), target_parts.length())
                userunitdir = join_paths(userunitdir, target_parts[part_index])
            endforeach
        endif
    endif

    dbus_update_path = get_option('dbus_update_activation_environment')
    if dbus_update_path != ''
        if not dbus_update_path.startswith('/')
            error('dbus_update_activation_environment must be an absolute target path')
        endif
    elif not meson.is_cross_build()
        # Native discovery is not evidence of an executable on a cross target.
        dbus_update_program = find_program('dbus-update-activation-environment', native: true, required: false)
        if dbus_update_program.found()
            dbus_update_path = dbus_update_program.full_path()
        endif
    endif

    if dbus_update_path != ''
        configure_file(
            input: 'appmenu-gtk-module-wayland.service.in',
            output: 'appmenu-gtk-module-wayland.service',
            install_dir: userunitdir,
            configuration: {'DBUS_UPDATE_ACTIVATION_ENVIRONMENT': dbus_update_path}
        )
    else
        message('Skipping optional systemd service: set dbus_update_activation_environment to its target path when cross-compiling, or install the helper for a native build')
    endif
endif

install_data('../appmenu-gtk-module-wayland.sh',
    install_dir: get_option('profile_install_dir'),
    install_mode: 'rwxr-xr-x')
