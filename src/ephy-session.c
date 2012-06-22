/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2002 Jorn Baayen
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005, 2006, 2008 Christian Persch
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ephy-session.h"

#include "ephy-bookmarks-editor.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-utils.h"
#include "ephy-embed.h"
#include "ephy-extension.h"
#include "ephy-file-helpers.h"
#include "ephy-gui.h"
#include "ephy-history-window.h"
#include "ephy-prefs.h"
#include "ephy-about-handler.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-window.h"

#include <errno.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	EphySessionCommand command;
	char *arg;
	char **args;
	guint32 user_time;
} SessionCommand;

#define EPHY_SESSION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_SESSION, EphySessionPrivate))

struct _EphySessionPrivate
{
	GList *windows;
	GList *tool_windows;
	GtkWidget *resume_window;

	GQueue *queue;
	guint queue_idle_id;

	guint dont_save : 1;
};

#define BOOKMARKS_EDITOR_ID	"BookmarksEditor"
#define HISTORY_WINDOW_ID	"HistoryWindow"
#define SESSION_STATE		"type:session_state"

static void ephy_session_class_init	(EphySessionClass *klass);
static void ephy_session_iface_init	(EphyExtensionIface *iface);
static void ephy_session_init		(EphySession *session);
static void session_command_queue_next	(EphySession *session);

enum
{
	PROP_0,
	PROP_ACTIVE_WINDOW
};

G_DEFINE_TYPE_WITH_CODE (EphySession, ephy_session, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (EPHY_TYPE_EXTENSION,
						ephy_session_iface_init))

/* Helper functions */

static GFile *
get_session_file (const char *filename)
{
	GFile *file;
	char *path;

	if (filename == NULL)
	{
		return NULL;
	}

	if (strcmp (filename, SESSION_STATE) == 0)
	{
		path = g_build_filename (ephy_dot_dir (),
					 "session_state.xml",
					 NULL);
	}
	else
	{
		path = g_strdup (filename);
	}

	file = g_file_new_for_path (path);
	g_free (path);

	return file;
}

static void
session_delete (EphySession *session,
		const char *filename)
{
	GFile *file;

	file = get_session_file (filename);

	g_file_delete (file, NULL, NULL);
	g_object_unref (file);
}

#ifdef HAVE_WEBKIT2
static void
load_changed_cb (WebKitWebView *view,
		 WebKitLoadEvent load_event,
		 EphySession *session)
{
	if (!ephy_web_view_load_failed (EPHY_WEB_VIEW (view)))
		ephy_session_save (session, SESSION_STATE);
}
#else
static void
load_status_notify_cb (EphyWebView *view,
		       GParamSpec *pspec,
		       EphySession *session)
{
	WebKitLoadStatus status = webkit_web_view_get_load_status (WEBKIT_WEB_VIEW (view));

	/* We won't know the URL we are loading in PROVISIONAL because
	   of bug #593149, but save session anyway */
	if (status == WEBKIT_LOAD_PROVISIONAL ||
	    status == WEBKIT_LOAD_COMMITTED || 
	    status == WEBKIT_LOAD_FINISHED)
		ephy_session_save (session, SESSION_STATE);
}
#endif

static void
notebook_page_added_cb (GtkWidget *notebook,
			EphyEmbed *embed,
			guint position,
			EphySession *session)
{
#ifdef HAVE_WEBKIT2
	g_signal_connect (ephy_embed_get_web_view (embed), "load-changed",
			  G_CALLBACK (load_changed_cb), session);
#else
	g_signal_connect (ephy_embed_get_web_view (embed), "notify::load-status",
			  G_CALLBACK (load_status_notify_cb), session);
#endif
}

static void
notebook_page_removed_cb (GtkWidget *notebook,
			  EphyEmbed *embed,
			  guint position,
			  EphySession *session)
{
	ephy_session_save (session, SESSION_STATE);

#ifdef HAVE_WEBKIT2
	g_signal_handlers_disconnect_by_func
		(ephy_embed_get_web_view (embed), G_CALLBACK (load_changed_cb),
		 session);
#else
	g_signal_handlers_disconnect_by_func
		(ephy_embed_get_web_view (embed), G_CALLBACK (load_status_notify_cb),
		 session);
#endif
}

static void
notebook_page_reordered_cb (GtkWidget *notebook,
			    GtkWidget *tab,
			    guint position,
			    EphySession *session)
{
	ephy_session_save (session, SESSION_STATE);
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

/* Queue worker */

static void
session_command_free (SessionCommand *cmd)
{
	g_assert (cmd != NULL);

	g_free (cmd->arg);
	if (cmd->args)
	{
		g_strfreev (cmd->args);
	}

	g_slice_free (SessionCommand, cmd);

	g_object_unref (ephy_shell_get_default ());
}

static int
session_command_find (const SessionCommand *cmd,
		      gpointer cmdptr)
{
	EphySessionCommand command = GPOINTER_TO_INT (cmdptr);

	return command != cmd->command;
}

static void
session_command_autoresume (EphySession *session,
			    guint32 user_time)
{
	EphySessionPrivate *priv = session->priv;
	GFile *saved_session_file;
	char *saved_session_file_path;
	gboolean crashed_session;
	EphyPrefsRestoreSessionPolicy policy;

	LOG ("ephy_session_autoresume");

	saved_session_file = get_session_file (SESSION_STATE);
	saved_session_file_path = g_file_get_path (saved_session_file);
	g_object_unref (saved_session_file);
	crashed_session = g_file_test (saved_session_file_path, G_FILE_TEST_EXISTS);
	
	g_free (saved_session_file_path);

	policy = g_settings_get_enum (EPHY_SETTINGS_MAIN,
				      EPHY_PREFS_RESTORE_SESSION_POLICY);

	if (crashed_session == FALSE ||
	    policy == EPHY_PREFS_RESTORE_SESSION_POLICY_NEVER ||
	    priv->windows != NULL ||
	    priv->tool_windows != NULL)
	{
		/* If we are auto-resuming, and we never want to
		 * restore the session, clobber the session state
		 * file. */
		if (policy == EPHY_PREFS_RESTORE_SESSION_POLICY_NEVER)
			session_delete (session, SESSION_STATE);

		ephy_session_queue_command (session,
					    EPHY_SESSION_CMD_MAYBE_OPEN_WINDOW,
					    NULL, NULL, user_time, FALSE);

		return;
	}

	if (priv->resume_window)
	{
		gtk_window_present_with_time (GTK_WINDOW (priv->resume_window),
					      user_time);

		return;
	}

	ephy_session_queue_command (session,
				    EPHY_SESSION_CMD_LOAD_SESSION,
				    SESSION_STATE, NULL, user_time, TRUE);
}

static void
session_command_open_bookmarks_editor (EphySession *session,
				       guint32 user_time)
{
	GtkWidget *editor;

	editor = ephy_shell_get_bookmarks_editor (ephy_shell_get_default ());
	
	gtk_window_present_with_time (GTK_WINDOW (editor), user_time);
}

static void
session_command_open_uris (EphySession *session,
			   char **uris,
			   const char *options,
			   guint32 user_time)
{
	EphyShell *shell;
	EphyWindow *window;
	EphyEmbed *embed;
	EphySessionPrivate *priv;
	EphyNewTabFlags flags = 0;
	guint i;

	priv = session->priv;

	shell = ephy_shell_get_default ();

	g_object_ref (shell);

	window = ephy_session_get_active_window (session);

	if (options != NULL && strstr (options, "external") != NULL)
	{
		flags |= EPHY_NEW_TAB_FROM_EXTERNAL;
	}
	if (options != NULL && strstr (options, "new-window") != NULL)
	{
		window = NULL;
		flags |= EPHY_NEW_TAB_IN_NEW_WINDOW;
	}
	else if (options != NULL && strstr (options, "new-tab") != NULL)
	{
		flags |= EPHY_NEW_TAB_IN_EXISTING_WINDOW |
			 EPHY_NEW_TAB_JUMP;
	}

	for (i = 0; uris[i] != NULL; ++i)
	{
		const char *url = uris[i];
		EphyNewTabFlags page_flags;
#ifdef HAVE_WEBKIT2
		WebKitURIRequest *request = NULL;
#else
		WebKitNetworkRequest *request = NULL;
#endif

		if (url[0] == '\0')
		{
			page_flags = EPHY_NEW_TAB_HOME_PAGE;
		}
		else
		{
			page_flags = EPHY_NEW_TAB_OPEN_PAGE;
#ifdef HAVE_WEBKIT2
			request = webkit_uri_request_new (url);
#else
			request = webkit_network_request_new (url);
#endif
		}

		/* For the first URI, if we have a valid recovery
		 * window, reuse the already existing embed instead of
		 * creating a new one, except if we still want to
		 * present the option to resume a crashed session, in
		 * that case use a new tab in the same window */
		if (i == 0 && priv->resume_window != NULL)
		{
			EphyWebView *web_view;
			
			embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (priv->resume_window));
			web_view = ephy_embed_get_web_view (embed);
			ephy_web_view_load_url (web_view, url);
		}
		else
		{
			embed = ephy_shell_new_tab_full (shell, window,
							 NULL /* parent tab */,
							 request,
							 flags | page_flags,
							 EPHY_WEB_VIEW_CHROME_ALL,
							 FALSE /* is popup? */,
							 user_time);
		}

		if (request)
			g_object_unref (request);

		window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed)));
	}

	g_object_unref (shell);
}

static gboolean
session_command_dispatch (EphySession *session)
{
	EphySessionPrivate *priv = session->priv;
	SessionCommand *cmd;
	gboolean run_again = TRUE;

	cmd = g_queue_pop_head (priv->queue);
	g_assert (cmd != NULL);

	LOG ("dispatching queue cmd:%d", cmd->command);

	switch (cmd->command)
	{
		case EPHY_SESSION_CMD_RESUME_SESSION:
			session_command_autoresume (session, cmd->user_time);
			break;
		case EPHY_SESSION_CMD_LOAD_SESSION:
			ephy_session_load (session, cmd->arg, cmd->user_time);
			break;
		case EPHY_SESSION_CMD_OPEN_BOOKMARKS_EDITOR:
			session_command_open_bookmarks_editor (session, cmd->user_time);
			break;
		case EPHY_SESSION_CMD_OPEN_URIS:
			session_command_open_uris (session, cmd->args, cmd->arg, cmd->user_time);
			break;
		case EPHY_SESSION_CMD_MAYBE_OPEN_WINDOW:
			/* FIXME: maybe just check for normal windows? */
			if (priv->windows == NULL &&
			    priv->tool_windows == NULL)
			{
				ephy_shell_new_tab_full (ephy_shell_get_default (),
							 NULL /* window */, NULL /* tab */,
							 NULL /* NetworkRequest */,
							 EPHY_NEW_TAB_IN_NEW_WINDOW |
							 EPHY_NEW_TAB_HOME_PAGE,
							 EPHY_WEB_VIEW_CHROME_ALL,
							 FALSE /* is popup? */,
							 cmd->user_time);
			}
			break;
		default:
			g_assert_not_reached ();
			break;
	}

	/* Look if there's anything else to dispatch */
	if (g_queue_is_empty (priv->queue))
	{
		priv->queue_idle_id = 0;
		run_again = FALSE;
	}

	g_application_release (G_APPLICATION (ephy_shell_get_default ()));

	/* This unrefs the shell! */
	session_command_free (cmd);

	return run_again;
}

static void
session_command_queue_next (EphySession *session)
{
	EphySessionPrivate *priv = session->priv;

	LOG ("queue_next");

	if (!g_queue_is_empty (priv->queue) &&
	    priv->queue_idle_id == 0)
	{
		priv->queue_idle_id =
			g_idle_add ((GSourceFunc) session_command_dispatch,
				    session);
	}
}

static void
session_command_queue_clear (EphySession *session)
{
	EphySessionPrivate *priv = session->priv;

	if (priv->queue_idle_id != 0)
	{
		g_source_remove (priv->queue_idle_id);
		priv->queue_idle_id = 0;
	}

	if (priv->queue != NULL)
	{
		g_queue_foreach (priv->queue, (GFunc) session_command_free, NULL);
		g_queue_free (priv->queue);
		priv->queue = NULL;
	}
}

/* EphyExtensionIface implementation */

static void
impl_attach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphySession *session = EPHY_SESSION (extension);
	GtkWidget *notebook;

	LOG ("impl_attach_window");

	session->priv->windows = g_list_append (session->priv->windows, window);
	ephy_session_save (session, SESSION_STATE);

	g_signal_connect (window, "focus-in-event",
			  G_CALLBACK (window_focus_in_event_cb), session);

	notebook = ephy_window_get_notebook (window);
	g_signal_connect (notebook, "page-added",
			  G_CALLBACK (notebook_page_added_cb), session);
	g_signal_connect (notebook, "page-removed",
			  G_CALLBACK (notebook_page_removed_cb), session);
	g_signal_connect (notebook, "page-reordered",
			  G_CALLBACK (notebook_page_reordered_cb), session);

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
	ephy_session_save (session, SESSION_STATE);

	/* NOTE: since the window will be destroyed anyway, we don't need to
	 * disconnect our signal handlers from its components.
	 */
}

/* Class implementation */

static void
ephy_session_init (EphySession *session)
{
	EphySessionPrivate *priv;

	LOG ("EphySession initialising");

	priv = session->priv = EPHY_SESSION_GET_PRIVATE (session);

	priv->queue = g_queue_new ();
}

static void
ephy_session_dispose (GObject *object)
{
	EphySession *session = EPHY_SESSION (object);

	LOG ("EphySession disposing");

	session_command_queue_clear (session);

	G_OBJECT_CLASS (ephy_session_parent_class)->dispose (object);
}

static void
ephy_session_finalize (GObject *object)
{
	EphySession *session = EPHY_SESSION (object);

	LOG ("EphySession finalising");

	/* FIXME: those should be NULL already!? */
	g_list_free (session->priv->windows);
	g_list_free (session->priv->tool_windows);

	G_OBJECT_CLASS (ephy_session_parent_class)->finalize (object);
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

/* Implementation */

void
ephy_session_close (EphySession *session)
{
	EphyPrefsRestoreSessionPolicy policy;

	LOG ("ephy_session_close");

	policy = g_settings_get_enum (EPHY_SETTINGS_MAIN,
				      EPHY_PREFS_RESTORE_SESSION_POLICY);
	if (policy == EPHY_PREFS_RESTORE_SESSION_POLICY_ALWAYS)
	{
		EphySessionPrivate *priv = session->priv;

		priv->dont_save = TRUE;

		session_command_queue_clear (session);

		ephy_embed_shell_prepare_close (embed_shell);

	}
}

static int
write_tab (xmlTextWriterPtr writer,
	   EphyEmbed *embed)
{
	const char *address, *title;
	char *new_address = NULL;
	int ret;

	ret = xmlTextWriterStartElement (writer, (xmlChar *) "embed");
	if (ret < 0) return ret;

	address = ephy_web_view_get_address (ephy_embed_get_web_view (embed));
	/* Do not store ephy-about: URIs, they are not valid for
	 * loading. */
	if (g_str_has_prefix (address, EPHY_ABOUT_SCHEME))
	{
		new_address = g_strconcat ("about", address + EPHY_ABOUT_SCHEME_LEN, NULL);
	}
	ret = xmlTextWriterWriteAttribute (writer, (xmlChar *) "url",
					   (const xmlChar *) (new_address ? new_address : address));
	g_free (new_address);
	if (ret < 0) return ret;

	title = ephy_web_view_get_title (ephy_embed_get_web_view (embed));
	ret = xmlTextWriterWriteAttribute (writer, (xmlChar *) "title",
					   (const xmlChar *) title);
	if (ret < 0) return ret;

	if (ephy_web_view_is_loading (ephy_embed_get_web_view (embed)))
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
write_active_tab (xmlTextWriterPtr writer,
		  GtkWidget *notebook)
{
	int ret;
	int current;

	current = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
    
	ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *) "active-tab", "%d", current);
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
	GtkWidget *notebook;
	const char *role;
	int ret;

	tabs = ephy_embed_container_get_children (EPHY_EMBED_CONTAINER (window));
	notebook = ephy_window_get_notebook (window);

	/* Do not save an empty EphyWindow.
	 * This only happens when the window was newly opened.
	 */
	if (tabs == NULL) return 0;

	ret = xmlTextWriterStartElement (writer, (xmlChar *) "window");
	if (ret < 0) return ret;

	ret = write_window_geometry (writer, GTK_WINDOW (window));
	if (ret < 0) return ret;

	ret = write_active_tab (writer, notebook);
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
		EphyEmbed *embed = EPHY_EMBED (l->data);
		ret = write_tab (writer, embed);
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
	EphySessionPrivate *priv;
	xmlTextWriterPtr writer;
	GList *w;
	GFile *save_to_file, *tmp_file;
	char *tmp_file_path, *save_to_file_path;
	int ret;

	g_return_val_if_fail (EPHY_IS_SESSION (session), FALSE);

	priv = session->priv;

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

	save_to_file = get_session_file (filename);
	save_to_file_path = g_file_get_path (save_to_file);
	tmp_file_path = g_strconcat (save_to_file_path, ".tmp", NULL);
	g_free (save_to_file_path);
	tmp_file = g_file_new_for_path (tmp_file_path);

	/* FIXME: do we want to turn on compression? */
	writer = xmlNewTextWriterFilename (tmp_file_path, 0);
	if (writer == NULL)
	{
		g_free (tmp_file_path);

		return FALSE;
	}

	ret = xmlTextWriterSetIndent (writer, 1);
	if (ret < 0) goto out;

	ret = xmlTextWriterSetIndentString (writer, (const xmlChar *) "	 ");
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
		if (ephy_file_switch_temp_file (save_to_file, tmp_file) == FALSE)
		{
			ret = -1;
		}
	}

	g_free (tmp_file_path);
	g_object_unref (save_to_file);
	g_object_unref (tmp_file);

	STOP_PROFILER ("Saving session")

	return ret >= 0 ? TRUE : FALSE;
}

static void
confirm_before_recover (EphyWindow* window, char* url, char* title)
{
	EphyEmbed *embed;

	embed = ephy_shell_new_tab (ephy_shell, window, NULL, NULL,
				    EPHY_NEW_TAB_IN_EXISTING_WINDOW |
				    EPHY_NEW_TAB_APPEND_LAST);

	ephy_web_view_load_error_page (ephy_embed_get_web_view (embed), url,
			               EPHY_WEB_VIEW_ERROR_PAGE_CRASH, NULL);
}

static void 
parse_embed (xmlNodePtr child,
	     EphyWindow *window,
	     EphySession *session)
{
	EphySessionPrivate *priv = session->priv;
	gboolean is_first_window;

	is_first_window = window == EPHY_WINDOW (priv->resume_window);

	while (child != NULL)
	{
		if (strcmp ((char *) child->name, "embed") == 0)
		{
			xmlChar *url, *attr;
			char *recover_url;
			gboolean was_loading;

			g_return_if_fail (window != NULL);

			/* Check if that tab wasn't fully loaded yet
			 * when the session crashed. */
			attr = xmlGetProp (child, (const xmlChar *) "loading");
			was_loading = attr != NULL &&
				      xmlStrEqual (attr, (const xmlChar *) "true");
			xmlFree (attr);

			url = xmlGetProp (child, (const xmlChar *) "url");
			if (url == NULL) 
				continue;

			/* In the case that crash happens before we receive the URL from the server, this will
			   open an about:blank tab. See http://bugzilla.gnome.org/show_bug.cgi?id=591294
			   Otherwise, if the web was fully loaded, it is reloaded again. */
			if (!was_loading ||
			    strcmp ((const char *) url, "about:blank") == 0)
			{
				recover_url = (char *) url;
				
				/* Reuse the window holding the recovery infobar instead of creating a new one. */
				if (is_first_window == TRUE && window != NULL)
				{
					EphyWebView *web_view;
					EphyEmbed *embed;

					embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
					web_view = ephy_embed_get_web_view (embed);
					ephy_web_view_load_url (web_view, recover_url);

					is_first_window = FALSE;
				}
				else
				{
					ephy_shell_new_tab (ephy_shell, window, NULL, recover_url,
							    EPHY_NEW_TAB_IN_EXISTING_WINDOW |
							    EPHY_NEW_TAB_OPEN_PAGE |
							    EPHY_NEW_TAB_APPEND_LAST);
				}
			}
			else if (was_loading && url != NULL &&
				 strcmp ((const char *) url, "about:blank") != 0)
			{
				/* Shows a message to the user that warns that this page was
				   loading during crash and make Epiphany crash again,
				   in this case we know the URL. */
				xmlChar* title = xmlGetProp (child, (const xmlChar *) "title");
			
				confirm_before_recover (window, (char*) url, (char*) title);
			}

			xmlFree (url);
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

	if (string == NULL) return FALSE;

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

/**
 * ephy_session_load_from_string:
 * @session: an #EphySession
 * @session_data: a string with the session data to load
 * @length: the length of @session_data, or -1 to assume %NULL terminated
 * @user_time: a user time, or 0
 * 
 * Loads the session described in @session_data, restoring windows and
 * their state.
 * 
 * Returns: TRUE if at least a window has been opened
 **/
gboolean
ephy_session_load_from_string (EphySession *session,
			       const char *session_data,
			       gssize length,
			       guint32 user_time)
{
	EphySessionPrivate *priv;
	xmlDocPtr doc;
	xmlNodePtr child;
	EphyWindow *window;
	GtkWidget *widget = NULL;
	gboolean first_window_created = FALSE;
	
	g_return_val_if_fail (EPHY_IS_SESSION (session), FALSE);
	g_return_val_if_fail (session_data, FALSE);

	priv = session->priv;

	/* If length is -1 assume the data is a NULL-terminated, UTF-8
	 * encoded string. */
	if (length == -1)
		length = g_utf8_strlen (session_data, -1);

	doc = xmlParseMemory (session_data, (int)length);

	if (doc == NULL)
	{
		return FALSE;
	}

	g_object_ref (ephy_shell_get_default ());

	priv->dont_save = TRUE;

	child = xmlDocGetRootElement (doc);

	/* skip the session node */
	child = child->children;

	while (child != NULL)
	{
		if (xmlStrEqual (child->name, (const xmlChar *) "window"))
		{
			xmlChar *tmp;
			EphyEmbed *active_child;
		    
			if (first_window_created == FALSE && priv->resume_window != NULL)
			{
				window = EPHY_WINDOW (priv->resume_window);
				first_window_created = TRUE;
			}
			else
				window = ephy_window_new ();

			widget = GTK_WIDGET (window);
			restore_geometry (GTK_WINDOW (widget), child);

			ephy_gui_window_update_user_time (widget, user_time);

			/* Now add the tabs */
			parse_embed (child->children, window, session);

			/* Set focus to something sane */
			tmp = xmlGetProp (child, (xmlChar *) "active-tab");
			if (tmp != NULL)
			{
				gboolean success;
				int active_tab;

				success = int_from_string ((char *) tmp, &active_tab);
				xmlFree (tmp);
				if (success)
				{
					GtkWidget *notebook;
					notebook = ephy_window_get_notebook (window);
					gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), active_tab);
				}
			}

			if (ephy_embed_shell_get_mode (embed_shell) != EPHY_EMBED_SHELL_MODE_TEST)
			{
				active_child = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
				gtk_widget_grab_focus (GTK_WIDGET (active_child));
				gtk_widget_show (widget);
			}
		}
		else if (xmlStrEqual (child->name, (const xmlChar *) "toolwindow"))
		{
			xmlChar *id;

			id = xmlGetProp (child, (const xmlChar *) "id");

			if (id && xmlStrEqual ((const xmlChar *) BOOKMARKS_EDITOR_ID, id))
			{
				if (!g_settings_get_boolean
				    (EPHY_SETTINGS_LOCKDOWN,
				     EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING))
				{
					widget = ephy_shell_get_bookmarks_editor (ephy_shell);
				}
			}
			else if (id && xmlStrEqual ((const xmlChar *) HISTORY_WINDOW_ID, id))
			{
				if (!g_settings_get_boolean
				    (EPHY_SETTINGS_LOCKDOWN,
				     EPHY_PREFS_LOCKDOWN_HISTORY))
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

	priv->dont_save = FALSE;
	priv->resume_window = NULL;

	ephy_session_save (session, SESSION_STATE);

	g_object_unref (ephy_shell_get_default ());

	return (priv->windows != NULL || priv->tool_windows != NULL);
}

/**
 * ephy_session_load:
 * @session: a #EphySession
 * @filename: the path of the source file
 * @user_time: a user_time, or 0
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
	GFile *save_to_file;
	char *save_to_path;
	gboolean ret_value;
	char *contents;
	gsize length;
	GError *error = NULL;

	g_return_val_if_fail (EPHY_IS_SESSION (session), FALSE);
	g_return_val_if_fail (filename, FALSE);

	LOG ("ephy_sesion_load %s", filename);

	save_to_file = get_session_file (filename);
	save_to_path = g_file_get_path (save_to_file);
	g_object_unref (save_to_file);

	if (!g_file_get_contents (save_to_path, &contents, &length, &error))
	{
		LOG ("Could not load session, error reading session file: %s", error->message);
		g_error_free (error);
		g_free (save_to_path);

		return FALSE;
	}

	ret_value = ephy_session_load_from_string (session, contents, length, user_time);

	g_free (contents);
	g_free (save_to_path);

	return ret_value;
}

/**
 * ephy_session_get_windows:
 * @session: the #EphySession
 *
 * Returns: (element-type EphyWindow) (transfer container): the list of
 *          open #EphyWindow:s.
 **/
GList *
ephy_session_get_windows (EphySession *session)
{
	g_return_val_if_fail (EPHY_IS_SESSION (session), NULL);

	return g_list_copy (session->priv->windows);
}

/**
 * ephy_session_add_window:
 * @session: a #EphySession
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
	gtk_application_add_window (GTK_APPLICATION (ephy_shell_get_default ()), window);

	ephy_session_save (session, SESSION_STATE);
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
	gtk_application_remove_window (GTK_APPLICATION (ephy_shell_get_default ()), window);

	ephy_session_save (session, SESSION_STATE);
}

/**
 * ephy_session_get_active_window:
 * @session: a #EphySession
 *
 * Get the current active browser window. Use it when you
 * need to take an action (like opening an url) on
 * a window but you dont have a target window.
 *
 * Return value: (transfer none): the current active non-popup browser
 *               window, or NULL of there is none.
 **/
EphyWindow *
ephy_session_get_active_window (EphySession *session)
{
	EphyWindow *window = NULL;
	EphyEmbedContainer *w;
	GList *l;

	g_return_val_if_fail (EPHY_IS_SESSION (session), NULL);

	for (l = session->priv->windows; l != NULL; l = l->next)
	{
		w = EPHY_EMBED_CONTAINER (l->data);

		if (ephy_embed_container_get_is_popup (w) == FALSE)
		{
			window = EPHY_WINDOW (w);
			break;
		}
	}

	return window;
}

/**
 * ephy_session_queue_command:
 * @session: a #EphySession
 **/
void
ephy_session_queue_command (EphySession *session,
			    EphySessionCommand command,
			    const char *arg,
			    const char **args,
			    guint32 user_time,
			    gboolean priority)
{
	EphySessionPrivate *priv;
	GList *element;
	SessionCommand *cmd;

	LOG ("queue_command command:%d", command);

	g_return_if_fail (EPHY_IS_SESSION (session));
	g_return_if_fail (command != EPHY_SESSION_CMD_OPEN_URIS || args != NULL);

	priv = session->priv;

	/* First look if the same command is already queued */
	if (command > EPHY_SESSION_CMD_RESUME_SESSION &&
	    command < EPHY_SESSION_CMD_OPEN_URIS)
	{
		element = g_queue_find_custom (priv->queue,
					       GINT_TO_POINTER (command),
					       (GCompareFunc) session_command_find);
		if (element != NULL)
		{
			cmd = (SessionCommand *) element->data;

			if ((command == EPHY_SESSION_CMD_LOAD_SESSION &&
			     strcmp (cmd->arg, arg) == 0) ||
			    command == EPHY_SESSION_CMD_OPEN_BOOKMARKS_EDITOR ||
			    command == EPHY_SESSION_CMD_RESUME_SESSION)
			{
				cmd->user_time = user_time;
				g_queue_remove (priv->queue, cmd);
				g_queue_push_tail (priv->queue, cmd);

				return;
			}
		}
	}

	cmd = g_slice_new0 (SessionCommand);
	cmd->command = command;
	cmd->arg = arg ? g_strdup (arg) : NULL;
	cmd->args = args ? g_strdupv ((gchar **)args) : NULL;
	cmd->user_time = user_time;
	/* This ref is released in session_command_free */
	g_object_ref (ephy_shell_get_default ());

	if (priority)
	{
		g_queue_push_head (priv->queue, cmd);
	}
	else
	{
		g_queue_push_tail (priv->queue, cmd);
	}

	session_command_queue_next (session);

	g_application_hold (G_APPLICATION (ephy_shell_get_default ()));

	if (priv->resume_window != NULL)
	{
		gtk_window_present_with_time (GTK_WINDOW (priv->resume_window),
					      user_time);
	}
}
