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
ephy_activation_load_uris (EphyDbus *ephy_dbus,
			   char **uris,
			   char *options,
			   guint startup_id,
			   GError **error)
{
	EphyShell *shell;
	EphySession *session;
	EphyNewTabFlags flags = 0;
	EphyWindow *window;
	EphyTab *tab;
	static char *empty_urls[] = { "", NULL };
	guint32 user_time = (guint32) startup_id;
	guint i;

	g_return_val_if_fail (uris != NULL && options != NULL, TRUE);

	shell = ephy_shell_get_default ();

	g_object_ref (shell);

	session = EPHY_SESSION (ephy_shell_get_session (shell));
	g_assert (session != NULL);

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_ARBITRARY_URL))
	{
		uris = empty_urls;
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

	for (i = 0; uris[i] != NULL; ++i)
	{
		const char *url = uris[i];
		EphyNewTabFlags page_flags;

		if (url[0] == '\0')
		{
			page_flags = EPHY_NEW_TAB_HOME_PAGE;
		}
		else
		{
			page_flags = EPHY_NEW_TAB_OPEN_PAGE;
		}

		tab = ephy_shell_new_tab_full (shell,window,
					       NULL, url,
					       flags | page_flags,
					       EPHY_EMBED_CHROME_ALL,
					       FALSE, user_time);

		window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tab)));
	}

	g_object_unref (shell);

	/* FIXME: do we have to g_strfreev (uris) ? */

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
				       guint startup_id,
				       GError **error)
{
	GtkWidget *editor;
	guint32 user_time = (guint32) startup_id;

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_BOOKMARK_EDITING))
	{
		g_set_error (error,
			     g_quark_from_static_string ("ephy-activation-error"),
			     0,
			     "Bookmarks editing is locked down.");

		return FALSE;
	}

	editor = ephy_shell_get_bookmarks_editor (ephy_shell);
	ephy_gui_window_update_user_time (editor, user_time);
	gtk_window_present (GTK_WINDOW (editor));

	return TRUE;
}
