/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* gul-bonobo-extensions.c - implementation of new functions that conceptually
                             belong in bonobo. Perhaps some of these will be
                             actually rolled into bonobo someday.

            This file is based on nautilus-bonobo-extensions.c from
            libnautilus-private.

   Copyright (C) 2000, 2001 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: John Sullivan <sullivan@eazel.com>
            Darin Adler <darin@bentspoon.com>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-bonobo-extensions.h"
#include "ephy-string.h"
#include <string.h>

#include <bonobo/bonobo-ui-util.h>
#include <gtk/gtkmain.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <bonobo/bonobo-control.h>

void
ephy_bonobo_set_hidden (BonoboUIComponent *ui,
		        const char *path,
		        gboolean hidden)
{
	if (bonobo_ui_component_get_container (ui)) /* should not do this here... */
	{
		bonobo_ui_component_set_prop (ui, path, "hidden", hidden ? "1" : "0", NULL);
	}
}

static void
ephy_bonobo_clear_path (BonoboUIComponent *uic,
		        const gchar *path)
{
	if (bonobo_ui_component_path_exists  (uic, path, NULL))
	{
		char *remove_wildcard = g_strdup_printf ("%s/*", path);
		bonobo_ui_component_rm (uic, remove_wildcard, NULL);
		g_free (remove_wildcard);
	}
}

void
ephy_bonobo_replace_path (BonoboUIComponent *uic, const gchar *path_src,
			  const char *path_dst)
{
	BonoboUINode *node;
	const char *name;
	char *path_dst_folder;

	name = strrchr (path_dst, '/');
	g_return_if_fail (name != NULL);
	path_dst_folder = g_strndup (path_dst, name - path_dst);
	name++;

	node = bonobo_ui_component_get_tree (uic, path_src, TRUE, NULL);
	bonobo_ui_node_set_attr (node, "name", name);

	ephy_bonobo_clear_path (uic, path_dst);

	bonobo_ui_component_set_tree (uic, path_dst_folder, node, NULL);

	g_free (path_dst_folder);
	bonobo_ui_node_free (node);
}
