#!/usr/bin/env python3
"""Run the GTK regression test under its build-host services and target wrapper."""

import argparse
import os
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cross-build', action='store_true')
    parser.add_argument('--exe-wrapper', action='append', default=None)
    parser.add_argument('--can-run-host-binaries', choices=('yes', 'no'), required=True)
    parser.add_argument('dbus_run_session')
    parser.add_argument('xvfb_run')
    parser.add_argument('binary')
    args = parser.parse_args()

    if args.exe_wrapper is not None:
        wrapper = args.exe_wrapper
    elif args.cross_build:
        wrapper = shlex.split(os.environ.get('MESON_EXE_WRAPPER', ''))
    else:
        wrapper = []
    if not wrapper and args.can_run_host_binaries == 'no':
        print('SKIP: target binaries cannot run without an exe_wrapper', flush=True)
        return 77

    # Only the target binary uses the wrapper; D-Bus and Xvfb run on the build host.
    command = [args.dbus_run_session, '--', args.xvfb_run, '-a', *wrapper, args.binary]
    # GTK also reads gtk-modules from settings.ini, independently of GTK_MODULES.
    with tempfile.TemporaryDirectory(prefix='appmenu-test-') as config:
        system_config = os.path.join(config, 'system')
        gtk_config = os.path.join(system_config, 'gtk-3.0')
        os.makedirs(gtk_config)
        # Override gtk-modules from GTK's compiled-in sysconfdir as well.
        with open(os.path.join(gtk_config, 'settings.ini'), 'w', encoding='ascii') as settings:
            settings.write('[Settings]\ngtk-modules=\n')
        env = os.environ.copy()
        env.update(GTK_MODULES='', GTK3_MODULES='',
                   XDG_CONFIG_HOME=os.path.join(config, 'user'),
                   XDG_CONFIG_DIRS=system_config)
        result = subprocess.call(command, env=env)
        if result != 0:
            return result
        # A fresh process avoids GIO's cached connection. Apply the target
        # wrapper here too, rather than spawning a target binary from GTest.
        return subprocess.call([*command, '--real-bus-failure'], env=env)


if __name__ == '__main__':
    raise SystemExit(main())
