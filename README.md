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
sudo chmod +x etc/profile.d/appmenu-gtk-module-wayland.sh
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


