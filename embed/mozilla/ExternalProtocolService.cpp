/*
 *  Copyright (C) 2001 Philip Langdale
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

#include <gtk/gtk.h>
#include <libgnome/gnome-exec.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <nsString.h>
#include <nsXPIDLString.h>
#include <nsCOMPtr.h>
#include <nsIURI.h>
#include <nsIDOMWindow.h>
#include <nsIWindowWatcher.h>
#include <nsIServiceManager.h>

#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"

#include "ExternalProtocolService.h"

#define WINDOWWATCHER_CONTRACTID "@mozilla.org/embedcomp/window-watcher;1"

/* Implementation file */
NS_IMPL_ISUPPORTS1(GExternalProtocolService, nsIExternalProtocolService)

GExternalProtocolService::GExternalProtocolService()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

GExternalProtocolService::~GExternalProtocolService()
{
  /* destructor code */
}

/* boolean externalProtocolHandlerExists (in string aProtocolScheme); */
NS_IMETHODIMP GExternalProtocolService::
		ExternalProtocolHandlerExists(const char *aProtocolScheme,
					      PRBool *_retval)
{
	/* build the config key */
	char *key = g_strconcat ("/desktop/gnome/url-handlers/",
				 aProtocolScheme,
				 "/command", NULL);

	char *tmp = eel_gconf_get_string(key);
	g_free (key);

	*_retval = (tmp != NULL);
	g_free (tmp);

	return NS_OK;
}

/* void loadUrl (in nsIURI aURL); */
NS_IMETHODIMP GExternalProtocolService::LoadUrl(nsIURI *aURL)
{
	nsCAutoString cSpec;
	aURL->GetSpec (cSpec);
	nsCAutoString cScheme;
	aURL->GetScheme (cScheme);

	if (cScheme.Equals("http"))
	{
		nsresult rv;
		nsCOMPtr<nsIWindowWatcher> ww;
		ww = do_GetService(WINDOWWATCHER_CONTRACTID, &rv);
		if (NS_SUCCEEDED(rv))
		{
			nsCOMPtr<nsIDOMWindow> newWin;
			rv = ww->OpenWindow(nsnull, cSpec.get(),
					    nsnull, nsnull, nsnull,
					    getter_AddRefs(newWin));
			if (NS_SUCCEEDED(rv)) return NS_OK;
		}
	}

	/* build the config key */
	const nsCAutoString key(NS_LITERAL_CSTRING("/desktop/gnome/url-handlers/") +
				cScheme + NS_LITERAL_CSTRING("/command"));

	/* find it */
	char *result = eel_gconf_get_string(key.get());
	if (result)
	{
		gnome_vfs_url_show(cSpec.get());
		g_free (result);
		return NS_OK;
	}

	/* no luck, so offer the user the option of trying the
	 * default handler -- we don't do this automatically in
	 * case the default handler is erroneously set to epiphany */
	result = eel_gconf_get_string("/desktop/gnome/url-handlers/unknown/command");

	/* check there is a default */
	{
		GtkWidget *dialog;
		
		/* throw the error */
		dialog = gtk_message_dialog_new (NULL, (GtkDialogFlags)0, 
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					         _("Epiphany cannot handle this protocol,\n"
				     	         "and no GNOME default handler is set"));
		gtk_dialog_run (GTK_DIALOG(dialog));
		gtk_widget_destroy (dialog);
		
		/* don't let mozilla try blindly */
		return NS_ERROR_FAILURE;
	}
	g_free (result);

	/* offer the choice */
	GtkWidget *dialog = gtk_message_dialog_new (NULL, (GtkDialogFlags)0, 
						    GTK_MESSAGE_QUESTION,
						    GTK_BUTTONS_YES_NO,
						    _("The protocol specified "
						     "is not recognised.\n\n"
						     "Would you like to try "
						     "the GNOME default?"));
	
	int ret = gtk_dialog_run (GTK_DIALOG(dialog));
	gtk_widget_destroy (dialog);

	if (ret == 0)
	{
		gnome_vfs_url_show(cSpec.get());		
		return NS_OK;
	}
	else
	{
		return NS_ERROR_FAILURE;
	}

	return NS_OK;
}
