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

#include "datastructs.h"
#include "datastructs-private.h"
#include "platform.h"

#include <libdbusmenu-glib/server.h>
#include <libdbusmenu-glib/menuitem.h>
#include <libdbusmenu-gtk/parser.h>
#include "unity-gtk-menu-item-private.h"

/* libdbusmenu-gtk internal but exported functions */
DbusmenuMenuitem *dbusmenu_gtk_parse_get_item(GtkWidget *widget);
DbusmenuMenuitem *dbusmenu_gtk_parse_get_cached_item(GtkWidget *widget);

G_GNUC_INTERNAL G_DEFINE_QUARK(window_data, window_data);
G_DEFINE_BOXED_TYPE(WindowData, window_data, (GBoxedCopyFunc)window_data_copy,
                    (GBoxedFreeFunc)window_data_free);
G_GNUC_INTERNAL G_DEFINE_QUARK(menu_shell_data, menu_shell_data);
G_DEFINE_BOXED_TYPE(MenuShellData, menu_shell_data, (GBoxedCopyFunc)menu_shell_data_copy,
                    (GBoxedFreeFunc)menu_shell_data_free);

G_GNUC_INTERNAL WindowData *window_data_new(void)
{
	return g_slice_new0(WindowData);
}

G_GNUC_INTERNAL void window_data_free(gpointer data)
{
	WindowData *window_data = data;

	if (window_data != NULL)
	{
		if (window_data->menu_model != NULL)
			g_object_unref(window_data->menu_model);

		if (window_data->old_model != NULL)
			g_object_unref(window_data->old_model);

		if (window_data->menus != NULL)
			g_slist_free_full(window_data->menus, g_object_unref);

		if (window_data->dbusmenu_servers != NULL)
			g_slist_free_full(window_data->dbusmenu_servers, g_object_unref);

		if (window_data->kde_appmenu != NULL)
			release_appmenu(window_data->kde_appmenu);

		g_slice_free(WindowData, window_data);
	}
}

G_GNUC_INTERNAL WindowData *window_data_copy(WindowData *source)
{
	WindowData *ret = window_data_new();

	if (source->menu_model != NULL)
		ret->menu_model = g_object_ref(source->menu_model);

	if (source->old_model != NULL)
		ret->old_model = g_object_ref(source->old_model);

	if (source->menus != NULL)
		ret->menus = g_slist_copy_deep(source->menus, (GCopyFunc)g_object_ref, NULL);

	return ret;
}

G_GNUC_INTERNAL MenuShellData *menu_shell_data_new(void)
{
	return g_slice_new0(MenuShellData);
}

G_GNUC_INTERNAL void menu_shell_data_free(gpointer data)
{
	MenuShellData *menu_shell_data = data;
	if (menu_shell_data != NULL)
	{
		if (menu_shell_data->server != NULL)
			g_object_unref(menu_shell_data->server);
		g_slice_free(MenuShellData, menu_shell_data);
	}
}

G_GNUC_INTERNAL MenuShellData *menu_shell_data_copy(MenuShellData *source)
{
	MenuShellData *ret = menu_shell_data_new();
	ret->window        = source->window;
	return ret;
}

G_GNUC_INTERNAL bool menu_shell_data_has_window(MenuShellData *source)
{
	return source->window != NULL;
}

G_GNUC_INTERNAL GtkWindow *menu_shell_data_get_window(MenuShellData *source)
{
	return source->window;
}

G_GNUC_INTERNAL MenuShellData *gtk_menu_shell_get_menu_shell_data(GtkMenuShell *menu_shell)
{
	MenuShellData *menu_shell_data;

	g_return_val_if_fail(GTK_IS_MENU_SHELL(menu_shell), NULL);

	menu_shell_data = g_object_get_qdata(G_OBJECT(menu_shell), menu_shell_data_quark());

	if (menu_shell_data == NULL)
	{
		menu_shell_data = menu_shell_data_new();

		g_object_set_qdata_full(G_OBJECT(menu_shell),
		                        menu_shell_data_quark(),
		                        menu_shell_data,
		                        menu_shell_data_free);
	}

	return menu_shell_data;
}

G_GNUC_INTERNAL WindowData *gtk_window_get_window_data(GtkWindow *window)
{
	WindowData *window_data = NULL;

	g_return_val_if_fail(GTK_IS_WINDOW(window), NULL);
	g_debug("gtk_window_get_window_data: GTK_IS_WINDOW");

#if (defined(GDK_WINDOWING_WAYLAND))
	if (GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default()))
	{
		g_debug("gtk_window_get_window_data: GDK_IS_WAYLAND_DISPLAY");
		window_data = gtk_wayland_window_get_window_data(window);
	}
#endif
#if (defined(GDK_WINDOWING_X11))
#if GTK_MAJOR_VERSION == 3
	if (GDK_IS_X11_DISPLAY(gdk_display_get_default()))
#endif
	{
		window_data = gtk_x11_window_get_window_data(window);
	}
#endif
	return window_data;
}

G_GNUC_INTERNAL void gtk_window_disconnect_menu_shell(GtkWindow *window, GtkMenuShell *menu_shell)
{
	g_debug("gtk_window_disconnect_menu_shell");
	WindowData *window_data;
	MenuShellData *menu_shell_data;

	g_return_if_fail(GTK_IS_WINDOW(window));
	g_return_if_fail(GTK_IS_MENU_SHELL(menu_shell));

	menu_shell_data = gtk_menu_shell_get_menu_shell_data(menu_shell);

	g_warn_if_fail(window == menu_shell_data->window);

	window_data = gtk_window_get_window_data(menu_shell_data->window);

	if (window_data != NULL)
	{
		GSList *iter;
		guint i = 0;

		if (window_data->old_model != NULL)
			i++;

		for (iter = window_data->menus; iter != NULL; iter = g_slist_next(iter), i++)
			if (GTK_MENU_SHELL(iter->data) == menu_shell)
				break;

		if (iter != NULL)
		{
			g_menu_remove(window_data->menu_model, i);

			g_object_unref(iter->data);

			window_data->menus = g_slist_delete_link(window_data->menus, iter);
		}

		if (menu_shell_data->server != NULL)
		{
			window_data->dbusmenu_servers =
			    g_slist_remove(window_data->dbusmenu_servers, menu_shell_data->server);
			g_object_unref(menu_shell_data->server);
			menu_shell_data->server = NULL;
		}

		menu_shell_data->window = NULL;
	}
}

static void fix_dbusmenu_icons(GtkWidget *widget, gpointer user_data);

static void on_menu_show(GtkWidget *widget, gpointer user_data)
{
	g_debug("APPMENU-GTK-WAYLAND: menu show, re-fixing icons for %p", widget);
	fix_dbusmenu_icons(widget, NULL);
}

static void fix_dbusmenu_icons(GtkWidget *widget, gpointer user_data)
{
	if (GTK_IS_MENU_ITEM(widget))
	{
		DbusmenuMenuitem *item = g_object_get_data(G_OBJECT(widget), "dbusmenu-gtk-item");

		/* Fallback to internal lookup functions if data is not found directly */
		if (item == NULL)
			item = dbusmenu_gtk_parse_get_cached_item(widget);
		if (item == NULL)
			item = dbusmenu_gtk_parse_get_item(widget);

		if (item != NULL)
		{
			const gchar *existing_name = dbusmenu_menuitem_property_get(item, "icon-name");
			GVariant *existing_data = dbusmenu_menuitem_property_get_variant(item, "icon-data");

			/* Only set the icon if it's not already set or is empty */
			if ((existing_name == NULL || existing_name[0] == '\0') &&
			    existing_data == NULL)
			{
				/* gtk_menu_item_get_icon returns a new reference (strongly reffed)
				 * to ensure the icon remains valid during processing.
				 */
				GIcon *icon = gtk_menu_item_get_icon(GTK_MENU_ITEM(widget));
				if (icon != NULL)
				{
					if (G_IS_THEMED_ICON(icon))
					{
						const gchar *const *names =
						    g_themed_icon_get_names(G_THEMED_ICON(icon));
						if (names != NULL && names[0] != NULL)
						{
							g_debug("APPMENU-GTK-WAYLAND: fixing icon-name: %s for %p",
							        names[0], widget);
							dbusmenu_menuitem_property_set(item,
							                               "icon-name",
							                               names[0]);
							dbusmenu_menuitem_property_set_bool(item, "icon-visible", TRUE);
						}
					}
					else
					{
						GdkPixbuf *pixbuf = NULL;
						gboolean new_pixbuf = FALSE;

						if (GDK_IS_PIXBUF(icon)) {
							pixbuf = GDK_PIXBUF(icon);
						} else {
							GError *error = NULL;
							gint width = 16, height = 16;
							gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
							GdkScreen *screen = gtk_widget_get_screen(widget);
							GtkIconTheme *icon_theme = screen ? gtk_icon_theme_get_for_screen(screen) : gtk_icon_theme_get_default();
							G_GNUC_BEGIN_IGNORE_DEPRECATIONS
							GtkIconInfo *icon_info = gtk_icon_theme_lookup_by_gicon(icon_theme, icon, width, GTK_ICON_LOOKUP_FORCE_SIZE);
							if (icon_info) {
								pixbuf = gtk_icon_info_load_icon(icon_info, &error);
								g_object_unref(icon_info);
							}
							G_GNUC_END_IGNORE_DEPRECATIONS

							if (error) {
								g_debug("APPMENU-GTK-WAYLAND: failed to load icon: %s", error->message);
								g_error_free(error);
							}
							new_pixbuf = (pixbuf != NULL);
						}

						if (pixbuf)
						{
							g_debug("APPMENU-GTK-WAYLAND: fixing icon-data for %p (new_pixbuf: %d)", widget, new_pixbuf);
							GVariant *variant = g_variant_new(
							    "(iiibay)",
							    gdk_pixbuf_get_width(pixbuf),
							    gdk_pixbuf_get_height(pixbuf),
							    gdk_pixbuf_get_rowstride(pixbuf),
							    gdk_pixbuf_get_has_alpha(pixbuf),
							    gdk_pixbuf_get_pixels(pixbuf),
							    (gsize)gdk_pixbuf_get_height(pixbuf) *
							        gdk_pixbuf_get_rowstride(pixbuf));

							dbusmenu_menuitem_property_set_variant(
							    item,
							    "icon-data",
							    variant);
							dbusmenu_menuitem_property_set_bool(item, "icon-visible", TRUE);
							if (new_pixbuf) {
								g_object_unref(pixbuf);
							}
						}
					}
					g_object_unref(icon);
				}
			}
		}

		GtkWidget *submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget));
		if (submenu != NULL)
		{
			if (g_signal_handler_find(submenu, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_menu_show,
			                          NULL) == 0)
			{
				g_signal_connect(submenu, "show", G_CALLBACK(on_menu_show), NULL);
			}
			fix_dbusmenu_icons(submenu, NULL);
		}
	}

	if (GTK_IS_CONTAINER(widget))
	{
		gtk_container_forall(GTK_CONTAINER(widget), (GtkCallback)fix_dbusmenu_icons, NULL);
	}
}

typedef struct
{
	GtkWidget *widget;
} FixIconsData;

static gboolean fix_icons_idle(gpointer data)
{
	FixIconsData *fid = data;
	if (fid->widget != NULL)
	{
		GtkWidget *widget = fid->widget;
		g_object_remove_weak_pointer(G_OBJECT(widget), (gpointer *)&fid->widget);
		fix_dbusmenu_icons(widget, NULL);
	}
	g_free(fid);
	return G_SOURCE_REMOVE;
}

static void schedule_fix_icons(GtkWidget *widget)
{
	FixIconsData *fid = g_new0(FixIconsData, 1);
	fid->widget       = widget;
	g_object_add_weak_pointer(G_OBJECT(widget), (gpointer *)&fid->widget);
	g_idle_add(fix_icons_idle, fid);
}

G_GNUC_INTERNAL void gtk_window_connect_menu_shell(GtkWindow *window, GtkMenuShell *menu_shell)
{
	g_debug("============== gtk_window_connect_menu_shell");
	MenuShellData *menu_shell_data;

	g_return_if_fail(GTK_IS_WINDOW(window));
	g_return_if_fail(GTK_IS_MENU_SHELL(menu_shell));

	menu_shell_data = gtk_menu_shell_get_menu_shell_data(menu_shell);

	if (window != menu_shell_data->window)
	{
		WindowData *window_data;

		if (menu_shell_data->window != NULL)
			gtk_window_disconnect_menu_shell(menu_shell_data->window, menu_shell);

		window_data = gtk_window_get_window_data(window);

		if (window_data != NULL)
		{
			GSList *iter;

			for (iter = window_data->menus; iter != NULL; iter = g_slist_next(iter))
				if (GTK_MENU_SHELL(iter->data) == menu_shell)
					break;

			if (iter == NULL)
			{
				g_debug("gtk_window_connect_menu_shell: connecting new menu shell");
				window_data->menus = g_slist_append(window_data->menus, g_object_ref(menu_shell));

				DbusmenuMenuitem *item = dbusmenu_gtk_parse_menu_structure(GTK_WIDGET(menu_shell));
				if (item == NULL)
				{
					g_debug("gtk_window_connect_menu_shell: failed to parse menu structure");
				}
				else
				{
					fix_dbusmenu_icons(GTK_WIDGET(menu_shell), NULL);
					schedule_fix_icons(GTK_WIDGET(menu_shell));
				}

				gchar *path = g_strdup_printf("/MenuBar/%d/%p", window_data->window_id, menu_shell);
				DbusmenuServer *srv = dbusmenu_server_new(path);
				dbusmenu_server_set_root(srv, item);
				if (item != NULL)
					g_object_unref(item);

				window_data->dbusmenu_servers = g_slist_append(window_data->dbusmenu_servers, srv);
				menu_shell_data->server       = srv;

				GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
				if (connection != NULL)
				{
					const char* unique_bus_name = g_dbus_connection_get_unique_name(connection);

					if (window_data->kde_appmenu != NULL)
						release_appmenu(window_data->kde_appmenu);

					window_data->kde_appmenu = appmenu_set_address(gtk_widget_get_window(GTK_WIDGET(window)), unique_bus_name, path);
					g_object_unref(connection);
				}
				else
				{
					g_debug("gtk_window_connect_menu_shell: failed to get session bus");
				}

				g_free(path);
			}
		}

		menu_shell_data->window = window;
	}
}
