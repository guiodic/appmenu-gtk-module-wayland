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

#include <gtk/gtk.h>

#include "consts.h"
#include "datastructs.h"
#include "hijack.h"
#include "glib-object.h"
#include "glib.h"
#include "platform.h"
#include "support.h"

static void (*pre_hijacked_window_realize)(GtkWidget *widget);

static void (*pre_hijacked_window_unrealize)(GtkWidget *widget);

#if GTK_MAJOR_VERSION == 3
static void (*pre_hijacked_application_window_realize)(GtkWidget *widget);
#endif

static void (*pre_hijacked_menu_bar_realize)(GtkWidget *widget);

static void (*pre_hijacked_menu_bar_unrealize)(GtkWidget *widget);

static void (*pre_hijacked_widget_size_allocate)(GtkWidget *widget, GtkAllocation *allocation);

static void (*pre_hijacked_menu_bar_size_allocate)(GtkWidget *widget, GtkAllocation *allocation);

#if GTK_MAJOR_VERSION == 2
static void (*pre_hijacked_menu_bar_size_request)(GtkWidget *widget, GtkRequisition *requisition);
#elif GTK_MAJOR_VERSION == 3
static void (*pre_hijacked_menu_bar_get_preferred_width)(GtkWidget *widget, gint *minimum_width,
                                                         gint *natural_width);

static void (*pre_hijacked_menu_bar_get_preferred_height)(GtkWidget *widget, gint *minimum_height,
                                                          gint *natural_height);

static void (*pre_hijacked_menu_bar_get_preferred_width_for_height)(GtkWidget *widget, gint height,
                                                                    gint *minimum_width,
                                                                    gint *natural_width);

static void (*pre_hijacked_menu_bar_get_preferred_height_for_width)(GtkWidget *widget, gint width,
                                                                    gint *minimum_height,
                                                                    gint *natural_height);
#endif

/* GtkApplicationWindow omits its menubar when the desktop exports GMenu.
 * Keep a private, offscreen bar in that case: GTK still registers its mnemonics
 * and resolves app./win. actions through the window, without changing settings. */
static const char application_bar_key[] = "appmenu-private-application-bar";
static const char application_owner_key[] = "appmenu-private-application-owner";

static void destroy_application_bar(gpointer pointer)
{
	GtkWidget *bar = pointer;
	gtk_widget_unparent(bar);
	gtk_widget_destroy(bar);
	g_object_unref(bar);
}

static void map_application_bar(GtkWidget *window, gpointer unused)
{
	GtkWidget *bar = g_object_get_data(G_OBJECT(window), application_bar_key);
	if (bar != NULL)
		gtk_widget_map(bar);
}

static void unmap_application_bar(GtkWidget *window, gpointer unused)
{
	GtkWidget *bar = g_object_get_data(G_OBJECT(window), application_bar_key);
	if (bar != NULL)
		gtk_widget_unmap(bar);
}

static void sync_application_bar(GObject *object, GParamSpec *pspec, GtkWidget *window)
{
	GtkApplication *app = gtk_window_get_application(GTK_WINDOW(window));
	gboolean shell_shows = FALSE;
	g_object_get(gtk_widget_get_settings(window), "gtk-shell-shows-menubar", &shell_shows, NULL);
	GMenuModel *model = app != NULL ? gtk_application_get_menubar(app) : NULL;
	GMenuModel *app_menu = app != NULL ? gtk_application_get_app_menu(app) : NULL;
	GtkWidget *bar = g_object_get_data(G_OBJECT(window), application_bar_key);
	if (!shell_shows || model == NULL ||
	    !gtk_application_window_get_show_menubar(GTK_APPLICATION_WINDOW(window)))
	{
		g_object_set_data(G_OBJECT(window), application_bar_key, NULL);
		return;
	}
	if (bar != NULL && g_object_get_data(G_OBJECT(bar), "appmenu-source-model") == model &&
	    g_object_get_data(G_OBJECT(bar), "appmenu-application-model") == app_menu)
		return;
	g_object_set_data(G_OBJECT(window), application_bar_key, NULL);
	GMenu *combined = g_menu_new();
	if (app_menu != NULL)
		g_menu_append_submenu(combined, g_get_application_name(), app_menu);
	g_menu_append_section(combined, NULL, model);
	bar = gtk_menu_bar_new_from_model(G_MENU_MODEL(combined));
	g_object_unref(combined);
	g_object_ref_sink(bar);
	if (app_menu != NULL)
		g_object_set_data_full(G_OBJECT(bar), "appmenu-application-model", g_object_ref(app_menu), g_object_unref);
	g_object_set_data_full(G_OBJECT(bar), "appmenu-source-model", g_object_ref(model), g_object_unref);
	g_object_set_data_full(G_OBJECT(window), application_bar_key, bar, destroy_application_bar);
	gtk_widget_set_parent(bar, window);
	gtk_widget_show(bar);
	gtk_widget_realize(bar);
	GtkAllocation allocation = { -1, -1, 1, 1 };
	gtk_widget_size_allocate(bar, &allocation);
	if (gtk_widget_get_mapped(window))
		gtk_widget_map(bar);
	if (GDK_IS_X11_DISPLAY(gtk_widget_get_display(window)))
		gtk_x11_window_use_dbusmenu(GTK_WINDOW(window));
}

static void free_application_owner(gpointer pointer)
{
	GWeakRef *owner = pointer;
	g_weak_ref_clear(owner);
	g_free(owner);
}

static void sync_application_owner(GObject *object, GParamSpec *pspec, GtkWidget *window)
{
	GWeakRef *owner = g_object_get_data(G_OBJECT(window), application_owner_key);
	if (owner == NULL)
	{
		owner = g_new0(GWeakRef, 1);
		g_weak_ref_init(owner, NULL);
		g_object_set_data_full(G_OBJECT(window), application_owner_key, owner, free_application_owner);
	}
	GtkApplication *old = g_weak_ref_get(owner);
	GtkApplication *app = gtk_window_get_application(GTK_WINDOW(window));
	if (old != app)
	{
		if (old != NULL)
			g_signal_handlers_disconnect_by_func(old, sync_application_bar, window);
		g_weak_ref_set(owner, app);
		if (app != NULL)
		{
			g_signal_connect_object(app, "notify::menubar", G_CALLBACK(sync_application_bar), window, 0);
			g_signal_connect_object(app, "notify::app-menu", G_CALLBACK(sync_application_bar), window, 0);
		}
	}
	g_clear_object(&old);
	sync_application_bar(NULL, NULL, window);
}

static void hijacked_window_realize(GtkWidget *widget)
{
	g_debug("hijacked_window_realize");
	g_return_if_fail(GTK_IS_WINDOW(widget));

	GdkScreen *screen      = gtk_widget_get_screen(widget);
	GdkVisual *visual      = gdk_screen_get_rgba_visual(screen);
	GdkWindowTypeHint hint = gtk_window_get_type_hint((GtkWindow *)widget);
	bool is_hint_viable =
	    ((hint == GDK_WINDOW_TYPE_HINT_NORMAL) || (hint == GDK_WINDOW_TYPE_HINT_DIALOG));
	if (visual && (hint == GDK_WINDOW_TYPE_HINT_DND))
		gtk_widget_set_visual(widget, visual);

	if (pre_hijacked_window_realize != NULL)
		pre_hijacked_window_realize(widget);

	GdkDisplay *display = gdk_display_get_default();
	if (is_hint_viable && !GTK_IS_APPLICATION_WINDOW((GtkWindow *)widget) &&
		display != NULL && (GDK_IS_X11_DISPLAY(display) || GDK_IS_WAYLAND_DISPLAY(display))
	)
	{
		gtk_window_get_window_data((GtkWindow *)widget);
	}
}

static void hijacked_window_unrealize(GtkWidget *widget)
{
	if (widget == NULL || !GTK_IS_WINDOW(widget))
		return;

	if (GTK_IS_APPLICATION_WINDOW(widget))
	{
		g_signal_handlers_disconnect_by_func(gtk_widget_get_settings(widget), sync_application_bar, widget);
		GWeakRef *owner = g_object_get_data(G_OBJECT(widget), application_owner_key);
		GtkApplication *app = owner != NULL ? g_weak_ref_get(owner) : NULL;
		if (app != NULL)
			g_signal_handlers_disconnect_by_func(app, sync_application_bar, widget);
		g_clear_object(&app);
		g_object_set_data(G_OBJECT(widget), application_owner_key, NULL);
		g_signal_handlers_disconnect_by_func(widget, sync_application_owner, widget);
		g_signal_handlers_disconnect_by_func(widget, sync_application_bar, widget);
		g_signal_handlers_disconnect_by_func(widget, map_application_bar, NULL);
		g_signal_handlers_disconnect_by_func(widget, unmap_application_bar, NULL);
		g_object_set_data(G_OBJECT(widget), application_bar_key, NULL);
	}
	if (pre_hijacked_window_unrealize != NULL)
		pre_hijacked_window_unrealize(widget);

	g_object_set_qdata(G_OBJECT(widget), appmenu_gtk_wayland_window_data_quark(), NULL);
}

static void hijacked_application_window_realize(GtkWidget *widget)
{
	g_debug("hijacked_application_window_realize");
	g_return_if_fail(GTK_IS_APPLICATION_WINDOW(widget));
	g_debug("hijacked_application_window_realize: GTK_IS_APPLICATION_WINDOW");

	if (pre_hijacked_application_window_realize != NULL)
		pre_hijacked_application_window_realize(widget);

	GdkDisplay *display = gdk_display_get_default();
	if (display != NULL && (GDK_IS_X11_DISPLAY(display) || GDK_IS_WAYLAND_DISPLAY(display)))
	{
		gtk_window_get_window_data((GtkWindow *)widget);
		g_signal_connect_object(gtk_widget_get_settings(widget), "notify::gtk-shell-shows-menubar",
		                        G_CALLBACK(sync_application_bar), widget, 0);
		g_signal_connect(widget, "notify::application", G_CALLBACK(sync_application_owner), widget);
		g_signal_connect(widget, "notify::show-menubar", G_CALLBACK(sync_application_bar), widget);
		g_signal_connect_after(widget, "map", G_CALLBACK(map_application_bar), NULL);
		g_signal_connect(widget, "unmap", G_CALLBACK(unmap_application_bar), NULL);
		sync_application_owner(NULL, NULL, widget);
		if (GDK_IS_X11_DISPLAY(display))
			gtk_x11_window_use_dbusmenu(GTK_WINDOW(widget));
	}
}

static void hijacked_menu_bar_realize(GtkWidget *widget)
{
	g_debug("hijacked_menu_bar_realize");
	GtkWidget *window;

	g_return_if_fail(GTK_IS_MENU_BAR(widget));

	if (pre_hijacked_menu_bar_realize != NULL)
		(*pre_hijacked_menu_bar_realize)(widget);

	window = gtk_widget_get_toplevel(widget);

	if (GTK_IS_WINDOW(window))
	{
		gtk_window_connect_menu_shell((GtkWindow *)window, (GtkMenuShell *)widget);
		if (GDK_IS_X11_DISPLAY(gtk_widget_get_display(window)))
			gtk_x11_window_use_dbusmenu(GTK_WINDOW(window));
	}

	gtk_widget_connect_settings(widget);
}

static void hijacked_menu_bar_unrealize(GtkWidget *widget)
{
	MenuShellData *menu_shell_data;

	if (widget == NULL || !GTK_IS_MENU_BAR(widget))
		return;

	menu_shell_data = gtk_menu_shell_get_menu_shell_data((GtkMenuShell *)widget);

	gtk_widget_disconnect_settings(widget);

	if (menu_shell_data != NULL && menu_shell_data_has_window(menu_shell_data))
		gtk_window_disconnect_menu_shell(menu_shell_data_get_window(menu_shell_data),
		                                 (GtkMenuShell *)widget);

	if (pre_hijacked_menu_bar_unrealize != NULL)
		pre_hijacked_menu_bar_unrealize(widget);
}

static void hijacked_menu_bar_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	GtkAllocation zero = { 0, 0, 0, 0 };
	GdkWindow *window;

	g_return_if_fail(GTK_IS_MENU_BAR(widget));

	if (gtk_widget_shell_shows_menubar(widget))
	{
		/*
		 * We manually assign an empty allocation to the menu bar to
		 * prevent the container from attempting to draw it at all.
		 */
		if (pre_hijacked_widget_size_allocate != NULL)
			pre_hijacked_widget_size_allocate(widget, &zero);

		/*
		 * Then we move the GdkWindow belonging to the menu bar outside of
		 * the clipping rectangle of the parent window so that we can't
		 * see it.
		 */
		window = gtk_widget_get_window(widget);

		if (window != NULL)
			gdk_window_move_resize(window, -1, -1, 1, 1);
	}
	else if (pre_hijacked_menu_bar_size_allocate != NULL)
		pre_hijacked_menu_bar_size_allocate(widget, allocation);
}

#if GTK_MAJOR_VERSION == 2
static void hijacked_menu_bar_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	g_return_if_fail(GTK_IS_MENU_BAR(widget));

	if (pre_hijacked_menu_bar_size_request != NULL)
		pre_hijacked_menu_bar_size_request(widget, requisition);

	if (gtk_widget_shell_shows_menubar(widget))
	{
		requisition->width  = 0;
		requisition->height = 0;
	}
}
#elif GTK_MAJOR_VERSION == 3
static void hijacked_menu_bar_get_preferred_width(GtkWidget *widget, gint *minimum_width,
                                                  gint *natural_width)
{
	g_return_if_fail(GTK_IS_MENU_BAR(widget));

	if (pre_hijacked_menu_bar_get_preferred_width != NULL)
		pre_hijacked_menu_bar_get_preferred_width(widget, minimum_width, natural_width);

	if (gtk_widget_shell_shows_menubar(widget))
	{
		*minimum_width = 0;
		*natural_width = 0;
	}
}

static void hijacked_menu_bar_get_preferred_height(GtkWidget *widget, gint *minimum_height,
                                                   gint *natural_height)
{
	g_return_if_fail(GTK_IS_MENU_BAR(widget));

	if (pre_hijacked_menu_bar_get_preferred_height != NULL)
		pre_hijacked_menu_bar_get_preferred_height(widget, minimum_height, natural_height);

	if (gtk_widget_shell_shows_menubar(widget))
	{
		*minimum_height = 0;
		*natural_height = 0;
	}
}

static void hijacked_menu_bar_get_preferred_width_for_height(GtkWidget *widget, gint height,
                                                             gint *minimum_width,
                                                             gint *natural_width)
{
	g_return_if_fail(GTK_IS_MENU_BAR(widget));

	if (pre_hijacked_menu_bar_get_preferred_width_for_height != NULL)
		pre_hijacked_menu_bar_get_preferred_width_for_height(widget,
		                                                     height,
		                                                     minimum_width,
		                                                     natural_width);

	if (gtk_widget_shell_shows_menubar(widget))
	{
		*minimum_width = 0;
		*natural_width = 0;
	}
}

static void hijacked_menu_bar_get_preferred_height_for_width(GtkWidget *widget, gint width,
                                                             gint *minimum_height,
                                                             gint *natural_height)
{
	g_return_if_fail(GTK_IS_MENU_BAR(widget));

	if (pre_hijacked_menu_bar_get_preferred_height_for_width != NULL)
		pre_hijacked_menu_bar_get_preferred_height_for_width(widget,
		                                                     width,
		                                                     minimum_height,
		                                                     natural_height);

	if (gtk_widget_shell_shows_menubar(widget))
	{
		*minimum_height = 0;
		*natural_height = 0;
	}
}
#endif

static void hijack_window_class_vtable(GType type)
{
	GtkWidgetClass *widget_class = g_type_class_ref(type);
	GType *children;
	guint n;
	guint i;

	if (widget_class == NULL)
		return;

	if (widget_class->realize == pre_hijacked_window_realize)
		widget_class->realize = hijacked_window_realize;

#if GTK_MAJOR_VERSION == 3
	if (widget_class->realize == pre_hijacked_application_window_realize)
		widget_class->realize = hijacked_application_window_realize;
#endif

	if (widget_class->unrealize == pre_hijacked_window_unrealize)
		widget_class->unrealize = hijacked_window_unrealize;

	children = g_type_children(type, &n);

	for (i = 0; i < n; i++)
		hijack_window_class_vtable(children[i]);

	g_free(children);
	g_type_class_unref(widget_class);
}

G_GNUC_INTERNAL void store_pre_hijacked()
{
	GtkWidgetClass *widget_class;
	/* store the base GtkWidget size_allocate vfunc */
	widget_class = g_type_class_ref(GTK_TYPE_WIDGET);
	if (widget_class != NULL)
	{
		pre_hijacked_widget_size_allocate = widget_class->size_allocate;
		g_type_class_unref(widget_class);
	}

#if GTK_MAJOR_VERSION == 3
	/* store the base GtkApplicationWindow realize vfunc */
	widget_class = g_type_class_ref(GTK_TYPE_APPLICATION_WINDOW);
	if (widget_class != NULL)
	{
		pre_hijacked_application_window_realize = widget_class->realize;
		g_type_class_unref(widget_class);
	}
#endif

	/* intercept window realize vcalls on GtkWindow */
	widget_class = g_type_class_ref(GTK_TYPE_WINDOW);
	if (widget_class != NULL)
	{
		pre_hijacked_window_realize   = widget_class->realize;
		pre_hijacked_window_unrealize = widget_class->unrealize;
		hijack_window_class_vtable(GTK_TYPE_WINDOW);
		g_type_class_unref(widget_class);
	}

	/* intercept size request and allocate vcalls on GtkMenuBar (for hiding) */
	widget_class = g_type_class_ref(GTK_TYPE_MENU_BAR);
	if (widget_class != NULL)
	{
		pre_hijacked_menu_bar_realize       = widget_class->realize;
		pre_hijacked_menu_bar_unrealize     = widget_class->unrealize;
		pre_hijacked_menu_bar_size_allocate = widget_class->size_allocate;
#if GTK_MAJOR_VERSION == 2
		pre_hijacked_menu_bar_size_request = widget_class->size_request;
#elif GTK_MAJOR_VERSION == 3
		pre_hijacked_menu_bar_get_preferred_width  = widget_class->get_preferred_width;
		pre_hijacked_menu_bar_get_preferred_height = widget_class->get_preferred_height;
		pre_hijacked_menu_bar_get_preferred_width_for_height =
		    widget_class->get_preferred_width_for_height;
		pre_hijacked_menu_bar_get_preferred_height_for_width =
		    widget_class->get_preferred_height_for_width;
#endif
		g_type_class_unref(widget_class);
	}
}
G_GNUC_INTERNAL void hijack_menu_bar_class_vtable(GType type)
{
	GtkWidgetClass *widget_class = g_type_class_ref(type);
	GType *children;
	guint n;
	guint i;

	if (widget_class == NULL)
		return;

	/* This fixes lp:1113008. */
	widget_class->hierarchy_changed = NULL;

	if (widget_class->realize == pre_hijacked_menu_bar_realize)
		widget_class->realize = hijacked_menu_bar_realize;

	if (widget_class->unrealize == pre_hijacked_menu_bar_unrealize)
		widget_class->unrealize = hijacked_menu_bar_unrealize;

	if (widget_class->size_allocate == pre_hijacked_menu_bar_size_allocate)
		widget_class->size_allocate = hijacked_menu_bar_size_allocate;

#if GTK_MAJOR_VERSION == 2
	if (widget_class->size_request == pre_hijacked_menu_bar_size_request)
		widget_class->size_request = hijacked_menu_bar_size_request;
#elif GTK_MAJOR_VERSION == 3
	if (widget_class->get_preferred_width == pre_hijacked_menu_bar_get_preferred_width)
		widget_class->get_preferred_width = hijacked_menu_bar_get_preferred_width;

	if (widget_class->get_preferred_height == pre_hijacked_menu_bar_get_preferred_height)
		widget_class->get_preferred_height = hijacked_menu_bar_get_preferred_height;

	if (widget_class->get_preferred_width_for_height ==
	    pre_hijacked_menu_bar_get_preferred_width_for_height)
		widget_class->get_preferred_width_for_height =
		    hijacked_menu_bar_get_preferred_width_for_height;

	if (widget_class->get_preferred_height_for_width ==
	    pre_hijacked_menu_bar_get_preferred_height_for_width)
		widget_class->get_preferred_height_for_width =
		    hijacked_menu_bar_get_preferred_height_for_width;
#endif

	children = g_type_children(type, &n);

	for (i = 0; i < n; i++)
		hijack_menu_bar_class_vtable(children[i]);

	g_free(children);
	g_type_class_unref(widget_class);
}
