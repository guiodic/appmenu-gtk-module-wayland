# Testing and appmenu integration

Run the regression suite using the CMake or Meson commands in the
[README](../README.md#regression-tests). Both Wayland and X11 development
libraries are required: the module targets native Wayland and Xwayland
applications within a Plasma Wayland session.

## Test environment

The shared Python runner starts a private D-Bus session and Xvfb. It clears
`GTK_MODULES` and `GTK3_MODULES`, isolates the XDG configuration directories,
and writes `gtk-modules=` to a temporary GTK settings file to override the
compiled-in system configuration. `GTK_THEME=Adwaita` prevents an inherited
theme from loading additional modules. This keeps installed modules from
interfering with the hooks installed by the tests.

The suite covers mnemonic forwarding, duplicate mnemonics, GTK action fallback,
viewer availability, export replacement and removal, parser failure, cancellation,
and native GTK menu metadata restoration. The GtkApplicationWindow cases cover
both values of `gtk-shell-shows-menubar` and changes to the menu model.

The runner starts a second target process with `--real-bus-failure` to avoid
GIO's cached session connection. An independent libdbusmenu probe confirms that
an unreachable address causes a real connection failure. The module then has to
recover without reconnecting the menu; a `GetLayout` call checks registration
and menu contents, including changes made while the bus was unavailable.

## Cross-compilation

Python, D-Bus and Xvfb run on the build machine. Only the target test executable,
including the second process described above, uses the emulator.

For CMake, set `CMAKE_CROSSCOMPILING_EMULATOR` to a semicolon-separated command
and argument list. For Meson, configure `exe_wrapper` in the cross file; the
runner honors `MESON_EXE_WRAPPER`. Explicit CMake wrapper arguments take
precedence over that environment variable. Without a required emulator, the
runner exits with status 77 and the test is reported as skipped.

## Recovery and GTK menu ownership

Session-bus acquisition is asynchronous. Failed lookups retry after 1, 2, 4, 8,
16 and at most 30 seconds; success resets the backoff. Menu replacement,
unrealize and window destruction cancel pending requests and retries, and late
callbacks cannot publish obsolete menus. The parsed model is retained until a
session connection is available before constructing DbusmenuServer, because
libdbusmenu does not retry its own failed initial connection. Recovery from a
complete session-bus restart is not supported by this mechanism.

Registrar and viewer name watches also acquire the bus asynchronously and retry
connection failures with bounded backoff. A lost connection clears the old watches
before registering new ones. A missing service name on a healthy connection is
handled by the existing watch, without reconnecting.

The module leaves `gtk-shell-shows-menubar` unchanged. When GTK omits the
GtkApplicationWindow menubar because that setting is true, a private offscreen
bar retains GTK mnemonic registration and `app.`/`win.` action lookup.

For Xwayland windows with a module-owned bar, native GTK menu discovery paths
are withdrawn before map so Plasma's GMenu proxy does not create a competing
export. GTK action metadata stays intact. Removing the last bar restores the
original menu paths at idle; a replacement bar cancels the restoration.

## Checks in Plasma with Material

Leave `gmenudbusmenuproxy` enabled and test applications with both native
Wayland and `GDK_BACKEND=x11`. Test the proxy starting before and after the
application. Use `xprop` to inspect `_KDE_NET_WM_APPMENU_SERVICE_NAME` and
`_KDE_NET_WM_APPMENU_OBJECT_PATH` on Xwayland windows: seeing a menu in the
decoration does not by itself verify mnemonic forwarding.

Check Alt+mnemonic popup placement and action execution, including after menu
replacement or opening another window. Forwarding targets KDE's appmenu viewer
(`org.kde.kappmenuview`); registrar presence alone does not imply a receiver.
