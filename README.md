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
sudo cp $PWD/build/libappmenu-gtk-module-wayland.so /usr/lib/gtk-3.0/modules/
```
Then create a file in /etc/profile.d

```console
sudo nano /etc/profile.d/appmenu-gtk-module-wayland.sh
```

Copy this

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

Make the file executable

```console
sudo chmod +x /etc/profile.d/appmenu-gtk-module-wayland.sh
```

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

With GTK 3, libdbusmenu-gtk3, Wayland and X11 development libraries, CMake,
Ninja, D-Bus, Xvfb and xauth installed:

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests use a private D-Bus session and Xvfb to check mnemonic forwarding,
GTK action fallback, viewer availability and the KDE appmenu X11 properties
used for Xwayland applications. They do not require a Plasma session.
Verify popup placement separately in Plasma with both native Wayland and
Xwayland GTK applications. Mnemonic forwarding targets KDE's appmenu viewer
(`org.kde.kappmenuview`); registrar presence alone does not imply a viewer.

The same regression suite is available with Meson:

```sh
meson setup build-meson -Dtests=true
meson compile -C build-meson
meson test -C build-meson --print-errorlogs
```

The regression runners clear `GTK_MODULES` and `GTK3_MODULES` so installed
GTK modules cannot interfere with the test's own hooks. Meson uses a build-host
Python 3 runner, honors `MESON_EXE_WRAPPER` for cross-built targets, and reports
a skipped test when the target cannot run and no wrapper is available.

Both test runners use a temporary, isolated XDG configuration so user or system
GTK settings cannot load additional modules. Python 3 is required for either
test runner. This module targets Plasma Wayland sessions with native GTK Wayland
and Xwayland applications; builds require both backend development dependencies.
Transient session-bus lookup failures withdraw the menu address and retry while
the window and menu remain alive. This does not promise recovery from a complete
session-bus restart, which may terminate GTK applications and invalidate exports.

Menu publication, including initial Xwayland bus-dependent setup, obtains the
session bus asynchronously. Failed lookups retry after 1, 2, 4, 8, 16 and at most
30 seconds while the menu remains alive. Replacing/unrealizing a menu or closing
its window cancels pending requests and retries; late replies cannot republish
an obsolete export. The backoff resets after a successful lookup.

When CMake cross-compiles the tests, set `CMAKE_CROSSCOMPILING_EMULATOR` to a
semicolon-separated command and argument list. The runner applies it only to
the target test binary; Python, D-Bus and Xvfb run on the build machine. Without
an emulator CTest reports the test as skipped (exit status 77). Explicit CMake
wrapper arguments take precedence over the Meson wrapper environment variable.

The module leaves GTK's `gtk-shell-shows-menubar` setting unchanged. When it
is false, GtkApplicationWindow supplies its normal menubar. When the desktop
sets it to true (as Plasma's GMenu proxy does), the module creates a private,
offscreen menubar from the application's GMenu models. It retains GTK's mnemonic
registration and `app.`/`win.` action lookup without adding a visible bar.
Model replacement, setting changes and window unrealize clean up the private
bar; registrar changes only request a new layout.

For Xwayland windows whose menubar the module exports, native GTK menu discovery
paths are withdrawn before the window is mapped. Plasma's `gmenudbusmenuproxy`
then skips those windows, including when the proxy starts later. Material and
the mnemonic handler use the same DBusMenu export and item IDs. GTK action
metadata is left intact; windows without an exported menubar keep their native
menu metadata. When the last module-owned bar is removed, the original native
menu paths (including absent properties) are restored at idle. A replacement bar
cancels that restoration, so synchronous menu replacement does not expose a
competing GMenu export. Window teardown cancels the pending restoration.
The module does not advertise fabricated GTK menu/action paths for applications
without a native GMenu export.

The regression suite exercises both shell-setting values, exported action
activation, model replacement, settings changes and window unrealize/realize.
For desktop integration tests, leave `gmenudbusmenuproxy` enabled, try both proxy
startup orders, and check the window's `_KDE_NET_WM_APPMENU_SERVICE_NAME` with
`xprop` alongside Alt+mnemonic: seeing a menu alone does not verify forwarding.
