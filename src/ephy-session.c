/*
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003, 2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004, 2005 Christian Persch
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

#include "ephy-session.h"

#include "ephy-window.h"
#include "ephy-tab.h"
#include "ephy-shell.h"
#include "ephy-history-window.h"
#include "ephy-bookmarks-editor.h"
#include "ephy-file-helpers.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-gui.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>

#define EPHY_SESSION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_SESSION, EphySessionPrivate))

struct _EphySessionPrivate
{
	GList *windows;
	GList *tool_windows;
	GtkWidget *resume_dialog;
	guint dont_save : 1;
	guint quit_while_resuming : 1;
};

#define BOOKMARKS_EDITOR_ID	"BookmarksEditor"
#define HISTORY_WINDOW_ID	"HistoryWindow"
#define SESSION_CRASHED		"type:session_crashed"

static void ephy_session_class_init	(EphySessionClass *klass);
static void ephy_session_iface_init	(EphyExtensionIface *iface);
static void ephy_session_init		(EphySession *session);

enum
{
	PROP_0,
	PROP_ACTIVE_WINDOW
};

static GObjectClass *parent_class = NULL;

GType
ephy_session_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
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
	      EphyTab *tab,
	      EphySession *session)
{
	g_signal_connect (ephy_tab_get_embed (tab), "net-stop",
			  G_CALLBACK (net_stop_cb), session);
}

static void
tab_removed_cb (GtkWidget *notebook,
		EphyTab *tab,
		EphySession *session)
{
	ephy_session_save (session, SESSION_CRASHED);

	g_signal_handlers_disconnect_by_func
		(ephy_tab_get_embed (tab), G_CALLBACK (net_stop_cb), session);
}

static void
tabs_reordered_cb (GtkWidget *notebook,
		   EphySession *session)
{
	ephy_session_save (session, SESSION_CRASHED);
}

static gboolean
window_focus_in_event_cb (EphyWindow *window,
			  GdkEventFocus *event,
			  EphySession *session)
{
	LOG ("focus-in-event for window %p", window);

	g_return_val_if_fail (g_list_find (session->priv->windows, window) != NULL, FALSE);

	/* move the active window to the front of the list */
	session->priv->windows = g_list_remove (session->priv->windows, window);
	session->priv->windows = g_list_prepend (session->priv->windows, window);

	g_object_notify (G_OBJECT (session), "active-window");

	/* propagate event */
	return FALSE;
}

static void
impl_attach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphySession *session = EPHY_SESSION (extension);
	GtkWidget *notebook;

	LOG ("impl_attach_window");

	session->priv->windows = g_list_append (session->priv->windows, window);
	ephy_session_save (session, SESSION_CRASHED);

	g_signal_connect (window, "focus-in-event",
			  G_CALLBACK (window_focus_in_event_cb), session);

	notebook = ephy_window_get_notebook (window);
	g_signal_connect (notebook, "tab-added",
			  G_CALLBACK (tab_added_cb), session);
	g_signal_connect (notebook, "tab-removed",
			  G_CALLBACK (tab_removed_cb), session);
	g_signal_connect (notebook, "tabs-reordered",
			  G_CALLBACK (tabs_reordered_cb), session);

	/* Set unique identifier as role, so that on restore, the WM can
	 * place the window on the right workspace
	 */

	if (gtk_window_get_role (GTK_WINDOW (window)) == NULL)
	{
		/* I guess rand() is unique enough, otherwise we could use
		 * time + pid or something
		 */
		char *role;

		role = g_strdup_printf ("epiphany-window-%x", rand());
		gtk_window_set_role (GTK_WINDOW (window), role);
		g_free (role);
	}
}

static void
impl_detach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphySession *session = EPHY_SESSION (extension);

	LOG ("impl_detach_window");

	session->priv->windows = g_list_remove (session->priv->windows, window);
	ephy_session_save (session, SESSION_CRASHED);

	/* NOTE: since the window will be destroyed anyway, we don't need to
	 * disconnect our signal handlers from its components.
	 */
}

static void
ephy_session_init (EphySession *session)
{
	session->priv = EPHY_SESSION_GET_PRIVATE (session);

	LOG ("EphySession initialising");
}

static void
ephy_session_dispose (GObject *object)
{
	EphySession *session = EPHY_SESSION(object);
	EphySessionPrivate *priv = session->priv;

	LOG ("EphySession disposing");

	/* Only remove the crashed session if we're not shutting down while
	 * the session resume dialogue was still shown!
	*/
	if (priv->quit_while_resuming == FALSE)
	{
		session_delete (session, SESSION_CRASHED);
	}

	parent_class->dispose (object);
}

static void
ephy_session_finalize (GObject *object)
{
	EphySession *session = EPHY_SESSION (object);

	LOG ("EphySession finalising");

	/* FIXME: those should be NULL already!? */
	g_list_free (session->priv->windows);
	g_list_free (session->priv->tool_windows);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_session_iface_init (EphyExtensionIface *iface)
{
	iface->attach_window = impl_attach_window;
	iface->detach_window = impl_detach_window;
}

static void
ephy_session_set_property (GObject *object,
			   guint prop_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	/* no writeable properties */
	g_return_if_reached ();
}

static void
ephy_session_get_property (GObject *object,
			   guint prop_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	EphySession *session = EPHY_SESSION (object);

	switch (prop_id)
	{
		case PROP_ACTIVE_WINDOW:
			g_value_set_object (value, ephy_session_get_active_window (session));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ephy_session_class_init (EphySessionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose = ephy_session_dispose;
	object_class->finalize = ephy_session_finalize;
	object_class->get_property = ephy_session_get_property;
	object_class->set_property = ephy_session_set_property;

	g_object_class_install_property
		(object_class,
		 PROP_ACTIVE_WINDOW,
		 g_param_spec_object ("active-window",
		 		      "Active Window",
		 		      "The active window",
				      EPHY_TYPE_WINDOW,
				      G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_type_class_add_private (object_class, sizeof (EphySessionPrivate));
}

static gboolean
offer_to_resume (EphySession *session,
		 guint32 user_time)
{
	GtkWidget *dialog;
	int response;

	dialog = gtk_message_dialog_new
		(NULL,
		 GTK_DIALOG_MODAL,
		 GTK_MESSAGE_WARNING,
		 GTK_BUTTONS_NONE,
		 _("Recover previous browser windows and tabs?"));

	gtk_message_dialog_format_secondary_text
		(GTK_MESSAGE_DIALOG (dialog),
		 _("Epiphany appears to have exited unexpectedly the last time "
		   "it was run. You can recover the opened windows and tabs."));
	
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_Don't Recover"), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_Recover"), GTK_RESPONSE_ACCEPT);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Crash Recovery"));
	gtk_window_set_icon_name (GTK_WINDOW (dialog), "web-browser");
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

	session->priv->resume_dialog = dialog;

	ephy_gui_window_update_user_time (session->priv->resume_dialog,
					  user_time);

	gtk_window_set_auto_startup_notification (FALSE);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_window_set_auto_startup_notification (TRUE);

	gtk_widget_destroy (dialog);

	session->priv->resume_dialog = NULL;

	return (response == GTK_RESPONSE_ACCEPT);
}

/**
 * ephy_session_autoresume:
 * @session: a #EphySession
 * @user_time: a timestamp, or 0
 *
 * Resume a crashed session when necessary (interactive)
 *
 * Return value: TRUE if handled; windows have actually
 * been opened or the dialog from a previous instance
 * has been re-presented to the user.
 **/
gboolean
ephy_session_autoresume (EphySession *session,
			 guint32 user_time)
{
	EphySessionPrivate *priv = session->priv;
	char *saved_session;
	gboolean retval = FALSE;

	LOG ("ephy_session_autoresume");

	if (priv->windows != NULL || priv->tool_windows != NULL) return FALSE;

	if (priv->resume_dialog)
	{
		ephy_gui_window_update_user_time (priv->resume_dialog,
						  user_time);
		ephy_gui_window_present (GTK_WINDOW (priv->resume_dialog),
					 user_time);
		return TRUE;
	}

	saved_session = get_session_filename (SESSION_CRASHED);

	if (g_file_test (saved_session, G_FILE_TEST_EXISTS)
	    && offer_to_resume (session, user_time))
	{
		priv->dont_save = TRUE;
		retval = ephy_session_load (session, saved_session,
					    0 /* since we've shown the dialogue */);
		priv->dont_save = FALSE;
		ephy_session_save (session, SESSION_CRASHED);
	}

	g_free (saved_session);

	/* ensure we don't open a blank window when quitting while resuming */
	return retval || priv->quit_while_resuming;
}

static void
close_dialog (GtkWidget *widget)
{
	if (GTK_IS_DIALOG (widget))
	{
		/* don't destroy them, someone might have a ref on them */
		gtk_dialog_response (GTK_DIALOG (widget),
				     GTK_RESPONSE_DELETE_EVENT);
	}
}

void
ephy_session_close (EphySession *session)
{
	EphySessionPrivate *priv = session->priv;
	GList *windows;

	LOG ("ephy_session_close");

	/* we have to ref the shell or else we may get finalised between
	 * destroying the windows and destroying the tool windows
	 */
	g_object_ref (ephy_shell);

	priv->dont_save = TRUE;
	/* need to set this up here while the dialogue hasn't been killed yet */
	priv->quit_while_resuming = priv->resume_dialog != NULL;	

	ephy_embed_shell_prepare_close (embed_shell);

	/* there may still be windows open, like dialogues posed from
	* web pages, etc. Try to kill them, but be sure NOT to destroy
	* the gtkmozembed offscreen window!
	* Here, we just check if it's a dialogue and close it if it is one.
	*/
	windows = gtk_window_list_toplevels ();
	g_list_foreach (windows, (GFunc) close_dialog, NULL);
	g_list_free (windows);

	windows	= ephy_session_get_windows (session);
	g_list_foreach (windows, (GFunc) gtk_widget_destroy, NULL);
	g_list_free (windows);

	windows = g_list_copy (session->priv->tool_windows);
	g_list_foreach (windows, (GFunc) gtk_widget_destroy, NULL);
	g_list_free (windows);	

	ephy_embed_shell_prepare_close (embed_shell);

	/* Just to be really sure, do it again: */
	windows = gtk_window_list_toplevels ();
	g_list_foreach (windows, (GFunc) close_dialog, NULL);
	g_list_free (windows);

	session->priv->dont_save = FALSE;
	g_object_unref (ephy_shell);
}

static int
write_tab (xmlTextWriterPtr writer,
	   EphyTab *tab)
{
	const char *address, *title;
	int ret;

	ret = xmlTextWriterStartElement (writer, (xmlChar *) "embed");
	if (ret < 0) return ret;

	address = ephy_tab_get_address (tab);
	ret = xmlTextWriterWriteAttribute (writer, (xmlChar *) "url",
					   (const xmlChar *) address);
	if (ret < 0) return ret;

	title = ephy_tab_get_title (tab);
	ret = xmlTextWriterWriteAttribute (writer, (xmlChar *) "title",
					   (const xmlChar *) title);
	if (ret < 0) return ret;

	if (ephy_tab_get_load_status (tab))
	{
		ret = xmlTextWriterWriteAttribute (writer,
						   (const xmlChar *) "loading",
						   (const xmlChar *) "true");
		if (ret < 0) return ret;
	}

	ret = xmlTextWriterEndElement (writer); /* embed */
	return ret;
}

static int
write_window_geometry (xmlTextWriterPtr writer,
		       GtkWindow *window)
{
	int x = 0, y = 0, width = -1, height = -1;
	int ret;

	/* get window geometry */
	gtk_window_get_size (window, &width, &height);
	gtk_window_get_position (window, &x, &y);

	/* set window properties */
	ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *) "x", "%d", x);
	if (ret < 0) return ret;

	ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *) "y", "%d", y);
	if (ret < 0) return ret;

	ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *) "width", "%d", width);
	if (ret < 0) return ret;

	ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *) "height", "%d", height);
	return ret;
}

static int
write_tool_window (xmlTextWriterPtr writer,
		   GtkWindow *window)
{
	const xmlChar *id;
	int ret;

	if (EPHY_IS_BOOKMARKS_EDITOR (window))
	{
		id = (const xmlChar *) BOOKMARKS_EDITOR_ID;
	}
	else if (EPHY_IS_HISTORY_WINDOW (window))
	{
		id = (const xmlChar *) HISTORY_WINDOW_ID;
	}
	else
	{
		g_return_val_if_reached (-1);
	}

	ret = xmlTextWriterStartElement (writer, (const xmlChar *) "toolwindow");
	if (ret < 0) return ret;

	ret = xmlTextWriterWriteAttribute (writer, (const xmlChar *) "id", id);
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
	const char *role;
	int ret;

	tabs = ephy_window_get_tabs (window);

	/* Do not save an empty EphyWindow.
	 * This only happens when the window was newly opened.
	 */
	if (tabs == NULL) return 0;

	ret = xmlTextWriterStartElement (writer, (xmlChar *) "window");
	if (ret < 0) return ret;

	ret = write_window_geometry (writer, GTK_WINDOW (window));
	if (ret < 0) return ret;

	role = gtk_window_get_role (GTK_WINDOW (window));
	if (role != NULL)
	{
		ret = xmlTextWriterWriteAttribute (writer, 
						   (const xmlChar *)"role", 
						   (const xmlChar *)role);
		if (ret < 0) return ret;
	}

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
	EphySessionPrivate *priv = session->priv;
	xmlTextWriterPtr writer;
	GList *w;
	char *save_to, *tmp_file;
	int ret;

	if (priv->dont_save)
	{
		return TRUE;
	}

	LOG ("ephy_sesion_save %s", filename);

	if (priv->windows == NULL && priv->tool_windows == NULL)
	{
		session_delete (session, filename);
		return TRUE;
	}

	save_to = get_session_filename (filename);
	tmp_file = g_strconcat (save_to, ".tmp", NULL);

	/* FIXME: do we want to turn on compression? */
	writer = xmlNewTextWriterFilename (tmp_file, 0);
	if (writer == NULL)
	{
		g_free (save_to);
		g_free (tmp_file);

		return FALSE;
	}

	ret = xmlTextWriterSetIndent (writer, 1);
	if (ret < 0) goto out;

	ret = xmlTextWriterSetIndentString (writer, (const xmlChar *) "  ");
	if (ret < 0) goto out;

	START_PROFILER ("Saving session")

	ret = xmlTextWriterStartDocument (writer, "1.0", NULL, NULL);
	if (ret < 0) goto out;

	/* create and set the root node for the session */
	ret = xmlTextWriterStartElement (writer, (const xmlChar *) "session");
	if (ret < 0) goto out;

	/* iterate through all the windows */	
	for (w = session->priv->windows; w != NULL && ret >= 0; w = w->next)
	{
		ret = write_ephy_window (writer, EPHY_WINDOW (w->data));
	}
	if (ret < 0) goto out;

	for (w = session->priv->tool_windows; w != NULL && ret >= 0; w = w->next)
	{
		ret = write_tool_window (writer, GTK_WINDOW (w->data));
	}
	if (ret < 0) goto out;

	ret = xmlTextWriterEndElement (writer); /* session */
	if (ret < 0) goto out;

	ret = xmlTextWriterEndDocument (writer);

out:
	xmlFreeTextWriter (writer);

	if (ret >= 0)
	{
		if (ephy_file_switch_temp_file (save_to, tmp_file) == FALSE)
		{
			ret = -1;
		}
	}

	g_free (save_to);
	g_free (tmp_file);

	STOP_PROFILER ("Saving session")

	return ret >= 0 ? TRUE : FALSE;
}

static void
parse_embed (xmlNodePtr child,
	     EphyWindow *window,
	     EphySession *session)
{
	EphySessionPrivate *priv = session->priv;

	while (child != NULL)
	{
		if (strcmp ((char *) child->name, "embed") == 0)
		{
			xmlChar *url, *attr;
			char *recover_url, *freeme = NULL;
			gboolean was_loading;

			g_return_if_fail (window != NULL);

			/* Check if that tab wasn't fully loaded yet when the session crashed */
			attr = xmlGetProp (child, (const xmlChar *) "loading");
			was_loading = attr != NULL &&
				      xmlStrEqual (attr, (const xmlChar *) "true");
			xmlFree (attr);

			url = xmlGetProp (child, (const xmlChar *) "url");
			if (url == NULL) continue;

			if (!was_loading ||
			    strcmp ((const char *) url, "about:blank") == 0)
			{
				recover_url = (char *) url;
			}
			else
			{
				xmlChar *title;
				char *escaped_url, *escaped_title;

				title = xmlGetProp (child, (const xmlChar *) "title");
				escaped_title = gnome_vfs_escape_string (title ? (const char*) title : _("Untitled"));

				escaped_url = gnome_vfs_escape_string ((const char *) url);
				freeme = recover_url =
					g_strconcat ("about:recover?u=",
						     escaped_url,
						     "&c=UTF-8&t=",
						     escaped_title, NULL);

				xmlFree (title);
				g_free (escaped_url);
				g_free (escaped_title);
			}

			ephy_shell_new_tab (ephy_shell, window, NULL, recover_url,
					    EPHY_NEW_TAB_IN_EXISTING_WINDOW |
					    EPHY_NEW_TAB_OPEN_PAGE |
					    EPHY_NEW_TAB_APPEND_LAST);

			xmlFree (url);
			g_free (freeme);
		}

		child = child->next;
	}
}

static gboolean
int_from_string (const char *string,
		 int *retval)
{
	char *tail = NULL;
	long int val;
	gboolean success = FALSE;

	errno = 0;
	val = strtol (string, &tail, 0);

	if (errno == 0 && tail != NULL && tail[0] == '\0')
	{
		*retval = (int) val;
		success = TRUE;
	}

	return success;
}

static void
restore_geometry (GtkWindow *window,
		  xmlNodePtr node)
{
	xmlChar *tmp;
	int x = 0, y = 0, width = -1, height = -1;
	gboolean success = TRUE;

	g_return_if_fail (window != NULL);

	tmp = xmlGetProp (node, (xmlChar *) "x");
	success &= int_from_string ((char *) tmp, &x);
	xmlFree (tmp);
	tmp = xmlGetProp (node, (xmlChar *) "y");
	success &= int_from_string ((char *) tmp, &y);
	xmlFree (tmp);
	tmp = xmlGetProp (node, (xmlChar *) "width");
	success &= int_from_string ((char *) tmp, &width);
	xmlFree (tmp);
	tmp = xmlGetProp (node, (xmlChar *) "height");
	success &= int_from_string ((char *) tmp, &height);
	xmlFree (tmp);

	if (success)
	{
		tmp = xmlGetProp (node, (xmlChar *)"role");
		if (tmp != NULL)
		{
			gtk_window_set_role (GTK_WINDOW (window), (const char *)tmp);
			xmlFree (tmp);
		}

		gtk_window_move (window, x, y);
		gtk_window_set_default_size (window, width, height);
		
	}
}

/*
 * ephy_session_load:
 * @session: a #EphySession
 * @filename: the path of the source file
 * @user_time: a timestamp, or 0
 *
 * Load a session from disk, restoring the windows and their state
 *
 * Return value: TRUE if at least a window has been opened
 **/
gboolean
ephy_session_load (EphySession *session,
		   const char *filename,
		   guint32 user_time)
{
	xmlDocPtr doc;
        xmlNodePtr child;
	EphyWindow *window;
	GtkWidget *widget = NULL;
	char *save_to;

	LOG ("ephy_sesion_load %s", filename);

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
		if (xmlStrEqual (child->name, (const xmlChar *) "window"))
		{
			window = ephy_window_new ();
			widget = GTK_WIDGET (window);
			restore_geometry (GTK_WINDOW (widget), child);

			ephy_gui_window_update_user_time (widget, user_time);

			/* Now add the tabs */
			parse_embed (child->children, window, session);

			/* Set focus to something sane */
			gtk_widget_grab_focus (ephy_window_get_notebook (window));

			gtk_widget_show (widget);
		}
		else if (xmlStrEqual (child->name, (const xmlChar *) "toolwindow"))
		{
			xmlChar *id;

			id = xmlGetProp (child, (const xmlChar *) "id");

			if (id && xmlStrEqual ((const xmlChar *) BOOKMARKS_EDITOR_ID, id))
			{
				if (!eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_BOOKMARK_EDITING))
				{
					widget = ephy_shell_get_bookmarks_editor (ephy_shell);
				}
			}
			else if (id && xmlStrEqual ((const xmlChar *) HISTORY_WINDOW_ID, id))
			{
				if (!eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_HISTORY))
				{
					widget = ephy_shell_get_history_window (ephy_shell);
				}
			}

			restore_geometry (GTK_WINDOW (widget), child);

			ephy_gui_window_update_user_time (widget, user_time);

			gtk_widget_show (widget);
		}

		child = child->next;
	}

	xmlFreeDoc (doc);

	return (session->priv->windows != NULL || session->priv->tool_windows != NULL);
}

/**
 * ephy_session_get_windows:
 * @ephy_session: the #EphySession
 *
 * Returns: the list of open #EphyWindow:s.
 **/
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
	LOG ("ephy_session_add_window %p", window);

	session->priv->tool_windows =
		g_list_append (session->priv->tool_windows, window);

	ephy_session_save (session, SESSION_CRASHED);
}

/**
 * ephy_session_remove_window:
 * @session: a #EphySession.
 * @window: a #GtkWindow, which must be either the bookmarks editor or the
 * history window.
 *
 * Remove a tool window from the session.
 **/
void
ephy_session_remove_window (EphySession *session,
			    GtkWindow *window)
{
	LOG ("ephy_session_remove_window %p", window);

	session->priv->tool_windows =
		g_list_remove (session->priv->tool_windows, window);

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
 * Return value: the current active non-popup browser window, or NULL of there is none.
 **/
EphyWindow *
ephy_session_get_active_window (EphySession *session)
{
	EphyWindow *window = NULL, *w;
	GList *l;

	g_return_val_if_fail (EPHY_IS_SESSION (session), NULL);

	for (l = session->priv->windows; l != NULL; l = l->next)
	{
		w = EPHY_WINDOW (l->data);

		if (ephy_window_get_is_popup (w) == FALSE)
		{
			window = w;
			break;
		}
	}

	return window;
}
