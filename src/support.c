/*
 * appmenu-gtk-module
 * Copyright 2012 Canonical Ltd.
 * Copyright (C) 2015-2017 Konstantin Pugin <ria.freelander@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 *          William Hua <william.hua@canonical.com>
 *          Konstantin Pugin <ria.freelander@gmail.com>
 *          Lester Carballo Perez <lestcape@gmail.com>
 */

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>

#include "blacklist.h"
#include "consts.h"
#include "support.h"
#include "platform.h"

#if (GTK_MAJOR_VERSION < 3) || defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
/* The viewer is watched independently and is not counted as a registrar. */
static const char *const WATCHED_NAMES[] = {
	"com.canonical.AppMenu.Registrar",
	"org.kde.KAppMenu",
	"org.kde.kappmenu",
	"org.ayatana.AppMenu.Registrar",
	"org.kde.kappmenuview"
};
enum { REGISTRAR_COUNT = 4, KDE_VIEWER_INDEX = REGISTRAR_COUNT };
static uint watcher_ids[G_N_ELEMENTS(WATCHED_NAMES)] = { 0 };
static bool bus_name_present[G_N_ELEMENTS(WATCHED_NAMES)] = { false };
#endif

static bool is_true(const char *value)
{
	return value != NULL && value[0] != '\0' && g_ascii_strcasecmp(value, "0") != 0 &&
	       g_ascii_strcasecmp(value, "no") != 0 && g_ascii_strcasecmp(value, "off") != 0 &&
	       g_ascii_strcasecmp(value, "false") != 0;
}

G_GNUC_INTERNAL bool gtk_module_should_run()
{
	const char *proxy          = g_getenv("UBUNTU_MENUPROXY");
	bool is_platform_supported = false;
	bool is_program_supported  = false;
	bool should_run            = false;
	static bool run_once       = true;
#if GTK_MAJOR_VERSION >= 3
	GdkDisplay *display = gdk_display_get_default();
	if (display != NULL && GDK_IS_X11_DISPLAY(display))
		is_platform_supported = true;
	else if (display != NULL && GDK_IS_WAYLAND_DISPLAY(display))
	{
		is_platform_supported = true;
	}
	else
		is_platform_supported = false;
#else
	is_platform_supported = true;
#endif
	is_program_supported =
	    (proxy == NULL || is_true(proxy)) && !is_blacklisted(g_get_prgname());
	should_run = is_program_supported && is_platform_supported && run_once;
	run_once   = !(is_program_supported && is_platform_supported);
	return should_run;
}

/* KDE only listens for ItemActivationRequested while its viewer is registered.
 * Keep this separate from registrar availability and menubar visibility. */
G_GNUC_INTERNAL bool appmenu_mnemonic_receiver_present(void)
{
#if (GTK_MAJOR_VERSION < 3) || defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
	return bus_name_present[KDE_VIEWER_INDEX];
#else
	return false;
#endif
}

G_GNUC_INTERNAL bool appmenu_registrar_present(void)
{
#if (GTK_MAJOR_VERSION < 3) || defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
	for (int i = 0; i < REGISTRAR_COUNT; i++)
		if (bus_name_present[i])
			return true;
#endif
	return false;
}

G_GNUC_INTERNAL bool gtk_widget_shell_shows_menubar(GtkWidget *widget)
{
#if (GTK_MAJOR_VERSION < 3) || defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
	if (appmenu_registrar_present())
		return true;
#ifdef GDK_WINDOWING_WAYLAND
	if (org_kde_kwin_appmenu_manager != NULL)
		return true;
#endif
#endif
	GtkSettings *settings;
	GParamSpec *pspec;
	gboolean shell_shows_menubar;

	g_return_val_if_fail(GTK_IS_WIDGET(widget), false);

	settings = gtk_widget_get_settings(widget);

	g_return_val_if_fail(GTK_IS_SETTINGS(settings), false);

	pspec =
	    g_object_class_find_property(G_OBJECT_GET_CLASS(settings), "gtk-shell-shows-menubar");

	if (pspec == NULL)
		return false;

	g_return_val_if_fail(pspec->value_type == G_TYPE_BOOLEAN, false);

	g_object_get(settings, "gtk-shell-shows-menubar", &shell_shows_menubar, NULL);

	return shell_shows_menubar;
}
static void gtk_settings_handle_gtk_shell_shows_menubar(GObject *object, GParamSpec *pspec,
                                                        gpointer user_data)
{
	gtk_widget_queue_resize(user_data);
}
G_GNUC_INTERNAL void gtk_widget_connect_settings(GtkWidget *widget)
{
	GtkSettings *settings = gtk_widget_get_settings(widget);
	g_signal_connect(settings,
	                 "notify::gtk-shell-shows-menubar",
	                 G_CALLBACK(gtk_settings_handle_gtk_shell_shows_menubar),
	                 widget);
}

G_GNUC_INTERNAL void gtk_widget_disconnect_settings(GtkWidget *widget)
{
	GtkSettings *settings = gtk_widget_get_settings(widget);
	if (settings != NULL)
		g_signal_handlers_disconnect_by_data(settings, widget);
}

#if (GTK_MAJOR_VERSION < 3) || defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
static void resize_menu_bar(GtkWidget *widget, gpointer user_data)
{
	(void)user_data;
	if (GTK_IS_MENU_BAR(widget))
		gtk_widget_queue_resize(widget);
	else if (GTK_IS_CONTAINER(widget))
		gtk_container_forall(GTK_CONTAINER(widget), resize_menu_bar, NULL);
}

G_GNUC_INTERNAL void gtk_module_notify_menu_state(void)
{
	/* Keep GTK's own shell setting intact. GtkApplicationWindow otherwise
	 * stops constructing its menubar, removing its mnemonic targets as well.
	 * Our size hooks hide bars using the module's registrar/Wayland state. */
	GList *windows = gtk_window_list_toplevels();
	for (GList *iter = windows; iter != NULL; iter = iter->next)
		resize_menu_bar(iter->data, NULL);
	g_list_free(windows);
}

static void update_registrar_state(void)
{
	gtk_module_notify_menu_state();
}

static void on_name_appeared(GDBusConnection *connection, const char *name, const char *name_owner,
                             gpointer user_data)
{
	g_debug("Name %s on the session bus is owned by %s", name, name_owner);

	int index = GPOINTER_TO_INT(user_data);
	bus_name_present[index] = true;
	if (index < REGISTRAR_COUNT)
		update_registrar_state();
}

static void on_name_vanished(GDBusConnection *connection, const char *name, gpointer user_data)
{
	g_debug("Name %s does not exist on the session bus", name);

	int index = GPOINTER_TO_INT(user_data);
	bus_name_present[index] = false;
	if (index < REGISTRAR_COUNT)
		update_registrar_state();
}
#endif

G_GNUC_INTERNAL void watch_registrar_dbus()
{
#if (GTK_MAJOR_VERSION < 3) || defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
	if (watcher_ids[0] == 0)
	{
		for (int i = 0; i < G_N_ELEMENTS(WATCHED_NAMES); i++)
			bus_name_present[i] = false;

		GError *error               = NULL;
		GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
		if (connection == NULL)
		{
			g_debug("Unable to connect to dbus: %s", error->message);
			g_error_free(error);
		}
		else
		{
			GVariant *ret = g_dbus_connection_call_sync(connection,
			                                            "org.freedesktop.DBus",
			                                            "/org/freedesktop/DBus",
			                                            "org.freedesktop.DBus",
			                                            "ListNames",
			                                            NULL,
			                                            G_VARIANT_TYPE("(as)"),
			                                            G_DBUS_CALL_FLAGS_NONE,
			                                            -1,
			                                            NULL,
			                                            &error);
			if (ret == NULL)
			{
				g_debug("Unable to query dbus for ListNames: %s", error->message);
				g_error_free(error);
			}
			else
			{
				GVariant *names = g_variant_get_child_value(ret, 0);
				GVariantIter iter;
				char *name;
				g_variant_iter_init(&iter, names);
				while (g_variant_iter_loop(&iter, "&s", &name))
				{
					for (int i = 0; i < G_N_ELEMENTS(WATCHED_NAMES); i++)
					{
						if (g_str_equal(name, WATCHED_NAMES[i]))
						{
							bus_name_present[i] = true;
						}
					}
				}
				g_variant_unref(names);
				g_variant_unref(ret);
			}
			g_object_unref(connection);
		}

		for (int i = 0; i < G_N_ELEMENTS(WATCHED_NAMES); i++)
		{
			watcher_ids[i] = g_bus_watch_name(G_BUS_TYPE_SESSION,
			                                  WATCHED_NAMES[i],
			                                  G_BUS_NAME_WATCHER_FLAGS_NONE,
			                                  on_name_appeared,
			                                  on_name_vanished,
			                                  GINT_TO_POINTER(i),
			                                  NULL);
		}
	}
	update_registrar_state();
#endif
}
