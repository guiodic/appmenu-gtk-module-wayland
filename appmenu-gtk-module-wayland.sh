#!/bin/sh
# Prefer the session type; fall back to the Wayland display only when unset.
if [ "${XDG_SESSION_TYPE:-}" = wayland ] ||
   { [ -z "${XDG_SESSION_TYPE:-}" ] && [ -n "${WAYLAND_DISPLAY:-}" ]; }; then
    case ":${GTK_MODULES:-}:" in
        *:appmenu-gtk-module-wayland:*) ;;
        *) GTK_MODULES="${GTK_MODULES:+${GTK_MODULES}:}appmenu-gtk-module-wayland" ;;
    esac

    if [ -z "${UBUNTU_MENUPROXY:-}" ]; then
        UBUNTU_MENUPROXY=1
    fi

    export GTK_MODULES UBUNTU_MENUPROXY
fi
