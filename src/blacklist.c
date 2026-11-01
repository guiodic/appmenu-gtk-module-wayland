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
#include "support.h"

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

G_GNUC_INTERNAL
bool is_blacklisted(const char *name)
{
	if (name == NULL)
		return false;

	GSettings *settings = appmenu_settings();
	if (settings != NULL)
	{
		gchar **list = g_settings_get_strv(settings, WHITELIST_KEY);
		gboolean listed = g_strv_contains((const gchar *const *)list, name);
		g_strfreev(list);
		if (listed)
			return false;
		list = g_settings_get_strv(settings, BLACKLIST_KEY);
		listed = g_strv_contains((const gchar *const *)list, name);
		g_strfreev(list);
		if (listed)
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
