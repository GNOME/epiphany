/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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

#include "ephy-automation.h"

#include "EphyAutomation.h"
#include "ephy-shell.h"
#include "ephy-embed.h"
#include "ephy-window.h"
#include "ephy-session.h"
#include "ephy-bookmarks.h"
#include "ephy-bookmarks-import.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"

#include <string.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-context.h>

static void ephy_automation_class_init (EphyAutomationClass *klass);

static GObjectClass *parent_class = NULL;

static void
impl_ephy_automation_loadurl (PortableServer_Servant _servant,
			      const CORBA_char *url,
			      const CORBA_boolean fullscreen,
			      const CORBA_boolean open_in_existing_tab,
			      const CORBA_boolean open_in_new_tab,
                              CORBA_Environment *ev)
{
	EphyNewTabFlags flags = 0;
	EphyWindow *window;
	EphySession *session;

	session = EPHY_SESSION (ephy_shell_get_session (ephy_shell));
	g_return_if_fail (session != NULL);

	if (ephy_session_autoresume (session) && *url == '\0')
	{
		/* no need to open the homepage,
		* we did already open session windows */
		return;
	}

	window = ephy_session_get_active_window (session);

	if (open_in_existing_tab && window != NULL)
	{
		ephy_window_load_url (window, url);
		return;
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
		flags |= EPHY_NEW_TAB_IN_EXISTING_WINDOW;
	}
	else
	{
		flags |= EPHY_NEW_TAB_IN_NEW_WINDOW;
	}

	if (fullscreen)
	{
		flags |= EPHY_NEW_TAB_FULLSCREEN_MODE;
	}

	ephy_shell_new_tab (ephy_shell, window, NULL, url,
			    flags);
}

static void
impl_ephy_automation_add_bookmark (PortableServer_Servant _servant,
				   const CORBA_char * url,
				   CORBA_Environment * ev)
{
	ephy_bookmarks_add (ephy_shell_get_bookmarks (ephy_shell),
			    url /* title */, url);
}

static void
impl_ephy_automation_import_bookmarks (PortableServer_Servant _servant,
				       const CORBA_char * filename,
				       CORBA_Environment * ev)
{
	ephy_bookmarks_import (ephy_shell_get_bookmarks (ephy_shell),
			       filename);
}

static void
impl_ephy_automation_load_session (PortableServer_Servant _servant,
				   const CORBA_char * filename,
				   CORBA_Environment * ev)
{
	EphySession *session;

	session = EPHY_SESSION (ephy_shell_get_session (ephy_shell));
	ephy_session_load (session, filename);
}

static void
impl_ephy_automation_open_bookmarks_editor (PortableServer_Servant _servant,
					    CORBA_Environment * ev)
{
	GtkWidget *editor;

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_BOOKMARK_EDITING)) return;
	editor = ephy_shell_get_bookmarks_editor (ephy_shell);

	gtk_window_present (GTK_WINDOW (editor));
}

static void
ephy_automation_init (EphyAutomation *c)
{
}

static void
ephy_automation_class_init (EphyAutomationClass *klass)
{
	POA_GNOME_EphyAutomation__epv *epv = &klass->epv;

	parent_class = g_type_class_peek_parent (klass);

	/* connect implementation callbacks */
	epv->loadurl = impl_ephy_automation_loadurl;
	epv->addBookmark = impl_ephy_automation_add_bookmark;
	epv->importBookmarks = impl_ephy_automation_import_bookmarks;
	epv->loadSession = impl_ephy_automation_load_session;
	epv->openBookmarksEditor = impl_ephy_automation_open_bookmarks_editor;
}

BONOBO_TYPE_FUNC_FULL (
        EphyAutomation,
        GNOME_EphyAutomation,
        BONOBO_TYPE_OBJECT,
        ephy_automation);
