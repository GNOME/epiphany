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
#include "print-dialog.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-embed-utils.h"
#include "pdm-dialog.h"
#include "ephy-bookmarks-editor.h"
#include "ephy-new-bookmark.h"
#include "ephy-file-helpers.h"
#include "toolbar.h"
#include "ephy-state.h"
#include "ephy-gui.h"
#include "ephy-zoom.h"
#include "prefs-dialog.h"
#include "ephy-toolbars-model.h"
#include "egg-editable-toolbar.h"
#include "egg-toolbar-editor.h"

#include <string.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <bonobo/bonobo-i18n.h>
#include <libgnomeui/gnome-about.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnomeui/gnome-icon-theme.h>
#include <libgnome/gnome-program.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtktoggleaction.h>

enum
{
	RESPONSE_ADD_TOOLBAR
};

void
window_cmd_edit_find (GtkAction *action,
		      EphyWindow *window)
{
	ephy_window_find (window);
}

void
window_cmd_file_print (GtkAction *action,
		       EphyWindow *window)
{
	ephy_window_print (window);
}

void
window_cmd_go_back (GtkAction *action,
		    EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_activate (embed);

	ephy_embed_go_back (embed);
}

void
window_cmd_go_up (GtkAction *action,
		  EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_activate (embed);

	ephy_embed_go_up (embed);
}

void
window_cmd_file_send_to	(GtkAction *action,
			 EphyWindow *window)
{
	EphyTab *tab;
	EphyEmbed *embed;
	char *url;
	char *embed_location;
	char *location;
	char *title;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_get_location (embed, TRUE, &embed_location);
	location = gnome_vfs_escape_string (embed_location);
	g_free (embed_location);

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
window_cmd_go_forward (GtkAction *action,
		       EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_activate (embed);

	ephy_embed_go_forward (embed);
}

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

	ephy_embed_activate (embed);

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

	ephy_embed_activate (embed);

	ephy_embed_reload (embed, force ? EMBED_RELOAD_NORMAL
					: EMBED_RELOAD_BYPASSCACHE);
}

void
window_cmd_file_new_window (GtkAction *action,
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
	ephy_shell_show_bookmarks_editor (ephy_shell, GTK_WIDGET (window));
}

void
window_cmd_file_bookmark_page (GtkAction *action,
			       EphyWindow *window)
{
	EphyTab *tab;
	EphyEmbed *embed;
	EphyBookmarks *bookmarks;
	GtkWidget *new_bookmark;
	char *location;
	const char *icon;
	char *title = NULL;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_get_location (embed, TRUE, &location);

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
	g_free (location);
}

void
window_cmd_file_open (GtkAction *action,
		      EphyWindow *window)
{
	char *dir, *ret_dir, *file;
	EphyEmbedShell *embed_shell;
	gresult result;
	EphyEmbedSingle *single;

	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));

	embed_shell = EPHY_EMBED_SHELL (ephy_shell);

        dir = eel_gconf_get_string (CONF_STATE_OPEN_DIR);

	result = ephy_embed_single_show_file_picker
		(single, GTK_WIDGET (window),
		 _("Open"),
                 dir, NULL, modeOpen,
                 &file, NULL, NULL);

	/* persist directory choice */
	/* Fix for bug 122780:
	 * if the user selected a directory, or aborted with no filename typed,
	 * g_path_get_dirname and gnome_vfs_uri_extract_dirname strip the last
	 * path component, so test if the returned file is actually a directory.
	 */
	if (g_file_test (file, G_FILE_TEST_IS_DIR))
	{
		ret_dir = g_strdup (file);
	}
	else
	{
		ret_dir = g_path_get_dirname (file);
	}

	eel_gconf_set_string (CONF_STATE_OPEN_DIR, ret_dir);

	if (result == G_OK)
	{
		ephy_window_load_url(window, file);
        }

	g_free (ret_dir);
	g_free (file);
        g_free (dir);
}

void
window_cmd_file_save_as (GtkAction *action,
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
			       _("Save As"),
			       CONF_STATE_SAVE_DIR,
			       TRUE, persist);

	g_object_unref (G_OBJECT(persist));
}

void
window_cmd_file_close_window (GtkAction *action,
		              EphyWindow *window)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	ephy_window_remove_tab (window, tab);
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

		ephy_embed_selection_cut (embed);
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

		ephy_embed_selection_copy (embed);
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

		ephy_embed_paste (embed);
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

		ephy_embed_select_all (embed);
	}
}

void
window_cmd_edit_find_next (GtkAction *action,
			   EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_find_next (embed, FALSE);
}

void
window_cmd_edit_find_prev (GtkAction *action,
			   EphyWindow *window)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_find_next (embed, TRUE);
}

void
window_cmd_view_bookmarks_bar (GtkAction *action,
			       EphyWindow *window)
{
	EmbedChromeMask mask;
	gboolean active;
	gboolean current_state;

	mask = ephy_window_get_chrome (window);
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	current_state = (mask & EMBED_CHROME_BOOKMARKSBARON) > 0;

	if (active != current_state)
	{
		mask ^= EMBED_CHROME_BOOKMARKSBARON;
		ephy_window_set_chrome (window, mask);
	}
}

void
window_cmd_view_toolbar (GtkAction *action,
			 EphyWindow *window)
{
	EmbedChromeMask mask;
	gboolean active;
	gboolean current_state;

	mask = ephy_window_get_chrome (window);
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	current_state = (mask & EMBED_CHROME_TOOLBARON) > 0;

	if (active != current_state)
	{
		mask ^= EMBED_CHROME_TOOLBARON;
		ephy_window_set_chrome (window, mask);
	}
}

void
window_cmd_view_statusbar (GtkAction *action,
			   EphyWindow *window)
{
	EmbedChromeMask mask;
	gboolean active;
	gboolean current_state;

	mask = ephy_window_get_chrome (window);
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	current_state = (mask & EMBED_CHROME_STATUSBARON) > 0;

	if (active != current_state)
	{
		mask ^= EMBED_CHROME_STATUSBARON;
		ephy_window_set_chrome (window, mask);
	}
}

void
window_cmd_view_fullscreen (GtkAction *action,
			    EphyWindow *window)
{
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

void
window_cmd_view_page_source (GtkAction *action,
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
window_cmd_go_history (GtkAction *action,
		       EphyWindow *window)
{
	ephy_shell_show_history_window (ephy_shell, GTK_WIDGET (window));
}

void
window_cmd_edit_personal_data (GtkAction *action,
		               EphyWindow *window)
{
	EphyDialog *dialog;

	dialog = pdm_dialog_new (GTK_WIDGET(window));

	ephy_dialog_show (dialog);
}

void
window_cmd_edit_prefs (GtkAction *action,
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
		model = EPHY_TOOLBARS_MODEL
			(ephy_shell_get_toolbars_model (ephy_shell, FALSE));
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
window_cmd_edit_toolbar (GtkAction *action,
			 EphyWindow *window)
{
	GtkWidget *editor;
	EphyToolbarsModel *model;
	Toolbar *t;
	GtkWidget *dialog;

	model = EPHY_TOOLBARS_MODEL
		(ephy_shell_get_toolbars_model (ephy_shell, FALSE));
	t = ephy_window_get_toolbar (window);

	dialog = gtk_dialog_new ();
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Toolbar Editor"));
        gtk_window_set_transient_for (GTK_WINDOW (dialog),
				      GTK_WINDOW (window));

	editor = egg_toolbar_editor_new
		(GTK_UI_MANAGER (window->ui_merge),
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
window_cmd_help_contents (GtkAction *action,
			 EphyWindow *window)
{
	ephy_gui_help (GTK_WINDOW (window), "epiphany", NULL);
}

void
window_cmd_help_about (GtkAction *action,
		       GtkWidget *window)
{
	static GtkWidget *about = NULL;
	GtkWidget** ptr;
	GdkPixbuf *icon;
	char *icon_path;
	GdkPixbuf *logo;
	GnomeIconTheme *icon_theme;

	static gchar *authors[] = {
		"Marco Pesenti Gritti <marco@gnome.org>",
		"Xan Lopez <xan@masilla.org>",
		"David Bordoley <bordoley@msu.edu>",
		"Christian Persch <chpe+epiphany@stud.uni-saarland.de>",
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

	icon_theme = gnome_icon_theme_new ();
	icon_path = gnome_icon_theme_lookup_icon (icon_theme, "web-browser",
						  -1, NULL, NULL);
	g_object_unref (icon_theme);

	if (icon_path)
	{
		logo = gdk_pixbuf_new_from_file (icon_path, NULL);
		g_free (icon_path);
	}
	else
	{
		logo = NULL;
		g_warning ("Web browser gnome icon not found");
	}

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
		ephy_notebook_move_page (EPHY_NOTEBOOK (notebook), NULL, child, page - 1);
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
		ephy_notebook_move_page (EPHY_NOTEBOOK (notebook), NULL, child, page + 1);
	}
}

void
window_cmd_tabs_detach  (GtkAction *action,
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
window_cmd_load_location (GtkAction *action,
			  EphyWindow *window)
{
	Toolbar *toolbar;
	const char *location;

	toolbar = ephy_window_get_toolbar (window);
	location = toolbar_get_location (toolbar);

	if (location)
	{
		ephy_window_load_url (window, location);
	}
}

