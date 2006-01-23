/*
 *  Copyright (C) 2005 Gustavo Gama
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "config.h"

#include "ephy-activation.h"

#include "ephy-shell.h"
#include "ephy-session.h"
#include "ephy-prefs.h"
#include "ephy-gui.h"
#include "eel-gconf-extensions.h"
#include "ephy-debug.h"

#include <string.h>

gboolean
ephy_activation_load_url (EphyDbus *ephy_dbus,
			  char *url,
			  char *options,
			  guint startup_id)
{
	EphyNewTabFlags flags = 0;
	EphyWindow *window;
	EphySession *session;
	guint32 user_time = (guint32) startup_id;

	g_return_val_if_fail (url != NULL && options != NULL, TRUE);

	session = EPHY_SESSION (ephy_shell_get_session (ephy_shell));
	g_return_val_if_fail (session != NULL, TRUE);

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_ARBITRARY_URL))
	{
		url = "";
	}

	window = ephy_session_get_active_window (session);

#if 0
	if (open_in_existing_tab && window != NULL)
	{
		ephy_gui_window_update_user_time (GTK_WIDGET (window),
						  user_time);
		ephy_window_load_url (window, url);
		return TRUE;
	}
#endif

	if (url[0] == '\0')
	{
		flags |= EPHY_NEW_TAB_HOME_PAGE;
	}
	else
	{
		flags |= EPHY_NEW_TAB_OPEN_PAGE;
	}

	if (strstr (options, "new-window") != NULL)
	{
		window = NULL;
		flags |= EPHY_NEW_TAB_IN_NEW_WINDOW;
	}
	else if (strstr (options, "new-tab") != NULL)
	{
		flags |= EPHY_NEW_TAB_IN_EXISTING_WINDOW |
			 EPHY_NEW_TAB_JUMP;
	}

	ephy_shell_new_tab_full (ephy_shell, window, NULL, url, flags,
				 EPHY_EMBED_CHROME_ALL, FALSE, user_time);

	return TRUE;
}

gboolean
ephy_activation_load_session (EphyDbus *ephy_dbus,
			      char *session_name,
			      guint startup_id,
			      GError **error)
{
	EphySession *session;
	guint32 user_time = (guint32) startup_id;

	session = EPHY_SESSION (ephy_shell_get_session (ephy_shell));
	ephy_session_load (session, session_name, user_time);
	return TRUE;
}

gboolean
ephy_activation_open_bookmarks_editor (EphyDbus *ephy_dbus,
				       guint startup_id)
{
	GtkWidget *editor;
	guint32 user_time = (guint32) startup_id;

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_BOOKMARK_EDITING))
	{
		return FALSE;
	}
	editor = ephy_shell_get_bookmarks_editor (ephy_shell);
	ephy_gui_window_update_user_time (editor, user_time);
	gtk_window_present (GTK_WINDOW (editor));
	return TRUE;
}
