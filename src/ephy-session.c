/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2002 Jorn Baayen
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005, 2006, 2008 Christian Persch
 *  Copyright © 2012 Igalia S.L.
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

#include "ephy-about-handler.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-utils.h"
#include "ephy-embed.h"
#include "ephy-file-helpers.h"
#include "ephy-gui.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-window.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>

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
	GQueue *queue;
	guint queue_idle_id;

	guint dont_save : 1;
};

#define SESSION_STATE		"type:session_state"

G_DEFINE_TYPE (EphySession, ephy_session, G_TYPE_OBJECT)

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

static void
session_command_open_uris (EphySession *session,
			   char **uris,
			   const char *options,
			   guint32 user_time)
{
	EphyShell *shell;
	EphyWindow *window;
	EphyEmbed *embed;
	EphyNewTabFlags flags = 0;
	guint i;
	gboolean new_windows_in_tabs;
	gboolean have_uris;

	shell = ephy_shell_get_default ();

	g_object_ref (shell);

	window = ephy_shell_get_main_window (shell);

	new_windows_in_tabs = g_settings_get_boolean (EPHY_SETTINGS_MAIN,
						      EPHY_PREFS_NEW_WINDOWS_IN_TABS);

	have_uris = ! (g_strv_length (uris) == 1 && g_str_equal (uris[0], ""));

	if (options != NULL && strstr (options, "external") != NULL)
	{
		flags |= EPHY_NEW_TAB_FROM_EXTERNAL;
	}
	if (options != NULL && strstr (options, "new-window") != NULL)
	{
		window = NULL;
		flags |= EPHY_NEW_TAB_IN_NEW_WINDOW;
	}
	else if ((options != NULL && strstr (options, "new-tab") != NULL) ||
		 (new_windows_in_tabs && have_uris))
	{
		flags |= EPHY_NEW_TAB_IN_EXISTING_WINDOW |
			 EPHY_NEW_TAB_JUMP |
			 EPHY_NEW_TAB_PRESENT_WINDOW;
	}
	else if (!have_uris)
	{
		window = NULL;
		flags |= EPHY_NEW_TAB_IN_NEW_WINDOW;
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

		embed = ephy_shell_new_tab_full (shell, window,
						 NULL /* parent tab */,
						 request,
						 flags | page_flags,
						 EPHY_WEB_VIEW_CHROME_ALL,
						 FALSE /* is popup? */,
						 user_time);

		if (request)
			g_object_unref (request);

		window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed)));
	}

	g_object_unref (shell);
}

static void
session_maybe_open_window (EphySession *session,
			   guint32 user_time)
{
	EphyShell *shell = ephy_shell_get_default ();

	/* FIXME: maybe just check for normal windows? */
	if (ephy_shell_get_n_windows (shell) == 0)
	{
		ephy_shell_new_tab_full (shell,
					 NULL /* window */, NULL /* tab */,
					 NULL /* NetworkRequest */,
					 EPHY_NEW_TAB_IN_NEW_WINDOW |
					 EPHY_NEW_TAB_HOME_PAGE,
					 EPHY_WEB_VIEW_CHROME_ALL,
					 FALSE /* is popup? */,
					 user_time);
	}
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
		case EPHY_SESSION_CMD_OPEN_URIS:
			session_command_open_uris (session, cmd->args, cmd->arg, cmd->user_time);
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

static void
window_added_cb (GtkApplication *application,
		 GtkWindow *window,
		 EphySession *session)
{
	GtkWidget *notebook;
	EphyWindow *ephy_window;

	ephy_session_save (session, SESSION_STATE);

	if (!EPHY_IS_WINDOW (window))
		return;

	ephy_window = EPHY_WINDOW (window);

	notebook = ephy_window_get_notebook (ephy_window);
	g_signal_connect (notebook, "page-added",
			  G_CALLBACK (notebook_page_added_cb), session);
	g_signal_connect (notebook, "page-removed",
			  G_CALLBACK (notebook_page_removed_cb), session);
	g_signal_connect (notebook, "page-reordered",
			  G_CALLBACK (notebook_page_reordered_cb), session);

	/* Set unique identifier as role, so that on restore, the WM can
	 * place the window on the right workspace
	 */

	if (gtk_window_get_role (window) == NULL)
	{
		/* I guess rand() is unique enough, otherwise we could use
		 * time + pid or something
		 */
		char *role;

		role = g_strdup_printf ("epiphany-window-%x", rand());
		gtk_window_set_role (window, role);
		g_free (role);
	}
}

static void
window_removed_cb (GtkApplication *application,
		   GtkWindow *window,
		   EphySession *session)
{
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
	EphyShell *shell;

	LOG ("EphySession initialising");

	priv = session->priv = EPHY_SESSION_GET_PRIVATE (session);

	priv->queue = g_queue_new ();

	shell = ephy_shell_get_default ();
	g_signal_connect (shell, "window-added",
			  G_CALLBACK (window_added_cb), session);
	g_signal_connect (shell, "window-removed",
			  G_CALLBACK (window_removed_cb), session);
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
ephy_session_class_init (EphySessionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = ephy_session_dispose;

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

		ephy_embed_shell_prepare_close (ephy_embed_shell_get_default ());

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
	EphyShell *shell;
	xmlTextWriterPtr writer;
	GList *w;
	GList *windows;
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

	shell = ephy_shell_get_default ();

	if (ephy_shell_get_n_windows (shell) == 0)
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
	windows = ephy_shell_get_windows (shell);
	for (w = windows; w != NULL && ret >= 0; w = w->next)
	{
		ret = write_ephy_window (writer, EPHY_WINDOW (w->data));
	}
	g_list_free (windows);
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
confirm_before_recover (EphyWindow *window, const char *url, const char *title)
{
	EphyEmbed *embed;

	embed = ephy_shell_new_tab (ephy_shell_get_default (),
				    window, NULL, NULL,
				    EPHY_NEW_TAB_IN_EXISTING_WINDOW |
				    EPHY_NEW_TAB_APPEND_LAST);

	ephy_web_view_load_error_page (ephy_embed_get_web_view (embed), url,
			               EPHY_WEB_VIEW_ERROR_PAGE_CRASH, NULL);
}

static void
restore_geometry (GtkWindow *window,
		  GdkRectangle *geometry)
{
	if (geometry->x >= 0 && geometry->y >= 0)
	{
		gtk_window_move (window, geometry->x, geometry->y);
	}

	if (geometry->width > 0 && geometry->height > 0)
	{
		gtk_window_set_default_size (window, geometry->width, geometry->height);
	}
}

typedef struct {
	EphySession *session;
	guint32 user_time;

	EphyWindow *window;
	gboolean is_first_window;
	gint active_tab;

	gboolean is_first_tab;
} SessionParserContext;

static SessionParserContext *
session_parser_context_new (EphySession *session,
			    guint32 user_time)
{
	SessionParserContext *context;

	context = g_slice_new0 (SessionParserContext);
	context->session = g_object_ref (session);
	context->user_time = user_time;
	context->is_first_window = TRUE;

	return context;
}

static void
session_parser_context_free (SessionParserContext *context)
{
	g_object_unref (context->session);

	g_slice_free (SessionParserContext, context);
}

static void
session_parse_window (SessionParserContext *context,
		      const gchar **names,
		      const gchar **values)
{
	GdkRectangle geometry = { -1, -1, 0, 0 };
	guint i;

	context->window = ephy_window_new ();

	for (i = 0; names[i]; i++)
	{
		gulong int_value;

		if (strcmp (names[i], "x") == 0)
		{
			ephy_string_to_int (values[i], &int_value);
			geometry.x = int_value;
		}
		else if (strcmp (names[i], "y") == 0)
		{
			ephy_string_to_int (values[i], &int_value);
			geometry.y = int_value;
		}
		else if (strcmp (names[i], "width") == 0)
		{
			ephy_string_to_int (values[i], &int_value);
			geometry.width = int_value;
		}
		else if (strcmp (names[i], "height") == 0)
		{
			ephy_string_to_int (values[i], &int_value);
			geometry.height = int_value;
		}
		else if (strcmp (names[i], "role") == 0)
		{
			gtk_window_set_role (GTK_WINDOW (context->window), values[i]);
		}
		else if (strcmp (names[i], "active-tab") == 0)
		{
			ephy_string_to_int (values[i], &int_value);
			context->active_tab = int_value;
		}
	}

	restore_geometry (GTK_WINDOW (context->window), &geometry);
	ephy_gui_window_update_user_time (GTK_WIDGET (context->window), context->user_time);
}

static void
session_parse_embed (SessionParserContext *context,
		     const gchar **names,
		     const gchar **values)
{
	const char *url = NULL;
	const char *title = NULL;
	gboolean was_loading = FALSE;
	gboolean is_blank_page = FALSE;
	guint i;

	for (i = 0; names[i]; i++)
	{
		if (strcmp (names[i], "url") == 0)
		{
			url = values[i];
			is_blank_page = (strcmp (url, "about:blank") == 0 ||
					 strcmp (url, "about:overview") == 0);
		}
		else if (strcmp (names[i], "title") == 0)
		{
			title = values[i];
		}
		else if (strcmp (names[i], "loading") == 0)
		{
			was_loading = strcmp (values[i], "true") == 0;
		}
	}

	/* In the case that crash happens before we receive the URL from the server,
	 * this will open an about:blank tab.
	 * See http://bugzilla.gnome.org/show_bug.cgi?id=591294
	 * Otherwise, if the web was fully loaded, it is reloaded again.
	 */
	if (!was_loading || is_blank_page)
	{
		ephy_shell_new_tab (ephy_shell_get_default (),
				    context->window, NULL, url,
				    EPHY_NEW_TAB_IN_EXISTING_WINDOW |
				    EPHY_NEW_TAB_OPEN_PAGE |
				    EPHY_NEW_TAB_APPEND_LAST);
	}
	else if (was_loading && url != NULL)
	{
		/* Shows a message to the user that warns that this page was
		 * loading during crash and make Epiphany crash again,
		 * in this case we know the URL.
		 */
		confirm_before_recover (context->window, url, title);
	}
}

static void
session_start_element (GMarkupParseContext  *ctx,
		       const gchar          *element_name,
		       const gchar         **names,
		       const gchar         **values,
		       gpointer              user_data,
		       GError              **error)
{
	SessionParserContext *context = (SessionParserContext *)user_data;

	if (strcmp (element_name, "window") == 0)
	{
		session_parse_window (context, names, values);
		context->is_first_tab = TRUE;
	}
	else if (strcmp (element_name, "embed") == 0)
	{
		session_parse_embed (context, names, values);
	}
}

static void
session_end_element (GMarkupParseContext  *ctx,
		     const gchar          *element_name,
		     gpointer              user_data,
		     GError              **error)
{
	SessionParserContext *context = (SessionParserContext *)user_data;

	if (strcmp (element_name, "window") == 0)
	{
		GtkWidget *notebook;

		notebook = ephy_window_get_notebook (context->window);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), context->active_tab);

		if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) != EPHY_EMBED_SHELL_MODE_TEST)
		{
			EphyEmbed *active_child;

			active_child = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (context->window));
			gtk_widget_grab_focus (GTK_WIDGET (active_child));
			gtk_widget_show (GTK_WIDGET (context->window));
		}

		context->window = NULL;
		context->active_tab = 0;
		context->is_first_window = FALSE;
	}
	else if (strcmp (element_name, "embed") == 0)
	{
		context->is_first_tab = FALSE;
	}
}

static const GMarkupParser session_parser = {
	session_start_element,
	session_end_element,
	NULL,
	NULL,
	NULL
};

typedef struct {
	EphyShell *shell;
	GMarkupParseContext *parser;
	GCancellable *cancellable;
	char buffer[1024];
} LoadFromStreamAsyncData;

static LoadFromStreamAsyncData *
load_from_stream_async_data_new (GMarkupParseContext *parser,
				 GCancellable *cancellable)
{
	LoadFromStreamAsyncData *data;

	data = g_slice_new (LoadFromStreamAsyncData);
	data->shell = g_object_ref (ephy_shell_get_default ());
	data->parser = parser;
	data->cancellable = cancellable ? g_object_ref (cancellable) : NULL;

	return data;
}

static void
load_from_stream_async_data_free (LoadFromStreamAsyncData *data)
{
	g_object_unref (data->shell);
	g_markup_parse_context_free (data->parser);
	g_clear_object (&data->cancellable);

	g_slice_free (LoadFromStreamAsyncData, data);
}

static void
load_stream_complete (GSimpleAsyncResult *simple)
{
	EphySession *session;

	g_simple_async_result_complete (simple);

	session = EPHY_SESSION (g_async_result_get_source_object (G_ASYNC_RESULT (simple)));
	session->priv->dont_save = FALSE;

	ephy_session_save (session, SESSION_STATE);
	g_object_unref (session);

	g_object_unref (simple);

	g_application_release (G_APPLICATION (ephy_shell_get_default ()));
}

static void
load_stream_complete_error (GSimpleAsyncResult *simple,
			    GError *error)
{
	EphySession *session;
	LoadFromStreamAsyncData *data;
	SessionParserContext *context;

	g_simple_async_result_take_error (simple, error);
	g_simple_async_result_complete (simple);

	session = EPHY_SESSION (g_async_result_get_source_object (G_ASYNC_RESULT (simple)));
	session->priv->dont_save = FALSE;
	/* If the session fails to load for whatever reason,
	 * delete the file and open an empty window.
	 */
	session_delete (session, SESSION_STATE);

	data = g_simple_async_result_get_op_res_gpointer (simple);
	context = (SessionParserContext *)g_markup_parse_context_get_user_data (data->parser);
	session_maybe_open_window (session, context->user_time);
	g_object_unref (session);

	g_object_unref (simple);

	g_application_release (G_APPLICATION (ephy_shell_get_default ()));
}

static void
load_stream_read_cb (GObject *object,
		     GAsyncResult *result,
		     gpointer user_data)
{
	GInputStream *stream = G_INPUT_STREAM (object);
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
	LoadFromStreamAsyncData *data;
	gssize bytes_read;
	GError *error = NULL;

	bytes_read = g_input_stream_read_finish (stream, result, &error);
	if (bytes_read < 0)
	{
		load_stream_complete_error (simple, error);

		return;
	}

	data = g_simple_async_result_get_op_res_gpointer (simple);
	if (bytes_read == 0)
	{
		if (!g_markup_parse_context_end_parse (data->parser, &error))
		{
			load_stream_complete_error (simple, error);
		}
		else
		{
			load_stream_complete (simple);
		}

		return;
	}

	if (!g_markup_parse_context_parse (data->parser, data->buffer, bytes_read, &error))
	{
		load_stream_complete_error (simple, error);

		return;
	}

	g_input_stream_read_async (stream, data->buffer, sizeof (data->buffer),
				   G_PRIORITY_HIGH, data->cancellable,
				   load_stream_read_cb, simple);
}

/**
 * ephy_session_load_from_stream:
 * @session: an #EphySession
 * @stream: a #GInputStream to read the session data from
 * @user_time: a user time, or 0
 * @cancellable: (allow-none): optional #GCancellable object, or %NULL
 * @callback: (scope async): a #GAsyncReadyCallback to call when the
 *    request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously loads the session reading the session data from @stream,
 * restoring windows and their state.
 *
 * When the operation is finished, @callback will be called. You can
 * then call ephy_session_load_from_stream_finish() to get the result of
 * the operation.
 **/
void
ephy_session_load_from_stream (EphySession *session,
			       GInputStream *stream,
			       guint32 user_time,
			       GCancellable *cancellable,
			       GAsyncReadyCallback callback,
			       gpointer user_data)
{
	GSimpleAsyncResult *result;
	SessionParserContext *context;
	GMarkupParseContext *parser;
	LoadFromStreamAsyncData *data;

	g_return_if_fail (EPHY_IS_SESSION (session));
	g_return_if_fail (G_IS_INPUT_STREAM (stream));

	g_application_hold (G_APPLICATION (ephy_shell_get_default ()));

	session->priv->dont_save = TRUE;

	result = g_simple_async_result_new (G_OBJECT (session), callback, user_data, ephy_session_load_from_stream);

	context = session_parser_context_new (session, user_time);
	parser = g_markup_parse_context_new (&session_parser, 0, context, (GDestroyNotify)session_parser_context_free);
	data = load_from_stream_async_data_new (parser, cancellable);
	g_simple_async_result_set_op_res_gpointer (result, data, (GDestroyNotify)load_from_stream_async_data_free);

	g_input_stream_read_async (stream, data->buffer, sizeof (data->buffer), G_PRIORITY_HIGH, cancellable,
				   load_stream_read_cb, result);
}

/**
 * ephy_session_load_from_stream_finish:
 * @session: an #EphySession
 * @result: a #GAsyncResult
 * @error: a #GError
 *
 * Finishes an async session load operation started with
 * ephy_session_load_from_stream().
 *
 * Returns: %TRUE if at least a window has been opened
 **/
gboolean
ephy_session_load_from_stream_finish (EphySession *session,
				      GAsyncResult *result,
				      GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (EPHY_IS_SESSION (session), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == ephy_session_load_from_stream);

	return !g_simple_async_result_propagate_error (simple, error);
}

typedef struct {
	guint32 user_time;
	GCancellable *cancellable;
} LoadAsyncData;

static LoadAsyncData *
load_async_data_new (guint32 user_time,
		     GCancellable *cancellable)
{
	LoadAsyncData *data;

	data = g_slice_new (LoadAsyncData);
	data->user_time = user_time;
	data->cancellable = cancellable ? g_object_ref (cancellable) : NULL;

	return data;
}

static void
load_async_data_free (LoadAsyncData *data)
{
	g_clear_object (&data->cancellable);

	g_slice_free (LoadAsyncData, data);
}

static void
load_from_stream_cb (GObject *object,
		     GAsyncResult *result,
		     gpointer user_data)
{
	EphySession *session = EPHY_SESSION (object);
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
	GError *error = NULL;

	if (!ephy_session_load_from_stream_finish (session, result, &error))
	{
		g_simple_async_result_take_error (simple, error);
	}
	g_simple_async_result_complete (simple);
	g_object_unref (simple);
}

static void
session_read_cb (GObject *object,
		 GAsyncResult *result,
		 gpointer user_data)
{
	GFileInputStream *stream;
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
	GError *error = NULL;

	stream = g_file_read_finish (G_FILE (object), result, &error);
	if (stream)
	{
		EphySession *session;
		LoadAsyncData *data;

		session = EPHY_SESSION (g_async_result_get_source_object (G_ASYNC_RESULT (simple)));
		data = g_simple_async_result_get_op_res_gpointer (simple);
		ephy_session_load_from_stream (session, G_INPUT_STREAM (stream), data->user_time,
					       data->cancellable, load_from_stream_cb, simple);
		g_object_unref (stream);
		g_object_unref (session);
	}
	else
	{
		g_simple_async_result_take_error (simple, error);
		g_simple_async_result_complete (simple);
		g_object_unref (simple);
	}

	g_application_release (G_APPLICATION (ephy_shell_get_default ()));
}

/**
 * ephy_session_load:
 * @session: an #EphySession
 * @filename: the path of the source file
 * @user_time: a user time, or 0
 * @cancellable: (allow-none): optional #GCancellable object, or %NULL
 * @callback: (scope async): a #GAsyncReadyCallback to call when the
 *    request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously loads the session reading the session data from @filename,
 * restoring windows and their state.
 *
 * When the operation is finished, @callback will be called. You can
 * then call ephy_session_load_finish() to get the result of
 * the operation.
 **/
void
ephy_session_load (EphySession *session,
		   const char *filename,
		   guint32 user_time,
		   GCancellable *cancellable,
		   GAsyncReadyCallback callback,
		   gpointer user_data)
{
	GFile *save_to_file;
	GSimpleAsyncResult *result;
	LoadAsyncData *data;

	g_return_if_fail (EPHY_IS_SESSION (session));
	g_return_if_fail (filename);

	LOG ("ephy_sesion_load %s", filename);

	g_application_hold (G_APPLICATION (ephy_shell_get_default ()));

	result = g_simple_async_result_new (G_OBJECT (session), callback, user_data, ephy_session_load);

	save_to_file = get_session_file (filename);
	data = load_async_data_new (user_time, cancellable);
	g_simple_async_result_set_op_res_gpointer (result, data, (GDestroyNotify)load_async_data_free);
	g_file_read_async (save_to_file, G_PRIORITY_HIGH, cancellable, session_read_cb, result);
	g_object_unref (save_to_file);
}

/**
 * ephy_session_load_finish:
 * @session: an #EphySession
 * @result: a #GAsyncResult
 * @error: a #GError
 *
 * Finishes an async session load operation started with
 * ephy_session_load().
 *
 * Returns: %TRUE if at least a window has been opened
 **/
gboolean
ephy_session_load_finish (EphySession *session,
			  GAsyncResult *result,
			  GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (EPHY_IS_SESSION (session), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == ephy_session_load);

	return !g_simple_async_result_propagate_error (simple, error);
}

static gboolean
session_state_file_exists (EphySession *session)
{
	GFile *saved_session_file;
	char *saved_session_file_path;
	gboolean retval;

	saved_session_file = get_session_file (SESSION_STATE);
	saved_session_file_path = g_file_get_path (saved_session_file);
	g_object_unref (saved_session_file);
	retval = g_file_test (saved_session_file_path, G_FILE_TEST_EXISTS);
	g_free (saved_session_file_path);

	return retval;
}

static void
session_resumed_cb (GObject *object,
		    GAsyncResult *result,
		    gpointer user_data)
{
	EphySession *session = EPHY_SESSION (object);
	GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
	GError *error = NULL;

	if (!ephy_session_load_finish (session, result, &error))
		g_simple_async_result_take_error (simple, error);
	g_simple_async_result_complete (simple);
	g_object_unref (simple);
}

void
ephy_session_resume (EphySession *session,
		     guint32 user_time,
		     GCancellable *cancellable,
		     GAsyncReadyCallback callback,
		     gpointer user_data)
{
	GSimpleAsyncResult *result;
	gboolean has_session_state;
	EphyPrefsRestoreSessionPolicy policy;
	EphyShell *shell;

	LOG ("ephy_session_autoresume");

	result = g_simple_async_result_new (G_OBJECT (session), callback, user_data, ephy_session_resume);

	has_session_state = session_state_file_exists (session);

	policy = g_settings_get_enum (EPHY_SETTINGS_MAIN,
				      EPHY_PREFS_RESTORE_SESSION_POLICY);

	shell = ephy_shell_get_default ();

	if (has_session_state == FALSE ||
	    policy == EPHY_PREFS_RESTORE_SESSION_POLICY_NEVER)
	{
		/* If we are auto-resuming, and we never want to
		 * restore the session, clobber the session state
		 * file. */
		if (policy == EPHY_PREFS_RESTORE_SESSION_POLICY_NEVER)
			session_delete (session, SESSION_STATE);

		session_maybe_open_window (session, user_time);
	}
	else if (ephy_shell_get_n_windows (shell) == 0)
	{
		ephy_session_load (session, SESSION_STATE, user_time, cancellable,
				   session_resumed_cb, result);
		return;
	}

	g_simple_async_result_complete_in_idle (result);
	g_object_unref (result);
}


gboolean
ephy_session_resume_finish (EphySession *session,
			    GAsyncResult *result,
			    GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (EPHY_IS_SESSION (session), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == ephy_session_resume);

	return !g_simple_async_result_propagate_error (simple, error);
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
	SessionCommand *cmd;

	LOG ("queue_command command:%d", command);

	g_return_if_fail (EPHY_IS_SESSION (session));
	g_return_if_fail (command != EPHY_SESSION_CMD_OPEN_URIS || args != NULL);

	priv = session->priv;

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
}
