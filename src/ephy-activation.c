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
 *  $Id:
 */

#include "ephy-activation.h"
#include "ephy-debug.h"
#include "ephy-shell.h"
#include "ephy-session.h"
#include "ephy-bookmarks-import.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-gui.h"

gboolean
ephy_activation_load_url (EphyDbus *ephy_dbus,
			  char *url,
			  gboolean fullscreen,
			  gboolean open_in_existing_tab,
			  gboolean open_in_new_tab,
			  guint startup_id)
{
	EphyNewTabFlags flags = 0;
	EphyWindow *window;
	EphySession *session;
	guint32 user_time = (guint32) startup_id;

	session = EPHY_SESSION (ephy_shell_get_session (ephy_shell));
	g_return_val_if_fail (session != NULL, FALSE);

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_ARBITRARY_URL))
	{
		url = "";
	}

	window = ephy_session_get_active_window (session);

	if (open_in_existing_tab && window != NULL)
	{
		ephy_gui_window_update_user_time (GTK_WIDGET (window),
						  user_time);
		ephy_window_load_url (window, url);
		return TRUE;
	}

	if (*url == '\0')
	{
		flags |= EPHY_NEW_TAB_HOME_PAGE;
	}
	else
	{
		flags |= EPHY_NEW_TAB_OPEN_PAGE;
	}

	if (open_in_new_tab)
	{
		flags |= EPHY_NEW_TAB_IN_EXISTING_WINDOW |
			 EPHY_NEW_TAB_JUMP;
	}
	else
	{
		flags |= EPHY_NEW_TAB_IN_NEW_WINDOW;
	}

	if (fullscreen)
	{
		flags |= EPHY_NEW_TAB_FULLSCREEN_MODE;
	}

	ephy_shell_new_tab_full (ephy_shell, window, NULL, url, flags,
				 EPHY_EMBED_CHROME_ALL, FALSE, user_time);

	return TRUE;
}

gboolean
ephy_activation_add_bookmark (EphyDbus *ephy_dbus,
			      char *url)
{
	ephy_bookmarks_add (ephy_shell_get_bookmarks (ephy_shell),
			    url /* title */, url);
	return TRUE;
}

gboolean
ephy_activation_import_bookmarks (EphyDbus *ephy_dbus,
				  char *filename)
{
	ephy_bookmarks_import (ephy_shell_get_bookmarks (ephy_shell),
			       filename);
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


void
ephy_activation_general_purpose_reply (DBusGProxy *proxy,
				       GError *error,
				       gpointer user_data)
{
	g_object_unref (proxy);
	if (error != NULL)
	{
		g_warning ("An error occured while calling remote method: %s", error->message);
		g_error_free (error);
		return;
	}
}
