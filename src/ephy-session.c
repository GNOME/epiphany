/*
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-session.h"

#include "ephy-window.h"
#include "ephy-tab.h"
#include "ephy-shell.h"
#include "ephy-history-window.h"
#include "ephy-bookmarks-editor.h"
#include "ephy-string.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <string.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>

#define EPHY_SESSION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_SESSION, EphySessionPrivate))

struct EphySessionPrivate
{
	GList *windows;
};

#define BOOKMARKS_EDITOR_ID	"BookmarksEditor"
#define HISTORY_WINDOW_ID	"HistoryWindow"
#define SESSION_CRASHED		"type:session_crashed"

static void ephy_session_class_init	(EphySessionClass *klass);
static void ephy_session_iface_init	(EphyExtensionClass *iface);
static void ephy_session_init		(EphySession *session);

static GObjectClass *parent_class = NULL;

GType
ephy_session_get_type (void)
{
	static GType type = 0;

	if (type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphySessionClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) ephy_session_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (EphySession),
			0, /* n_preallocs */
			(GInstanceInitFunc) ephy_session_init
		};

		static const GInterfaceInfo extension_info =
		{
			(GInterfaceInitFunc) ephy_session_iface_init,
			NULL,
			NULL
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphySession",
					       &our_info, 0);

		g_type_add_interface_static (type,
					     EPHY_TYPE_EXTENSION,
					     &extension_info);
	}

	return type;
}

static char *
get_session_filename (const char *filename)
{
	char *save_to;

	if (filename == NULL)
	{
		return NULL;
	}

	if (strcmp (filename, SESSION_CRASHED) == 0)
	{
		save_to = g_build_filename (ephy_dot_dir (),
					    "session_crashed.xml",
					    NULL);
	}
	else
	{
		save_to = g_strdup (filename);
	}

	return save_to;
}

static void
session_delete (EphySession *session,
		const char *filename)
{
	char *save_to;

	save_to = get_session_filename (filename);

	gnome_vfs_unlink (save_to);

	g_free (save_to);
}

static void
net_stop_cb (EphyEmbed *embed,
	     EphySession *session)
{
	ephy_session_save (session, SESSION_CRASHED);
}

static void
tab_added_cb (GtkWidget *notebook,
	      GtkWidget *child,
	      EphySession *session)
{
	g_signal_connect (child, "net_stop",
			  G_CALLBACK (net_stop_cb), session);
}

static void
tab_removed_cb (GtkWidget *notebook,
		GtkWidget *child,
		EphySession *session)
{
	ephy_session_save (session, SESSION_CRASHED);

	g_signal_handlers_disconnect_by_func
		(child, G_CALLBACK (net_stop_cb), session);
}

static void
tabs_reordered_cb (GtkWidget *notebook,
		   EphySession *session)
{
	ephy_session_save (session, SESSION_CRASHED);
}

static void
impl_attach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphySession *session = EPHY_SESSION (extension);
	GtkWidget *notebook;

	LOG ("impl_attach_window")

	session->priv->windows = g_list_prepend (session->priv->windows, window);
	ephy_session_save (session, SESSION_CRASHED);

	notebook = ephy_window_get_notebook (window);
	g_signal_connect (notebook, "tab_added",
			  G_CALLBACK (tab_added_cb), session);
	g_signal_connect (notebook, "tab_removed",
			  G_CALLBACK (tab_removed_cb), session);
	g_signal_connect (notebook, "tabs_reordered",
			  G_CALLBACK (tabs_reordered_cb), session);
}

static void
impl_detach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphySession *session = EPHY_SESSION (extension);

	LOG ("impl_detach_window")

	session->priv->windows = g_list_remove (session->priv->windows, window);
	ephy_session_save (session, SESSION_CRASHED);

	/* NOTE: since the window will be destroyed anyway, we don't need to
	 * disconnect our signal handlers from its components.
	 */
}

static void
ensure_session_directory (void)
{
	char *dir;

	dir = g_build_filename (ephy_dot_dir (), "sessions", NULL);
	if (g_file_test (dir, G_FILE_TEST_EXISTS) == FALSE)
	{
		if (mkdir (dir, 488) != 0)
		{
			g_error ("Unable to create session directory '%s'\n", dir);
		}
	}

	g_free (dir);
}

static void
ephy_session_init (EphySession *session)
{
	session->priv = EPHY_SESSION_GET_PRIVATE (session);

	LOG ("EphySession initialising")

	session->priv->windows = NULL;

	ensure_session_directory ();
}

static void
ephy_session_dispose (GObject *object)
{
	EphySession *session = EPHY_SESSION(object);

	LOG ("EphySession disposing")

	session_delete (session, SESSION_CRASHED);
}

static void
ephy_session_finalize (GObject *object)
{
	EphySession *session = EPHY_SESSION (object);

	LOG ("EphySession finalising")

	g_list_free (session->priv->windows);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_session_iface_init (EphyExtensionClass *iface)
{
	iface->attach_window = impl_attach_window;
	iface->detach_window = impl_detach_window;
}

static void
ephy_session_class_init (EphySessionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = ephy_session_dispose;
	object_class->finalize = ephy_session_finalize;

	g_type_class_add_private (object_class, sizeof (EphySessionPrivate));
}

static gboolean
offer_to_resume (EphySession *session)
{
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *image;
	char *str;
	int response;

	dialog = gtk_dialog_new_with_buttons
		(_("Crash Recovery"), NULL,
		 GTK_DIALOG_NO_SEPARATOR,
		 _("_Don't Recover"), GTK_RESPONSE_CANCEL,
		 _("_Recover"), GTK_RESPONSE_OK,
		 NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 14);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_BOX (hbox)), 5);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
			    TRUE, TRUE, 0);

	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING,
					  GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	gtk_widget_show (image);
	gtk_box_pack_start (GTK_BOX (hbox), image,
			    TRUE, TRUE, 0);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (hbox), vbox,
			    TRUE, TRUE, 0);

	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_widget_show (label);
	str = g_strconcat ("<b>", _("Epiphany appears to have crashed or been killed the last time it was run."),
			   "</b>", NULL);
	gtk_label_set_markup (GTK_LABEL (label), str);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	g_free (str);

	label = gtk_label_new (_("You can recover the opened tabs and windows."));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (vbox), label,
			    TRUE, TRUE, 0);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	return (response == GTK_RESPONSE_OK);
}

/**
 * ephy_session_autoresume:
 * @session: a #EphySession
 *
 * Resume a crashed session when necessary (interactive)
 *
 * Return value: TRUE if at least a window has been opened
 **/
gboolean
ephy_session_autoresume (EphySession *session)
{
	char *saved_session;
	gboolean retval = FALSE;

	if (session->priv->windows != NULL) return FALSE;

	LOG ("ephy_session_autoresume")

	saved_session = get_session_filename (SESSION_CRASHED);

	if (g_file_test (saved_session, G_FILE_TEST_EXISTS)
	    && offer_to_resume (session))
	{
		retval = ephy_session_load (session, saved_session);
	}

	g_free (saved_session);

	return retval;
}

void
ephy_session_close (EphySession *session)
{
	GList *windows;

	LOG ("ephy_session_close")

	windows	= ephy_session_get_windows (session);

	/* close all windows */
	g_list_foreach (windows, (GFunc) gtk_widget_destroy, NULL);

	g_list_free (windows);
}

static int
write_tab (xmlTextWriterPtr writer,
	   EphyTab *tab)
{
	EphyEmbed *embed;
	char *location;
	int ret;

	ret = xmlTextWriterStartElement (writer, "embed");
	if (ret < 0) return ret;

	embed = ephy_tab_get_embed (tab);
	location = ephy_embed_get_location (embed, TRUE);
	ret = xmlTextWriterWriteAttribute (writer, "url", location);
	g_free (location);
	if (ret < 0) return ret;

	ret = xmlTextWriterEndElement (writer); /* embed */
	return ret;
}

static int
write_window_geometry (xmlTextWriterPtr writer,
		       GtkWindow *window)
{
	int x = 0, y = 0, width = 0, height = 0;
	int ret;

	/* get window geometry */
	gtk_window_get_size (window, &width, &height);
	gtk_window_get_position (window, &x, &y);

	/* set window properties */
	ret = xmlTextWriterWriteFormatAttribute (writer, "x", "%d", x);
	if (ret < 0) return ret;

	ret = xmlTextWriterWriteFormatAttribute (writer, "y", "%d", y);
	if (ret < 0) return ret;

	ret = xmlTextWriterWriteFormatAttribute (writer, "width", "%d", width);
	if (ret < 0) return ret;

	ret = xmlTextWriterWriteFormatAttribute (writer, "height", "%d", height);
	return ret;
}

static int
write_tool_window (xmlTextWriterPtr writer,
		   GtkWindow *window,
		   xmlChar *id)
{
	int ret;

	ret = xmlTextWriterStartElement (writer, "toolwindow");
	if (ret < 0) return ret;

	ret = xmlTextWriterWriteAttribute (writer, "id", id);
	if (ret < 0) return ret;

	ret = write_window_geometry (writer, window);
	if (ret < 0) return ret;

	ret = xmlTextWriterEndElement (writer); /* toolwindow */
	return ret;
}

static int
write_ephy_window (xmlTextWriterPtr writer,
		   EphyWindow *window)
{
	GList *tabs, *l;
	int ret;

	tabs = ephy_window_get_tabs (window);

	/* Do not save an empty EphyWindow.
	 * This only happens when the window was newly opened.
	 */
	if (tabs == NULL) return 0;

	ret = xmlTextWriterStartElement (writer, "window");
	if (ret < 0) return ret;

	ret = write_window_geometry (writer, GTK_WINDOW (window));
	if (ret < 0) return ret;

	for (l = tabs; l != NULL; l = l->next)
	{
		EphyTab *tab = EPHY_TAB(l->data);
		ret = write_tab (writer, tab);
		if (ret < 0) break;
	}
	g_list_free (tabs);
	if (ret < 0) return ret;

	ret = xmlTextWriterEndElement (writer); /* window */
	return ret;
}

gboolean
ephy_session_save (EphySession *session,
		   const char *filename)
{
	xmlTextWriterPtr writer;
	GList *w;
	char *save_to;
	int ret;

	LOG ("ephy_sesion_save %s", filename)

	if (session->priv->windows == NULL)
	{
		session_delete (session, filename);
		return TRUE;
	}

	save_to = get_session_filename (filename);

	/* FIXME: do we want to turn on compression? */
	writer = xmlNewTextWriterFilename (save_to, 0);
	if (writer == NULL)
	{
		return FALSE;
	}

	START_PROFILER ("Saving session")

	ret = xmlTextWriterStartDocument (writer, "1.0", NULL, NULL);
	if (ret < 0) goto out;

	/* create and set the root node for the session */
	ret = xmlTextWriterStartElement (writer, "session");
	if (ret < 0) goto out;

	/* iterate through all the windows */
	for (w = session->priv->windows; w != NULL; w = w->next)
	{
		GtkWindow *window = w->data;
	
		if (EPHY_IS_WINDOW (window))
		{
			ret = write_ephy_window (writer, EPHY_WINDOW (window));
		}
		else if (EPHY_IS_BOOKMARKS_EDITOR (window))
		{
			ret = write_tool_window (writer, window, BOOKMARKS_EDITOR_ID);
		}
		else if (EPHY_IS_HISTORY_WINDOW (window))
		{
			ret = write_tool_window (writer, window, HISTORY_WINDOW_ID);
		}
		if (ret < 0) break;
	}
	if (ret < 0) goto out;

	ret = xmlTextWriterEndElement (writer); /* session */
	if (ret < 0) goto out;

	ret = xmlTextWriterEndDocument (writer);

out:
	xmlFreeTextWriter (writer);

	g_free (save_to);

	STOP_PROFILER ("Saving session")

	return ret >= 0 ? TRUE : FALSE;
}

static void
parse_embed (xmlNodePtr child, EphyWindow *window)
{
	while (child != NULL)
	{
		if (strcmp (child->name, "embed") == 0)
		{
			xmlChar *url;

			g_return_if_fail (window != NULL);

			url = xmlGetProp (child, "url");

			ephy_shell_new_tab (ephy_shell, window, NULL, url,
					    EPHY_NEW_TAB_IN_EXISTING_WINDOW |
					    EPHY_NEW_TAB_OPEN_PAGE |
					    EPHY_NEW_TAB_APPEND_LAST);

			xmlFree (url);
		}

		child = child->next;
	}
}

/*
 * ephy_session_load:
 * @session: a #EphySession
 * @filename: the path of the source file
 *
 * Load a session from disk, restoring the windows and their state
 *
 * Return value: TRUE if at least a window has been opened
 **/
gboolean
ephy_session_load (EphySession *session,
		   const char *filename)
{
	xmlDocPtr doc;
        xmlNodePtr child;
	GtkWidget *widget = NULL;
	char *save_to;

	LOG ("ephy_sesion_load %s", filename)

	save_to = get_session_filename (filename);

	doc = xmlParseFile (save_to);
	g_free (save_to);

	if (doc == NULL)
	{
		return FALSE;
	}

	child = xmlDocGetRootElement (doc);

	/* skip the session node */
	child = child->children;

	while (child != NULL)
	{
		if (xmlStrEqual (child->name, "window"))
		{
			widget = GTK_WIDGET (ephy_window_new ());
			parse_embed (child->children, EPHY_WINDOW (widget));
		}
		else if (xmlStrEqual (child->name, "toolwindow"))
		{
			xmlChar *id;

			id = xmlGetProp (child, "id");

			if (id && xmlStrEqual (BOOKMARKS_EDITOR_ID, id))
			{
				widget = ephy_shell_get_bookmarks_editor (ephy_shell);
			}
			else if (id && xmlStrEqual (HISTORY_WINDOW_ID, id))
			{
				widget = ephy_shell_get_history_window (ephy_shell);
			}
		}

		if (widget)
		{
			xmlChar *tmp;
			gulong x = 0, y = 0, width = 0, height = 0;

			tmp = xmlGetProp (child, "x");
			ephy_string_to_int (tmp, &x);
			xmlFree (tmp);
			tmp = xmlGetProp (child, "y");
			ephy_string_to_int (tmp, &y);
			xmlFree (tmp);
			tmp = xmlGetProp (child, "width");
			ephy_string_to_int (tmp, &width);
			xmlFree (tmp);
			tmp = xmlGetProp (child, "height");
			ephy_string_to_int (tmp, &height);
			xmlFree (tmp);
			gtk_window_move (GTK_WINDOW (widget), x, y);
			gtk_window_set_default_size (GTK_WINDOW (widget),
				                     width, height);
			gtk_widget_show (widget);
		}

		child = child->next;
	}

	xmlFreeDoc (doc);

	return (session->priv->windows != NULL);
}

GList *
ephy_session_get_windows (EphySession *session)
{
	g_return_val_if_fail (EPHY_IS_SESSION (session), NULL);

	return g_list_copy (session->priv->windows);
}

/**
 * ephy_session_add_window:
 * @ephy_session: a #EphySession
 * @window: a #EphyWindow
 *
 * Add a tool window to the session. #EphyWindow take care of adding
 * itself to session.
 **/
void
ephy_session_add_window (EphySession *session,
			 GtkWindow *window)
{
	LOG ("ephy_session_add_window")

	session->priv->windows = g_list_prepend (session->priv->windows, window);

	ephy_session_save (session, SESSION_CRASHED);
}

/**
 * ephy_session_remove_window:
 * @session: a #EphySession.
 * @window: a #GtkWindow. This is either the bookmarks editor or the
 * history window.
 *
 * Remove a tool window from the session.
 **/
void
ephy_session_remove_window (EphySession *session,
			    GtkWindow *window)
{
	LOG ("ephy_session_remove_window")

	session->priv->windows = g_list_remove (session->priv->windows, window);

	ephy_session_save (session, SESSION_CRASHED);
}

/**
 * ephy_session_get_active_window:
 * @session: a #EphySession
 *
 * Get the current active browser window. Use it when you
 * need to take an action (like opening an url) on
 * a window but you dont have a target window.
 *
 * Return value: the current active browser window, or NULL of there is none.
 **/
EphyWindow *
ephy_session_get_active_window (EphySession *session)
{
	GList *l;
	EphyWindow *window = NULL;

	g_return_val_if_fail (EPHY_IS_SESSION (session), NULL);

	for (l = session->priv->windows; l != NULL; l = l->next)
	{
		if (EPHY_IS_WINDOW (l->data))
		{
			window = EPHY_WINDOW (l->data);
			break;
		}
	}

	return window;
}
