#!/usr/bin/env python3
"""Run the GTK regression test under its build-host services and target wrapper."""

import argparse
import os
import shlex


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cross-build', action='store_true')
    parser.add_argument('--can-run-host-binaries', choices=('yes', 'no'), required=True)
    parser.add_argument('dbus_run_session')
    parser.add_argument('xvfb_run')
    parser.add_argument('binary')
    args = parser.parse_args()

    wrapper = shlex.split(os.environ.get('MESON_EXE_WRAPPER', '')) if args.cross_build else []
    if not wrapper and args.can_run_host_binaries == 'no':
        print('SKIP: target binaries cannot run without an exe_wrapper', flush=True)
        return 77

    # Only the target binary uses the wrapper; D-Bus and Xvfb run on the build host.
    command = [args.dbus_run_session, '--', args.xvfb_run, '-a', *wrapper, args.binary]
    os.execv(command[0], command)


if __name__ == '__main__':
    raise SystemExit(main())
