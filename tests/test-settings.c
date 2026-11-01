/* Exercise the installed schema together with the module's consumers. */
#include "../src/support.h"
#include "../src/blacklist.h"
#include "../src/consts.h"

int main(int argc, char **argv)
{
    gtk_test_init(&argc, &argv, NULL);
    GSettings *settings = appmenu_settings();
    g_assert_nonnull(settings);
    g_assert_true(is_blacklisted("glade"));
    g_assert_true(is_blacklisted("mate-panel"));
    g_assert_false(is_blacklisted(NULL));
    const char *blacklist[] = { "settings-test", NULL };
    const char *whitelist[] = { "settings-test", "mate-panel", NULL };
    g_assert_true(g_settings_set_strv(settings, BLACKLIST_KEY, blacklist));
    g_assert_true(is_blacklisted("settings-test"));
    g_assert_false(is_blacklisted("glade"));
    g_assert_true(g_settings_set_strv(settings, WHITELIST_KEY, whitelist));
    g_assert_false(is_blacklisted("settings-test"));
    g_assert_false(is_blacklisted("mate-panel"));
    GtkWidget *bar = gtk_menu_bar_new();
    g_object_ref_sink(bar);
    g_object_set(gtk_widget_get_settings(bar), "gtk-shell-shows-menubar", TRUE, NULL);
    g_assert_true(gtk_widget_shell_shows_menubar(bar));
    g_assert_true(g_settings_set_boolean(settings, INNER_MENU_KEY, TRUE));
    g_assert_false(gtk_widget_shell_shows_menubar(bar));
    g_assert_true(g_settings_set_boolean(settings, INNER_MENU_KEY, FALSE));
    g_assert_true(gtk_widget_shell_shows_menubar(bar));
    /* Disabling Wayland must leave X11 applications enabled. */
    g_assert_true(g_settings_set_boolean(settings, RUN_ON_WAYLAND, FALSE));
    g_assert_true(gtk_module_should_run());
    g_assert_false(gtk_module_should_run());
    g_object_unref(bar);
    return 0;
}
