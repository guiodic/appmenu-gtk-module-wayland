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

Direct installation compiles the installed schemas. For packaging, use
`DESTDIR=/path/to/staging cmake --install build`; the package must run
`glib-compile-schemas` on the destination schema directory through its usual
package trigger. Cross builds need a host `glib-compile-schemas` executable.

### GSettings

```sh
gsettings list-recursively org.appmenu.gtk-module
gsettings set org.appmenu.gtk-module always-show-inner-menu true
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
**XFCE**
- Type following into your console:
`xfconf-query -c xsettings -p /Gtk/Modules -n -t string -s "appmenu-gtk-module-wayland"`

**BUDGIE***
- Type following into your console:
`gsettings set org.gnome.settings-daemon.plugins.xsettings enabled-gtk-modules "['appmenu-gtk-module-wayland']"`

**OTHER**
- Create file .config/gtk-3.0/settings.ini into your home directory, if it do not exists already
Add to this file:
    - If it is just created, `[Settings]`
    - And then ``gtk-modules="appmenu-gtk-module-wayland"``

**IF ABOVE DOES NOT WORK**
* Add to .profile or .bashrc:

```sh
if [ -n "$GTK_MODULES" ]; then
    GTK_MODULES="${GTK_MODULES}:appmenu-gtk-module-wayland"
else
    GTK_MODULES="appmenu-gtk-module-wayland"
fi

if [ -z "$UBUNTU_MENUPROXY" ]; then
    UBUNTU_MENUPROXY=1
fi

export GTK_MODULES
export UBUNTU_MENUPROXY
```

**IF NONE OF THESE ARE WORKING**
* Add above snippet to any place where environment variables should set.

**YOU SHOULD RELOGIN AFTER INSTALLING THIS MODULE FIRST TIME**

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
