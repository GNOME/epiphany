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
 *
 *  $Id$
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
#include "egg-toolbar-editor.h"
#include "ephy-file-helpers.h"
#include "toolbar.h"
#include "ephy-state.h"
#include "ephy-gui.h"
#include "ephy-zoom.h"

#include <string.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <bonobo/bonobo-i18n.h>
#include <libgnomeui/gnome-about.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnome/gnome-program.h>
#include <gtk/gtkeditable.h>

enum
{
	RESPONSE_ADD_TOOLBAR
};

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
	EphyTab *tab;
	EphyEmbed *embed;
	char *url;
	char *location;
	char *title;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

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
	GdkEvent *event;
	GdkEventType type;
	guint state = 0;
	gboolean force = FALSE;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	event = gtk_get_current_event ();
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

	if (state & GDK_SHIFT_MASK)
	{
		force = TRUE;
	}

	ephy_embed_reload (embed, force ? EMBED_RELOAD_NORMAL
					: EMBED_RELOAD_BYPASSCACHE);
}

void
window_cmd_file_new_window (EggAction *action,
		            EphyWindow *window)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	ephy_shell_new_tab (ephy_shell, NULL, tab, NULL,
			    EPHY_NEW_TAB_NEW_PAGE |
			    EPHY_NEW_TAB_IN_NEW_WINDOW);
}

void
window_cmd_file_new_tab (EggAction *action,
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
window_cmd_go_bookmarks (EggAction *action,
			 EphyWindow *window)
{
	ephy_shell_show_bookmarks_editor (ephy_shell, GTK_WIDGET (window));
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
	char *title = NULL;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	location = ephy_tab_get_location (tab);
	if (ephy_embed_get_title (embed, &title) != G_OK)
	{
		title = g_strdup (_("Untitled"));
	}

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

	g_free (title);
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
                 &file, NULL, NULL);

	uri = gnome_vfs_uri_new (file);
	if (uri)
	{
		retDir = gnome_vfs_uri_extract_dirname (uri);

		/* set default open dir */
		eel_gconf_set_string (CONF_STATE_OPEN_DIR,
				      retDir);

		g_free (retDir);
		gnome_vfs_uri_unref (uri);

		if (result == G_OK)
		{
			ephy_window_load_url(window, file);
	        }
	}

	g_free (file);
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
}

void
window_cmd_edit_find_prev (EggAction *action,
			   EphyWindow *window)
{
	EphyDialog *dialog;

	dialog = ephy_window_get_find_dialog (window);

	find_dialog_go_prev (FIND_DIALOG(dialog), FALSE);
}

void
window_cmd_view_bookmarks_bar (EggAction *action,
			       EphyWindow *window)
{
	EmbedChromeMask mask;
	gboolean active;
	gboolean current_state;

	mask = ephy_window_get_chrome (window);
	active = EGG_TOGGLE_ACTION (action)->active;
	current_state = (mask & EMBED_CHROME_BOOKMARKSBARON) > 0;

	if (active != current_state)
	{
		mask ^= EMBED_CHROME_BOOKMARKSBARON;
		ephy_window_set_chrome (window, mask);
	}
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
	ephy_window_set_zoom (window, ZOOM_IN);
}

void
window_cmd_view_zoom_out (EggAction *action,
			  EphyWindow *window)
{
	ephy_window_set_zoom (window, ZOOM_OUT);
}

void
window_cmd_view_zoom_normal (EggAction *action,
			     EphyWindow *window)
{
	ephy_window_set_zoom (window, 1.0);
}

void
window_cmd_view_page_source (EggAction *action,
			     EphyWindow *window)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	ephy_shell_new_tab (ephy_shell, window, tab, NULL,
			    EPHY_NEW_TAB_CLONE_PAGE |
			    EPHY_NEW_TAB_SOURCE_MODE);
}

void
window_cmd_go_history (EggAction *action,
		       EphyWindow *window)
{
	ephy_shell_show_history_window (ephy_shell, GTK_WIDGET (window));
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
	EphyDialog *dialog;

	dialog = prefs_dialog_new (GTK_WIDGET (window));
	ephy_dialog_show (dialog);
}

static void
toolbar_editor_destroy_cb (GtkWidget *tbe,
			   Toolbar *t)
{
	egg_editable_toolbar_set_edit_mode (EGG_EDITABLE_TOOLBAR (t), FALSE);
}

static void
toolbar_editor_response_cb (GtkDialog  *dialog,
			    gint response_id,
			    gpointer data)
{
	EphyToolbarsModel *model;
	int n;

	switch (response_id)
	{
	case GTK_RESPONSE_CLOSE:
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	case RESPONSE_ADD_TOOLBAR:
		model = ephy_shell_get_toolbars_model (ephy_shell);
		n = egg_toolbars_model_n_toolbars (EGG_TOOLBARS_MODEL (model));
		egg_toolbars_model_add_toolbar (EGG_TOOLBARS_MODEL (model),
						n - 1, "UserCreated");
		break;
	case GTK_RESPONSE_HELP:
		/* FIXME: Connect toolbar editor help */
		break;
	}
}

void
window_cmd_edit_toolbar (EggAction *action,
			 EphyWindow *window)
{
	GtkWidget *editor;
	EphyToolbarsModel *model;
	Toolbar *t;
	GtkWidget *dialog;

	model = ephy_shell_get_toolbars_model (ephy_shell);
	t = ephy_window_get_toolbar (window);

	dialog = gtk_dialog_new ();
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Toolbar Editor"));
        gtk_window_set_transient_for (GTK_WINDOW (dialog),
				      GTK_WINDOW (window));

	editor = egg_toolbar_editor_new
		(EGG_MENU_MERGE (window->ui_merge),
		 EGG_TOOLBARS_MODEL (model));
	egg_toolbar_editor_load_actions (EGG_TOOLBAR_EDITOR (editor),
					 ephy_file ("epiphany-toolbar.xml"));
	gtk_container_set_border_width (GTK_CONTAINER (EGG_TOOLBAR_EDITOR (editor)), 5);
	gtk_box_set_spacing (GTK_BOX (EGG_TOOLBAR_EDITOR (editor)), 5);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), editor);
	g_signal_connect (editor, "destroy",
			  G_CALLBACK (toolbar_editor_destroy_cb),
			  t);
	gtk_widget_show (editor);
	
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);

	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)), 5);
	
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_Add a New Toolbar"), RESPONSE_ADD_TOOLBAR);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       GTK_STOCK_HELP, GTK_RESPONSE_HELP);
	
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (toolbar_editor_response_cb), NULL);
	ephy_state_add_window (dialog,
			       "toolbar_editor",
		               500, 330,
			       EPHY_STATE_WINDOW_SAVE_SIZE);
	gtk_widget_show (dialog);

	egg_editable_toolbar_set_edit_mode (EGG_EDITABLE_TOOLBAR (t), TRUE);
}

void
window_cmd_help_contents (EggAction *action,
			 EphyWindow *window)
{
	ephy_gui_help (GTK_WINDOW (window), "epiphany", NULL);
}

void
window_cmd_help_about (EggAction *action,
		       GtkWidget *window)
{
	static GtkWidget *about = NULL;
	GtkWidget** ptr;
	GdkPixbuf *icon;
	const char *icon_path;
	GdkPixbuf *logo;

	static gchar *authors[] = {
		"Marco Pesenti Gritti <mpeseng@tin.it>",
		"Xan Lopez <xan@masilla.org>",
		"David Bordoley <bordoley@msu.edu>",
		NULL
	};

	gchar *documenters[] = {
		"Patanjali Somayaji <patanjali@codito.com>",
		"David Bordoley <bordoley@msu.edu>",
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

	icon_path = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP,
					  "epiphany.png", TRUE, NULL);
	logo = gdk_pixbuf_new_from_file (icon_path, NULL);
	g_return_if_fail (logo != NULL);

	about = gnome_about_new(
		       "Epiphany", VERSION,
		       "Copyright \xc2\xa9 2002-2003 Marco Pesenti Gritti",
		       _("A GNOME browser based on Mozilla"),
		       (const char **)authors,
		       (const char **)documenters,
		       strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
		       logo);

	g_object_unref (logo);

	gtk_window_set_transient_for (GTK_WINDOW (about),
				      GTK_WINDOW (window));
	
	icon = gtk_widget_render_icon (about, 
						      GNOME_STOCK_ABOUT,
						      GTK_ICON_SIZE_MENU,
						      NULL);
	gtk_window_set_icon (GTK_WINDOW (about), icon);
	g_object_unref(icon);

	ptr = &about;
	g_object_add_weak_pointer (G_OBJECT (about), (gpointer *)ptr);
	gtk_widget_show (about);
}

void
window_cmd_tabs_next (EggAction *action,
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
window_cmd_tabs_previous (EggAction *action,
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
		ephy_notebook_move_page (EPHY_NOTEBOOK (notebook), NULL, child, page - 1);
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
		ephy_notebook_move_page (EPHY_NOTEBOOK (notebook), NULL, child, page + 1);
	}
}

void
window_cmd_tabs_detach  (EggAction *action,
			 EphyWindow *window)
{
	EphyTab *tab;
	GtkWidget *src_page, *nb;
	EphyWindow *new_win;

	nb = ephy_window_get_notebook (window);
	if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (nb)) <= 1) return;

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

