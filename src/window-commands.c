/*
 *  Copyright (C) 2000-2004 Marco Pesenti Gritti
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

#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
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
#include "ephy-new-bookmark.h"
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

#include <string.h>
#include <glib.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <gtk/gtkaboutdialog.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkicontheme.h>
#include <gtk/gtktoggleaction.h>
#include <glib/gi18n.h>

void
window_cmd_file_print_setup (GtkAction *action,
			     EphyWindow *window)
{
	EphyDialog *dialog;

	dialog = EPHY_DIALOG (ephy_shell_get_print_setup_dialog (ephy_shell));

	ephy_dialog_show (dialog);
}

void
window_cmd_file_print_preview (GtkAction *action,
			       EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	ephy_embed_set_print_preview_mode (embed, TRUE);
	ephy_window_set_print_preview (window, TRUE);
}

void
window_cmd_file_print (GtkAction *action,
		       EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	ephy_embed_print (embed);
}

void
window_cmd_go_back (GtkAction *action,
		    EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	gtk_widget_grab_focus (GTK_WIDGET (embed));

	ephy_embed_go_back (embed);
}

void
window_cmd_go_up (GtkAction *action,
		  EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	gtk_widget_grab_focus (GTK_WIDGET (embed));

	ephy_embed_go_up (embed);
}

void
window_cmd_file_send_to	(GtkAction *action,
			 EphyWindow *window)
{
	EphyTab *tab;
	EphyEmbed *embed;
	char *url, *location, *title;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	location = gnome_vfs_escape_string (ephy_tab_get_address (tab));
	title = gnome_vfs_escape_string (ephy_tab_get_title (tab));

	url = g_strconcat ("mailto:",
                           "?Subject=", title,
                           "&Body=", location, NULL);

	gnome_vfs_url_show (url);

	g_free (title);
	g_free (location);
	g_free (url);
}

void
window_cmd_go_forward (GtkAction *action,
		       EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	gtk_widget_grab_focus (GTK_WIDGET (embed));

	ephy_embed_go_forward (embed);
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
	
	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	gtk_widget_grab_focus (GTK_WIDGET (embed));

	ephy_embed_stop_load (embed);
}

void
window_cmd_view_reload (GtkAction *action,
		        EphyWindow *window)
{
	EphyEmbed *embed;
	GdkEvent *event;
	GdkEventType type;
	guint state = 0;
	gboolean force = FALSE;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	event = gtk_get_current_event ();
	if (event)
	{
		type = event->type;

		if (type == GDK_BUTTON_RELEASE)
		{
			state = event->button.state; 
		}
		else if (type == GDK_KEY_RELEASE)
		{
			state = event->key.state;
		}

		gdk_event_free (event);
	}

	if (state & GDK_SHIFT_MASK)
	{
		force = TRUE;
	}

	gtk_widget_grab_focus (GTK_WIDGET (embed));

	ephy_embed_reload (embed, force);
}

void
window_cmd_file_new_window (GtkAction *action,
		            EphyWindow *window)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	ephy_shell_new_tab (ephy_shell, window, tab, NULL,
			    EPHY_NEW_TAB_NEW_PAGE |
			    EPHY_NEW_TAB_IN_NEW_WINDOW);
}

void
window_cmd_file_new_tab (GtkAction *action,
		         EphyWindow *window)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	ephy_shell_new_tab (ephy_shell, window, tab, NULL,
			    EPHY_NEW_TAB_NEW_PAGE |
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW |
			    EPHY_NEW_TAB_JUMP);
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
	EphyTab *tab;
	EphyEmbed *embed;
	EphyBookmarks *bookmarks;
	GtkWidget *new_bookmark;
	const char *location, *icon, *title;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	location = ephy_tab_get_address (tab);
	title = ephy_tab_get_title (tab);
	icon = ephy_tab_get_icon_address (tab);

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	if (ephy_new_bookmark_is_unique (bookmarks, GTK_WINDOW (window),
					 location))
	{
		new_bookmark = ephy_new_bookmark_new
			(bookmarks, GTK_WINDOW (window), location);
		ephy_new_bookmark_set_title
			(EPHY_NEW_BOOKMARK (new_bookmark), title);
		ephy_new_bookmark_set_icon
			(EPHY_NEW_BOOKMARK (new_bookmark), icon);
		gtk_widget_show (new_bookmark);
	}
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

	embed = ephy_window_get_active_embed (window);
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
	ephy_embed_single_set_offline_mode (single, offline);
}

void
window_cmd_file_close_window (GtkAction *action,
		              EphyWindow *window)
{
	EphyTab *tab;

	if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_QUIT) &&
	    gtk_notebook_get_n_pages (GTK_NOTEBOOK (ephy_window_get_notebook (window))) == 1)
	{
		return;
	}

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	ephy_window_remove_tab (window, tab);
}

void
window_cmd_edit_undo (GtkAction *action,
		      EphyWindow *window)
{
	GtkWidget *widget;
	GtkWidget *embed;

	widget = gtk_window_get_focus (GTK_WINDOW (window));
	embed = gtk_widget_get_ancestor (widget, EPHY_TYPE_EMBED);

	if (embed)
	{
		ephy_command_manager_do_command (EPHY_COMMAND_MANAGER (embed), 
						 "cmd_undo");
	}
}

void
window_cmd_edit_redo (GtkAction *action,
		      EphyWindow *window)
{
	GtkWidget *widget;
	GtkWidget *embed;

	widget = gtk_window_get_focus (GTK_WINDOW (window));
	embed = gtk_widget_get_ancestor (widget, EPHY_TYPE_EMBED);

	if (embed)
	{
		ephy_command_manager_do_command (EPHY_COMMAND_MANAGER (embed), 
						 "cmd_redo");
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
		embed = ephy_window_get_active_embed (window);
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

		embed = ephy_window_get_active_embed (window);
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

		embed = ephy_window_get_active_embed (window);
		g_return_if_fail (embed != NULL);

		ephy_command_manager_do_command (EPHY_COMMAND_MANAGER (embed),
						 "cmd_paste");
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

		embed = ephy_window_get_active_embed (window);
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

	user_time = ephy_embed_persist_get_user_time (persist);
	dest = ephy_embed_persist_get_dest (persist);
	g_return_if_fail (dest != NULL);

	ephy_file_delete_on_exit (dest);

	ephy_file_launch_handler ("text/plain", dest, user_time);
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
	EphyTab *tab;
	EphyEmbed *embed;
	const char *address;
	guint32 user_time;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	address = ephy_tab_get_address (tab);
	user_time = gtk_get_current_event_time ();

	if (g_str_has_prefix (address, "file://"))
	{
		ephy_file_launch_handler ("text/plain", address, user_time);
	}
	else
	{
		save_temp_source (embed, user_time);
	}
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
	EphyDialog *dialog;

	dialog = EPHY_DIALOG (ephy_shell_get_pdm_dialog (ephy_shell));

	ephy_dialog_show (dialog);
}

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

void
window_cmd_help_about (GtkAction *action,
		       GtkWidget *window)
{
	const char * const authors[] = {
		"Marco Pesenti Gritti",
		"Adam Hooper",
		"Xan Lopez",
		"Christian Persch",
		"Jean-François Rameau",
		"",
		_("Contact us at:"),
		"<epiphany-list@gnome.org>",
		"",
		_("Contributors:"),
		"Crispin Flowerday",
		"Raphaël Slinckx",
		"",
		_("Past developers:"),
		"David Bordoley",
		NULL,
	};
	const char * const documenters[] = {
		"Piers Cornwell", 
		"Patanjali Somayaji",
		"David Bordoley",
		"",
		_("Contact us at:"),
		"<epiphany-list@gnome.org> or <gnome-doc-list@gnome.org>",
		NULL
	};

	gtk_show_about_dialog (GTK_WINDOW (window),
			       "name", _("Web Browser"),
			       "version", VERSION,
			       "copyright", "Copyright © 2002-2004 Marco Pesenti Gritti\n"
					    "Copyright © 2003-2005 The GNOME Web Browser Developers",
			       "authors", authors,
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
			       "logo-icon-name", "web-browser",
			       "website", "http://www.gnome.org/projects/epiphany",
			       NULL);
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
	GtkWidget *notebook;
	int page;

	notebook = ephy_window_get_notebook (window);
	page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));

	if (page > 0)
	{
		GtkWidget *child;

		child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), page);
		ephy_notebook_move_tab (EPHY_NOTEBOOK (notebook), NULL,
					EPHY_TAB (child), page - 1);
	}
}

void window_cmd_tabs_move_right (GtkAction *action,
				 EphyWindow *window)
{
	GtkWidget *notebook;
	int page;
	int last_page;

	notebook = ephy_window_get_notebook (window);
	page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	last_page = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) - 1;

	if (page != last_page)
	{
		GtkWidget *child;

		child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), page);
		ephy_notebook_move_tab (EPHY_NOTEBOOK (notebook), NULL,
					EPHY_TAB (child), page + 1);
	}
}

void
window_cmd_tabs_detach  (GtkAction *action,
			 EphyWindow *window)
{
	EphyTab *tab;
	GtkWidget *nb;
	EphyWindow *new_win;

	nb = ephy_window_get_notebook (window);
	if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (nb)) <= 1) return;

	tab = ephy_window_get_active_tab (window);

	new_win = ephy_window_new ();
	ephy_notebook_move_tab (EPHY_NOTEBOOK (ephy_window_get_notebook (window)),
				EPHY_NOTEBOOK (ephy_window_get_notebook (new_win)),
				tab, 0);
	gtk_widget_show (GTK_WIDGET (new_win));
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
		ephy_window_load_url (window, location);
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

#ifdef HAVE_X11_XF86KEYSYM_H
void
window_cmd_go_home (GtkAction *action,
		    EphyWindow *window)
{
	char *location;

	location = eel_gconf_get_string (CONF_GENERAL_HOMEPAGE);

	if (location == NULL || location[0] == '\0')
	{
		g_free (location);

		location = g_strdup ("about:blank");
	}

	ephy_window_load_url (window, location);

	g_free (location);
}
#endif /* HAVE_X11_XF86KEYSYM_H */
