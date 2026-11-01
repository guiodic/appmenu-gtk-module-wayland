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

#include "blacklist.h"
#include "consts.h"

#include <stdlib.h>
#include <string.h>

/* BLACKLIST is sorted alphabetically to allow O(log N) binary search */
static const char *const BLACKLIST[] = {
	"acroread",
	"appmenu-mate",
	"budgie-panel",
	"emacs",
	"emacs23",
	"emacs23-lucid",
	"emacs24",
	"emacs24-lucid",
	"indicator-applet",
	"mate-indicator-applet",
	"mate-indicator-applet-appmenu",
	"mate-indicator-applet-complete",
	"mate-menu",
	"mate-panel",
	"vala-panel",
	"wrapper-1.0",
	"wrapper-2.0"
};

static int compare_strings(const void *key, const void *element)
{
	return strcmp(*(const char * const *)key, *(const char * const *)element);
}

static gboolean is_in_strv(gchar **list, const char *name)
{
	if (list == NULL || name == NULL)
		return FALSE;
	for (gint i = 0; list[i] != NULL; i++)
	{
		if (g_strcmp0(list[i], name) == 0)
			return TRUE;
	}
	return FALSE;
}

G_GNUC_INTERNAL
bool is_blacklisted(const char *name)
{
	if (name == NULL)
		return false;

	GSettingsSchemaSource *source = g_settings_schema_source_get_default();
	GSettingsSchema *schema = source != NULL ?
		g_settings_schema_source_lookup(source, UNITY_GTK_MODULE_SCHEMA, TRUE) : NULL;

	if (schema != NULL)
	{
		GSettings *settings = g_settings_new_full(schema, NULL, NULL);
		g_settings_schema_unref(schema);

		gchar **whitelist = g_settings_get_strv(settings, WHITELIST_KEY);
		gchar **blacklist = g_settings_get_strv(settings, BLACKLIST_KEY);
		g_object_unref(settings);

		if (is_in_strv(whitelist, name))
		{
			g_strfreev(whitelist);
			g_strfreev(blacklist);
			return false;
		}

		gboolean blacklisted = is_in_strv(blacklist, name);
		g_strfreev(whitelist);
		g_strfreev(blacklist);

		if (blacklisted)
			return true;
	}

	const char *key = name;
	const char *const *res = bsearch(&key,
	                                 BLACKLIST,
	                                 G_N_ELEMENTS(BLACKLIST),
	                                 sizeof(char *),
	                                 compare_strings);
	return res != NULL;
}
