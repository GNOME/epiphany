/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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

/*
 * 25 Mar 2003: Added support for help manual : Patanjali Somayaji
 * 						(patanjali@codito.com)
 */

#include <config.h>

#include "ephy-shell.h"
#include "ephy-debug.h"
#include "window-commands.h"
#include "find-dialog.h"
#include "print-dialog.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-embed-utils.h"
#include "pdm-dialog.h"
#include "ephy-bookmarks-editor.h"
#include "ephy-new-bookmark.h"
#include "egg-toggle-action.h"
#include "egg-editable-toolbar.h"
#include "toolbar.h"

#include <string.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <bonobo/bonobo-i18n.h>
#include <libgnomeui/gnome-about.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnome/gnome-help.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkeditable.h>

void
window_cmd_edit_find (EggAction *action,
		      EphyWindow *window)
{
	EphyDialog *dialog;
	dialog = ephy_window_get_find_dialog (window);

	g_object_ref (dialog);

	ephy_dialog_show (dialog);
}

static void
print_dialog_preview_cb (PrintDialog *dialog,
			 EphyWindow *window)
{
	ephy_window_set_chrome (window, EMBED_CHROME_PPVIEWTOOLBARON);
}

void
window_cmd_file_print (EggAction *action,
		       EphyWindow *window)
{
	EphyDialog *dialog;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	dialog = print_dialog_new_with_parent (GTK_WIDGET(window),
					       embed, NULL);
	g_signal_connect (G_OBJECT(dialog),
			  "preview",
			  G_CALLBACK (print_dialog_preview_cb),
			  window);
	ephy_dialog_set_modal (dialog, TRUE);
	ephy_dialog_show (dialog);
}

void
window_cmd_go_back (EggAction *action,
		    EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_go_back (embed);
}

void
window_cmd_go_up (EggAction *action,
		  EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_go_up (embed);
}

void
window_cmd_file_send_to	(EggAction *action,
			 EphyWindow *window)
{
	char *url;
	EphyTab *tab;
	EphyEmbed *embed;
	char *location;
	char *title;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (tab);

	location = gnome_vfs_escape_string (ephy_tab_get_location (tab));
	if (ephy_embed_get_title (embed, &title) == G_OK)
	{
		char *tmp = gnome_vfs_escape_string (title);
		g_free (title);
		title = tmp;
	}
	else
	{
		title = gnome_vfs_escape_string (_("Check this out!"));
	}

	url = g_strconcat ("mailto:",
                           "?Subject=", title,
                           "&Body=", location, NULL);

	ephy_embed_load_url (embed, url);

	g_free (title);
	g_free (location);
	g_free (url);
}

void
window_cmd_go_forward (EggAction *action,
		       EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_go_forward (embed);
}

void
window_cmd_go_home (EggAction *action,
		    EphyWindow *window)
{
	EphyEmbed *embed;
	char *location;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	location = eel_gconf_get_string (CONF_GENERAL_HOMEPAGE);
	g_return_if_fail (location != NULL);

	ephy_embed_load_url (embed, location);

	g_free (location);
}

void
window_cmd_go_location (EggAction *action,
		        EphyWindow *window)
{
	ephy_window_activate_location (window);
}

void
window_cmd_view_stop (EggAction *action,
		      EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_stop_load (embed);
}

void
window_cmd_view_reload (EggAction *action,
		        EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_reload (embed, EMBED_RELOAD_NORMAL);
}

void
window_cmd_file_new_window (EggAction *action,
		            EphyWindow *window)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);

	ephy_shell_new_tab (ephy_shell, NULL, tab, NULL,
			    EPHY_NEW_TAB_HOMEPAGE |
			    EPHY_NEW_TAB_IN_NEW_WINDOW |
			    EPHY_NEW_TAB_JUMP);
}

void
window_cmd_file_new_tab (EggAction *action,
		         EphyWindow *window)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);

	ephy_shell_new_tab (ephy_shell, window, tab, NULL,
			      EPHY_NEW_TAB_HOMEPAGE |
			      EPHY_NEW_TAB_IN_EXISTING_WINDOW |
			      EPHY_NEW_TAB_JUMP);
}

void
window_cmd_go_bookmarks (EggAction *action,
			 EphyWindow *window)
{
	ephy_shell_show_bookmarks_editor (ephy_shell);
}

void
window_cmd_file_bookmark_page (EggAction *action,
			      EphyWindow *window)
{
	EphyTab *tab;
	EphyEmbed *embed;
	EphyBookmarks *bookmarks;
	GtkWidget *new_bookmark;
	const char *location;
	const char *icon;
	char *title;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (tab);

	location = ephy_tab_get_location (tab);
	if (ephy_embed_get_title (embed, &title) != G_OK)
	{
		title = _("Untitled");
	}

	icon = ephy_tab_get_favicon_url (tab);

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	new_bookmark = ephy_new_bookmark_new
		(bookmarks, GTK_WINDOW (window), location);
	ephy_new_bookmark_set_title
		(EPHY_NEW_BOOKMARK (new_bookmark), title);
	ephy_new_bookmark_set_icon
		(EPHY_NEW_BOOKMARK (new_bookmark), icon);
	gtk_dialog_run (GTK_DIALOG (new_bookmark));
	gtk_widget_destroy (new_bookmark);
}

void
window_cmd_file_open (EggAction *action,
		      EphyWindow *window)
{
	gchar *dir, *retDir;
        gchar *file;
        GnomeVFSURI *uri;
	GtkWidget *wmain;
	EphyEmbedShell *embed_shell;
	gresult result;
	EphyEmbedSingle *single;

	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));

	embed_shell = EPHY_EMBED_SHELL (ephy_shell);

	wmain = GTK_WIDGET (window);
	g_return_if_fail (wmain != NULL);

        dir = eel_gconf_get_string (CONF_STATE_OPEN_DIR);

	result = ephy_embed_single_show_file_picker
		(single, wmain,
		 _("Select the file to open"),
                 dir, NULL, modeOpen,
                 &file, NULL, NULL, NULL);

	if (result == G_OK)
	{
                uri = gnome_vfs_uri_new (file);
                if (uri)
                {

                        ephy_window_load_url(window, file);

                        retDir = gnome_vfs_uri_extract_dirname (uri);

                        /* set default open dir */
                        eel_gconf_set_string (CONF_STATE_OPEN_DIR,
                                              retDir);

                        g_free (retDir);
                        gnome_vfs_uri_unref (uri);
                }
	        g_free (file);
        }

        g_free (dir);
}

void
window_cmd_file_save_as (EggAction *action,
			 EphyWindow *window)
{
	EphyEmbed *embed;
	EphyEmbedPersist *persist;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	persist = ephy_embed_persist_new (embed);
	ephy_embed_persist_set_flags (persist,
					EMBED_PERSIST_MAINDOC);

	ephy_embed_utils_save (GTK_WIDGET(window),
			       CONF_STATE_SAVE_DIR,
			       TRUE,
			       TRUE,
			       persist);

	g_object_unref (G_OBJECT(persist));
}

void
window_cmd_file_close_window (EggAction *action,
		               EphyWindow *window)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	ephy_window_remove_tab (window, tab);
}

void
window_cmd_edit_cut (EggAction *action,
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

		ephy_embed_selection_cut (embed);
	}
}

void
window_cmd_edit_copy (EggAction *action,
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

		ephy_embed_selection_copy (embed);
	}
}

void
window_cmd_edit_paste (EggAction *action,
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

		ephy_embed_paste (embed);
	}
}

void
window_cmd_edit_select_all (EggAction *action,
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

		ephy_embed_select_all (embed);
	}
}

void
window_cmd_edit_find_next (EggAction *action,
			   EphyWindow *window)
{
	EphyDialog *dialog;

	dialog = ephy_window_get_find_dialog (window);

	find_dialog_go_next (FIND_DIALOG(dialog), FALSE);

	ephy_window_update_control (window, FindControl);
}

void
window_cmd_edit_find_prev (EggAction *action,
			   EphyWindow *window)
{
	EphyDialog *dialog;

	dialog = ephy_window_get_find_dialog (window);

	find_dialog_go_prev (FIND_DIALOG(dialog), FALSE);

	ephy_window_update_control (window, FindControl);
}

void
window_cmd_view_toolbar (EggAction *action,
			 EphyWindow *window)
{
	EmbedChromeMask mask;
	gboolean active;
	gboolean current_state;

	mask = ephy_window_get_chrome (window);
	active = EGG_TOGGLE_ACTION (action)->active;
	current_state = (mask & EMBED_CHROME_TOOLBARON) > 0;

	if (active != current_state)
	{
		mask ^= EMBED_CHROME_TOOLBARON;
		ephy_window_set_chrome (window, mask);
	}
}

void
window_cmd_view_statusbar (EggAction *action,
			   EphyWindow *window)
{
	EmbedChromeMask mask;
	gboolean active;
	gboolean current_state;

	mask = ephy_window_get_chrome (window);
	active = EGG_TOGGLE_ACTION (action)->active;
	current_state = (mask & EMBED_CHROME_STATUSBARON) > 0;

	if (active != current_state)
	{
		mask ^= EMBED_CHROME_STATUSBARON;
		ephy_window_set_chrome (window, mask);
	}
}

void
window_cmd_view_fullscreen (EggAction *action,
			    EphyWindow *window)
{
	EmbedChromeMask mask;
	gboolean active;
	gboolean current_state;

	mask = ephy_window_get_chrome (window);
	active = EGG_TOGGLE_ACTION (action)->active;
	current_state = (mask & EMBED_CHROME_OPENASFULLSCREEN) > 0;

	if (active != current_state)
	{
		mask ^= EMBED_CHROME_OPENASFULLSCREEN;
		mask |= EMBED_CHROME_DEFAULT;
		ephy_window_set_chrome (window, mask);
	}
}

void
window_cmd_view_zoom_in	(EggAction *action,
			 EphyWindow *window)
{
	EphyEmbed *embed;
	int zoom;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_zoom_get (embed, &zoom);
	ephy_window_set_zoom (window, zoom + 10);
}

void
window_cmd_view_zoom_out (EggAction *action,
			  EphyWindow *window)
{
	EphyEmbed *embed;
	int zoom;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_zoom_get (embed, &zoom);
	if (zoom >= 10)
	{
		ephy_window_set_zoom (window, zoom - 10);
	}
}

void
window_cmd_view_zoom_normal (EggAction *action,
			     EphyWindow *window)
{
	ephy_window_set_zoom (window, 100);
}

void
window_cmd_view_page_source (EggAction *action,
			     EphyWindow *window)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	ephy_shell_new_tab (ephy_shell, window, tab, NULL,
			      EPHY_NEW_TAB_VIEW_SOURCE);
}

void
window_cmd_go_history (EggAction *action,
		       EphyWindow *window)
{
	ephy_shell_show_history_window (ephy_shell);
}

void
window_cmd_edit_personal_data (EggAction *action,
		               EphyWindow *window)
{
	EphyDialog *dialog;

	dialog = pdm_dialog_new (GTK_WIDGET(window));

	ephy_dialog_show (dialog);
}

void
window_cmd_edit_prefs (EggAction *action,
		       EphyWindow *window)
{
	GtkDialog *dialog;

	dialog = prefs_dialog_new ();
	prefs_dialog_show_page (PREFS_DIALOG(dialog),
				PREFS_PAGE_GENERAL);
	gtk_window_set_transient_for (GTK_WINDOW (dialog),
				      GTK_WINDOW (window));
	gtk_widget_show (GTK_WIDGET(dialog));
}

void
window_cmd_edit_toolbar (EggAction *action,
			 EphyWindow *window)
{
	Toolbar *toolbar;

	toolbar = ephy_window_get_toolbar (window);

	egg_editable_toolbar_edit (EGG_EDITABLE_TOOLBAR (toolbar),
				   GTK_WIDGET (window));
}

void 
window_cmd_help_contents (EggAction *action,
			 EphyWindow *window)
{
	GError *err;

	err = NULL;
	gnome_help_display ("epiphany", NULL, &err);

	if (err != NULL) 
	{
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new (GTK_WINDOW (window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				_("Could not display help: %s"), err->message);

		g_signal_connect (G_OBJECT (dialog), "response",
			G_CALLBACK (gtk_widget_destroy),
			NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_widget_show (dialog);
		g_error_free (err);	
	}
}

void
window_cmd_help_about (EggAction *action,
		       GtkWidget *window)
{
	static GtkWidget *about = NULL;
	GdkPixbuf *icon;

	static gchar *authors[] = {
		"Marco Pesenti Gritti <mpeseng@tin.it>",
		NULL
	};

	gchar *documenters[] = {
		"Patanjali Somayaji <patanjali@codito.com>",
		NULL
	};

	/* Translator credits */
	gchar *translator_credits = _("translator_credits");

	if (about != NULL)
	{
		gdk_window_show(about->window);
		gdk_window_raise(about->window);
		return;
	}

	about = gnome_about_new(
		       "Epiphany", VERSION,
		       "Copyright \xc2\xa9 2002-2003 Marco Pesenti Gritti",
		       _("A GNOME browser based on Mozilla"),
		       (const char **)authors,
		       (const char **)documenters,
		       strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
		       NULL);

	gtk_window_set_transient_for (GTK_WINDOW (about),
				      GTK_WINDOW (window));
	
	icon = gtk_widget_render_icon (about, 
						      GNOME_STOCK_ABOUT,
						      GTK_ICON_SIZE_MENU,
						      NULL);
	gtk_window_set_icon (GTK_WINDOW (about), icon);
	g_object_unref(icon);

	g_object_add_weak_pointer (G_OBJECT (about), (gpointer *)&about);
	gtk_widget_show (about);
}

void
window_cmd_tabs_next (EggAction *action,
		      EphyWindow *window)
{
	GList *tabs;
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);
	tabs = ephy_window_get_tabs (window);
	g_return_if_fail (tab != NULL);

	tabs = g_list_find (tabs, (gpointer)tab);
	tabs = tabs->next;

	if (tabs)
	{
		tab = EPHY_TAB (tabs->data);
		ephy_window_jump_to_tab (window, tab);
		g_list_free (tabs);
	}
}

void
window_cmd_tabs_previous (EggAction *action,
			  EphyWindow *window)
{
	GList *tabs;
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);
	tabs = ephy_window_get_tabs (window);
	g_return_if_fail (tab != NULL);

	tabs = g_list_find (tabs, (gpointer)tab);
	tabs = tabs->prev;

	if (tabs)
	{
		tab = EPHY_TAB (tabs->data);
		ephy_window_jump_to_tab (window, tab);
		g_list_free (tabs);
	}
}

void
window_cmd_tabs_move_left  (EggAction *action,
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
		gtk_notebook_reorder_child (GTK_NOTEBOOK (notebook), child, page - 1);
	}
}

void window_cmd_tabs_move_right (EggAction *action,
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
		gtk_notebook_reorder_child (GTK_NOTEBOOK (notebook), child, page + 1);
	}
}

void
window_cmd_tabs_detach  (EggAction *action,
			 EphyWindow *window)
{
	EphyTab *tab;
	GtkWidget *src_page;
	EphyWindow *new_win;

	if (g_list_length (ephy_window_get_tabs (window)) <= 1) {
		return;
	}

	tab = ephy_window_get_active_tab (window);
	src_page = GTK_WIDGET (ephy_tab_get_embed (tab));
	new_win = ephy_window_new ();
	ephy_notebook_move_page (EPHY_NOTEBOOK (ephy_window_get_notebook (window)),
				EPHY_NOTEBOOK (ephy_window_get_notebook (new_win)),
				src_page, 0);
	ephy_tab_set_window (tab, new_win);
	gtk_widget_show (GTK_WIDGET (new_win));
}

void
window_cmd_load_location (EggAction *action,
			  EphyWindow *window)
{
	Toolbar *toolbar;
	char *location;

	toolbar = ephy_window_get_toolbar (window);
	location = toolbar_get_location (toolbar);

	if (location)
	{
		ephy_window_load_url (window, location);
		g_free (location);
	}
}

