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
 */

#include "ephy-automation.h"
#include "ephy-shell.h"
#include "EphyAutomation.h"
#include "ephy-embed.h"
#include "ephy-window.h"

#include <string.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-context.h>

static CORBA_boolean
impl_ephy_automation_add_bookmark (PortableServer_Servant _servant,
				   const CORBA_char * url,
				   CORBA_Environment * ev);
static CORBA_boolean
impl_ephy_automation_quit (PortableServer_Servant _servant,
                           const CORBA_boolean disableServer,
                           CORBA_Environment * ev);
static CORBA_boolean
impl_ephy_automation_load_session (PortableServer_Servant _servant,
				   const CORBA_char * filename,
				   CORBA_Environment * ev);
static void
ephy_automation_class_init (EphyAutomationClass *klass);
static void
ephy_automation_init (EphyAutomation *a);
static void
ephy_automation_object_finalize (GObject *object);
static BonoboObject *
ephy_automation_factory (BonoboGenericFactory *this_factory,
			 const char *iid,
			 gpointer user_data);

static GObjectClass *ephy_automation_parent_class;

#define EPHY_FACTORY_OAFIID "OAFIID:GNOME_Epiphany_Automation_Factory"

static BonoboObject *
ephy_automation_factory (BonoboGenericFactory *this_factory,
			 const char *iid,
			 gpointer user_data)
{
        EphyAutomation *a;

        a  = g_object_new (EPHY_AUTOMATION_TYPE, NULL);

        return BONOBO_OBJECT(a);
}

BonoboObject *
ephy_automation_new (void)
{
	BonoboGenericFactory   *factory;

	factory = bonobo_generic_factory_new (EPHY_FACTORY_OAFIID,
					      ephy_automation_factory,
					      NULL);

	g_return_val_if_fail (factory != NULL, NULL);

	return BONOBO_OBJECT (factory);
}

static CORBA_boolean
impl_ephy_automation_loadurl (PortableServer_Servant _servant,
			      const CORBA_char * url,
			      const CORBA_char * geometry,
			      const CORBA_boolean fullscreen,
			      const CORBA_boolean open_in_existing_tab,
			      const CORBA_boolean open_in_new_window,
			      const CORBA_boolean open_in_new_tab,
			      const CORBA_boolean raise,
                              CORBA_Environment * ev)
{
	EphyNewTabFlags flags = 0;
	const char *load_page = NULL;
	EphyWindow *window;
	Session *session;

	session = ephy_shell_get_session (ephy_shell);

	/* no window open, let's try to autoresume */
	if (session_get_windows (session) == NULL)
	{
		gboolean res;
		res = session_autoresume (session);
		/* no need to open the homepage,
		 * we did already open session windows */
		if (res && *url == '\0') return TRUE;
	}

	window = ephy_shell_get_active_window (ephy_shell);

	if (open_in_existing_tab && window != NULL)
	{
		ephy_window_load_url (window, url);
		return TRUE;
	}

	if (*url == '\0')
	{
		flags = EPHY_NEW_TAB_HOMEPAGE;
	}
	else
	{
		load_page = url;
	}

	if (open_in_new_window)
	{
		flags |= EPHY_NEW_TAB_IN_NEW_WINDOW;
	}

	if (open_in_new_tab)
	{
		flags |= EPHY_NEW_TAB_IN_EXISTING_WINDOW;
	}

	if (fullscreen)
	{
		flags |= EPHY_NEW_TAB_FULLSCREEN;
	}

	ephy_shell_new_tab (ephy_shell, window, NULL, load_page,
			    flags);

	return TRUE;
}

static CORBA_boolean
impl_ephy_automation_add_bookmark (PortableServer_Servant _servant,
				   const CORBA_char * url,
				   CORBA_Environment * ev)
{
	CORBA_boolean retval = TRUE;
	return retval;
}

static CORBA_boolean
impl_ephy_automation_quit (PortableServer_Servant _servant,
                           const CORBA_boolean disableServer,
                           CORBA_Environment * ev)
{
	CORBA_boolean retval = TRUE;

	Session *session;

	session = ephy_shell_get_session (ephy_shell);

	session_close (session);

	return retval;
}

static CORBA_boolean
impl_ephy_automation_load_session (PortableServer_Servant _servant,
				   const CORBA_char * filename,
				   CORBA_Environment * ev)
{
	CORBA_boolean retval = TRUE;
	Session *session;

	session = ephy_shell_get_session (ephy_shell);
	session_load (session, filename);

	return retval;
}

static void
ephy_automation_class_init (EphyAutomationClass *klass)
{
        GObjectClass *object_class = (GObjectClass *) klass;
        POA_GNOME_EphyAutomation__epv *epv = &klass->epv;

        ephy_automation_parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_automation_object_finalize;

        /* connect implementation callbacks */
        epv->loadurl = impl_ephy_automation_loadurl;
	epv->addBookmark = impl_ephy_automation_add_bookmark;
	epv->quit = impl_ephy_automation_quit;
	epv->loadSession = impl_ephy_automation_load_session;
}

static void
ephy_automation_init (EphyAutomation *c)
{
}

static void
ephy_automation_object_finalize (GObject *object)
{
        EphyAutomation *a = EPHY_AUTOMATION (object);

        ephy_automation_parent_class->finalize (G_OBJECT (a));
}

BONOBO_TYPE_FUNC_FULL (
        EphyAutomation,
        GNOME_EphyAutomation,
        BONOBO_TYPE_OBJECT,
        ephy_automation);
