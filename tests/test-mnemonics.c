/* Exercise the private mnemonic handler together with real GTK and D-Bus state. */
#include <gio/gio.h>

static gboolean fail_session_bus, delay_session_bus;
static guint bus_requests;
static GList *delayed_requests;
static void test_bus_get(GBusType type, GCancellable *cancellable,
                         GAsyncReadyCallback callback, gpointer user_data);
static GDBusConnection *test_bus_get_finish(GAsyncResult *result, GError **error);
#define g_bus_get test_bus_get
#define g_bus_get_finish test_bus_get_finish
#include "../src/datastructs.c"
#undef g_bus_get
#undef g_bus_get_finish

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
        for (guint attempt = 0; attempt < 50 && data->published_server != shell->server; attempt++)
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

static void test_export_lifecycle(void)
{
    name("RequestName", "org.kde.kappmenuview");
    g_object_set(gtk_widget_get_settings(bar), "gtk-shell-shows-menubar", TRUE, NULL);
    /* Install the real realize/unrealize hooks: no manual disconnect in this test. */
    store_pre_hijacked();
    hijack_menu_bar_class_vtable(GTK_TYPE_MENU_BAR);
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
    fail_session_bus = TRUE;
    GtkWidget *test_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(test_window), box);
    GtkWidget *menubar = append_bar(box);
    gtk_widget_show_all(test_window);
    WindowData *data = gtk_window_peek_window_data(GTK_WINDOW(test_window));
    g_assert_nonnull(data);
    g_assert_false(data->x11_bus_initialized);
    g_assert_nonnull(gtk_menu_shell_get_menu_shell_data(GTK_MENU_SHELL(menubar))->server);
    drain();
    assert_export(test_window, NULL);
    g_assert_cmpuint(data->appmenu_retry_id, !=, 0);
    fail_session_bus = FALSE;
    /* No realize, connect, or explicit update after the bus becomes available. */
    assert_export(test_window, menubar);
    g_assert_true(data->x11_bus_initialized);
    gtk_widget_destroy(test_window);
    drain();
}

static gboolean heartbeat(gpointer data)
{
    *(gboolean *)data = TRUE;
    return G_SOURCE_REMOVE;
}

static void test_slow_bus_cancellation(void)
{
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

static void test_application_menubar(void)
{
    GtkSettings *settings = gtk_settings_get_default();
    g_object_set(settings, "gtk-shell-shows-menubar", FALSE, NULL);
    name("RequestName", "org.kde.kappmenu");
    name("RequestName", "org.kde.kappmenuview");
    gboolean shell_setting = TRUE;
    g_object_get(settings, "gtk-shell-shows-menubar", &shell_setting, NULL);
    g_assert_false(shell_setting);
    GtkApplication *app = gtk_application_new("org.example.AppmenuRegression", G_APPLICATION_NON_UNIQUE);
    GError *error = NULL;
    g_assert_true(g_application_register(G_APPLICATION(app), NULL, &error));
    g_assert_no_error(error);
    GSimpleAction *open = g_simple_action_new("open", NULL);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(open));
    g_object_unref(open);
    GMenu *model = g_menu_new(), *submenu = g_menu_new();
    g_menu_append(submenu, "Open", "app.open");
    g_menu_append_submenu(model, "_File", G_MENU_MODEL(submenu));
    gtk_application_set_menubar(app, G_MENU_MODEL(model));
    GtkWidget *test_window = gtk_application_window_new(app);
    gtk_container_add(GTK_CONTAINER(test_window), gtk_label_new("Application menu regression"));
    gtk_widget_show_all(test_window);
    drain();
    GtkWidget *menubar = NULL;
    find_menubar(test_window, &menubar);
    g_assert_nonnull(menubar);
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
    name("ReleaseName", "org.kde.kappmenuview");
    name("ReleaseName", "org.kde.kappmenu");
    drain();
    g_assert_false(gtk_widget_shell_shows_menubar(menubar));
    g_assert_cmpint(gtk_widget_get_allocated_height(menubar), >, 1);
    gtk_widget_destroy(test_window);
    g_object_unref(submenu);
    g_object_unref(model);
    g_object_unref(app);
    drain();
}

int main(int argc, char **argv)
{
    gtk_test_init(&argc, &argv, NULL);
    gchar *configured_modules = NULL;
    g_object_get(gtk_settings_get_default(), "gtk-modules", &configured_modules, NULL);
    g_assert_true(configured_modules == NULL || *configured_modules == '\0');
    g_free(configured_modules);
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
    g_test_add_func("/appmenu/initial-bus-failure", test_initial_bus_failure);
    g_test_add_func("/appmenu/slow-bus-cancellation", test_slow_bus_cancellation);
    g_test_add_func("/appmenu/application-menubar", test_application_menubar);
    int result = g_test_run();
    gtk_widget_destroy(window);
    g_object_unref(bus);
    return result;
}
