install_data([
    'org.appmenu.gtk-module.wayland.gschema.xml'
], install_dir: schema_dir)

systemd = dependency('systemd', required: false)

if systemd.found()
    userunitdir = systemd.get_variable(pkgconfig:'systemduserunitdir')
    dbus_update_activation_environment = find_program('dbus-update-activation-environment', required: false)
    if dbus_update_activation_environment.found()
        configure_file(
            input: 'appmenu-gtk-module-wayland.service.in',
            output: 'appmenu-gtk-module-wayland.service',
            install_dir: userunitdir,
            configuration: {'DBUS_UPDATE_ACTIVATION_ENVIRONMENT': dbus_update_activation_environment.full_path()}
        )
    else
        message('Skipping optional systemd service: dbus-update-activation-environment not found')
    endif
endif

install_data('../appmenu-gtk-module-wayland.sh',
    install_dir: get_option('profile_install_dir'),
    install_mode: 'rwxr-xr-x')
