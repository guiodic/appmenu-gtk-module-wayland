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
#include "support.h"

#include <libdbusmenu-glib/server.h>
#include <libdbusmenu-glib/menuitem.h>
#include <libdbusmenu-gtk/menuitem.h>
#include <libdbusmenu-gtk/parser.h>
#include "unity-gtk-menu-item-private.h"

/* libdbusmenu-gtk internal but exported cache lookup */
DbusmenuMenuitem *dbusmenu_gtk_parse_get_cached_item(GtkWidget *widget);

G_GNUC_INTERNAL G_DEFINE_QUARK(appmenu_gtk_wayland_window_data, appmenu_gtk_wayland_window_data);
G_DEFINE_BOXED_TYPE(WindowData, appmenu_gtk_wayland_window_data, (GBoxedCopyFunc)window_data_copy,
                    (GBoxedFreeFunc)window_data_free);
G_GNUC_INTERNAL G_DEFINE_QUARK(appmenu_gtk_wayland_menu_shell_data, appmenu_gtk_wayland_menu_shell_data);
G_DEFINE_BOXED_TYPE(MenuShellData, appmenu_gtk_wayland_menu_shell_data, (GBoxedCopyFunc)menu_shell_data_copy,
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
		if (window_data->x11_restore_menu_paths_id != 0)
			g_source_remove(window_data->x11_restore_menu_paths_id);
		g_free(window_data->x11_native_menubar_path);
		g_free(window_data->x11_native_appmenu_path);
		if (window_data->appmenu_retry_id != 0)
			g_source_remove(window_data->appmenu_retry_id);
		if (window_data->appmenu_request != NULL)
			g_cancellable_cancel(window_data->appmenu_request);
		g_clear_object(&window_data->appmenu_request);
		if (window_data->menu_model != NULL)
			g_object_unref(window_data->menu_model);

		if (window_data->old_model != NULL)
			g_object_unref(window_data->old_model);

		if (window_data->menus != NULL)
		{
			GSList *menus = window_data->menus;
			window_data->menus = NULL;
			g_slist_free_full(menus, g_object_unref);
		}

		if (window_data->dbusmenu_servers != NULL)
		{
			GSList *servers = window_data->dbusmenu_servers;
			window_data->dbusmenu_servers = NULL;
			g_slist_free_full(servers, g_object_unref);
		}

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
		{
			DbusmenuServer *server = menu_shell_data->server;
			menu_shell_data->server = NULL;
			g_object_unref(server);
		}
		g_clear_object(&menu_shell_data->pending_root);
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

	if (menu_shell == NULL || !GTK_IS_MENU_SHELL(menu_shell))
		return NULL;

	menu_shell_data = g_object_get_qdata(G_OBJECT(menu_shell), appmenu_gtk_wayland_menu_shell_data_quark());

	if (menu_shell_data == NULL)
	{
		menu_shell_data = menu_shell_data_new();

		g_object_set_qdata_full(G_OBJECT(menu_shell),
		                        appmenu_gtk_wayland_menu_shell_data_quark(),
		                        menu_shell_data,
		                        menu_shell_data_free);
	}

	return menu_shell_data;
}

G_GNUC_INTERNAL WindowData *gtk_window_peek_window_data(GtkWindow *window)
{
	if (window == NULL || !GTK_IS_WINDOW(window))
		return NULL;

	return g_object_get_qdata(G_OBJECT(window), appmenu_gtk_wayland_window_data_quark());
}

G_GNUC_INTERNAL WindowData *gtk_window_get_window_data(GtkWindow *window)
{
	WindowData *window_data = NULL;
	GdkDisplay *display;

	if (window == NULL || !GTK_IS_WINDOW(window))
		return NULL;

	g_debug("gtk_window_get_window_data: GTK_IS_WINDOW");

	display = gdk_display_get_default();

	if (display == NULL)
		return NULL;

#if (defined(GDK_WINDOWING_WAYLAND))
	if (GDK_IS_WAYLAND_DISPLAY(display))
	{
		g_debug("gtk_window_get_window_data: GDK_IS_WAYLAND_DISPLAY");
		window_data = gtk_wayland_window_get_window_data(window);
	}
#endif
#if (defined(GDK_WINDOWING_X11))
#if GTK_MAJOR_VERSION == 3
	if (GDK_IS_X11_DISPLAY(display))
#endif
	{
		window_data = gtk_x11_window_get_window_data(window);
	}
#endif
	return window_data;
}

static void update_window_appmenu(GtkWindow *window, WindowData *data);

static void free_appmenu_retry(gpointer user_data)
{
	GWeakRef *window_ref = user_data;
	g_weak_ref_clear(window_ref);
	g_free(window_ref);
}

static gboolean retry_window_appmenu(gpointer user_data)
{
	GtkWindow *window = g_weak_ref_get(user_data);
	if (window != NULL)
	{
		WindowData *data = gtk_window_peek_window_data(window);
		if (data != NULL)
		{
			data->appmenu_retry_id = 0;
			if (!gtk_widget_in_destruction(GTK_WIDGET(window)))
				update_window_appmenu(window, data);
		}
		g_object_unref(window);
	}
	return G_SOURCE_REMOVE;
}

typedef struct
{
	GWeakRef window;
	GCancellable *cancellable;
} AppmenuRequest;

static void appmenu_bus_ready(GObject *source, GAsyncResult *result, gpointer user_data)
{
	AppmenuRequest *request = user_data;
	GError *error = NULL;
	GDBusConnection *connection = g_bus_get_finish(result, &error);
	GtkWindow *window = g_weak_ref_get(&request->window);
	WindowData *data = window != NULL ? gtk_window_peek_window_data(window) : NULL;
	/* A menu replacement or unrealize may have superseded this request. */
	if (data != NULL && data->appmenu_request == request->cancellable)
	{
		g_clear_object(&data->appmenu_request);
		GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(window));
		GSList *last = g_slist_last(data->menus);
		if (gdk_window != NULL && last != NULL &&
		    !gtk_widget_in_destruction(GTK_WIDGET(window)))
		{
			if (connection != NULL)
			{
				if (GDK_IS_X11_WINDOW(gdk_window))
					gtk_x11_window_init_bus_properties(window, data, connection);
				/* libdbusmenu does not retry a failed initial bus acquisition.
				 * Construct its servers only while holding the shared session
				 * connection, so their own async acquisitions can reuse it. */
				for (GSList *iter = data->menus; iter != NULL; iter = iter->next)
				{
					MenuShellData *shell = gtk_menu_shell_get_menu_shell_data(iter->data);
					if (shell->server == NULL)
					{
						gchar *object_path = g_strdup_printf("/MenuBar/%u/%p", data->window_id, iter->data);
						shell->server = dbusmenu_server_new(object_path);
						dbusmenu_server_set_root(shell->server, shell->pending_root);
						g_clear_object(&shell->pending_root);
						data->dbusmenu_servers = g_slist_append(data->dbusmenu_servers,
						    g_object_ref(shell->server));
						g_free(object_path);
					}
				}
				MenuShellData *shell = gtk_menu_shell_get_menu_shell_data(last->data);
				gchar *path = NULL;
				g_object_get(shell->server, DBUSMENU_SERVER_PROP_DBUS_OBJECT, &path, NULL);
				data->kde_appmenu = appmenu_set_address(gdk_window,
				    g_dbus_connection_get_unique_name(connection), path);
				if (GDK_IS_X11_WINDOW(gdk_window) || data->kde_appmenu != NULL)
					data->published_server = shell->server;
				data->appmenu_retry_delay = 0;
				g_free(path);
			}
			else
			{
				guint delay = MAX(data->appmenu_retry_delay, 1);
				data->appmenu_retry_delay = MIN(delay * 2, 30);
				g_message("APPMENU-GTK-WAYLAND: session bus lookup failed for window %u: %s; retrying in %u seconds",
				          data->window_id, error != NULL ? error->message : "unknown error", delay);
				GWeakRef *window_ref = g_new0(GWeakRef, 1);
				g_weak_ref_init(window_ref, window);
				data->appmenu_retry_id = g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
				    delay, retry_window_appmenu, window_ref, free_appmenu_retry);
			}
		}
	}
	g_clear_object(&connection);
	g_clear_error(&error);
	g_clear_object(&window);
	g_weak_ref_clear(&request->window);
	g_object_unref(request->cancellable);
	g_free(request);
}

/* Publish asynchronously; neither initial setup nor retries block the GTK loop. */
static void update_window_appmenu(GtkWindow *window, WindowData *data)
{
	if (data->appmenu_retry_id != 0)
	{
		g_source_remove(data->appmenu_retry_id);
		data->appmenu_retry_id = 0;
	}
	if (data->appmenu_request != NULL)
		g_cancellable_cancel(data->appmenu_request);
	g_clear_object(&data->appmenu_request);
	data->published_server = NULL;
	if (data->kde_appmenu != NULL)
	{
		release_appmenu(data->kde_appmenu);
		data->kde_appmenu = NULL;
	}
	GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(window));
	if (gdk_window == NULL)
		return;
	appmenu_set_address(gdk_window, NULL, NULL);
	if (GDK_IS_X11_WINDOW(gdk_window))
		gtk_x11_window_use_dbusmenu(window);
	if (data->menus == NULL)
		return;

	AppmenuRequest *request = g_new0(AppmenuRequest, 1);
	g_weak_ref_init(&request->window, window);
	data->appmenu_request = g_cancellable_new();
	request->cancellable = g_object_ref(data->appmenu_request);
	g_bus_get(G_BUS_TYPE_SESSION, data->appmenu_request, appmenu_bus_ready, request);
}

G_GNUC_INTERNAL void gtk_window_disconnect_menu_shell(GtkWindow *window, GtkMenuShell *menu_shell)
{
	g_debug("gtk_window_disconnect_menu_shell");
	WindowData *window_data;
	MenuShellData *menu_shell_data;

	if (window == NULL || !GTK_IS_WINDOW(window))
		return;

	if (menu_shell == NULL || !GTK_IS_MENU_SHELL(menu_shell))
		return;

	menu_shell_data = gtk_menu_shell_get_menu_shell_data(menu_shell);

	if (menu_shell_data == NULL)
		return;

	g_warn_if_fail(window == menu_shell_data->window);

	window_data = gtk_window_peek_window_data(menu_shell_data->window);

	if (window_data != NULL)
	{
		GSList *iter;

		for (iter = window_data->menus; iter != NULL; iter = g_slist_next(iter))
			if (iter->data == menu_shell)
				break;

		if (iter != NULL)
		{
			g_object_unref(iter->data);

			window_data->menus = g_slist_delete_link(window_data->menus, iter);
		}

		if (menu_shell_data->server != NULL)
		{
		    DbusmenuServer *srv = menu_shell_data->server;
		    GSList *link = g_slist_find(window_data->dbusmenu_servers, srv);
		    if (link) {
			window_data->dbusmenu_servers = g_slist_delete_link(window_data->dbusmenu_servers, link);
			g_object_unref(srv); // unref for list
		    }
		    g_object_unref(srv); // unref for MenuShellData
		    menu_shell_data->server = NULL;
		}

		g_clear_object(&menu_shell_data->pending_root);
		update_window_appmenu(window, window_data);
		menu_shell_data->window = NULL;
	}
}

static gboolean forward_menu_mnemonic(GtkWidget *widget, gboolean group_cycling, gpointer user_data)
{
	(void)user_data;
	/* GTK already rotates duplicate mnemonic targets before emitting this signal.
	 * Forward its chosen item even when, e.g., Italian File/Finestre share Alt+F. */
	(void)group_cycling;
	GtkWidget *parent = gtk_widget_get_parent(widget);
	if (!GTK_IS_MENU_BAR(parent) || !gtk_widget_is_sensitive(widget) ||
	    !gtk_widget_get_visible(widget) ||
	    gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget)) == NULL ||
	    !appmenu_mnemonic_receiver_present() ||
	    !gtk_widget_shell_shows_menubar(parent))
		return FALSE;

	MenuShellData *shell_data = gtk_menu_shell_get_menu_shell_data(GTK_MENU_SHELL(parent));
	if (shell_data == NULL || shell_data->window == NULL || shell_data->server == NULL)
		return FALSE;
	WindowData *window_data = gtk_window_peek_window_data(shell_data->window);
	if (window_data == NULL)
		return FALSE;
	/* The window advertises only its newest server to the global menu viewer. */
	GSList *published = g_slist_last(window_data->dbusmenu_servers);
	if (published == NULL || published->data != shell_data->server ||
	    window_data->published_server != shell_data->server)
		return FALSE;
	GdkDisplay *display = gtk_widget_get_display(widget);
	if (GDK_IS_WAYLAND_DISPLAY(display) && window_data->kde_appmenu == NULL)
		return FALSE;

	/* Look up the current export instead of retaining an item across menu rebuilds. */
	DbusmenuMenuitem *item = dbusmenu_gtk_parse_get_cached_item(widget);
	if (item == NULL)
		return FALSE;
	DbusmenuMenuitem *root = NULL;
	g_object_get(shell_data->server, DBUSMENU_SERVER_PROP_ROOT_NODE, &root, NULL);
	gboolean current_export = root != NULL && dbusmenu_menuitem_get_parent(item) == root;
	g_clear_object(&root);
	if (!current_export)
		return FALSE;

	/* Let the global menu position and draw the popup, not the zero-sized GTK bar. */
	dbusmenu_menuitem_show_to_user(item, gtk_get_current_event_time());
	return TRUE;
}

static void connect_menu_mnemonic(GtkWidget *widget, gpointer user_data)
{
	(void)user_data;
	if (GTK_IS_MENU_ITEM(widget) &&
	    g_signal_handler_find(widget, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, forward_menu_mnemonic, NULL) == 0)
		g_signal_connect(widget, "mnemonic-activate", G_CALLBACK(forward_menu_mnemonic), NULL);
}

static void on_menu_item_inserted(GtkMenuShell *shell, GtkWidget *child, gint position, gpointer user_data)
{
	(void)shell;
	(void)position;
	(void)user_data;
	connect_menu_mnemonic(child, NULL);
}

static void fix_dbusmenu(GtkWidget *widget, gpointer user_data);
static void fix_dbusmenu_accelerator(GtkMenuItem *menu_item, DbusmenuMenuitem* item);
static void fix_dbusmenu_checkbox(GtkMenuItem *menu_item, DbusmenuMenuitem* item);
static void fix_dbusmenu_icon(GtkMenuItem *menu_item, DbusmenuMenuitem* item);

static void on_menu_show(GtkWidget *widget, gpointer user_data)
{
	g_debug("APPMENU-GTK-WAYLAND: menu show, re-fixing icons/accelerators/checkboxs for %p", widget);
	fix_dbusmenu(widget, NULL);
}

static void fix_dbusmenu(GtkWidget *widget, gpointer user_data)
{
	if (widget == NULL || gtk_widget_in_destruction(widget))
		return;

	if (GTK_IS_MENU_ITEM(widget))
	{
		DbusmenuMenuitem *item = g_object_get_data(G_OBJECT(widget), "dbusmenu-gtk-item");

		/* Parsed items may also be available through libdbusmenu's cache. */
		if (item == NULL)
			item = dbusmenu_gtk_parse_get_cached_item(widget);

		if (item != NULL)
		{
			GtkMenuItem* menu_item = GTK_MENU_ITEM(widget);
			fix_dbusmenu_accelerator(menu_item, item);
			fix_dbusmenu_checkbox(menu_item, item);
			fix_dbusmenu_icon(menu_item, item);
		}

		GtkWidget *submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget));
		if (submenu != NULL)
		{
			if (g_signal_handler_find(submenu, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_menu_show,
			                          NULL) == 0)
			{
				g_signal_connect(submenu, "show", G_CALLBACK(on_menu_show), NULL);
			}
			fix_dbusmenu(submenu, NULL);
		}
	}

	if (GTK_IS_CONTAINER(widget))
	{
		gtk_container_forall(GTK_CONTAINER(widget), (GtkCallback)fix_dbusmenu, NULL);
	}
}

static void fix_dbusmenu_accelerator(GtkMenuItem *menu_item, DbusmenuMenuitem *item)
{
	guint accel_key = 0;
	GdkModifierType accel_mods = 0;

	GtkAccelLabel* accel_label = gtk_menu_item_get_nth_accel_label(menu_item, 0);
	if (GTK_IS_ACCEL_LABEL(accel_label))
	{
		gtk_accel_label_get_accel(accel_label, &accel_key, &accel_mods);
	}

	if (accel_key != 0)
	{
		dbusmenu_menuitem_property_set_shortcut(item, accel_key, accel_mods);
	} else {
		// NOTE: Gimp adds a GimpAccelLabel (whose parent class is GtkLabel) instead of using the GtkAccelLabel.
		// To avoid depending on Gimp's source, look for a second label & grab the text from it if the type name matches.
		// While hacky, it's the best way I could find; the action the label represents can be gotten
		// but the accelerators inside it don't appear to have a getter.
		GtkLabel* gimp_label = gtk_menu_item_get_nth_label(menu_item, 1);
		if (gimp_label != NULL)
		{
			GType type = G_OBJECT_TYPE(G_OBJECT(gimp_label));
			const gchar* accel_text = gtk_label_get_text(gimp_label);
			if (g_strcmp0(g_type_name(type), "GimpAccelLabel") == 0 && accel_text != NULL && accel_text[0] != '\0') {
				gchar **parts = g_strsplit(accel_text, "+", -1);
				GVariantBuilder builder;
				g_variant_builder_init(&builder, G_VARIANT_TYPE("aas"));
				g_variant_builder_open(&builder, G_VARIANT_TYPE("as"));
				for (gint i = 0; parts[i] != NULL; i++) {
					g_variant_builder_add(&builder, "s", parts[i]);
				}
				g_variant_builder_close(&builder);
				g_strfreev(parts);

				GVariant *variant = g_variant_builder_end(&builder);
				dbusmenu_menuitem_property_set_variant(item, DBUSMENU_MENUITEM_PROP_SHORTCUT, variant);
			}
		}
	}
}

static void fix_dbusmenu_checkbox(GtkMenuItem *menu_item, DbusmenuMenuitem *item)
{
	// NOTE: Some applications use GtkModelMenuItem, which will incorrectly be determined to be a checkbox.
	// This checks the accessibility role & removes the checkbox/radiobutton if it's not that type.
	if (GTK_IS_CHECK_MENU_ITEM(menu_item))
	{
		AtkObject* accessibility = gtk_widget_get_accessible(GTK_WIDGET(menu_item));
		if (accessibility != NULL)
		{
			AtkRole role = atk_object_get_role(accessibility);
			if (!(role == ATK_ROLE_CHECK_MENU_ITEM || role == ATK_ROLE_RADIO_MENU_ITEM))
				dbusmenu_menuitem_property_set(item, DBUSMENU_MENUITEM_PROP_TOGGLE_TYPE, "");
		}
	}
}

static GdkPixbuf *resolve_pixbuf_for_icon(GtkWidget *widget, GIcon *icon, gboolean *new_pixbuf)
{
	GdkPixbuf *pixbuf = NULL;
	*new_pixbuf = FALSE;

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
		*new_pixbuf = (pixbuf != NULL);
	}

	return pixbuf;
}

static void fix_dbusmenu_icon(GtkMenuItem *menu_item, DbusmenuMenuitem *item)
{
	const gchar *existing_name = dbusmenu_menuitem_property_get(item, "icon-name");
	GVariant *existing_data = dbusmenu_menuitem_property_get_variant(item, "icon-data");

	/* Only set the icon if it's not already set or is empty */
	if ((existing_name == NULL || existing_name[0] == '\0') &&
		existing_data == NULL)
	{
		GtkWidget* widget = GTK_WIDGET(menu_item);
		/* gtk_menu_item_get_icon returns a new reference (strongly reffed)
			* to ensure the icon remains valid during processing.
			*/
		GIcon *icon = gtk_menu_item_get_icon(menu_item);
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

				pixbuf = resolve_pixbuf_for_icon(widget, icon, &new_pixbuf);

				if (pixbuf)
				{
					g_debug("APPMENU-GTK-WAYLAND: fixing icon-data for %p (new_pixbuf: %d)", widget, new_pixbuf);
					if (dbusmenu_menuitem_property_set_image(item, "icon-data", pixbuf))
						dbusmenu_menuitem_property_set_bool(item, "icon-visible", TRUE);
					else
						g_debug("APPMENU-GTK-WAYLAND: failed to serialize icon-data for %p",
								widget);
					if (new_pixbuf) {
						g_object_unref(pixbuf);
					}
				}
			}
			g_object_unref(icon);
		}
	}
}

typedef struct
{
	GtkWidget *widget;
} FixIconsData;

static gboolean fix_idle(gpointer data)
{
	FixIconsData *fid = data;
	if (fid->widget != NULL)
	{
		GtkWidget *widget = fid->widget;
		g_object_remove_weak_pointer(G_OBJECT(widget), (gpointer *)&fid->widget);
		fix_dbusmenu(widget, NULL);
	}
	g_free(fid);
	return G_SOURCE_REMOVE;
}

static void schedule_fix(GtkWidget *widget)
{
	FixIconsData *fid = g_new0(FixIconsData, 1);
	fid->widget       = widget;
	g_object_add_weak_pointer(G_OBJECT(widget), (gpointer *)&fid->widget);
	g_idle_add(fix_idle, fid);
}

G_GNUC_INTERNAL void gtk_window_connect_menu_shell(GtkWindow *window, GtkMenuShell *menu_shell)
{
	g_debug("============== gtk_window_connect_menu_shell");
	MenuShellData *menu_shell_data;

	if (window == NULL || !GTK_IS_WINDOW(window))
		return;

	if (menu_shell == NULL || !GTK_IS_MENU_SHELL(menu_shell))
		return;

	menu_shell_data = gtk_menu_shell_get_menu_shell_data(menu_shell);

	if (menu_shell_data == NULL)
		return;

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
				if (iter->data == menu_shell)
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
					fix_dbusmenu(GTK_WIDGET(menu_shell), NULL);
					schedule_fix(GTK_WIDGET(menu_shell));
					if (GTK_IS_MENU_BAR(menu_shell))
					{
						gtk_container_foreach(GTK_CONTAINER(menu_shell), connect_menu_mnemonic, NULL);
						if (g_signal_handler_find(menu_shell, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
						                          on_menu_item_inserted, NULL) == 0)
							g_signal_connect_after(menu_shell, "insert", G_CALLBACK(on_menu_item_inserted), NULL);
					}
				}

				/* Keep the parsed model until the session bus is available. */
				menu_shell_data->pending_root = item;
				update_window_appmenu(window, window_data);
			}
		}

		menu_shell_data->window = window;
	}
}
