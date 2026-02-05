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

static const char *const BLACKLIST[] = { "acroread",
	                                 "emacs",
	                                 "emacs23",
	                                 "emacs23-lucid",
	                                 "emacs24",
	                                 "emacs24-lucid",
	                                 "budgie-panel",
	                                 "mate-panel",
	                                 "mate-menu",
	                                 "vala-panel",
	                                 "wrapper-1.0",
	                                 "wrapper-2.0",
	                                 "indicator-applet",
	                                 "mate-indicator-applet",
	                                 "mate-indicator-applet-appmenu",
	                                 "mate-indicator-applet-complete",
	                                 "appmenu-mate",
	                                 NULL };

G_GNUC_INTERNAL
bool is_blacklisted(const char *name)
{
	guint i;

	for (i = 0; BLACKLIST[i] != NULL; i++)
	{
		if (g_strcmp0(name, BLACKLIST[i]) == 0)
			return true;
	}

	return false;
}
