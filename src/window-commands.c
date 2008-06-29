/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
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

#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-embed-utils.h"
#include "ephy-shell.h"
#include "ephy-embed-factory.h"
#include "ephy-embed-persist.h"
#include "ephy-debug.h"
#include "ephy-command-manager.h"
#include "window-commands.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-embed-prefs.h"
#include "ephy-dialog.h"
#include "ephy-bookmarks-editor.h"
#include "ephy-history-window.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-toolbar.h"
#include "ephy-state.h"
#include "ephy-gui.h"
#include "ephy-zoom.h"
#include "ephy-notebook.h"
#include "ephy-toolbar-editor.h"
#include "ephy-find-toolbar.h"
#include "ephy-location-entry.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-link.h"
#include "ephy-stock-icons.h"
#include "ephy-string.h"
#include "eel-app-launch-context.h"
#include "pdm-dialog.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

static void
page_setup_done_cb (GtkPageSetup *setup,
		    EphyEmbedShell *shell)
{
	if (setup != NULL)
	{
		ephy_embed_shell_set_page_setup (shell, setup);
	}
}

void
window_cmd_file_print_setup (GtkAction *action,
			     EphyWindow *window)
{
	EphyEmbedShell *shell;

	shell = ephy_embed_shell_get_default ();
	gtk_print_run_page_setup_dialog_async
		(GTK_WINDOW (window),
		 ephy_embed_shell_get_page_setup (shell),
		 ephy_embed_shell_get_print_settings (shell),
		 (GtkPageSetupDoneFunc) page_setup_done_cb,
		 shell);
}

void
window_cmd_file_print_preview (GtkAction *action,
			       EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (EPHY_IS_EMBED (embed));

	ephy_embed_set_print_preview_mode (embed, TRUE);
	_ephy_window_set_print_preview (window, TRUE);
}

void
window_cmd_file_print (GtkAction *action,
		       EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (EPHY_IS_EMBED (embed));

	webkit_web_view_execute_script (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), "print();");
}

void
window_cmd_file_send_to	(GtkAction *action,
			 EphyWindow *window)
{
	EphyEmbed *embed;
	char *handler, *command;
	const char *location, *title;
	GAppInfo *appinfo;
	
	if (eel_gconf_get_boolean ("/desktop/gnome/url-handlers/mailto/enabled") == FALSE)
	{
		/* FIXME: add some UI to inform the user? */
		return;
	}

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	location = ephy_embed_get_address (embed);
	title = ephy_embed_get_title (embed);

	/* FIXME: better use g_app_info_get_default_for_uri_scheme () when it is
	 * implemented.
	 */
	handler = eel_gconf_get_string ("/desktop/gnome/url-handlers/mailto/command");
	command = g_strconcat (handler, "mailto:",
			       "?Subject=\"", title,
			       "\"&Body=\"", location, "\"", NULL);
	
	appinfo = g_app_info_create_from_commandline (command, NULL, 0, NULL);
	ephy_file_launch_application (appinfo, NULL,
				      gtk_get_current_event_time (),
				      GTK_WIDGET (window));

	g_free (handler);
	g_free (command);
}

static gboolean
event_with_shift (void)
{
	GdkEvent *event;
	GdkEventType type = 0;
	guint state = 0;

	event = gtk_get_current_event ();
	if (event)
	{
		type = event->type;

		if (type == GDK_BUTTON_RELEASE)
		{
			state = event->button.state; 
		}
		else if (type == GDK_KEY_PRESS || type == GDK_KEY_RELEASE)
		{
			state = event->key.state;
		}

		gdk_event_free (event);
	}

	return (state & GDK_SHIFT_MASK) != 0;
}

void
window_cmd_go_location (GtkAction *action,
		        EphyWindow *window)
{
	ephy_window_activate_location (window);
}

void
window_cmd_view_stop (GtkAction *action,
		      EphyWindow *window)
{
	EphyEmbed *embed;
	
	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	gtk_widget_grab_focus (GTK_WIDGET (embed));

	webkit_web_view_stop_loading (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
}

void
window_cmd_view_reload (GtkAction *action,
		        EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	gtk_widget_grab_focus (GTK_WIDGET (embed));

	/* FIXME: no way to bypass cache when reloading
	 * with WebKit right now. Old code did:
	 * ephy_embed_reload (embed, event_with_shift ());
	 */
	webkit_web_view_reload (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
}

void
window_cmd_go_bookmarks (GtkAction *action,
			 EphyWindow *window)
{
	GtkWidget *bwindow;

	bwindow = ephy_shell_get_bookmarks_editor (ephy_shell);
	ephy_bookmarks_editor_set_parent (EPHY_BOOKMARKS_EDITOR (bwindow),
			                  GTK_WIDGET (window));
	gtk_window_present (GTK_WINDOW (bwindow));
}

void
window_cmd_file_bookmark_page (GtkAction *action,
			       EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	ephy_bookmarks_ui_add_bookmark (GTK_WINDOW (window),
					ephy_embed_get_address (embed),
					ephy_embed_get_title (embed));
}

static void
open_response_cb (GtkDialog *dialog, int response, EphyWindow *window)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		char *uri, *converted;

		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
		if (uri != NULL)
		{
			converted = g_filename_to_utf8 (uri, -1, NULL, NULL, NULL);
	
			if (converted != NULL)
			{
				ephy_window_load_url (window, converted);
			}
	
			g_free (converted);
			g_free (uri);
	        }
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
window_cmd_file_open (GtkAction *action,
		      EphyWindow *window)
{
	EphyFileChooser *dialog;

	dialog = ephy_file_chooser_new (_("Open"),
					GTK_WIDGET (window),
					GTK_FILE_CHOOSER_ACTION_OPEN,
					CONF_STATE_OPEN_DIR,
					EPHY_FILE_FILTER_ALL_SUPPORTED);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (open_response_cb), window);

	gtk_widget_show (GTK_WIDGET (dialog));
}

void
window_cmd_file_save_as (GtkAction *action,
			 EphyWindow *window)
{
	EphyEmbed *embed;
	EphyEmbedPersist *persist;

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	persist = EPHY_EMBED_PERSIST
		(ephy_embed_factory_new_object (EPHY_TYPE_EMBED_PERSIST));

	ephy_embed_persist_set_embed (persist, embed);
	ephy_embed_persist_set_fc_title (persist, _("Save As"));
	ephy_embed_persist_set_fc_parent (persist,GTK_WINDOW (window));

	ephy_embed_persist_set_flags
		(persist, EPHY_EMBED_PERSIST_MAINDOC | EPHY_EMBED_PERSIST_ASK_DESTINATION);
	ephy_embed_persist_set_persist_key
		(persist, CONF_STATE_SAVE_DIR);

	ephy_embed_persist_save (persist);

	g_object_unref (G_OBJECT(persist));
}

void
window_cmd_file_work_offline (GtkAction *action,
		              EphyWindow *window)
{
	EphyEmbedSingle *single;
	gboolean offline;

	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell));
	offline = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	ephy_embed_single_set_network_status (single, !offline);
}

void
window_cmd_file_close_window (GtkAction *action,
		              EphyWindow *window)
{
	GtkWidget *notebook;
	EphyEmbed *embed;

	notebook = ephy_window_get_notebook (window);

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_QUIT) &&
	    gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) == 1)
	{
		return;
	}

	embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	g_signal_emit_by_name (notebook, "tab-close-request", embed);
}

void
window_cmd_edit_undo (GtkAction *action,
		      EphyWindow *window)
{
	GtkWidget *widget;
	GtkWidget *embed;
	GtkWidget *location_entry;

	widget = gtk_window_get_focus (GTK_WINDOW (window));
	location_entry = gtk_widget_get_ancestor (widget, EPHY_TYPE_LOCATION_ENTRY);
	
	if (location_entry)
	{
		ephy_location_entry_reset (EPHY_LOCATION_ENTRY (location_entry));
	}
	else 
	{
		embed = gtk_widget_get_ancestor (widget, EPHY_TYPE_EMBED);

		if (embed)
		{
			ephy_command_manager_do_command (EPHY_COMMAND_MANAGER (embed), 
							 "cmd_undo");
		}
	}
}

void
window_cmd_edit_redo (GtkAction *action,
		      EphyWindow *window)
{
	GtkWidget *widget;
	GtkWidget *embed;
	GtkWidget *location_entry;

	widget = gtk_window_get_focus (GTK_WINDOW (window));
	location_entry = gtk_widget_get_ancestor (widget, EPHY_TYPE_LOCATION_ENTRY);
	
	if (location_entry)
	{
		ephy_location_entry_undo_reset (EPHY_LOCATION_ENTRY (location_entry));
	}
	else
	{
		embed = gtk_widget_get_ancestor (widget, EPHY_TYPE_EMBED);
		if (embed)
		{
			ephy_command_manager_do_command (EPHY_COMMAND_MANAGER (embed), 
							 "cmd_redo");
		}
	}
}
void
window_cmd_edit_cut (GtkAction *action,
		     EphyWindow *window)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_cut_clipboard (GTK_EDITABLE (widget));
	}
	else
	{
		EphyEmbed *embed;
		embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
		g_return_if_fail (embed != NULL);

		ephy_command_manager_do_command (EPHY_COMMAND_MANAGER (embed),
						 "cmd_cut");
	}
}

void
window_cmd_edit_copy (GtkAction *action,
		      EphyWindow *window)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_copy_clipboard (GTK_EDITABLE (widget));
	}
	else
	{
		EphyEmbed *embed;

		embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
		g_return_if_fail (embed != NULL);

		ephy_command_manager_do_command (EPHY_COMMAND_MANAGER (embed),
						 "cmd_copy");
	}
}

void
window_cmd_edit_paste (GtkAction *action,
		       EphyWindow *window)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_paste_clipboard (GTK_EDITABLE (widget));
	}
	else
	{
		EphyEmbed *embed;

		embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
		g_return_if_fail (embed != NULL);

		ephy_command_manager_do_command (EPHY_COMMAND_MANAGER (embed),
						 "cmd_paste");
	}
}

void
window_cmd_edit_delete (GtkAction *action,
			EphyWindow *window)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_delete_text (GTK_EDITABLE (widget), 0, -1);
	}
	else
	{
		EphyEmbed *embed;

		embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
		g_return_if_fail (embed != NULL);

		ephy_command_manager_do_command (EPHY_COMMAND_MANAGER (embed),
						 "cmd_delete");
	}
}

void
window_cmd_edit_select_all (GtkAction *action,
			    EphyWindow *window)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
	}
	else
	{
		EphyEmbed *embed;

		embed = ephy_embed_container_get_active_child 
                  (EPHY_EMBED_CONTAINER (window));
		g_return_if_fail (embed != NULL);

		ephy_command_manager_do_command (EPHY_COMMAND_MANAGER (embed),
						 "cmd_selectAll");
	}
}

void
window_cmd_edit_find (GtkAction *action,
		      EphyWindow *window)
{
	EphyFindToolbar *toolbar;
	
	toolbar = EPHY_FIND_TOOLBAR (ephy_window_get_find_toolbar (window));
	ephy_find_toolbar_open (toolbar, FALSE, FALSE);
}

void
window_cmd_edit_find_next (GtkAction *action,
			   EphyWindow *window)
{
	EphyFindToolbar *toolbar;

	toolbar = EPHY_FIND_TOOLBAR (ephy_window_get_find_toolbar (window));
	ephy_find_toolbar_find_next (toolbar);
}

void
window_cmd_edit_find_prev (GtkAction *action,
			   EphyWindow *window)
{
	EphyFindToolbar *toolbar;

	toolbar = EPHY_FIND_TOOLBAR (ephy_window_get_find_toolbar (window));
	ephy_find_toolbar_find_previous (toolbar);
}

void
window_cmd_view_fullscreen (GtkAction *action,
			    EphyWindow *window)
{
	/* Otherwise the other toolbar layout shows briefly while switching */
	gtk_widget_hide (ephy_window_get_toolbar (window));

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
	{
		GtkWidget *toolbar_editor;
		toolbar_editor = GTK_WIDGET (g_object_get_data (G_OBJECT (window),
								"EphyToolbarEditor"));
		if (toolbar_editor != NULL)
		{
			/* We don't want the toolbar editor to show
			 * while in fullscreen.
			 */
			gtk_dialog_response (GTK_DIALOG (toolbar_editor),
					     GTK_RESPONSE_DELETE_EVENT);
		}
		gtk_window_fullscreen (GTK_WINDOW (window));
	}
	else
	{
		gtk_window_unfullscreen (GTK_WINDOW (window));
	}
}

void
window_cmd_view_zoom_in	(GtkAction *action,
			 EphyWindow *window)
{
	ephy_window_set_zoom (window, ZOOM_IN);
}

void
window_cmd_view_zoom_out (GtkAction *action,
			  EphyWindow *window)
{
	ephy_window_set_zoom (window, ZOOM_OUT);
}

void
window_cmd_view_zoom_normal (GtkAction *action,
			     EphyWindow *window)
{
	ephy_window_set_zoom (window, 1.0);
}

static void
save_source_completed_cb (EphyEmbedPersist *persist)
{
	const char *dest;
	guint32 user_time;
	GFile *file;

	user_time = ephy_embed_persist_get_user_time (persist);
	dest = ephy_embed_persist_get_dest (persist);
	g_return_if_fail (dest != NULL);

	file = g_file_new_for_path (dest);
	ephy_file_delete_on_exit (file);

	ephy_file_launch_handler ("text/plain", file, user_time);
	g_object_unref (file);
}

static void
save_temp_source (EphyEmbed *embed,
		  guint32 user_time)
{
	char *tmp, *base;
	EphyEmbedPersist *persist;
	const char *static_temp_dir;

	static_temp_dir = ephy_file_tmp_dir ();
	if (static_temp_dir == NULL)
	{
		return;
	}

	base = g_build_filename (static_temp_dir, "viewsourceXXXXXX", NULL);
	tmp = ephy_file_tmp_filename (base, "html");
	g_free (base);
	if (tmp == NULL)
	{
		return;
	}

	persist = EPHY_EMBED_PERSIST
		(ephy_embed_factory_new_object (EPHY_TYPE_EMBED_PERSIST));

	ephy_embed_persist_set_embed (persist, embed);
	ephy_embed_persist_set_flags (persist, EPHY_EMBED_PERSIST_COPY_PAGE |
				      EPHY_EMBED_PERSIST_NO_VIEW);
	ephy_embed_persist_set_dest (persist, tmp);
	ephy_embed_persist_set_user_time (persist, user_time);

	g_signal_connect (persist, "completed",
			  G_CALLBACK (save_source_completed_cb), NULL);

	ephy_embed_persist_save (persist);
	g_object_unref (persist);

	g_free (tmp);
}

void
window_cmd_view_page_source (GtkAction *action,
			     EphyWindow *window)
{
	EphyEmbed *embed;
	const char *address;
	guint32 user_time;

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (embed != NULL);

	address = ephy_embed_get_address (embed);
	user_time = gtk_get_current_event_time ();

	if (g_str_has_prefix (address, "file://"))
	{
		GFile *file;
		
		file = g_file_new_for_uri (address);
		ephy_file_launch_handler ("text/plain", file, user_time);
		
		g_object_unref (file);
	}
	else
	{
		save_temp_source (embed, user_time);
	}
}

void
window_cmd_view_page_security_info (GtkAction *action,
				    EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	g_return_if_fail (EPHY_IS_EMBED (embed));

	ephy_embed_show_page_certificate (embed);
}

void
window_cmd_go_history (GtkAction *action,
		       EphyWindow *window)
{
	GtkWidget *hwindow;

	hwindow = ephy_shell_get_history_window (ephy_shell);
	ephy_history_window_set_parent (EPHY_HISTORY_WINDOW (hwindow),
					GTK_WIDGET (window));
	gtk_window_present (GTK_WINDOW (hwindow));
}

void
window_cmd_edit_personal_data (GtkAction *action,
		               EphyWindow *window)
{
	PdmDialog *dialog;
	EphyEmbed *embed;
	const char *address;
	char *host;

	embed = ephy_embed_container_get_active_child 
          (EPHY_EMBED_CONTAINER (window));
	if (embed == NULL) return;

	address = ephy_embed_get_address (embed);
	
	host = address != NULL ? ephy_string_get_host_name (address) : NULL;

	dialog = EPHY_PDM_DIALOG (ephy_shell_get_pdm_dialog (ephy_shell));
	pdm_dialog_open (dialog, host);

	g_free (host);
}

#if 0
void
window_cmd_edit_certificates (GtkAction *action,
			      EphyWindow *window)
{
}
#endif

void
window_cmd_edit_prefs (GtkAction *action,
		       EphyWindow *window)
{
	EphyDialog *dialog;

	dialog = EPHY_DIALOG (ephy_shell_get_prefs_dialog (ephy_shell));

	ephy_dialog_show (dialog);
}

void
window_cmd_edit_toolbar (GtkAction *action,
			 EphyWindow *window)
{
	ephy_toolbar_editor_show (window);
}

void
window_cmd_help_contents (GtkAction *action,
			 EphyWindow *window)
{
	ephy_gui_help (GTK_WINDOW (window), "epiphany", NULL);
}

#define ABOUT_GROUP "About"

void
window_cmd_help_about (GtkAction *action,
		       GtkWidget *window)
{
	const char *licence_part[] = {
		N_("The GNOME Web Browser is free software; you can redistribute it and/or modify "
		   "it under the terms of the GNU General Public License as published by "
		   "the Free Software Foundation; either version 2 of the License, or "
		   "(at your option) any later version."),
		N_("The GNOME Web Browser is distributed in the hope that it will be useful, "
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		   "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License "
		   "along with the GNOME Web Browser; if not, write to the Free Software Foundation, Inc., "
		   "51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA")
	};

	EphyEmbedShell *shell;
	EphyEmbedSingle *single;
	char *licence, *comments;
	GKeyFile *key_file;
	GError *error = NULL;
	char **list, **authors, **contributors, **past_authors, **artists, **documenters;
	gsize n_authors, n_contributors, n_past_authors, n_artists, n_documenters, i, j;

	key_file = g_key_file_new ();
	if (!g_key_file_load_from_file (key_file, DATADIR G_DIR_SEPARATOR_S "about.ini",
				        0, &error))
	{
		g_warning ("Couldn't load about data: %s\n", error->message);
		g_error_free (error);
		return;
	}

	list = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Authors",
					   &n_authors, NULL);
	contributors = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Contributors",
						   &n_contributors, NULL);
	past_authors = g_key_file_get_string_list (key_file, ABOUT_GROUP, "PastAuthors",
						   &n_past_authors, NULL);

#define APPEND(_to,_from) \
	_to[i++] = g_strdup (_from);

#define APPEND_STRV_AND_FREE(_to,_from) \
	if (_from)\
	{\
		for (j = 0; _from[j] != NULL; ++j)\
		{\
			_to[i++] = _from[j];\
		}\
		g_free (_from);\
	}

	authors = g_new (char *, (list ? n_authors : 0) +
			 	 (contributors ? n_contributors : 0) +
				 (past_authors ? n_past_authors : 0) + 7 + 1);
	i = 0;
	APPEND_STRV_AND_FREE (authors, list);
	APPEND (authors, "");
	APPEND (authors, _("Contact us at:"));
	APPEND (authors, "<epiphany-list@gnome.org>");
	APPEND (authors, "");
	APPEND (authors, _("Contributors:"));
	APPEND_STRV_AND_FREE (authors, contributors);
	APPEND (authors, "");
	APPEND (authors, _("Past developers:"));
	APPEND_STRV_AND_FREE (authors, past_authors);
	authors[i++] = NULL;

	list = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Artists", &n_artists, NULL);

	artists = g_new (char *, (list ? n_artists : 0) + 4 + 1);
	i = 0;
	APPEND_STRV_AND_FREE (artists, list);
	APPEND (artists, "");
	APPEND (artists, _("Contact us at:"));
	APPEND (artists, "<gnome-art-list@gnome.org>");
	APPEND (artists, "<gnome-themes-list@gnome.org>");
	artists[i++] = NULL;
	
	list = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Documenters", &n_documenters, NULL);

	documenters = g_new (char *, (list ? n_documenters : 0) + 3 + 1);
	i = 0;
	APPEND_STRV_AND_FREE (documenters, list);
	APPEND (documenters, "");
	APPEND (documenters, _("Contact us at:"));
	APPEND (documenters, "<gnome-doc-list@gnome.org>");
	documenters[i++] = NULL;
	
#undef APPEND
#undef APPEND_STRV_AND_FREE

	g_key_file_free (key_file);

	shell = ephy_embed_shell_get_default ();
	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (shell));

	comments = g_strdup_printf (_("Lets you view web pages and find information on the internet.\n"
				      "Powered by %s"), ephy_embed_single_get_backend_name (single) );

	licence = g_strjoin ("\n\n",
			     _(licence_part[0]),
			     _(licence_part[1]),
			     _(licence_part[2]),
			    NULL);

	gtk_show_about_dialog (GTK_WINDOW (window),
			       "name", _("GNOME Web Browser"),
			       "version", VERSION,
			       "copyright", "Copyright © 2002-2004 Marco Pesenti Gritti\n"
					    "Copyright © 2003-2007 The GNOME Web Browser Developers",
			       "artists", artists,
			       "authors", authors,
			       "comments", comments,
			       "documenters", documenters,
				/* Translators: This is a special message that shouldn't be translated
				 * literally. It is used in the about box to give credits to
				 * the translators.
				 * Thus, you should translate it to your name and email address.
				 * You should also include other translators who have contributed to
				 * this translation; in that case, please write each of them on a separate
				 * line seperated by newlines (\n).
				 */
			       "translator-credits", _("translator-credits"),
			       "logo-icon-name", EPHY_STOCK_EPHY,
			       "website", "http://www.gnome.org/projects/epiphany",
			       "website-label", _("GNOME Web Browser Website"),
			       "license", licence,
			       "wrap-license", TRUE,
			       NULL);

	g_free (comments);
	g_free (licence);
	g_strfreev (artists);
	g_strfreev (authors);
	g_strfreev (documenters);
}

void
window_cmd_tabs_next (GtkAction *action,
		      EphyWindow *window)
{
	GtkNotebook *nb;
	gint page;

	nb = GTK_NOTEBOOK (ephy_window_get_notebook (window));
	g_return_if_fail (nb != NULL);

	page = gtk_notebook_get_current_page (nb);
	g_return_if_fail (page != -1);

	if (page < gtk_notebook_get_n_pages (nb) - 1)
	{
		gtk_notebook_set_current_page (nb, page + 1);
	}
}

void
window_cmd_tabs_previous (GtkAction *action,
			  EphyWindow *window)
{
	GtkNotebook *nb;
	gint page;

	nb = GTK_NOTEBOOK (ephy_window_get_notebook (window));
	g_return_if_fail (nb != NULL);

	page = gtk_notebook_get_current_page (nb);
	g_return_if_fail (page != -1);

	if (page > 0)
	{
		gtk_notebook_set_current_page (nb, page - 1);
	}
}

void
window_cmd_tabs_move_left  (GtkAction *action,
			    EphyWindow *window)
{
	GtkWidget *child;
	GtkNotebook *notebook;
	int page;

	notebook = GTK_NOTEBOOK (ephy_window_get_notebook (window));
	page = gtk_notebook_get_current_page (notebook);
	if (page < 1) return;

	child = gtk_notebook_get_nth_page (notebook, page);
	gtk_notebook_reorder_child (notebook, child, page - 1);
}

void window_cmd_tabs_move_right (GtkAction *action,
				 EphyWindow *window)
{
	GtkWidget *child;
	GtkNotebook *notebook;
	int page, n_pages;

	notebook = GTK_NOTEBOOK (ephy_window_get_notebook (window));
	page = gtk_notebook_get_current_page (notebook);
	n_pages = gtk_notebook_get_n_pages (notebook) - 1;
	if (page > n_pages - 1) return;

	child = gtk_notebook_get_nth_page (notebook, page);
	gtk_notebook_reorder_child (notebook, child, page + 1);
}

void
window_cmd_tabs_detach  (GtkAction *action,
			 EphyWindow *window)
{
        EphyEmbed *embed;
        GtkNotebook *notebook;
        EphyWindow *new_window;

        notebook = GTK_NOTEBOOK (ephy_window_get_notebook (window));
        if (gtk_notebook_get_n_pages (notebook) <= 1)
                return;

        embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));

        g_object_ref_sink (embed);
        gtk_notebook_remove_page (notebook, gtk_notebook_page_num (notebook, GTK_WIDGET (embed)));

        new_window = ephy_window_new ();
        ephy_embed_container_add_child (EPHY_EMBED_CONTAINER (new_window), embed, 0, FALSE);
        g_object_unref (embed);

        gtk_window_present (GTK_WINDOW (new_window));
}

void
window_cmd_load_location (GtkAction *action,
			  EphyWindow *window)
{
	EphyToolbar *toolbar;
	const char *location;

	toolbar = EPHY_TOOLBAR (ephy_window_get_toolbar (window));
	location = ephy_toolbar_get_location (toolbar);

	if (location)
	{
		EphyBookmarks *bookmarks;
		char *address;

		bookmarks = ephy_shell_get_bookmarks (ephy_shell_get_default ());

		address = ephy_bookmarks_resolve_address (bookmarks, location, NULL);
		g_return_if_fail (address != NULL);

		ephy_link_open (EPHY_LINK (window), address,
			        ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window)),
				ephy_link_flags_from_current_event () | EPHY_LINK_ALLOW_FIXUP);
	}
}

void
window_cmd_browse_with_caret (GtkAction *action,
			      EphyWindow *window)
{
	gboolean active;

	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	eel_gconf_set_boolean (CONF_BROWSE_WITH_CARET, active);
}
