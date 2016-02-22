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
#include "ephy-notebook.h"
#include "ephy-prefs.h"
#include "ephy-private.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-window.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>

#define EPHY_SESSION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_SESSION, EphySessionPrivate))

typedef struct
{
	gpointer* parent_location;
	int position;
	char *url;
	GList *bflist;
} ClosedTab;

struct _EphySessionPrivate
{
	GQueue *closed_tabs;
	GCancellable *save_cancellable;
	guint dont_save : 1;
};

#define SESSION_STATE		"type:session_state"
#define MAX_CLOSED_TABS		10

enum
{
	PROP_0,
	PROP_CAN_UNDO_TAB_CLOSED
};

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

static void
load_changed_cb (WebKitWebView *view,
		 WebKitLoadEvent load_event,
		 EphySession *session)
{
	if (!ephy_web_view_load_failed (EPHY_WEB_VIEW (view)))
		ephy_session_save (session, SESSION_STATE);
}

static gpointer *
parent_location_new (EphyNotebook *notebook)
{
	gpointer *location = g_slice_new (gpointer);
	*location = notebook;
	g_object_add_weak_pointer (G_OBJECT (notebook), location);

	return location;
}

static void
parent_location_free (gpointer *location, gboolean last_reference)
{
	if (!location)
		return;

	if (*location && last_reference)
	{
		g_object_remove_weak_pointer (G_OBJECT (*location), location);
	}

	g_slice_free (gpointer, location);
}

static void
closed_tab_free (ClosedTab *tab)
{
	if (tab->bflist)
	{
		g_list_free_full (tab->bflist, g_object_unref);
		tab->bflist = NULL;
	}

	if (tab->url)
	{
		g_free (tab->url);
		tab->url = NULL;
	}

	g_slice_free (ClosedTab, tab);
}

static int
compare_func (ClosedTab *iter, EphyNotebook *notebook)
{
	return (EphyNotebook *)*iter->parent_location - notebook;
}

static ClosedTab *
find_tab_with_notebook (GQueue *queue, EphyNotebook *notebook)
{
	GList *item = g_queue_find_custom (queue, notebook, (GCompareFunc)compare_func);
	return item ? (ClosedTab*)item->data : NULL;
}

static ClosedTab *
closed_tab_new (GQueue *closed_tabs,
		const char *address,
		GList *bflist,
		int position,
		EphyNotebook *parent_notebook)
{
	ClosedTab *tab = g_slice_new0 (ClosedTab);
	ClosedTab *sibling_tab;

	tab->url = g_strdup (address);
	tab->position = position;

	sibling_tab = find_tab_with_notebook (closed_tabs, parent_notebook);
	if (sibling_tab)
		tab->parent_location = sibling_tab->parent_location;
	else
		tab->parent_location = parent_location_new (parent_notebook);

	return tab;
}

static void
post_restore_cleanup (GQueue *closed_tabs, ClosedTab *restored_tab, gboolean notebook_is_new)
{

	if (find_tab_with_notebook (closed_tabs, *restored_tab->parent_location))
	{
		if (notebook_is_new == TRUE)
		{
			/* If this is a newly opened notebook and
			   there are other tabs that must be restored
			   here, add a weak poiner to keep track of
			   the lifetime of it. */
			g_object_add_weak_pointer (G_OBJECT (*restored_tab->parent_location),
						   restored_tab->parent_location);
		}
	}
	else
	{
		/* If there are no other tabs that must be restored to this notebook,
		   we can remove the pointer keeping track of its location.
		   If this is a new window, we don't need to remove any weak
		   pointer, as no one has been added yet. */
		parent_location_free (restored_tab->parent_location, !notebook_is_new);
	}
}

void
ephy_session_undo_close_tab (EphySession *session)
{
	EphySessionPrivate *priv;
	EphyEmbed *embed, *new_tab;
	ClosedTab *tab;
	EphyWindow *window;
	EphyNewTabFlags flags = EPHY_NEW_TAB_JUMP;

	g_return_if_fail (EPHY_IS_SESSION (session));

	priv = session->priv;

	tab = g_queue_pop_head (priv->closed_tabs);
	if (tab == NULL)
		return;

	LOG ("UNDO CLOSE TAB: %s", tab->url);
	if (*tab->parent_location != NULL)
	{
		if (tab->position > 0)
		{
			/* Append in the n-th position. */
			embed = EPHY_EMBED (gtk_notebook_get_nth_page (GTK_NOTEBOOK (*tab->parent_location),
								       tab->position - 1));
			flags |= EPHY_NEW_TAB_APPEND_AFTER;
		}
		else
		{
			/* Just prepend in the first position. */
			embed = NULL;
			flags |= EPHY_NEW_TAB_FIRST;
		}

		window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (*tab->parent_location)));
		new_tab = ephy_shell_new_tab (ephy_shell_get_default (),
					      window, embed,
					      flags);
		post_restore_cleanup (priv->closed_tabs, tab, FALSE);
	}
	else
	{
		EphyNotebook *notebook;

		window = ephy_window_new ();
		new_tab = ephy_shell_new_tab (ephy_shell_get_default (),
					      window, NULL, flags);

		/* FIXME: This makes the assumption that the notebook
		   is the parent of the returned EphyEmbed. */
		notebook = EPHY_NOTEBOOK (gtk_widget_get_parent (GTK_WIDGET (new_tab)));
		*tab->parent_location = notebook;
		post_restore_cleanup (priv->closed_tabs, tab, TRUE);
	}

	ephy_web_view_load_url (ephy_embed_get_web_view (new_tab), tab->url);
	gtk_widget_grab_focus (GTK_WIDGET (new_tab));
	gtk_window_present (GTK_WINDOW (window));

	closed_tab_free (tab);

	if (g_queue_is_empty (priv->closed_tabs))
		g_object_notify (G_OBJECT (session), "can-undo-tab-closed");
}

static void
ephy_session_tab_closed (EphySession *session,
			 EphyNotebook *notebook,
			 EphyEmbed *embed,
			 gint position)
{
	EphySessionPrivate *priv = session->priv;
	EphyWebView *view;
	const char *address;
	WebKitBackForwardList *source;
	ClosedTab *tab;
	GList *items = NULL;

	view = ephy_embed_get_web_view (embed);
	address = ephy_web_view_get_address (view);

	source = webkit_web_view_get_back_forward_list (WEBKIT_WEB_VIEW (view));
	items = webkit_back_forward_list_get_back_list_with_limit (source, EPHY_WEBKIT_BACK_FORWARD_LIMIT);
	if (items == NULL && g_strcmp0 (address, "ephy-about:overview") == 0)
		return;

	if (g_queue_get_length (priv->closed_tabs) == MAX_CLOSED_TABS)
	{
		tab = g_queue_pop_tail (priv->closed_tabs);
		if (tab->parent_location && !find_tab_with_notebook (priv->closed_tabs, *tab->parent_location))
		{
			parent_location_free (tab->parent_location, TRUE);
		}

		closed_tab_free (tab);
		tab = NULL;
	}

	items = g_list_reverse (items);
	tab = closed_tab_new (priv->closed_tabs, address, items, position, notebook);
	g_list_free (items);

	g_queue_push_head (priv->closed_tabs, tab);

	if (g_queue_get_length (priv->closed_tabs) == 1)
		g_object_notify (G_OBJECT (session), "can-undo-tab-closed");

	LOG ("Added: %s to the list (%d elements)",
	     address, g_queue_get_length (priv->closed_tabs));
}

gboolean
ephy_session_get_can_undo_tab_closed (EphySession *session)
{
	g_return_val_if_fail (EPHY_IS_SESSION (session), FALSE);

	return g_queue_is_empty (session->priv->closed_tabs) == FALSE;
}

static void
notebook_page_added_cb (GtkWidget *notebook,
			EphyEmbed *embed,
			guint position,
			EphySession *session)
{
	g_signal_connect (ephy_embed_get_web_view (embed), "load-changed",
			  G_CALLBACK (load_changed_cb), session);
}

static void
notebook_page_removed_cb (GtkWidget *notebook,
			  EphyEmbed *embed,
			  guint position,
			  EphySession *session)
{
	ephy_session_save (session, SESSION_STATE);

	g_signal_handlers_disconnect_by_func
		(ephy_embed_get_web_view (embed), G_CALLBACK (load_changed_cb),
		 session);

	ephy_session_tab_closed (session, EPHY_NOTEBOOK (notebook), embed, position);
}

static void
notebook_page_reordered_cb (GtkWidget *notebook,
			    GtkWidget *tab,
			    guint position,
			    EphySession *session)
{
	ephy_session_save (session, SESSION_STATE);
}

static void
session_maybe_open_window (EphySession *session,
			   guint32 user_time)
{
	EphyShell *shell = ephy_shell_get_default ();

	/* FIXME: maybe just check for normal windows? */
	if (ephy_shell_get_n_windows (shell) == 0)
	{
		EphyWindow *window = ephy_window_new ();
		EphyEmbed *embed;

		embed = ephy_shell_new_tab_full (shell,
						 NULL /* title */,
						 NULL /* related view */,
						 window, NULL /* tab */,
						 0,
						 user_time);
		ephy_web_view_load_homepage (ephy_embed_get_web_view (embed));
		ephy_window_activate_location (window);
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
	EphyShell *shell;

	LOG ("EphySession initialising");

	session->priv = EPHY_SESSION_GET_PRIVATE (session);

	session->priv->closed_tabs = g_queue_new ();
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

	g_queue_free_full (session->priv->closed_tabs,
			   (GDestroyNotify)closed_tab_free);

	G_OBJECT_CLASS (ephy_session_parent_class)->dispose (object);
}

static void
ephy_session_get_property (GObject        *object,
			   guint           property_id,
			   GValue         *value,
			   GParamSpec     *pspec)
{
	EphySession *session = EPHY_SESSION (object);

	switch (property_id)
	{
	case PROP_CAN_UNDO_TAB_CLOSED:
		g_value_set_boolean (value,
				     ephy_session_get_can_undo_tab_closed (session));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
ephy_session_class_init (EphySessionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = ephy_session_dispose;
	object_class->get_property = ephy_session_get_property;

	g_object_class_install_property (object_class,
					 PROP_CAN_UNDO_TAB_CLOSED,
					 g_param_spec_boolean ("can-undo-tab-closed",
							       "Can undo tab close",
							       "Session can undo a tab closure",
							       FALSE,
							       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

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

		ephy_embed_shell_prepare_close (ephy_embed_shell_get_default ());
	}
}

static void
get_window_geometry (GtkWindow *window,
		     GdkRectangle *rectangle)
{
	gtk_window_get_size (window, &rectangle->width, &rectangle->height);
	gtk_window_get_position (window, &rectangle->x, &rectangle->y);
}

typedef struct {
	char *url;
	char *title;
	gboolean loading;
} SessionTab;

static SessionTab *
session_tab_new (EphyEmbed *embed)
{
	SessionTab *session_tab;
	const char *address;
	EphyWebView *web_view = ephy_embed_get_web_view (embed);

	session_tab = g_slice_new (SessionTab);

	address = ephy_web_view_get_address (web_view);
	/* Do not store ephy-about: URIs, they are not valid for loading. */
	if (g_str_has_prefix (address, EPHY_ABOUT_SCHEME))
	{
		session_tab->url = g_strconcat ("about", address + EPHY_ABOUT_SCHEME_LEN, NULL);
	}
	else if (g_str_equal (address, "about:blank"))
	{
		/* EphyWebView address is NULL between load_uri() and WEBKIT_LOAD_STARTED,
		 * but WebKitWebView knows the pending API request URL, so use that instead of about:blank.
		 */
		session_tab->url = g_strdup (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (web_view)));
	}
	else
	{
		session_tab->url = g_strdup (address);
	}

	session_tab->title = g_strdup (ephy_embed_get_title (embed));
	session_tab->loading = ephy_web_view_is_loading (web_view) && !ephy_embed_has_load_pending (embed);

	return session_tab;
}

static void
session_tab_free (SessionTab *tab)
{
	g_free (tab->url);
	g_free (tab->title);

	g_slice_free (SessionTab, tab);
}

typedef struct {
	GdkRectangle geometry;
	char *role;

	GList *tabs;
	gint active_tab;
} SessionWindow;

static SessionWindow *
session_window_new (EphyWindow *window)
{
	SessionWindow *session_window;
	GList *tabs, *l;
	GtkNotebook *notebook;

	tabs = ephy_embed_container_get_children (EPHY_EMBED_CONTAINER (window));
	/* Do not save an empty EphyWindow.
	 * This only happens when the window was newly opened.
	 */
	if (!tabs)
	{
		return NULL;
	}

	session_window = g_slice_new0 (SessionWindow);
	get_window_geometry (GTK_WINDOW (window), &session_window->geometry);
	session_window->role = g_strdup (gtk_window_get_role (GTK_WINDOW (window)));

	for (l = tabs; l != NULL; l = l->next)
	{
		SessionTab *tab;

		tab = session_tab_new (EPHY_EMBED (l->data));
		session_window->tabs = g_list_prepend (session_window->tabs, tab);
	}
	g_list_free (tabs);
	session_window->tabs = g_list_reverse (session_window->tabs);

	notebook = GTK_NOTEBOOK (ephy_window_get_notebook (window));
	session_window->active_tab = gtk_notebook_get_current_page (notebook);

	return session_window;
}

static void
session_window_free (SessionWindow *session_window)
{
	g_free (session_window->role);
	g_list_free_full (session_window->tabs, (GDestroyNotify)session_tab_free);

	g_slice_free (SessionWindow, session_window);
}

typedef struct {
	EphySession *session;
	GFile *save_file;

	GList *windows;
} SaveData;

static SaveData *
save_data_new (EphySession *session,
	       const char *filename)
{
	SaveData *data;
	EphyShell *shell = ephy_shell_get_default ();
	GList *windows, *w;

	data = g_slice_new0 (SaveData);
	data->session = g_object_ref (session);
	data->save_file = get_session_file (filename);

	windows = gtk_application_get_windows (GTK_APPLICATION (shell));
	for (w = windows; w != NULL ; w = w->next)
	{
		SessionWindow *session_window;

		session_window = session_window_new (EPHY_WINDOW (w->data));
		if (session_window)
			data->windows = g_list_prepend (data->windows, session_window);
	}
	data->windows = g_list_reverse (data->windows);

	return data;
}

static void
save_data_free (SaveData *data)
{
	g_list_free_full (data->windows, (GDestroyNotify)session_window_free);

	g_object_unref (data->save_file);
	g_object_unref (data->session);

	g_slice_free (SaveData, data);
}

static int
write_tab (xmlTextWriterPtr writer,
	   SessionTab *tab)
{
	int ret;

	ret = xmlTextWriterStartElement (writer, (xmlChar *) "embed");
	if (ret < 0) return ret;

	ret = xmlTextWriterWriteAttribute (writer, (xmlChar *) "url",
					   (const xmlChar *) tab->url);
	if (ret < 0) return ret;

	ret = xmlTextWriterWriteAttribute (writer, (xmlChar *) "title",
					   (const xmlChar *) tab->title);
	if (ret < 0) return ret;

	if (tab->loading)
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
		       GdkRectangle *geometry)
{
	int ret;

	/* set window properties */
	ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *) "x", "%d",
						 geometry->x);
	if (ret < 0) return ret;

	ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *) "y", "%d",
						 geometry->y);
	if (ret < 0) return ret;

	ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *) "width", "%d",
						 geometry->width);
	if (ret < 0) return ret;

	ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *) "height", "%d",
						 geometry->height);
	return ret;
}

static int
write_ephy_window (xmlTextWriterPtr writer,
		   SessionWindow *window)
{
	GList *l;
	int ret;

	ret = xmlTextWriterStartElement (writer, (xmlChar *) "window");
	if (ret < 0) return ret;

	ret = write_window_geometry (writer, &window->geometry);
	if (ret < 0) return ret;

	ret = xmlTextWriterWriteFormatAttribute (writer, (const xmlChar *) "active-tab", "%d",
						 window->active_tab);
	if (ret < 0) return ret;

	if (window->role != NULL)
	{
		ret = xmlTextWriterWriteAttribute (writer,
						   (const xmlChar *) "role",
						   (const xmlChar *) window->role);
		if (ret < 0) return ret;
	}

	for (l = window->tabs; l != NULL; l = l->next)
	{
		SessionTab *tab = (SessionTab *) l->data;
		ret = write_tab (writer, tab);
		if (ret < 0) break;
	}
	if (ret < 0) return ret;

	ret = xmlTextWriterEndElement (writer); /* window */
	return ret;
}

static void
save_session_in_thread_cb (GObject *source_object,
			   GAsyncResult *res,
			   gpointer user_data)
{
	g_application_release (G_APPLICATION (ephy_shell_get_default ()));
}

static void
save_session_sync (GTask *task,
		   gpointer source_object,
		   gpointer task_data,
		   GCancellable *cancellable)
{
	SaveData *data = (SaveData *)g_task_get_task_data (task);
	xmlBufferPtr buffer;
	xmlTextWriterPtr writer;
	GList *w;
	int ret = -1;

	buffer = xmlBufferCreate ();
	writer = xmlNewTextWriterMemory (buffer, 0);
	if (writer == NULL) goto out;

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
	for (w = data->windows; w != NULL && ret >= 0; w = w->next)
	{
		ret = write_ephy_window (writer, (SessionWindow *) w->data);
	}
	if (ret < 0) goto out;

	ret = xmlTextWriterEndElement (writer); /* session */
	if (ret < 0) goto out;

	ret = xmlTextWriterEndDocument (writer);

out:
	if (writer)
		xmlFreeTextWriter (writer);

	if (ret >= 0 && !g_cancellable_is_cancelled (cancellable))
	{
		GError *error = NULL;

		if (!g_file_replace_contents (data->save_file,
					      (const char *)buffer->content,
					      buffer->use,
					      NULL, TRUE, 0, NULL,
					      cancellable, &error))
		{
			if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			{
				g_warning ("Error saving session: %s", error->message);
			}
			g_error_free (error);
		}
	}

	xmlBufferFree (buffer);

	g_task_return_boolean (task, TRUE);

	STOP_PROFILER ("Saving session")
}

void
ephy_session_save (EphySession *session,
		   const char *filename)
{
	EphySessionPrivate *priv;
	EphyShell *shell;
	SaveData *data;
	GTask *task;

	g_return_if_fail (EPHY_IS_SESSION (session));

	priv = session->priv;

	if (priv->save_cancellable)
	{
		g_cancellable_cancel (priv->save_cancellable);
		g_object_unref (priv->save_cancellable);
		priv->save_cancellable = NULL;
	}

	if (priv->dont_save)
	{
		return;
	}

	LOG ("ephy_sesion_save %s", filename);

	shell = ephy_shell_get_default ();

	if (ephy_shell_get_n_windows (shell) == 0)
	{
		session_delete (session, filename);
		return;
	}

	priv->save_cancellable = g_cancellable_new ();
	data = save_data_new (session, filename);
	g_application_hold (G_APPLICATION (shell));

	task = g_task_new (session, priv->save_cancellable,
			   save_session_in_thread_cb, NULL);
	g_task_set_task_data (task, data, (GDestroyNotify)save_data_free);
	g_task_run_in_thread (task, save_session_sync);
	g_object_unref (task);
}

static void
confirm_before_recover (EphyWindow *window, const char *url, const char *title)
{
	EphyEmbed *embed;

	embed = ephy_shell_new_tab_full (ephy_shell_get_default (),
					 title, NULL,
					 window, NULL,
					 EPHY_NEW_TAB_APPEND_LAST,
					 0);

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
		EphyNewTabFlags flags;
		EphyEmbed *embed;
		EphyWebView *web_view;
		gboolean delay_loading;

		delay_loading = g_settings_get_boolean (EPHY_SETTINGS_MAIN,
							EPHY_PREFS_RESTORE_SESSION_DELAYING_LOADS);

		flags = EPHY_NEW_TAB_APPEND_LAST;

		embed = ephy_shell_new_tab_full (ephy_shell_get_default (),
						 title, NULL,
						 context->window, NULL, flags,
						 0);

		web_view = ephy_embed_get_web_view (embed);
		if (delay_loading)
		{
			WebKitURIRequest *request = webkit_uri_request_new (url);

			ephy_embed_set_delayed_load_request (embed, request);
			ephy_web_view_set_placeholder (web_view, url, title);
			g_object_unref (request);
		}
		else
		{
			ephy_web_view_load_url (web_view, url);
		}
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
		EphyEmbedShell *shell = ephy_embed_shell_get_default ();

		notebook = ephy_window_get_notebook (context->window);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), context->active_tab);

		if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) != EPHY_EMBED_SHELL_MODE_TEST)
		{
			EphyEmbed *active_child;

			active_child = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (context->window));
			gtk_widget_grab_focus (GTK_WIDGET (active_child));
			gtk_widget_show (GTK_WIDGET (context->window));
		}

		ephy_embed_shell_restored_window (shell);

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
	char buffer[1024];
} LoadFromStreamAsyncData;

static LoadFromStreamAsyncData *
load_from_stream_async_data_new (GMarkupParseContext *parser)
{
	LoadFromStreamAsyncData *data;

	data = g_slice_new (LoadFromStreamAsyncData);
	data->shell = g_object_ref (ephy_shell_get_default ());
	data->parser = parser;

	return data;
}

static void
load_from_stream_async_data_free (LoadFromStreamAsyncData *data)
{
	g_object_unref (data->shell);
	g_markup_parse_context_free (data->parser);

	g_slice_free (LoadFromStreamAsyncData, data);
}

static void
load_stream_complete (GTask *task)
{
	EphySession *session;

	g_task_return_boolean (task, TRUE);

	session = EPHY_SESSION (g_task_get_source_object (task));
	session->priv->dont_save = FALSE;

	ephy_session_save (session, SESSION_STATE);

	g_object_unref (task);

	g_application_release (G_APPLICATION (ephy_shell_get_default ()));
}

static void
load_stream_complete_error (GTask *task,
			    GError *error)
{
	EphySession *session;
	LoadFromStreamAsyncData *data;
	SessionParserContext *context;

	g_task_return_error (task, error);

	session = EPHY_SESSION (g_task_get_source_object (task));
	session->priv->dont_save = FALSE;
	/* If the session fails to load for whatever reason,
	 * delete the file and open an empty window.
	 */
	session_delete (session, SESSION_STATE);

	data = g_task_get_task_data (task);
	context = (SessionParserContext *)g_markup_parse_context_get_user_data (data->parser);
	session_maybe_open_window (session, context->user_time);

	g_object_unref (task);

	g_application_release (G_APPLICATION (ephy_shell_get_default ()));
}

static void
load_stream_read_cb (GObject *object,
		     GAsyncResult *result,
		     gpointer user_data)
{
	GInputStream *stream = G_INPUT_STREAM (object);
	GTask *task = G_TASK (user_data);
	LoadFromStreamAsyncData *data;
	gssize bytes_read;
	GError *error = NULL;

	bytes_read = g_input_stream_read_finish (stream, result, &error);
	if (bytes_read < 0)
	{
		load_stream_complete_error (task, error);

		return;
	}

	data = g_task_get_task_data (task);
	if (bytes_read == 0)
	{
		if (!g_markup_parse_context_end_parse (data->parser, &error))
		{
			load_stream_complete_error (task, error);
		}
		else
		{
			load_stream_complete (task);
		}

		return;
	}

	if (!g_markup_parse_context_parse (data->parser, data->buffer, bytes_read, &error))
	{
		load_stream_complete_error (task, error);

		return;
	}

	g_input_stream_read_async (stream, data->buffer, sizeof (data->buffer),
				   g_task_get_priority (task),
				   g_task_get_cancellable (task),
				   load_stream_read_cb, task);
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
	GTask *task;
	SessionParserContext *context;
	GMarkupParseContext *parser;
	LoadFromStreamAsyncData *data;

	g_return_if_fail (EPHY_IS_SESSION (session));
	g_return_if_fail (G_IS_INPUT_STREAM (stream));

	g_application_hold (G_APPLICATION (ephy_shell_get_default ()));

	session->priv->dont_save = TRUE;

	task = g_task_new (session, cancellable, callback, user_data);
	/* Use a priority lower than drawing events (HIGH_IDLE + 20) to make sure
	 * the main window is shown as soon as possible at startup
	 */
	g_task_set_priority (task, G_PRIORITY_HIGH_IDLE + 30);

	context = session_parser_context_new (session, user_time);
	parser = g_markup_parse_context_new (&session_parser, 0, context, (GDestroyNotify)session_parser_context_free);
	data = load_from_stream_async_data_new (parser);
	g_task_set_task_data (task, data, (GDestroyNotify)load_from_stream_async_data_free);

	g_input_stream_read_async (stream, data->buffer, sizeof (data->buffer),
				   g_task_get_priority (task), cancellable,
				   load_stream_read_cb, task);
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
	g_return_val_if_fail (g_task_is_valid (result, session), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct {
	guint32 user_time;
} LoadAsyncData;

static LoadAsyncData *
load_async_data_new (guint32 user_time)
{
	LoadAsyncData *data;

	data = g_slice_new (LoadAsyncData);
	data->user_time = user_time;

	return data;
}

static void
load_async_data_free (LoadAsyncData *data)
{
	g_slice_free (LoadAsyncData, data);
}

static void
load_from_stream_cb (GObject *object,
		     GAsyncResult *result,
		     gpointer user_data)
{
	EphySession *session = EPHY_SESSION (object);
	GTask *task = G_TASK (user_data);
	GError *error = NULL;

	if (!ephy_session_load_from_stream_finish (session, result, &error))
	{
		g_task_return_error (task, error);
	}
	else
	{
		g_task_return_boolean (task, TRUE);
	}

	g_object_unref (task);
}

static void
session_read_cb (GObject *object,
		 GAsyncResult *result,
		 gpointer user_data)
{
	GFileInputStream *stream;
	GTask *task = G_TASK (user_data);
	GError *error = NULL;

	stream = g_file_read_finish (G_FILE (object), result, &error);
	if (stream)
	{
		EphySession *session;
		LoadAsyncData *data;

		session = EPHY_SESSION (g_task_get_source_object (task));
		data = g_task_get_task_data (task);
		ephy_session_load_from_stream (session, G_INPUT_STREAM (stream), data->user_time,
					       g_task_get_cancellable (task), load_from_stream_cb, task);
		g_object_unref (stream);
	}
	else
	{
		g_task_return_error (task, error);
		g_object_unref (task);
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
	GTask *task;
	LoadAsyncData *data;

	g_return_if_fail (EPHY_IS_SESSION (session));
	g_return_if_fail (filename);

	LOG ("ephy_sesion_load %s", filename);

	g_application_hold (G_APPLICATION (ephy_shell_get_default ()));

	task = g_task_new (session, cancellable, callback, user_data);
	/* Use a priority lower than drawing events (HIGH_IDLE + 20) to make sure
	 * the main window is shown as soon as possible at startup
	 */
	g_task_set_priority (task, G_PRIORITY_HIGH_IDLE + 30);

	save_to_file = get_session_file (filename);
	data = load_async_data_new (user_time);
	g_task_set_task_data (task, data, (GDestroyNotify)load_async_data_free);
	g_file_read_async (save_to_file, g_task_get_priority (task), cancellable, session_read_cb, task);
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
	g_return_val_if_fail (g_task_is_valid (result, session), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
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
	GTask *task = G_TASK (user_data);
	GError *error = NULL;

	if (!ephy_session_load_finish (session, result, &error))
	{
		g_task_return_error (task, error);
	}
	else
	{
		g_task_return_boolean (task, TRUE);
	}

	g_object_unref (task);
}

void
ephy_session_resume (EphySession *session,
		     guint32 user_time,
		     GCancellable *cancellable,
		     GAsyncReadyCallback callback,
		     gpointer user_data)
{
	GTask *task;
	gboolean has_session_state;
	EphyPrefsRestoreSessionPolicy policy;
	EphyShell *shell;

	LOG ("ephy_session_resume");

	task = g_task_new (session, cancellable, callback, user_data);

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
				   session_resumed_cb, task);
		return;
	}

	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

gboolean
ephy_session_resume_finish (EphySession *session,
			    GAsyncResult *result,
			    GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, session), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}


void
ephy_session_clear (EphySession *session)
{
	EphyShell *shell;
	GList *windows, *p;

	g_return_if_fail (EPHY_IS_SESSION (session));

	shell = ephy_shell_get_default ();
	windows = g_list_copy (gtk_application_get_windows (GTK_APPLICATION (shell)));
	for (p = windows; p; p = p->next)
		gtk_widget_destroy (GTK_WIDGET (p->data));
	g_list_free (windows);
	g_queue_foreach (session->priv->closed_tabs,
			 (GFunc)closed_tab_free, NULL);
	g_queue_clear (session->priv->closed_tabs);

	ephy_session_save (session, SESSION_STATE);
}
