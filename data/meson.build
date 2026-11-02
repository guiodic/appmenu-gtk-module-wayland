install_data([
    'org.appmenu.gtk-module.wayland.gschema.xml'
], install_dir: schema_dir)

systemd = dependency('systemd', required: false)

if systemd.found()
    userunitdir = systemd.get_variable(pkgconfig:'systemduserunitdir')
    configure_file(
        input: 'appmenu-gtk-module-wayland.service.in',
        output: 'appmenu-gtk-module-wayland.service',
        install_dir: userunitdir,
        configuration: {'DBUS_UPDATE_ACTIVATION_ENVIRONMENT': find_program('dbus-update-activation-environment').full_path()}
    )
endif

install_data('../appmenu-gtk-module-wayland.sh',
    install_dir: get_option('profile_install_dir'),
    install_mode: 'rwxr-xr-x')
