/*
 *  Copyright © 2002 Jorn Baayen
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005, 2006 Christian Persch
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
#include "ephy-stock-icons.h"
#include "ephy-glib-compat.h"
#include "ephy-notebook.h"

#include <glib/gi18n.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>

#include <libgnomeui/gnome-client.h>

#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <libxml/tree.h>
#include <libxml/xmlwriter.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>

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
	GtkWidget *resume_dialog;

	GQueue *queue;
	guint queue_idle_id;

	guint dont_save : 1;
	guint quit_while_resuming : 1;
};

#define BOOKMARKS_EDITOR_ID	"BookmarksEditor"
#define HISTORY_WINDOW_ID	"HistoryWindow"
#define SESSION_CRASHED		"type:session_crashed"

static void ephy_session_class_init	(EphySessionClass *klass);
static void ephy_session_iface_init	(EphyExtensionIface *iface);
static void ephy_session_init		(EphySession *session);
static void session_command_queue_next	(EphySession *session);

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
		const GTypeInfo our_info =
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

		const GInterfaceInfo extension_info =
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

/* Gnome session client */

typedef struct
{
	GtkWidget *dialog;
	GtkWidget *label;
	guint timeout_id;
	guint ticks;
	int response;
	int key;
} InteractData;

static void
confirm_shutdown_dialog_update_timeout_label (InteractData *data)
{
	char *text;

	text = g_strdup_printf (ngettext ("Downloads will be aborted and logout proceed in %d second.",
					  "Downloads will be aborted and logout proceed in %d seconds.",
					  data->ticks),
				data->ticks);

	gtk_label_set_text (GTK_LABEL (data->label), text);
	g_free (text);
}
		
static gboolean
confirm_shutdown_dialog_tick_cb (InteractData *data)
{
	if (data->ticks > 0)
	{
		--data->ticks;
		confirm_shutdown_dialog_update_timeout_label (data);
		return TRUE;
	}

	data->timeout_id = 0;
	gtk_dialog_response (GTK_DIALOG (data->dialog),
			     GTK_RESPONSE_ACCEPT);
	return FALSE;
}

static void
confirm_shutdown_dialog_response_cb (GtkWidget *dialog,
				     int response,
				     InteractData *data)
{
	LOG ("confirm_shutdown_dialog_response_cb response %d", response);

	data->response = response;

	gtk_widget_destroy (dialog);
}

static void
confirm_shutdown_dialog_accept_cb (InteractData *data,
				   GObject *zombie)
{
	gtk_dialog_response (GTK_DIALOG (data->dialog),
			     GTK_RESPONSE_ACCEPT);
}

static void
confirm_shutdown_dialog_weak_ref_cb (InteractData *data,
				     GObject *zombie)
{
	EphyShell *shell;
	GObject *dv;
	int key;
	gboolean cancel_shutdown;

	LOG ("confirm_shutdown_dialog_weak_ref_cb response %d", data->response);

	shell = ephy_shell_get_default ();
	if (shell != NULL)
	{
		g_object_weak_unref (G_OBJECT (shell),
				     (GWeakNotify) confirm_shutdown_dialog_accept_cb,
				     data);

		dv = ephy_embed_shell_get_downloader_view_nocreate (ephy_embed_shell_get_default ());
		if (dv != NULL)
		{
			g_object_weak_unref (dv,
					     (GWeakNotify) confirm_shutdown_dialog_accept_cb,
					     data);
		}
	}

	if (data->timeout_id != 0)
	{
		g_source_remove (data->timeout_id);
	}

	key = data->key;
	cancel_shutdown = data->response != GTK_RESPONSE_ACCEPT;

	g_free (data);

	gnome_interaction_key_return (key, cancel_shutdown);
}

static void
confirm_shutdown_cb (GnomeClient *client,
		     int key,
		     GnomeDialogType dialog_type,
		     gpointer user_data)
{
	GObject *dv;
	GtkWidget *dialog, *box;
	InteractData *data;

	/* FIXME: Can this happen: We already quit? */
	if (ephy_shell_get_default () == NULL)
	{
		gnome_interaction_key_return (key, FALSE);
		return;
	}

	dv = ephy_embed_shell_get_downloader_view_nocreate (ephy_embed_shell_get_default ());

	/* Check if there are still downloads pending */
	if (dv == NULL)
	{
		gnome_interaction_key_return (key, FALSE);
		return;
	}

	dialog = gtk_message_dialog_new
		(NULL,
		 GTK_DIALOG_MODAL,
		 GTK_MESSAGE_WARNING,
		 GTK_BUTTONS_NONE,
		 _("Abort pending downloads?"));

	gtk_message_dialog_format_secondary_text
		(GTK_MESSAGE_DIALOG (dialog),
		 _("There are still downloads pending. If you log out, "
		   "they will be aborted and lost."));

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_Cancel Logout"), GTK_RESPONSE_REJECT);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_Abort Downloads"), GTK_RESPONSE_ACCEPT);

	gtk_window_set_title (GTK_WINDOW (dialog), "");
	gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT);

	data = g_new (InteractData, 1);
	data->dialog = dialog;
	data->response = GTK_RESPONSE_REJECT;
	data->key = key;

	/* This isn't very exact, but it's good enough here */
	data->timeout_id = g_timeout_add_seconds (1,
					  (GSourceFunc) confirm_shutdown_dialog_tick_cb,
					  data);
	data->ticks = 60;

	/* Add timeout label */
	data->label = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (data->label), TRUE);
	confirm_shutdown_dialog_update_timeout_label (data);

	box = ephy_gui_message_dialog_get_content_box (dialog);
	gtk_box_pack_end (GTK_BOX (box), data->label, FALSE, FALSE, 0);
	gtk_widget_show (data->label);

	/* When we're quitting, un-veto the shutdown  */
	g_object_weak_ref (G_OBJECT (ephy_shell_get_default ()),
			   (GWeakNotify) confirm_shutdown_dialog_accept_cb,
			   data);

	/* When the download finishes, un-veto the shutdown */
	g_object_weak_ref (dv,
			   (GWeakNotify) confirm_shutdown_dialog_accept_cb,
			   data);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (confirm_shutdown_dialog_response_cb), data);
	g_object_weak_ref (G_OBJECT (dialog),
			   (GWeakNotify) confirm_shutdown_dialog_weak_ref_cb,
			   data);

	gtk_window_present (GTK_WINDOW (dialog));
}

static gboolean
save_yourself_cb (GnomeClient *client,
		  int phase,
		  GnomeSaveStyle save_style,
		  gboolean shutdown,
		  GnomeInteractStyle interact_style,
		  gboolean fast,
		  EphySession *session)
{
	char *argv[] = { NULL, "--load-session", NULL };
	char *discard_argv[] = { "rm", "-f", NULL };
	char *tmp, *save_to;

	LOG ("save_yourself_cb");

	tmp = g_build_filename (ephy_dot_dir (),
				"session_gnome-XXXXXX",
				NULL);
	save_to = ephy_file_tmp_filename (tmp, "xml");
	g_free (tmp);

	argv[0] = g_get_prgname ();
	argv[2] = save_to;
	gnome_client_set_restart_command
		(client, 3, argv);

	discard_argv[2] = save_to;
	gnome_client_set_discard_command (client, 3,
					  discard_argv);

	ephy_session_save (session, save_to);

	g_free (save_to);

	/* If we're shutting down, check if there are downloads
	 * remaining, since they can't be restarted.
	 */
	if (shutdown &&
	    ephy_embed_shell_get_downloader_view_nocreate (ephy_embed_shell_get_default ()) != NULL)
	{
		gnome_client_request_interaction (client,
						  GNOME_DIALOG_NORMAL,
						  (GnomeInteractFunction) confirm_shutdown_cb,
						  session);
	}

	return TRUE;
}

static void
die_cb (GnomeClient* client,
	EphySession *session)
	
{
	LOG ("die_cb");

	ephy_session_close (session);
}

/* Helper functions */

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
notebook_page_added_cb (GtkWidget *notebook,
			EphyTab *tab,
			guint position,
			EphySession *session)
{
	g_signal_connect (ephy_tab_get_embed (tab), "net-stop",
			  G_CALLBACK (net_stop_cb), session);
}

static void
notebook_page_removed_cb (GtkWidget *notebook,
			  EphyTab *tab,
			  guint position,
			  EphySession *session)
{
	ephy_session_save (session, SESSION_CRASHED);

	g_signal_handlers_disconnect_by_func
		(ephy_tab_get_embed (tab), G_CALLBACK (net_stop_cb), session);
}

static void
notebook_page_reordered_cb (GtkWidget *notebook,
			    GtkWidget *tab,
			    guint position,
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

	g_free (cmd);

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
resume_dialog_response_cb (GtkWidget *dialog,
			   int response,
			   EphySession *session)
{
	guint32 user_time;

	LOG ("resume_dialog_response_cb response:%d", response);

	gtk_widget_hide (dialog);

	user_time = gtk_get_current_event_time ();

	if (response == GTK_RESPONSE_ACCEPT)
	{
		ephy_session_queue_command (session,
				  	    EPHY_SESSION_CMD_LOAD_SESSION,
					    SESSION_CRASHED, NULL,
					    user_time, TRUE);
	}

	ephy_session_queue_command (session,
				    EPHY_SESSION_CMD_MAYBE_OPEN_WINDOW,
				    NULL, NULL, user_time, FALSE);

	gtk_widget_destroy (dialog);
}

static void
resume_dialog_weak_ref_cb (EphySession *session,
			   GObject *zombie)
{
	EphySessionPrivate *priv = session->priv;

	LOG ("resume_dialog_weak_ref_cb");

	priv->resume_dialog = NULL;

	gtk_window_set_auto_startup_notification (TRUE);

	session_command_queue_next (session);

	g_object_unref (ephy_shell_get_default ());
}

static void
session_command_autoresume (EphySession *session,
			    guint32 user_time)
{
	EphySessionPrivate *priv = session->priv;
	GtkWidget *dialog;
	char *saved_session;
	gboolean crashed_session;

	LOG ("ephy_session_autoresume");

	saved_session = get_session_filename (SESSION_CRASHED);
	crashed_session = g_file_test (saved_session, G_FILE_TEST_EXISTS);
	g_free (saved_session);

	if (crashed_session == FALSE ||
	    priv->windows != NULL ||
	    priv->tool_windows != NULL)
	{
		/* FIXME can this happen? */
		if (priv->resume_dialog != NULL)
		{
			gtk_widget_hide (priv->resume_dialog);
			gtk_widget_destroy (priv->resume_dialog);
		}

		ephy_session_queue_command (session,
					    EPHY_SESSION_CMD_MAYBE_OPEN_WINDOW,
					    NULL, NULL, user_time, FALSE);

		return;
	}

	if (priv->resume_dialog)
	{
		gtk_window_present_with_time (GTK_WINDOW (priv->resume_dialog),
					      user_time);

		return;
	}

	/* Ref the shell while we show the dialogue. The unref
	 * happens in the weak ref notification when the dialogue
	 * is destroyed.
	 */
	g_object_ref (ephy_shell_get_default ());

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
	gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (resume_dialog_response_cb), session);
	g_object_weak_ref (G_OBJECT (dialog),
			   (GWeakNotify) resume_dialog_weak_ref_cb,
			   session);

	/* FIXME ? */
	gtk_window_set_auto_startup_notification (FALSE);

	priv->resume_dialog = dialog;

	gtk_window_present_with_time (GTK_WINDOW (dialog), user_time);
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
	EphyTab *tab;
	EphyNewTabFlags flags = 0;
	guint i;

	shell = ephy_shell_get_default ();

	g_object_ref (shell);

	window = ephy_session_get_active_window (session);

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

		if (url[0] == '\0')
		{
			page_flags = EPHY_NEW_TAB_HOME_PAGE;
		}
		else
		{
			page_flags = EPHY_NEW_TAB_OPEN_PAGE;
		}

		tab = ephy_shell_new_tab_full (shell, window,
					       NULL /* parent tab */,
					       url,
					       flags | page_flags,
					       EPHY_EMBED_CHROME_ALL,
					       FALSE /* is popup? */,
					       user_time);

		window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tab)));
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
							 NULL /* URL */,
							 EPHY_NEW_TAB_IN_NEW_WINDOW |
							 EPHY_NEW_TAB_HOME_PAGE,
							 EPHY_EMBED_CHROME_ALL,
							 FALSE /* is popup? */,
							 cmd->user_time);
			}
			break;
		default:
			g_assert_not_reached ();
			break;
	}

	/* Look if there's anything else to dispatch */
	if (g_queue_is_empty (priv->queue) ||
	    priv->resume_dialog != NULL)
	{
		priv->queue_idle_id = 0;
		run_again = FALSE;
	}

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
	    priv->resume_dialog == NULL &&
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

	if (priv->resume_dialog != NULL)
	{
		gtk_widget_destroy (priv->resume_dialog);
		/* destroying the resume dialogue will set this to NULL */
		g_assert (priv->resume_dialog == NULL);
	}

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
	ephy_session_save (session, SESSION_CRASHED);

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
	ephy_session_save (session, SESSION_CRASHED);

	/* NOTE: since the window will be destroyed anyway, we don't need to
	 * disconnect our signal handlers from its components.
	 */
}

/* Class implementation */

static void
ephy_session_init (EphySession *session)
{
	EphySessionPrivate *priv;
	GnomeClient *client;

	LOG ("EphySession initialising");

	priv = session->priv = EPHY_SESSION_GET_PRIVATE (session);

	priv->queue = g_queue_new ();

	client = gnome_master_client ();
	g_signal_connect (client, "save-yourself",
			  G_CALLBACK (save_yourself_cb), session);
	g_signal_connect (client, "die",
			  G_CALLBACK (die_cb), session);
}

static void
ephy_session_dispose (GObject *object)
{
	EphySession *session = EPHY_SESSION (object);
	EphySessionPrivate *priv = session->priv;
	GnomeClient *client;

	LOG ("EphySession disposing");

	/* Only remove the crashed session if we're not shutting down while
	 * the session resume dialogue was still shown!
	*/
	if (priv->quit_while_resuming == FALSE)
	{
		session_delete (session, SESSION_CRASHED);
	}

	session_command_queue_clear (session);

	client = gnome_master_client ();
	g_signal_handlers_disconnect_by_func
		(client, G_CALLBACK (save_yourself_cb), session);
	g_signal_handlers_disconnect_by_func
		(client, G_CALLBACK (die_cb), session);

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

	parent_class->finalize (object);
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

/* Implementation */

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
	g_object_ref (ephy_shell_get_default ());

	priv->dont_save = TRUE;
	/* need to set this up here while the dialogue hasn't been killed yet */
	priv->quit_while_resuming = priv->resume_dialog != NULL;	

	/* Clear command queue */
	session_command_queue_clear (session);

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

	/* Clear command queue */
	session_command_queue_clear (session);

	g_object_unref (ephy_shell_get_default ());
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

	tabs = ephy_window_get_tabs (window);
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
	EphySessionPrivate *priv = session->priv;
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
		    	gboolean success;
		    	int active_tab;
		    
			window = ephy_window_new ();
			widget = GTK_WIDGET (window);
			restore_geometry (GTK_WINDOW (widget), child);

			ephy_gui_window_update_user_time (widget, user_time);

			/* Now add the tabs */
			parse_embed (child->children, window, session);

			/* Set focus to something sane */
			tmp = xmlGetProp (child, (xmlChar *) "active-tab");
			success = int_from_string ((char *) tmp, &active_tab);
			xmlFree (tmp);
		    	if (success)
		    	{
				GtkWidget *notebook;
				notebook = ephy_window_get_notebook (window);
				gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), active_tab);
			}

			gtk_widget_grab_focus (GTK_WIDGET (ephy_window_get_active_tab (window)));
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

	priv->dont_save = FALSE;

	ephy_session_save (session, SESSION_CRASHED);

	g_object_unref (ephy_shell_get_default ());

	return (priv->windows != NULL || priv->tool_windows != NULL);
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

/**
 * ephy_session_queue_command:
 * @session: a #EphySession
 **/
void
ephy_session_queue_command (EphySession *session,
			    EphySessionCommand command,
			    const char *arg,
			    char **args,
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

	/* FIXME: use g_slice_new */
	cmd = g_new0 (SessionCommand, 1);
	cmd->command = command;
	cmd->arg = arg ? g_strdup (arg) : NULL;
	cmd->args = args ? g_strdupv (args) : NULL;
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

	if (priv->resume_dialog != NULL)
	{
		gtk_window_present_with_time (GTK_WINDOW (priv->resume_dialog),
					      user_time);
	}
}
