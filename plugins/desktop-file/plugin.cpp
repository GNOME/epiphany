/*
 *  Copyright (C) 2005 Jorn Baayen <jbaayen@gnome.org>
 *  Copyright (C) 2005 Christian Persch
 *
 *  Based on the work of:
 *
 *  Copyright (C) 2004 Bastien Nocera <hadess@hadess.net>
 *  Copyright (C) 2002 David A. Schleef <ds@schleef.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "mozilla-config.h"
#include "config.h"

#include <string.h>
#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkmessagedialog.h>

#include <npupp.h>
#include <nsCOMPtr.h>
#include <nsIDOMWindow.h>
#include "../../embed/mozilla/EphyUtils.h"

/* NOTE: For simplicity, we use the Epiphany domain for translations,
 * instead of setting up another one for just a few strings. So we
 * don't need gi18n-lib.h here.
 */
#include <glib/gi18n.h>

static const char kDesktopEntry[] = "Desktop Entry";

typedef struct {
	NPP instance;
	guint handled : 1;
} Plugin;

static NPNetscapeFuncs mozilla_functions;

static void
show_error_dialog (NPP instance,
		   const char *primary_text,
		   const char *secondary_text)
{
	GtkWidget *dialog, *parent;

	nsCOMPtr<nsIDOMWindow> domWin;
	mozilla_functions.getvalue (instance, NPNVDOMWindow,
				    NS_STATIC_CAST (nsIDOMWindow **, getter_AddRefs (domWin)));
	parent = EphyUtils::FindGtkParent (domWin);

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
					 (GtkDialogFlags) (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
					 GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					 primary_text, NULL);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  secondary_text);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);

	if (parent && GTK_WINDOW (parent)->group) {
		gtk_window_group_add_window (GTK_WINDOW (parent)->group,
					     GTK_WINDOW (dialog));
	}

	gtk_widget_show (dialog);
}

/* Check for unsafe protocols. See:
 * http://www.mozilla.org/projects/security/components/reviewguide.html
 */

#define JAVASCRIPT_PROTOCOL "javascript:"
#define DATA_PROTOCOL       "data:"

static gboolean
is_safe_url (char *url)
{
	/* FIXME: when we allow non-file: .desktop files, add a security check here,
	 * like the one HTPP protocol handler does on redirect.
	 */

	url = g_strstrip (url);

	return (g_ascii_strncasecmp (url, JAVASCRIPT_PROTOCOL,
			             strlen (JAVASCRIPT_PROTOCOL)) != 0 &&
	        g_ascii_strncasecmp (url, DATA_PROTOCOL,
			             strlen (DATA_PROTOCOL)) != 0);
}

static NPError
plugin_new_instance (NPMIMEType mime_type,
		     NPP instance,
		     guint16 mode,
		     gint16 argc,
		     char **argn,
		     char **argv,
		     NPSavedData *saved)
{
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	/* We don't do embedded */
	if (mode != NP_FULL)
		return NPERR_INVALID_PARAM;

	/* FIXME: Until I've thought more about security when loading .desktop
	 * files from non-file: locations, only allow local desktop files.
	*/
	PRBool isSafe = PR_FALSE;
	for (gint16 i = 0; i < argc; ++i) {
		if (argn[i] && g_ascii_strcasecmp (argn[i], "src") == 0) {
			isSafe = argv[i] && g_ascii_strncasecmp (argv[i], "file:/", strlen ("file:/")) == 0;
			break;
		}
	}

	if (!isSafe)
		return NPERR_INVALID_URL;

	instance->pdata = mozilla_functions.memalloc (sizeof (Plugin));

	Plugin *plugin = (Plugin *) instance->pdata;
	if (plugin == NULL)
		return NPERR_OUT_OF_MEMORY_ERROR;

	memset (plugin, 0, sizeof (Plugin));

	plugin->instance = instance;

	mozilla_functions.setvalue (plugin->instance, NPPVpluginWindowBool,
				    GINT_TO_POINTER (FALSE));

	return NPERR_NO_ERROR;
}

static NPError
plugin_destroy_instance (NPP instance,
			 NPSavedData **save)
{
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	Plugin *plugin = (Plugin *) instance->pdata;
	if (plugin == NULL)
		return NPERR_NO_ERROR;

	mozilla_functions.memfree (instance->pdata);
	instance->pdata = NULL;

	return NPERR_NO_ERROR;
}

static NPError
plugin_new_stream (NPP instance,
		   NPMIMEType type,
		   NPStream *stream_ptr,
		   NPBool seekable,
		   guint16 *stype)
{
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	Plugin *plugin = (Plugin *) instance->pdata;
	if (plugin == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	*stype = NP_ASFILEONLY;

	plugin->handled = FALSE;

	return NPERR_NO_ERROR;
}

static NPError
plugin_stream_as_file (NPP instance,
		       NPStream* stream,
		       const char *filename)
{
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	Plugin *plugin = (Plugin *) instance->pdata;
	if (plugin == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	if (!filename)
		return NPERR_INVALID_PARAM;

	GKeyFile *keyfile = g_key_file_new ();
	if (!g_key_file_load_from_file (keyfile, filename, (GKeyFileFlags) 0, NULL)) {
		/* Not a valid key file */
		g_key_file_free (keyfile);
		return NPERR_GENERIC_ERROR;
	}

	char *group = g_key_file_get_start_group (keyfile);
	if (!group || strcmp (group, kDesktopEntry) != 0) {
		/* Not a valid Desktop file */
		g_free (group);
		g_key_file_free (keyfile);
		return NPERR_GENERIC_ERROR;
	}

	char *encoding = g_key_file_get_string (keyfile, kDesktopEntry, "Encoding", NULL);
	if (!encoding || strcmp (encoding, "UTF-8") != 0) {
		/* Not a properly encoded desktop file */
		g_free (encoding);
		g_key_file_free (keyfile);
		return NPERR_GENERIC_ERROR;
	}

	char *type = g_key_file_get_string (keyfile, kDesktopEntry, "Type", NULL);
	if (!type || strcmp (type, "Link") != 0) {
		/* Not a "Link" file */
		g_free (type);
		g_key_file_free (keyfile);
		return NPERR_GENERIC_ERROR;
	}

	char *url = g_key_file_get_string (keyfile, kDesktopEntry, "URL", NULL);
	if (!url || !url[0]) {
		/* Not a valid URL */
		g_free (url);
		g_key_file_free (keyfile);
		return NPERR_GENERIC_ERROR;
	}

	plugin->handled = TRUE;

	if (is_safe_url (url)) {
		mozilla_functions.geturl (instance, url, "_top");
	} else {
		/* Load blank page, so that further drags to the embed work */
		mozilla_functions.geturl (instance, "about:blank", "_top");

		show_error_dialog (instance,
				   _("Unsafe protocol."),
				   _("The contained address has not been "
				     "loaded, because it refers to an "
				     "unsafe protocol and thereby presents "
				     "a security risk to your system."));
	}

	g_free (url);
	g_key_file_free (keyfile);

	return NPERR_NO_ERROR;
}

static NPError
plugin_destroy_stream (NPP instance,
		       NPStream *stream,
		       NPError reason)
{
	if (instance == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	Plugin *plugin = (Plugin *) instance->pdata;
	if (plugin == NULL)
		return NPERR_INVALID_INSTANCE_ERROR;

	if (!plugin->handled) {
		show_error_dialog (instance,
				   _("No address found."),
				   _("No web address could be found in this file."));
	}

	return NPERR_NO_ERROR;
}

static int32
plugin_write_ready (NPP instance,
		    NPStream *stream)
{
	/* Ready to read 4 MB - should do. Can always get more. */
	return 4096;
}

static int32
plugin_write (NPP instance,
	      NPStream *stream,
	      int32 offset,
	      int32 len,
	      gpointer buffer)
{
	/* FIXME */
	return len;
}

static NPError
plugin_get_value (NPP instance,
		  NPPVariable variable,
		  gpointer value)
{
	NPError err = NPERR_NO_ERROR;

	switch (variable) {
	case NPPVpluginNameString:
		/* Translators: "Desktop File" refers to .desktop files containing a link */
		*((char **) value) = _("Epiphany Desktop File Plugin");
		break;

	case NPPVpluginDescriptionString:
		*((char **) value) = _("This plugin handles “.desktop” files containing web links.");
		break;

	case NPPVpluginNeedsXEmbed:
		*((PRBool *) value) = PR_FALSE;
		break;

	default:
		err = NPERR_INVALID_PARAM;
		break;
	}

	return err;
}

NPError
NP_GetValue (void *future,
	     NPPVariable variable,
	     gpointer value)
{
	return plugin_get_value (NULL, variable, value);
}

char *
NP_GetMIMEDescription (void)
{
	return "application/x-desktop:desktop:desktop configuration file;";
}

NPError
NP_Initialize (NPNetscapeFuncs *moz_funcs,
               NPPluginFuncs *plugin_funcs)
{
	if (moz_funcs == NULL || plugin_funcs == NULL)
		return NPERR_INVALID_FUNCTABLE_ERROR;

	if ((moz_funcs->version >> 8) > NP_VERSION_MAJOR)
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
	if (moz_funcs->size < sizeof (NPNetscapeFuncs))
		return NPERR_INVALID_FUNCTABLE_ERROR;
	if (plugin_funcs->size < sizeof (NPPluginFuncs))
		return NPERR_INVALID_FUNCTABLE_ERROR;

	/*
	 * Copy all of the fields of the Mozilla function table into our
	 * copy so we can call back into Mozilla later.  Note that we need
	 * to copy the fields one by one, rather than assigning the whole
	 * structure, because the Mozilla function table could actually be
	 * bigger than what we expect.
	 */
	mozilla_functions.size             = moz_funcs->size;
	mozilla_functions.version          = moz_funcs->version;
	mozilla_functions.geturl           = moz_funcs->geturl;
	mozilla_functions.posturl          = moz_funcs->posturl;
	mozilla_functions.requestread      = moz_funcs->requestread;
	mozilla_functions.newstream        = moz_funcs->newstream;
	mozilla_functions.write            = moz_funcs->write;
	mozilla_functions.destroystream    = moz_funcs->destroystream;
	mozilla_functions.status           = moz_funcs->status;
	mozilla_functions.uagent           = moz_funcs->uagent;
	mozilla_functions.memalloc         = moz_funcs->memalloc;
	mozilla_functions.memfree          = moz_funcs->memfree;
	mozilla_functions.memflush         = moz_funcs->memflush;
	mozilla_functions.reloadplugins    = moz_funcs->reloadplugins;
	mozilla_functions.getJavaEnv       = moz_funcs->getJavaEnv;
	mozilla_functions.getJavaPeer      = moz_funcs->getJavaPeer;
	mozilla_functions.geturlnotify     = moz_funcs->geturlnotify;
	mozilla_functions.posturlnotify    = moz_funcs->posturlnotify;
	mozilla_functions.getvalue         = moz_funcs->getvalue;
	mozilla_functions.setvalue         = moz_funcs->setvalue;
	mozilla_functions.invalidaterect   = moz_funcs->invalidaterect;
	mozilla_functions.invalidateregion = moz_funcs->invalidateregion;
	mozilla_functions.forceredraw      = moz_funcs->forceredraw;
	mozilla_functions.geturl           = moz_funcs->geturl;

	/*
	 * Set up a plugin function table that Mozilla will use to call
	 * into us.  Mozilla needs to know about our version and size and
	 * have a UniversalProcPointer for every function we implement.
	 */

	plugin_funcs->size = sizeof (NPPluginFuncs);
	plugin_funcs->version = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
	plugin_funcs->newp = NewNPP_NewProc (plugin_new_instance);
	plugin_funcs->destroy = NewNPP_DestroyProc (plugin_destroy_instance);
	plugin_funcs->setwindow = NewNPP_SetWindowProc (NULL);
	plugin_funcs->newstream = NewNPP_NewStreamProc (plugin_new_stream);
	plugin_funcs->destroystream = NewNPP_DestroyStreamProc (plugin_destroy_stream);
	plugin_funcs->asfile = NewNPP_StreamAsFileProc (plugin_stream_as_file);
	plugin_funcs->writeready = NewNPP_WriteReadyProc (plugin_write_ready);
	plugin_funcs->write = NewNPP_WriteProc (plugin_write);
	plugin_funcs->print = NewNPP_PrintProc (NULL);
	plugin_funcs->event = NewNPP_HandleEventProc (NULL);
	plugin_funcs->urlnotify = NewNPP_URLNotifyProc (NULL);
	plugin_funcs->javaClass = NULL;
	plugin_funcs->getvalue = NewNPP_GetValueProc (plugin_get_value);
	plugin_funcs->setvalue = NewNPP_SetValueProc (NULL);

	return NPERR_NO_ERROR;
}

NPError
NP_Shutdown (void)
{
	return NPERR_NO_ERROR;
}
