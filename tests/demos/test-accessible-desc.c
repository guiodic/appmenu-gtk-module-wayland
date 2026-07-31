#include <gtk/gtk.h>
#include "datastructs.h"
#include <libdbusmenu-glib/menuitem.h>
#include <libdbusmenu-glib/server.h>
#include "datastructs-private.h"

int main(int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *menu_bar;
	GtkWidget *menu_item;
	GtkWidget *submenu;
	GtkWidget *sub_item;
	WindowData *window_data;
	DbusmenuServer *server;
	DbusmenuMenuitem *root;
	DbusmenuMenuitem *dbus_item = NULL;
	const gchar *desc;

	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	menu_bar = gtk_menu_bar_new();

	menu_item = gtk_menu_item_new_with_label("File");
	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
	gtk_container_add(GTK_CONTAINER(menu_bar), menu_item);

	sub_item = gtk_menu_item_new_with_label("New");
	gtk_widget_set_tooltip_text(sub_item, "Create a new file");
	gtk_container_add(GTK_CONTAINER(submenu), sub_item);

	gtk_container_add(GTK_CONTAINER(window), menu_bar);
	gtk_widget_show_all(window);

	/* Connect the menu shell using our function which triggers the post-processing fix-ups */
	gtk_window_connect_menu_shell(GTK_WINDOW(window), GTK_MENU_SHELL(menu_bar));

	/* Retrieve WindowData */
	window_data = gtk_window_peek_window_data(GTK_WINDOW(window));
	if (window_data == NULL)
	{
		g_printerr("Failed to retrieve WindowData!\n");
		return 1;
	}

	if (window_data->dbusmenu_servers == NULL)
	{
		g_printerr("No D-Bus menu servers registered!\n");
		return 1;
	}

	server = window_data->dbusmenu_servers->data;
	g_object_get(G_OBJECT(server), "root-node", &root, NULL);
	if (root == NULL)
	{
		g_printerr("Failed to get root node from server!\n");
		return 1;
	}

	/* We need to process the idle queue for schedule_fix_icons to run */
	while (g_main_context_pending(NULL))
	{
		g_main_context_iteration(NULL, FALSE);
	}

	/* The layout is: root -> "File" -> submenu -> "New" */
	/* Let's find "New" */
	GList *file_children = dbusmenu_menuitem_get_children(root);
	if (file_children == NULL)
	{
		g_printerr("No children found on root!\n");
		return 1;
	}

	DbusmenuMenuitem *file_item = file_children->data;
	GList *sub_children = dbusmenu_menuitem_get_children(file_item);
	if (sub_children == NULL)
	{
		g_printerr("No children found on File menu!\n");
		return 1;
	}

	dbus_item = sub_children->data;
	desc = dbusmenu_menuitem_property_get(dbus_item, "accessible-desc");

	if (desc == NULL)
	{
		g_printerr("accessible-desc property is NULL on parsed item!\n");
		return 1;
	}

	g_print("Successfully retrieved accessible-desc property: '%s'\n", desc);

	if (g_strcmp0(desc, "Create a new file") != 0)
	{
		g_printerr("Expected description 'Create a new file' but got '%s'!\n", desc);
		return 1;
	}

	g_print("Test PASSED!\n");
	return 0;
}
