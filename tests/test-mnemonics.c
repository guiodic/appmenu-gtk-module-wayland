/* Exercise the private mnemonic handler together with real GTK and D-Bus state. */
#include <gio/gio.h>
#include <libdbusmenu-gtk/parser.h>

static GtkWidget *failed_parse_widget;
static guint parse_failures;
static DbusmenuMenuitem *test_parse_menu_structure(GtkWidget *widget);

static gboolean fail_session_bus, delay_session_bus;
static guint bus_requests;
static GList *delayed_requests;
static void test_bus_get(GBusType type, GCancellable *cancellable,
                         GAsyncReadyCallback callback, gpointer user_data);
static GDBusConnection *test_bus_get_finish(GAsyncResult *result, GError **error);
#define dbusmenu_gtk_parse_menu_structure test_parse_menu_structure
#define g_bus_get test_bus_get
#define g_bus_get_finish test_bus_get_finish
#include "../src/datastructs.c"
#undef dbusmenu_gtk_parse_menu_structure
#undef g_bus_get
#undef g_bus_get_finish

static DbusmenuMenuitem *test_parse_menu_structure(GtkWidget *widget)
{
    if (widget == failed_parse_widget) {
        parse_failures++;
        return NULL;
    }
    return dbusmenu_gtk_parse_menu_structure(widget);
}

static void test_bus_get(GBusType type, GCancellable *cancellable,
                         GAsyncReadyCallback callback, gpointer user_data)
{
    bus_requests++;
    if (fail_session_bus || delay_session_bus) {
        GTask *task = g_task_new(NULL, cancellable, callback, user_data);
        g_task_set_source_tag(task, test_bus_get);
        if (delay_session_bus) {
            delayed_requests = g_list_append(delayed_requests, task);
        } else {
            g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED, "Injected bus failure");
            g_object_unref(task);
        }
    } else {
        g_bus_get(type, cancellable, callback, user_data);
    }
}

static GDBusConnection *test_bus_get_finish(GAsyncResult *result, GError **error)
{
    if (g_async_result_is_tagged(result, test_bus_get))
        return g_task_propagate_pointer(G_TASK(result), error);
    return g_bus_get_finish(result, error);
}
#include "../src/hijack.h"

/* Vfunc originals must be saved before the first replacement, never again. */
static void ensure_test_hooks(void)
{
    static gsize initialized;
    if (g_once_init_enter(&initialized)) {
        store_pre_hijacked();
        hijack_menu_bar_class_vtable(GTK_TYPE_MENU_BAR);
        g_once_init_leave(&initialized, 1);
    }
}

static GDBusConnection *bus;
static GtkWidget *window, *bar, *submenu_item, *leaf;
static guint requests, activations;
static DbusmenuMenuitem *last_requested;

static void drain(void)
{
    gint64 deadline = g_get_monotonic_time() + G_TIME_SPAN_SECOND / 10;
    do {
        while (g_main_context_iteration(NULL, FALSE)) {}
        g_usleep(1000);
    } while (g_get_monotonic_time() < deadline);
}

static void name(const char *method, const char *service)
{
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_sync(bus, "org.freedesktop.DBus",
        "/org/freedesktop/DBus", "org.freedesktop.DBus", method,
        g_str_equal(method, "RequestName") ? g_variant_new("(su)", service, 0u) :
                                            g_variant_new("(s)", service),
        G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(reply);
    g_variant_unref(reply);
    drain();
}

static void requested(DbusmenuMenuitem *item, guint timestamp, gpointer data)
{
    requests++;
    last_requested = item;
}

static void activated(GtkMenuItem *item, gpointer data)
{
    activations++;
}

static void test_mnemonics(void)
{
    requests = activations = 0;
    /* A registrar can hide the bar without providing a mnemonic receiver. */
    name("RequestName", "org.kde.kappmenu");
    g_assert_true(appmenu_registrar_present());
    g_assert_false(appmenu_mnemonic_receiver_present());
    g_assert_true(gtk_widget_shell_shows_menubar(bar));
    g_assert_false(forward_menu_mnemonic(submenu_item, FALSE, NULL));
    g_assert_cmpuint(requests, ==, 0);

    name("RequestName", "org.kde.kappmenuview");
    g_assert_true(appmenu_mnemonic_receiver_present());
    /* X11 has no kde_appmenu Wayland object, but must still forward. */
    g_assert_null(gtk_window_peek_window_data(GTK_WINDOW(window))->kde_appmenu);
    g_assert_true(gtk_widget_mnemonic_activate(submenu_item, FALSE));
    g_assert_cmpuint(requests, ==, 1);
    gtk_widget_set_sensitive(submenu_item, FALSE);
    g_assert_false(forward_menu_mnemonic(submenu_item, FALSE, NULL));
    gtk_widget_set_sensitive(submenu_item, TRUE);
    gtk_widget_hide(submenu_item);
    g_assert_false(forward_menu_mnemonic(submenu_item, FALSE, NULL));
    gtk_widget_show(submenu_item);

    /* Direct actions must retain GTK's default mnemonic activation. */
    g_assert_false(forward_menu_mnemonic(leaf, FALSE, NULL));
    gtk_widget_mnemonic_activate(leaf, FALSE);
    g_assert_cmpuint(activations, ==, 1);

    /* Later insertions get exactly one handler and use the current export. */
    GtkWidget *later = gtk_menu_item_new_with_mnemonic("_Later");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(later), gtk_menu_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), later);
    gtk_widget_show(later);
    drain();
    DbusmenuMenuitem *item = dbusmenu_gtk_parse_get_cached_item(later);
    g_assert_nonnull(item);
    g_signal_connect(item, DBUSMENU_MENUITEM_SIGNAL_SHOW_TO_USER, G_CALLBACK(requested), NULL);
    g_assert_cmpuint(g_signal_handler_find(later, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
        forward_menu_mnemonic, NULL), !=, 0);
    g_assert_true(gtk_widget_mnemonic_activate(later, FALSE));
    g_assert_cmpuint(requests, ==, 2);
    MenuShellData *data = gtk_menu_shell_get_menu_shell_data(GTK_MENU_SHELL(bar));
    DbusmenuMenuitem *original = NULL;
    g_object_get(data->server, DBUSMENU_SERVER_PROP_ROOT_NODE, &original, NULL);
    DbusmenuMenuitem *replacement = dbusmenu_menuitem_new();
    dbusmenu_server_set_root(data->server, replacement);
    g_object_unref(replacement);
    g_assert_false(forward_menu_mnemonic(later, FALSE, NULL));
    dbusmenu_server_set_root(data->server, original);
    g_object_unref(original);

    /* Viewer loss must restore fallback even while the registrar remains. */
    name("ReleaseName", "org.kde.kappmenuview");
    g_assert_false(appmenu_mnemonic_receiver_present());
    g_assert_true(appmenu_registrar_present());
    g_assert_false(forward_menu_mnemonic(submenu_item, FALSE, NULL));
    name("ReleaseName", "org.kde.kappmenu");
    name("RequestName", "org.kde.kappmenuview");
    g_assert_false(appmenu_registrar_present());
    g_assert_true(appmenu_mnemonic_receiver_present());
    /* A shell may advertise a global menu independently of the registrar. */
    g_object_set(gtk_widget_get_settings(bar), "gtk-shell-shows-menubar", TRUE, NULL);
    g_assert_true(forward_menu_mnemonic(submenu_item, FALSE, NULL));
    g_assert_cmpuint(requests, ==, 3);
    name("ReleaseName", "org.kde.kappmenuview");
}

static void test_duplicate_mnemonics(void)
{
    name("RequestName", "org.kde.kappmenuview");
    g_object_set(gtk_widget_get_settings(bar), "gtk-shell-shows-menubar", TRUE, NULL);
    GtkWidget *duplicate = gtk_menu_item_new_with_mnemonic("_Finestre");
    GtkWidget *menu = gtk_menu_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_menu_item_new_with_label("Window"));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(duplicate), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), duplicate);
    gtk_widget_show_all(duplicate);
    drain();
    DbusmenuMenuitem *item = dbusmenu_gtk_parse_get_cached_item(duplicate);
    g_assert_nonnull(item);
    g_signal_connect(item, DBUSMENU_MENUITEM_SIGNAL_SHOW_TO_USER, G_CALLBACK(requested), NULL);
    guint before = requests;
    /* Exercise GTK's real mnemonic hash, which marks these targets overloaded. */
    g_assert_true(gtk_window_mnemonic_activate(GTK_WINDOW(window), GDK_KEY_f, GDK_MOD1_MASK));
    DbusmenuMenuitem *first_requested = last_requested;
    g_assert_true(gtk_window_mnemonic_activate(GTK_WINDOW(window), GDK_KEY_f, GDK_MOD1_MASK));
    g_assert_cmpuint(requests, ==, before + 2);
    g_assert_true(last_requested != first_requested);
    g_assert_false(gtk_widget_get_mapped(menu));
    g_assert_false(gtk_widget_get_mapped(gtk_menu_item_get_submenu(GTK_MENU_ITEM(submenu_item))));
    gtk_widget_destroy(duplicate);
    name("ReleaseName", "org.kde.kappmenuview");
}

static void test_x11_export(void)
{
    Display *display = GDK_DISPLAY_XDISPLAY(gtk_widget_get_display(window));
    Window xid = GDK_WINDOW_XID(gtk_widget_get_window(window));
    const char *properties[] = { "_KDE_NET_WM_APPMENU_SERVICE_NAME", "_KDE_NET_WM_APPMENU_OBJECT_PATH" };
    for (guint i = 0; i < G_N_ELEMENTS(properties); i++) {
        Atom actual_type;
        int format;
        unsigned long count, remaining;
        unsigned char *value = NULL;
        g_assert_cmpint(XGetWindowProperty(display, xid,
            XInternAtom(display, properties[i], False), 0, 1024, False, AnyPropertyType,
            &actual_type, &format, &count, &remaining, &value), ==, Success);
        g_assert_cmpuint(actual_type, ==, XInternAtom(display, "STRING", False));
        g_assert_cmpint(format, ==, 8);
        g_assert_cmpuint(count, >, 0);
        if (i == 0)
            g_assert_cmpstr((char *)value, ==, g_dbus_connection_get_unique_name(bus));
        else
            g_assert_true(g_variant_is_object_path((char *)value));
        XFree(value);
    }
}

static void setting_changed(GObject *object, GParamSpec *pspec, gpointer data)
{
    (*(guint *)data)++;
}

static void test_viewer_settings(void)
{
    g_assert_false(appmenu_registrar_present());
    GtkSettings *settings = gtk_widget_get_settings(bar);
    for (guint initial = 0; initial < 2; initial++) {
        g_object_set(settings, "gtk-shell-shows-menubar", initial, NULL);
        guint notifications = 0;
        gulong handler = g_signal_connect(settings, "notify::gtk-shell-shows-menubar",
            G_CALLBACK(setting_changed), &notifications);
        name("RequestName", "org.kde.kappmenuview");
        g_assert_true(appmenu_mnemonic_receiver_present());
        gboolean current;
        g_object_get(settings, "gtk-shell-shows-menubar", &current, NULL);
        g_assert_cmpint(current, ==, initial);
        name("ReleaseName", "org.kde.kappmenuview");
        g_assert_false(appmenu_mnemonic_receiver_present());
        g_object_get(settings, "gtk-shell-shows-menubar", &current, NULL);
        g_assert_cmpint(current, ==, initial);
        g_assert_cmpuint(notifications, ==, 0);
        g_signal_handler_disconnect(settings, handler);
    }
}

static gchar *x11_property(GtkWidget *widget, const char *property)
{
    GdkWindow *gdk_window = gtk_widget_get_window(widget);
    Display *display = GDK_WINDOW_XDISPLAY(gdk_window);
    Atom type;
    int format;
    unsigned long count, remaining;
    unsigned char *value = NULL;
    g_assert_cmpint(XGetWindowProperty(display, GDK_WINDOW_XID(gdk_window),
        XInternAtom(display, property, False), 0, 1024, False, AnyPropertyType,
        &type, &format, &count, &remaining, &value), ==, Success);
    gchar *result = type == None ? NULL : g_strndup((char *)value, count);
    if (value) XFree(value);
    return result;
}

static void assert_export(GtkWidget *widget, GtkWidget *menubar)
{
    if (menubar) {
        WindowData *data = gtk_window_peek_window_data(GTK_WINDOW(widget));
        MenuShellData *shell = gtk_menu_shell_get_menu_shell_data(GTK_MENU_SHELL(menubar));
        for (guint attempt = 0; attempt < 50 && (shell->server == NULL || data->published_server != shell->server); attempt++)
            drain();
    }
    gchar *service = x11_property(widget, "_KDE_NET_WM_APPMENU_SERVICE_NAME");
    gchar *path = x11_property(widget, "_KDE_NET_WM_APPMENU_OBJECT_PATH");
    if (menubar) {
        MenuShellData *data = gtk_menu_shell_get_menu_shell_data(GTK_MENU_SHELL(menubar));
        g_assert_nonnull(data->server);
        gchar *expected = NULL;
        g_object_get(data->server, DBUSMENU_SERVER_PROP_DBUS_OBJECT, &expected, NULL);
        g_assert_cmpstr(service, ==, g_dbus_connection_get_unique_name(bus));
        g_assert_cmpstr(path, ==, expected);
        g_free(expected);
    } else {
        g_assert_null(service);
        g_assert_null(path);
    }
    g_free(service);
    g_free(path);
}

static GtkWidget *append_bar(GtkWidget *box)
{
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *item = gtk_menu_item_new_with_label("File");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), gtk_menu_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), item);
    gtk_box_pack_start(GTK_BOX(box), menubar, FALSE, FALSE, 0);
    gtk_widget_show_all(menubar);
    return menubar;
}

static void set_native_menu_path(GtkWidget *widget, const char *property, const char *value)
{
    GdkWindow *win = gtk_widget_get_window(widget);
    Display *display = GDK_WINDOW_XDISPLAY(win);
    Atom atom = XInternAtom(display, property, False);
    if (value)
        XChangeProperty(display, GDK_WINDOW_XID(win), atom,
            XInternAtom(display, "UTF8_STRING", False), 8, PropModeReplace,
            (const unsigned char *)value, strlen(value));
    else
        XDeleteProperty(display, GDK_WINDOW_XID(win), atom);
}

static void assert_native_menu_path(GtkWidget *widget, const char *property, const char *expected)
{
    gchar *actual = x11_property(widget, property);
    g_assert_cmpstr(actual, ==, expected);
    g_free(actual);
}

static void test_native_menu_path_release(void)
{
    ensure_test_hooks();
    /* Preserve each property's value or absence independently. */
    for (guint mask = 0; mask < 4; mask++) {
        GtkWidget *test_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add(GTK_CONTAINER(test_window), box);
        gtk_widget_show_all(test_window);
        const char *menu_path = mask & 1 ? "/original/menubar" : NULL;
        const char *app_path = mask & 2 ? "/original/appmenu" : NULL;
        set_native_menu_path(test_window, "_GTK_MENUBAR_OBJECT_PATH", menu_path);
        set_native_menu_path(test_window, "_GTK_APP_MENU_OBJECT_PATH", app_path);
        GtkWidget *menubar = append_bar(box);
        drain();
        WindowData *data = gtk_window_peek_window_data(GTK_WINDOW(test_window));
        assert_export(test_window, menubar);
        assert_native_menu_path(test_window, "_GTK_MENUBAR_OBJECT_PATH", NULL);
        assert_native_menu_path(test_window, "_GTK_APP_MENU_OBJECT_PATH", NULL);
        gtk_widget_destroy(menubar);
        g_assert_cmpuint(data->x11_restore_menu_paths_id, !=, 0);
        /* A replacement in the same transaction must not reveal GMenu. */
        assert_native_menu_path(test_window, "_GTK_MENUBAR_OBJECT_PATH", NULL);
        menubar = append_bar(box);
        g_assert_cmpuint(data->x11_restore_menu_paths_id, ==, 0);
        drain();
        assert_export(test_window, menubar);
        assert_native_menu_path(test_window, "_GTK_MENUBAR_OBJECT_PATH", NULL);
        assert_native_menu_path(test_window, "_GTK_APP_MENU_OBJECT_PATH", NULL);
        gtk_widget_destroy(menubar);
        drain();
        assert_export(test_window, NULL);
        assert_native_menu_path(test_window, "_GTK_MENUBAR_OBJECT_PATH", menu_path);
        assert_native_menu_path(test_window, "_GTK_APP_MENU_OBJECT_PATH", app_path);
        /* A later ownership cycle takes a new snapshot, not stale values. */
        set_native_menu_path(test_window, "_GTK_MENUBAR_OBJECT_PATH", "/updated/menubar");
        menubar = append_bar(box);
        drain();
        gtk_widget_destroy(menubar);
        drain();
        assert_native_menu_path(test_window, "_GTK_MENUBAR_OBJECT_PATH", "/updated/menubar");
        menubar = append_bar(box);
        gtk_widget_destroy(menubar);
        g_assert_cmpuint(data->x11_restore_menu_paths_id, !=, 0);
        gtk_widget_destroy(test_window); /* Cancels the pending weak callback. */
        drain();
    }
}

static void assert_mnemonic_forwarded(GtkWidget *menubar, gboolean expected)
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(menubar));
    g_assert_nonnull(children);
    GtkWidget *item = children->data;
    DbusmenuMenuitem *exported = dbusmenu_gtk_parse_get_cached_item(item);
    g_assert_nonnull(exported);
    guint before = requests;
    gulong handler = g_signal_connect(exported, DBUSMENU_MENUITEM_SIGNAL_SHOW_TO_USER,
        G_CALLBACK(requested), NULL);
    g_assert_cmpint(forward_menu_mnemonic(item, FALSE, NULL), ==, expected);
    g_assert_cmpuint(requests - before, ==, expected ? 1 : 0);
    g_signal_handler_disconnect(exported, handler);
    g_list_free(children);
}

static GtkWidget *append_unparsed_bar(GtkWidget *box)
{
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *item = gtk_menu_item_new_with_mnemonic("_Broken");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), gtk_menu_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), item);
    failed_parse_widget = menubar;
    gtk_box_pack_start(GTK_BOX(box), menubar, FALSE, FALSE, 0);
    gtk_widget_show_all(menubar);
    failed_parse_widget = NULL;
    return menubar;
}

static void test_parse_failure(void)
{
    ensure_test_hooks();
    name("RequestName", "org.kde.kappmenu");
    name("RequestName", "org.kde.kappmenuview");
    GtkWidget *test_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(test_window), box);
    gtk_widget_show_all(test_window);
    set_native_menu_path(test_window, "_GTK_MENUBAR_OBJECT_PATH", "/native/menubar");
    guint before = bus_requests, failures = parse_failures;
    GtkWidget *invalid = append_unparsed_bar(box);
    drain();
    g_assert_cmpuint(parse_failures, >, failures);
    WindowData *data = gtk_window_peek_window_data(GTK_WINDOW(test_window));
    g_assert_null(data->menus);
    g_assert_null(data->dbusmenu_servers);
    g_assert_null(data->appmenu_request);
    g_assert_cmpuint(bus_requests, ==, before);
    assert_export(test_window, NULL);
    assert_native_menu_path(test_window, "_GTK_MENUBAR_OBJECT_PATH", "/native/menubar");
    gtk_widget_destroy(invalid);
    GtkWidget *valid = append_bar(box);
    drain();
    assert_export(test_window, valid);
    before = bus_requests;
    invalid = append_unparsed_bar(box);
    /* Rejection must not withdraw or replace the already published menu. */
    assert_export(test_window, valid);
    g_assert_cmpuint(bus_requests, ==, before);
    g_assert_cmpuint(g_slist_length(data->menus), ==, 1);
    assert_mnemonic_forwarded(valid, TRUE);
    /* An existing server has a NULL pending_root and must remain usable. */
    update_window_appmenu(GTK_WINDOW(test_window), data);
    assert_export(test_window, valid);
    /* A later explicit connection can retry parsing the rejected bar. */
    gtk_window_connect_menu_shell(GTK_WINDOW(test_window), GTK_MENU_SHELL(invalid));
    assert_export(test_window, invalid);
    assert_mnemonic_forwarded(invalid, TRUE);
    gtk_widget_destroy(invalid);
    assert_export(test_window, valid);
    gtk_widget_destroy(test_window);
    name("ReleaseName", "org.kde.kappmenuview");
    name("ReleaseName", "org.kde.kappmenu");
    drain();
}

static void test_export_lifecycle(void)
{
    name("RequestName", "org.kde.kappmenuview");
    g_object_set(gtk_widget_get_settings(bar), "gtk-shell-shows-menubar", TRUE, NULL);
    /* Install the real realize/unrealize hooks: no manual disconnect in this test. */
    ensure_test_hooks();
    GtkWidget *test_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(test_window), box);
    GtkWidget *first = append_bar(box);
    GtkWidget *second = append_bar(box);
    gtk_widget_show_all(test_window);
    drain();
    assert_export(test_window, second);
    assert_mnemonic_forwarded(first, FALSE);
    assert_mnemonic_forwarded(second, TRUE);
    /* A server retained by a bar is not enough if it is absent from the list. */
    WindowData *window_data = gtk_window_peek_window_data(GTK_WINDOW(test_window));
    GSList *saved = window_data->dbusmenu_servers;
    window_data->dbusmenu_servers = NULL;
    assert_mnemonic_forwarded(second, FALSE);
    window_data->dbusmenu_servers = saved;
    /* Removing a non-published menu must preserve the newest export. */
    gtk_widget_destroy(first);
    assert_export(test_window, second);
    GtkWidget *replacement = append_bar(box);
    drain();
    assert_export(test_window, replacement);
    assert_mnemonic_forwarded(second, FALSE);
    assert_mnemonic_forwarded(replacement, TRUE);
    /* Removing the published menu restores its surviving predecessor. */
    gtk_widget_destroy(replacement);
    assert_export(test_window, second);
    assert_mnemonic_forwarded(second, TRUE);
    gtk_widget_destroy(second);
    assert_export(test_window, NULL);
    GtkWidget *fresh = append_bar(box);
    drain();
    assert_export(test_window, fresh);
    gtk_widget_unrealize(fresh);
    assert_export(test_window, NULL);
    assert_mnemonic_forwarded(fresh, FALSE);
    gtk_widget_realize(fresh);
    assert_export(test_window, fresh);
    assert_mnemonic_forwarded(fresh, TRUE);
    /* Async failures withdraw the export, retry, and back off without blocking. */
    WindowData *fresh_data = gtk_window_peek_window_data(GTK_WINDOW(test_window));
    fail_session_bus = TRUE;
    update_window_appmenu(GTK_WINDOW(test_window), fresh_data);
    g_assert_nonnull(fresh_data->appmenu_request);
    assert_export(test_window, NULL);
    assert_mnemonic_forwarded(fresh, FALSE);
    drain();
    g_assert_cmpuint(fresh_data->appmenu_retry_id, !=, 0);
    g_assert_cmpuint(fresh_data->appmenu_retry_delay, ==, 2);
    guint previous_retry = fresh_data->appmenu_retry_id;
    /* Force the next attempt; the GTK loop must continue processing. */
    update_window_appmenu(GTK_WINDOW(test_window), fresh_data);
    g_assert_null(g_main_context_find_source_by_id(NULL, previous_retry));
    drain();
    g_assert_cmpuint(fresh_data->appmenu_retry_delay, ==, 4);
    for (guint attempt = 0; attempt < 5; attempt++) {
        update_window_appmenu(GTK_WINDOW(test_window), fresh_data);
        drain();
    }
    g_assert_cmpuint(fresh_data->appmenu_retry_delay, ==, 30);
    /* Return to a short retry to keep this integration test bounded. */
    fresh_data->appmenu_retry_delay = 1;
    update_window_appmenu(GTK_WINDOW(test_window), fresh_data);
    drain();
    fail_session_bus = FALSE;
    assert_export(test_window, fresh);
    assert_mnemonic_forwarded(fresh, TRUE);
    g_assert_cmpuint(fresh_data->appmenu_retry_id, ==, 0);
    g_assert_cmpuint(fresh_data->appmenu_retry_delay, ==, 0);
    fail_session_bus = TRUE;
    update_window_appmenu(GTK_WINDOW(test_window), fresh_data);
    drain();
    guint pending_retry = fresh_data->appmenu_retry_id;
    gtk_widget_unrealize(fresh);
    g_assert_null(g_main_context_find_source_by_id(NULL, pending_retry));
    gtk_widget_realize(fresh);
    drain();
    pending_retry = fresh_data->appmenu_retry_id;
    g_assert_cmpuint(pending_retry, !=, 0);
    gtk_widget_destroy(test_window);
    fail_session_bus = FALSE;
    drain();
    g_assert_null(g_main_context_find_source_by_id(NULL, pending_retry));
    name("ReleaseName", "org.kde.kappmenuview");
}

static void complete_delayed_requests(void)
{
    GList *requests = delayed_requests;
    delayed_requests = NULL;
    for (GList *iter = requests; iter; iter = iter->next) {
        g_task_return_pointer(iter->data, g_object_ref(bus), g_object_unref);
        g_object_unref(iter->data);
    }
    g_list_free(requests);
    drain();
}

static void test_initial_bus_failure(void)
{
    ensure_test_hooks();
    fail_session_bus = TRUE;
    GtkWidget *test_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(test_window), box);
    GtkWidget *menubar = append_bar(box);
    gtk_widget_show_all(test_window);
    WindowData *data = gtk_window_peek_window_data(GTK_WINDOW(test_window));
    g_assert_nonnull(data);
    g_assert_null(gtk_menu_shell_get_menu_shell_data(GTK_MENU_SHELL(menubar))->server);
    g_assert_nonnull(gtk_menu_shell_get_menu_shell_data(GTK_MENU_SHELL(menubar))->pending_root);
    drain();
    assert_export(test_window, NULL);
    g_assert_cmpuint(data->appmenu_retry_id, !=, 0);
    fail_session_bus = FALSE;
    /* No realize, connect, or explicit update after the bus becomes available. */
    assert_export(test_window, menubar);
    gtk_widget_destroy(test_window);
    drain();
}

typedef struct {
    gboolean done;
    GVariant *reply;
    GError *error;
} LayoutReply;

static void layout_received(GObject *source, GAsyncResult *result, gpointer pointer)
{
    LayoutReply *reply = pointer;
    reply->reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &reply->error);
    reply->done = TRUE;
}

static void test_real_initial_bus_failure(void)
{
    /* A fresh process has no cached session connection. Both this module and
     * the unmodified libdbusmenu must encounter the real connection failure. */
    gchar *healthy_address = g_strdup(g_getenv("DBUS_SESSION_BUS_ADDRESS"));
    g_assert_nonnull(healthy_address);
    g_setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent/appmenu-test-bus", TRUE);
    GError *error = NULL;
    GDBusConnection *unavailable = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    g_assert_null(unavailable);
    g_assert_nonnull(error);
    g_clear_error(&error);
    ensure_test_hooks();
    watch_registrar_dbus();
    /* Prove that the fault reaches libdbusmenu, outside our test wrappers. */
    g_test_expect_message("LIBDBUSMENU-GLIB", G_LOG_LEVEL_WARNING, "Unable to get session bus:*");
    DbusmenuServer *probe = dbusmenu_server_new("/InitialFailureProbe");
    drain();
    g_test_assert_expected_messages();
    g_object_unref(probe);
    GtkWidget *test_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(test_window), box);
    GtkWidget *menubar = append_bar(box);
    GList *top_items = gtk_container_get_children(GTK_CONTAINER(menubar));
    GtkWidget *file = top_items->data;
    gtk_menu_item_set_label(GTK_MENU_ITEM(file), "_File");
    gtk_menu_item_set_use_underline(GTK_MENU_ITEM(file), TRUE);
    g_list_free(top_items);
    gtk_widget_show_all(test_window);
    drain();
    /* Module-owned servers must not attempt this unrecoverable acquisition. */
    g_assert_null(gtk_menu_shell_get_menu_shell_data(GTK_MENU_SHELL(menubar))->server);
    WindowData *data = gtk_window_peek_window_data(GTK_WINDOW(test_window));
    g_assert_nonnull(data);
    g_assert_null(data->published_server);
    g_assert_cmpuint(data->appmenu_retry_id, !=, 0);
    g_assert_false(appmenu_registrar_present());
    g_assert_false(appmenu_mnemonic_receiver_present());
    /* The pending model must still track changes made before registration. */
    GtkWidget *extra = gtk_menu_item_new_with_mnemonic("_Extra");
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), extra);
    gtk_widget_show(extra);
    drain();
    g_setenv("DBUS_SESSION_BUS_ADDRESS", healthy_address, TRUE);
    g_free(healthy_address);
    bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(bus);
    name("RequestName", "org.kde.kappmenu");
    name("RequestName", "org.kde.kappmenuview");
    assert_export(test_window, menubar);
    gint64 names_deadline = g_get_monotonic_time() + 5 * G_TIME_SPAN_SECOND;
    while ((!appmenu_registrar_present() || !appmenu_mnemonic_receiver_present()) &&
           g_get_monotonic_time() < names_deadline)
        drain();
    g_assert_true(appmenu_registrar_present());
    g_assert_true(appmenu_mnemonic_receiver_present());
    DbusmenuMenuitem *exported_file = dbusmenu_gtk_parse_get_cached_item(file);
    g_assert_nonnull(exported_file);
    gulong handler = g_signal_connect(exported_file, DBUSMENU_MENUITEM_SIGNAL_SHOW_TO_USER,
        G_CALLBACK(requested), NULL);
    guint before = requests;
    g_assert_true(gtk_window_mnemonic_activate(GTK_WINDOW(test_window), GDK_KEY_f, GDK_MOD1_MASK));
    g_assert_cmpuint(requests, ==, before + 1);
    g_signal_handler_disconnect(exported_file, handler);
    /* An absent name on a healthy bus must remain watched. */
    name("ReleaseName", "org.kde.kappmenuview");
    names_deadline = g_get_monotonic_time() + 5 * G_TIME_SPAN_SECOND;
    while (appmenu_mnemonic_receiver_present() &&
           g_get_monotonic_time() < names_deadline)
        drain();
    g_assert_false(appmenu_mnemonic_receiver_present());
    g_assert_true(appmenu_registrar_present());
    name("RequestName", "org.kde.kappmenuview");
    names_deadline = g_get_monotonic_time() + 5 * G_TIME_SPAN_SECOND;
    while (!appmenu_mnemonic_receiver_present() &&
           g_get_monotonic_time() < names_deadline)
        drain();
    g_assert_true(appmenu_mnemonic_receiver_present());
    gchar *service = x11_property(test_window, "_KDE_NET_WM_APPMENU_SERVICE_NAME");
    gchar *path = x11_property(test_window, "_KDE_NET_WM_APPMENU_OBJECT_PATH");
    LayoutReply reply = { 0 };
    g_dbus_connection_call(bus, service, path, "com.canonical.dbusmenu", "GetLayout",
        g_variant_new("(ii@as)", 0, -1, g_variant_new_strv(NULL, 0)),
        G_VARIANT_TYPE("(u(ia{sv}av))"), G_DBUS_CALL_FLAGS_NONE, 3000, NULL,
        layout_received, &reply);
    gint64 deadline = g_get_monotonic_time() + 5 * G_TIME_SPAN_SECOND;
    while (!reply.done && g_get_monotonic_time() < deadline)
        drain();
    g_assert_true(reply.done);
    g_assert_no_error(reply.error);
    g_assert_nonnull(reply.reply);
    GVariant *layout = g_variant_get_child_value(reply.reply, 1);
    GVariant *children = g_variant_get_child_value(layout, 2);
    g_assert_cmpuint(g_variant_n_children(children), ==, 2);
    g_variant_unref(children);
    g_variant_unref(layout);
    g_variant_unref(reply.reply);
    g_free(path);
    g_free(service);
    gtk_widget_destroy(test_window);
    name("ReleaseName", "org.kde.kappmenuview");
    name("ReleaseName", "org.kde.kappmenu");
    drain();
    g_object_unref(bus);
}

static gboolean heartbeat(gpointer data)
{
    *(gboolean *)data = TRUE;
    return G_SOURCE_REMOVE;
}

static void test_slow_bus_cancellation(void)
{
    ensure_test_hooks();
    delay_session_bus = TRUE;
    guint before = bus_requests;
    GtkWidget *test_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(test_window), box);
    GtkWidget *first = append_bar(box);
    gtk_widget_show_all(test_window);
    WindowData *data = gtk_window_peek_window_data(GTK_WINDOW(test_window));
    g_assert_cmpuint(bus_requests, ==, before + 1);
    GCancellable *original = g_object_ref(data->appmenu_request);
    gboolean ticked = FALSE;
    g_idle_add(heartbeat, &ticked);
    drain();
    g_assert_true(ticked);
    g_assert_null(data->published_server);
    /* A slow reply must not trigger overlapping timer-based requests. */
    g_assert_cmpuint(bus_requests, ==, before + 1);
    g_assert_cmpuint(data->appmenu_retry_id, ==, 0);
    GtkWidget *second = append_bar(box);
    g_assert_true(g_cancellable_is_cancelled(original));
    g_object_unref(original);
    gtk_widget_destroy(second);
    delay_session_bus = FALSE;
    complete_delayed_requests();
    assert_export(test_window, first);
    g_assert_cmpuint(data->appmenu_retry_id, ==, 0);
    /* A reply after destruction must neither retain nor resurrect the window. */
    delay_session_bus = TRUE;
    update_window_appmenu(GTK_WINDOW(test_window), data);
    GCancellable *pending = g_object_ref(data->appmenu_request);
    GtkWidget *weak_window = test_window;
    g_object_add_weak_pointer(G_OBJECT(test_window), (gpointer *)&weak_window);
    gtk_widget_destroy(test_window);
    g_assert_true(g_cancellable_is_cancelled(pending));
    g_object_unref(pending);
    g_assert_null(weak_window);
    delay_session_bus = FALSE;
    complete_delayed_requests();
    g_assert_null(delayed_requests);
}

static void find_menubar(GtkWidget *widget, gpointer data)
{
    if (GTK_IS_MENU_BAR(widget))
        *(GtkWidget **)data = widget;
    else if (GTK_IS_CONTAINER(widget))
        gtk_container_forall(GTK_CONTAINER(widget), find_menubar, data);
}

static void action_invoked(GSimpleAction *action, GVariant *parameter, gpointer data)
{
    (*(guint *)data)++;
}

static void test_application_menubar(void)
{
    ensure_test_hooks();
    GtkApplication *app = gtk_application_new("org.example.AppmenuRegression", G_APPLICATION_NON_UNIQUE);
    GError *error = NULL;
    g_assert_true(g_application_register(G_APPLICATION(app), NULL, &error));
    g_assert_no_error(error);
    for (gboolean desktop_exports = FALSE; desktop_exports <= TRUE; desktop_exports++) {
        GtkSettings *settings = gtk_settings_get_default();
        g_object_set(settings, "gtk-shell-shows-menubar", desktop_exports, NULL);
        name("RequestName", "org.kde.kappmenu");
        name("RequestName", "org.kde.kappmenuview");
        gboolean shell_setting = TRUE;
        g_object_get(settings, "gtk-shell-shows-menubar", &shell_setting, NULL);
        g_assert_cmpint(shell_setting, ==, desktop_exports);
        guint invoked = 0;
        GSimpleAction *open = g_simple_action_new("open", NULL);
        g_signal_connect(open, "activate", G_CALLBACK(action_invoked), &invoked);
        g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(open));
        g_object_unref(open);
        GMenu *model = g_menu_new(), *submenu = g_menu_new();
        g_menu_append(submenu, "Open", "app.open");
        g_menu_append_submenu(model, "_File", G_MENU_MODEL(submenu));
        gtk_application_set_menubar(app, G_MENU_MODEL(model));
        /* A window without a module-owned bar keeps its native GMenu export. */
        GtkWidget *native_window = gtk_application_window_new(app);
        gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(native_window), FALSE);
        gtk_widget_show_all(native_window);
        drain();
        gchar *native_path = x11_property(native_window, "_GTK_MENUBAR_OBJECT_PATH");
        g_assert_nonnull(native_path);
        g_free(native_path);
        WindowData *native_data = gtk_window_peek_window_data(GTK_WINDOW(native_window));
        g_assert_null(native_data->dbusmenu_servers);
        gtk_widget_destroy(native_window);
        GtkWidget *test_window = gtk_application_window_new(app);
        gtk_container_add(GTK_CONTAINER(test_window), gtk_label_new("Application menu regression"));
        gtk_widget_show_all(test_window);
        drain();
        GtkWidget *menubar = NULL;
        find_menubar(test_window, &menubar);
        if (desktop_exports) {
            g_assert_null(menubar); /* GTK itself omitted its bar. */
            WindowData *data = gtk_window_peek_window_data(GTK_WINDOW(test_window));
            g_assert_nonnull(data->menus);
            menubar = data->menus->data;
            g_object_get(settings, "gtk-shell-shows-menubar", &shell_setting, NULL);
            g_assert_true(shell_setting);
        }
        g_assert_nonnull(menubar);
        g_assert_null(x11_property(test_window, "_GTK_MENUBAR_OBJECT_PATH"));
        g_assert_null(x11_property(test_window, "_GTK_APP_MENU_OBJECT_PATH"));
        assert_export(test_window, menubar);
        GList *children = gtk_container_get_children(GTK_CONTAINER(menubar));
        g_assert_nonnull(children);
        DbusmenuMenuitem *item = dbusmenu_gtk_parse_get_cached_item(children->data);
        guint before = requests;
        gulong handler = g_signal_connect(item, DBUSMENU_MENUITEM_SIGNAL_SHOW_TO_USER,
            G_CALLBACK(requested), NULL);
        g_assert_true(gtk_window_mnemonic_activate(GTK_WINDOW(test_window), GDK_KEY_f, GDK_MOD1_MASK));
        g_assert_cmpuint(requests, ==, before + 1);
        g_signal_handler_disconnect(item, handler);
        g_list_free(children);
        g_assert_cmpint(gtk_widget_get_allocated_height(menubar), <=, 1);
        /* Follow the exported event all the way through to the GAction. */
        DbusmenuMenuitem *open_item = dbusmenu_menuitem_get_children(item)->data;
        dbusmenu_menuitem_handle_event(open_item, "clicked", NULL, GDK_CURRENT_TIME);
        drain();
        g_assert_cmpuint(invoked, ==, 1);
        name("ReleaseName", "org.kde.kappmenuview");
        name("ReleaseName", "org.kde.kappmenu");
        drain();
        if (!desktop_exports) {
            g_assert_false(gtk_widget_shell_shows_menubar(menubar));
            g_assert_cmpint(gtk_widget_get_allocated_height(menubar), >, 1);
        }
        if (desktop_exports) {
            /* Updates must keep actions live and never leave a stale export. */
            name("RequestName", "org.kde.kappmenu");
            name("RequestName", "org.kde.kappmenuview");
            /* Application assignment can change after the window is realized. */
            g_object_ref(test_window);
            gtk_window_set_application(GTK_WINDOW(test_window), NULL);
            drain();
            g_assert_null(gtk_window_peek_window_data(GTK_WINDOW(test_window))->menus);
            gtk_window_set_application(GTK_WINDOW(test_window), app);
            g_object_unref(test_window);
            drain();
            WindowData *reassigned = gtk_window_peek_window_data(GTK_WINDOW(test_window));
            g_assert_cmpuint(g_slist_length(reassigned->menus), ==, 1);
            assert_export(test_window, reassigned->menus->data);
            assert_mnemonic_forwarded(reassigned->menus->data, TRUE);
            GMenu *replacement = g_menu_new();
            g_menu_append_submenu(replacement, "_View", G_MENU_MODEL(submenu));
            gtk_application_set_menubar(app, G_MENU_MODEL(replacement));
            drain();
            WindowData *data = gtk_window_peek_window_data(GTK_WINDOW(test_window));
            g_assert_cmpuint(g_slist_length(data->menus), ==, 1);
            menubar = data->menus->data;
            assert_export(test_window, menubar);
            assert_mnemonic_forwarded(menubar, TRUE);
            gtk_widget_hide(test_window);
            gtk_widget_unrealize(test_window);
            gtk_widget_show_all(test_window);
            drain();
            data = gtk_window_peek_window_data(GTK_WINDOW(test_window));
            g_assert_cmpuint(g_slist_length(data->menus), ==, 1);
            menubar = data->menus->data;
            assert_export(test_window, menubar);
            assert_mnemonic_forwarded(menubar, TRUE);
            /* Switch between GTK's real bar and the private bar at runtime. */
            g_object_set(settings, "gtk-shell-shows-menubar", FALSE, NULL);
            drain();
            data = gtk_window_peek_window_data(GTK_WINDOW(test_window));
            g_assert_cmpuint(g_slist_length(data->menus), ==, 1);
            menubar = data->menus->data;
            assert_export(test_window, menubar);
            g_object_set(settings, "gtk-shell-shows-menubar", TRUE, NULL);
            drain();
            g_assert_cmpuint(g_slist_length(data->menus), ==, 1);
            menubar = data->menus->data;
            assert_export(test_window, menubar);
            assert_mnemonic_forwarded(menubar, TRUE);
            gchar *saved_path = g_strdup(data->x11_native_menubar_path);
            g_assert_nonnull(saved_path);
            gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(test_window), FALSE);
            assert_native_menu_path(test_window, "_GTK_MENUBAR_OBJECT_PATH", NULL);
            drain();
            assert_native_menu_path(test_window, "_GTK_MENUBAR_OBJECT_PATH", saved_path);
            g_free(saved_path);
            g_assert_null(data->menus);
            assert_export(test_window, NULL);
            gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(test_window), TRUE);
            drain();
            g_assert_cmpuint(g_slist_length(data->menus), ==, 1);
            assert_export(test_window, data->menus->data);
            GMenu *application_menu = g_menu_new();
            g_menu_append(application_menu, "About", "app.open");
            gtk_application_set_app_menu(app, G_MENU_MODEL(application_menu));
            drain();
            /* Export the application menu too, before hiding its native path. */
            menubar = g_slist_last(data->menus)->data;
            assert_export(test_window, menubar);
            GList *top = gtk_container_get_children(GTK_CONTAINER(menubar));
            g_assert_cmpuint(g_list_length(top), ==, 2);
            DbusmenuMenuitem *application_item = dbusmenu_gtk_parse_get_cached_item(top->data);
            DbusmenuMenuitem *about = dbusmenu_menuitem_get_children(application_item)->data;
            guint before_about = invoked;
            dbusmenu_menuitem_handle_event(about, "clicked", NULL, GDK_CURRENT_TIME);
            drain();
            g_assert_cmpuint(invoked, ==, before_about + 1);
            g_list_free(top);
            gtk_application_set_app_menu(app, NULL);
            g_object_unref(application_menu);
            drain();
            g_assert_cmpuint(g_slist_length(data->menus), ==, 1);
            name("ReleaseName", "org.kde.kappmenuview");
            name("ReleaseName", "org.kde.kappmenu");
            g_object_unref(replacement);
        }
        gtk_widget_destroy(test_window);
        gtk_application_set_menubar(app, NULL);
        g_object_unref(submenu);
        g_object_unref(model);
        drain();
    }
    g_object_unref(app);
}

int main(int argc, char **argv)
{
    gboolean real_bus_failure = argc == 2 && g_str_equal(argv[1], "--real-bus-failure");
    if (real_bus_failure) {
        argc = 1;
        argv[1] = NULL;
    }
    gtk_test_init(&argc, &argv, NULL);
    gchar *configured_modules = NULL;
    g_object_get(gtk_settings_get_default(), "gtk-modules", &configured_modules, NULL);
    g_assert_true(configured_modules == NULL || *configured_modules == '\0');
    g_free(configured_modules);
    if (real_bus_failure) {
        g_test_add_func("/appmenu/real-initial-bus-failure", test_real_initial_bus_failure);
        return g_test_run();
    }
    bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    g_assert_nonnull(bus);
    watch_registrar_dbus();
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    bar = gtk_menu_bar_new();
    submenu_item = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *menu = gtk_menu_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_menu_item_new_with_label("Open"));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(submenu_item), menu);
    leaf = gtk_menu_item_new_with_mnemonic("_Action");
    g_signal_connect(leaf, "activate", G_CALLBACK(activated), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), submenu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), leaf);
    gtk_container_add(GTK_CONTAINER(window), bar);
    gtk_widget_show_all(window);
    gtk_window_connect_menu_shell(GTK_WINDOW(window), GTK_MENU_SHELL(bar));
    drain();
    DbusmenuMenuitem *item = dbusmenu_gtk_parse_get_cached_item(submenu_item);
    g_assert_nonnull(item);
    g_signal_connect(item, DBUSMENU_MENUITEM_SIGNAL_SHOW_TO_USER, G_CALLBACK(requested), NULL);
    g_test_add_func("/appmenu/x11-export", test_x11_export);
    g_test_add_func("/appmenu/duplicate-mnemonics", test_duplicate_mnemonics);
    g_test_add_func("/appmenu/mnemonics", test_mnemonics);
    g_test_add_func("/appmenu/viewer-settings", test_viewer_settings);
    g_test_add_func("/appmenu/export-lifecycle", test_export_lifecycle);
    g_test_add_func("/appmenu/parse-failure", test_parse_failure);
    g_test_add_func("/appmenu/initial-bus-failure", test_initial_bus_failure);
    g_test_add_func("/appmenu/slow-bus-cancellation", test_slow_bus_cancellation);
    g_test_add_func("/appmenu/application-menubar", test_application_menubar);
    g_test_add_func("/appmenu/native-menu-path-release", test_native_menu_path_release);
    int result = g_test_run();
    gtk_widget_destroy(window);
    g_object_unref(bus);
    return result;
}
