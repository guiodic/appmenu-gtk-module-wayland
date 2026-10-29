if build_gtk3 and get_option('tests')
    client_sources = files (
	    join_paths('demos','altgrabber.h'),
	    join_paths('demos','altgrabber.c'),
	    join_paths('demos','altmonitor.h'),
	    join_paths('demos','altmonitor.c'),
	    join_paths('demos','menusource.h'),
	    join_paths('demos','menusource.c'),
	    join_paths('demos','client.c'))

    wnck_ver = '>=3.4.8'
    wnck = dependency('libwnck-3.0',version: wnck_ver, required: false)
    if(wnck.found() and x11.found())
        client = executable('menumodel-client',client_sources,
            dependencies: [gtk3_parser_dep, wnck, x11]
        )
#        test('client', client)
    endif
    tester = executable('tester',join_paths('demos','unity-gtk-menu-tester.c'), dependencies: gtk3)
#    test('tester',tester)
    radio = executable('radio',join_paths('demos','test-radio.c'), dependencies: gtk3)
#    test('radio',radio)
    hello = executable('hello',join_paths('demos','hello.c'), dependencies: gtk3)
#    test('hello',hello)
    # test-mnemonics.c includes datastructs.c to exercise the private handler.
    mnemonic_sources = files(
        'test-mnemonics.c',
        '../src/appmenu-gtk-module.c',
        '../src/hijack.c',
        '../src/support.c',
        '../src/blacklist.c',
        '../src/platform.c',
    )
    mnemonics = executable('test-mnemonics',
        [mnemonic_sources, lib_sources, wayland_sources],
        dependencies: [gtk3, dbusmenu_glib, dbusmenu_gtk3, wayland_client, x11],
        include_directories: include_directories('../wayland/generated', '../lib'))
    dbus_run_session = find_program('dbus-run-session', native: true)
    xvfb_run = find_program('xvfb-run', native: true)
    python = find_program('python3', native: true)
    runner_args = [files('run-mnemonics.py'),
        '--can-run-host-binaries', meson.can_run_host_binaries() ? 'yes' : 'no']
    if meson.is_cross_build()
        runner_args += ['--cross-build']
    endif
    test('mnemonics', python,
        # Keep the target object so Meson supplies MESON_EXE_WRAPPER when needed.
        args: runner_args + [dbus_run_session.full_path(), xvfb_run.full_path(), mnemonics],
        depends: mnemonics,
        env: ['GDK_BACKEND=x11', 'NO_AT_BRIDGE=1', 'GTK3_MODULES=', 'GTK_MODULES='],
        timeout: 30)

    vala_found = add_languages('vala', required: false)
    if vala_found
        black = executable('black',join_paths('demos','black.vala'), dependencies: gtk3)
#        test('black',black)
    endif
endif

#TODO: fix autopilot
run_target('autopilot',
        command: ['autopilot/test.sh'],
    )
