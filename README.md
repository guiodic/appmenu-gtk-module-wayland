PoC: GTK global menu on Plasma Wayland

### dependencies

- libdbusmenu-gtk3

### build instruction

```console
mkdir build
cmake -B build .
cmake --build build
```

### run the tester

```console
GTK_MODULES=$PWD/build/libappmenu-gtk-module-wayland.so your_gtk_app
```

### install it

```console
cmake -S . -B build
cmake --build build
sudo cmake --install build
```

CMake installs the module in `${CMAKE_INSTALL_LIBDIR}/gtk-3.0/modules`, the
session script in `/etc/profile.d` (independent of the prefix), and the GSettings
schema in `${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas`. The default prefix is
`/usr`; set `-DCMAKE_INSTALL_PREFIX=/your/prefix` to choose another prefix. Override
`GTK_MODULE_INSTALL_DIR` or `PROFILE_INSTALL_DIR` if your distribution needs
other locations. When reusing an existing build directory configured for
`/usr/local`, run
`cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr -DPROFILE_INSTALL_DIR=/etc/profile.d`
to replace cached destinations.
Alternatively, configure a fresh build directory to use the new defaults.
Log out and back in after installing the session script.

The session script activates the module only when `XDG_SESSION_TYPE=wayland`.
If that variable is unset or empty, a nonempty `WAYLAND_DISPLAY` is used as a
fallback. An explicit X11 session is never treated as Wayland. Repeated sourcing
does not add duplicate entries to `GTK_MODULES`.

When systemd development metadata is available, CMake and Meson also install
`appmenu-gtk-module-wayland.service` as an optional user service. It uses the same
session detection and can be enabled with
`systemctl --user enable appmenu-gtk-module-wayland.service` for subsequent
graphical sessions. The user manager must receive `XDG_SESSION_TYPE` and, when
needed, `WAYLAND_DISPLAY` from the desktop session. A manually started service
uses the user manager's environment, not the invoking terminal's environment.
The service preserves other GTK modules, including the X11 counterpart; stopping
it removes only `appmenu-gtk-module-wayland`. The module's settings remain
separate, but enabling both appmenu modules in the same application should be
avoided.

Direct installation with CMake or Meson compiles the installed schemas. For packaging, use
`DESTDIR=/path/to/staging cmake --install build`; the package must run
`glib-compile-schemas` on the destination schema directory through its usual
package trigger. Cross builds need a host `glib-compile-schemas` executable.

With Meson, the GTK module is also named `libappmenu-gtk-module-wayland.so`.
The parser library, pkg-config file and public include directory use the
`appmenu-gtk3-parser-wayland` / `appmenu-gtk-parser-wayland` names, so they do not
replace the X11 package's files. Meson respects `--prefix` and `--libdir`; override
`-Dprofile_install_dir=/your/profile.d` for the session script if needed.
On upgrade, package managers should remove the former Wayland package's old
service and schema files. Do not delete files belonging to an installed X11
package. An enabled old `appmenu-gtk-module.service` must be handled according
to which package owns it.

### GSettings

The Wayland module uses schema ID `org.appmenu.gtk-module.wayland` and
settings path `/org/appmenu/gtk-module/wayland/`, separate from the X11
module. Settings previously stored under `/org/appmenu/gtk-module/` are
not migrated automatically.

```sh
gsettings list-recursively org.appmenu.gtk-module.wayland
gsettings set org.appmenu.gtk-module.wayland always-show-inner-menu true
```

- `blacklist`: additional program names to disable, alongside the built-in list.
- `whitelist`: program names that override both blacklists. Names match
  `g_get_prgname()` exactly; this does not override `UBUNTU_MENUPROXY=0`.
- `run-on-wayland`: allow native Wayland applications (default `true`);
  X11 and Xwayland applications are unaffected.
- `always-show-inner-menu`: retain application menus even when a registrar is
  available. Changes apply to running applications; export remains enabled.

Restart applications after changing the lists or `run-on-wayland`, since these
control module initialization. Without an installed schema the module keeps
its built-in defaults. For an uninstalled build, set
`GSETTINGS_SCHEMA_DIR=$PWD/build/schemas` when launching an application.

###

---
Application Menu GTK+ Module
---

This is renamed port of [Launchpad repository](https://launchpad.net/unity-gtk-module) of Unity GTK+ Module.

Unity GTK+ Module is small GTK Module than strips menus from all GTK programs, converts to MenuModel and send to AppMenu.
Ubuntu users now does not need to install this.

**REQUIRED DEPENDENCES**

*All:*
 * GLib (>= 2.50.0)
 * GTK+ (>= 3.22.0)

*Demos*
 * valac (>= 0.24.0)
 
---
Usage Instructions
---
Use the installed session script or the optional systemd user service described
above. For manual activation in the current shell, source the script:

```sh
. /etc/profile.d/appmenu-gtk-module-wayland.sh
```

Use the configured profile directory if you overrode its installation location.
Session variables may become available only after login profiles have run; in
that case use the user service after the desktop imports its session environment.
Do not set the module unconditionally in GTK settings, XSettings or `.bashrc`:
that would bypass Wayland session detection. Remove any such older configuration
before relying on automatic selection. Log out and back in after installation.

### Regression tests

Tests require Python 3, D-Bus, Xvfb and xauth in addition to the GTK 3,
libdbusmenu-gtk3, Wayland and X11 development dependencies.

With CMake and Ninja:

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

With Meson:

```sh
meson setup build-meson -Dtests=true
meson compile -C build-meson
meson test -C build-meson --print-errorlogs
```

The suite checks mnemonic forwarding, GTK action fallback, menu export lifecycle
and recovery from initial session-bus connection failures without requiring Plasma.
Check actual popup placement separately with Material in a Plasma Wayland session,
using both native Wayland applications and Xwayland applications (`GDK_BACKEND=x11`).

See [Testing and appmenu integration](docs/testing.md) for cross-compilation,
test isolation, integration checks and supported recovery behavior.

The CMake `settings` test uses the compiled build schema and an isolated memory
backend to check blacklist/whitelist precedence, live inner-menu changes and
X11 startup with Wayland disabled. Native Wayland startup should additionally
be checked in a Wayland session with `run-on-wayland` both enabled and disabled.
