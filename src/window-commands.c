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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-shell.h"
#include "ephy-embed-factory.h"
#include "ephy-embed-persist.h"
#include "ephy-debug.h"
#include "ephy-command-manager.h"
#include "window-commands.h"
#include "print-dialog.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-embed-prefs.h"
#include "ephy-dialog.h"
#include "ephy-bookmarks-editor.h"
#include "ephy-history-window.h"
#include "ephy-new-bookmark.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "toolbar.h"
#include "ephy-state.h"
#include "ephy-gui.h"
#include "ephy-zoom.h"
#include "egg-toolbars-model.h"
#include "egg-editable-toolbar.h"
#include "egg-toolbar-editor.h"

#include <string.h>
#include <glib.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtktoggleaction.h>
#include <libgnomeui/gnome-about.h>
#include <glib/gi18n.h>

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
window_cmd_file_print_setup (GtkAction *action,
			     EphyWindow *window)
{
	EphyDialog *dialog;

	dialog = EPHY_DIALOG (ephy_shell_get_print_setup_dialog (ephy_shell));
	ephy_dialog_set_parent (dialog, GTK_WIDGET (window));

	ephy_dialog_show (dialog);
}

void
window_cmd_file_print_preview (GtkAction *action,
			       EphyWindow *window)
{
	EphyEmbed *embed;
	EmbedPrintInfo *info;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	info = ephy_print_get_print_info ();
	info->preview = TRUE;

	ephy_embed_print (embed, info);

	ephy_print_info_free (info);

	ephy_window_set_print_preview (window, TRUE);
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

	embed_location = ephy_embed_get_location (embed, TRUE);
	location = gnome_vfs_escape_string (embed_location);
	g_free (embed_location);

	title = ephy_embed_get_title (embed);
	if (title != NULL)
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

	ephy_embed_reload (embed, force ? EMBED_RELOAD_FORCE
					: EMBED_RELOAD_NORMAL);
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
	char *location;
	const char *icon;
	char *title = NULL;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	location = ephy_embed_get_location (embed, TRUE);

	title = ephy_embed_get_title (embed);
	if (title == NULL)
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
		(ephy_embed_factory_new_object ("EphyEmbedPersist"));

	ephy_embed_persist_set_embed (persist, embed);
	ephy_embed_persist_set_fc_title (persist, _("Save As"));
	ephy_embed_persist_set_fc_parent (persist,GTK_WINDOW (window));
	ephy_embed_persist_set_flags
		(persist, EMBED_PERSIST_MAINDOC | EMBED_PERSIST_ASK_DESTINATION);
	ephy_embed_persist_set_persist_key
		(persist, CONF_STATE_SAVE_DIR);

	ephy_embed_persist_save (persist);

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

static GnomeVFSMimeApplication *
get_editor_application (void)
{
	GnomeVFSMimeApplication *app;

	app = gnome_vfs_mime_get_default_application ("text/plain");
	if (app == NULL)
	{
		g_warning ("Cannot find a text editor.");
	}
	return app;
}

static void
editor_open_uri (const char *address)
{
	GList *uris = NULL;
	char *canonical;
	GnomeVFSMimeApplication *app;

	canonical = gnome_vfs_make_uri_canonical (address);

	uris = g_list_append (uris, canonical);

	app = get_editor_application ();
	if (app)
	{
		gnome_vfs_mime_application_launch (app, uris);
		gnome_vfs_mime_application_free (app);
	}

	g_free (canonical);
	g_list_free (uris);
}

static void
save_source_completed_cb (EphyEmbedPersist *persist)
{
	const char *dest;

	dest = ephy_embed_persist_get_dest (persist);
	g_return_if_fail (dest != NULL);

	ephy_file_delete_on_exit (dest);

	editor_open_uri (dest);
}

static gboolean
editor_can_open_uri (char *address)
{
	GnomeVFSMimeApplication *app;
	GnomeVFSURI *uri;
	const char *scheme;
	gboolean result = FALSE;

	app = get_editor_application ();

	uri = gnome_vfs_uri_new (address);
	scheme = uri ? gnome_vfs_uri_get_scheme (uri) : NULL;

	/* Open directly only read/write protocols, otherwise
	   you just get extra network overhead without any advantage */
	if (scheme && strcmp (scheme, "file") != 0)
	{
		scheme = NULL;
	}
	
	if (scheme && app && app->supported_uri_schemes)
	{
		if (g_list_find_custom (app->supported_uri_schemes,
                                        scheme, (GCompareFunc) strcmp))
		{
			result = TRUE;
		}
	}

	if (uri)
	{
		gnome_vfs_uri_unref (uri);
	}

	return result;
}

static void
save_temp_source (EphyEmbed *embed)
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
		(ephy_embed_factory_new_object ("EphyEmbedPersist"));

	ephy_embed_persist_set_embed (persist, embed);
	ephy_embed_persist_set_flags (persist, EMBED_PERSIST_COPY_PAGE |
				      EMBED_PERSIST_NO_VIEW);
	ephy_embed_persist_set_dest (persist, tmp);

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
	char *address;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	address = ephy_embed_get_location (embed, TRUE);

	if (editor_can_open_uri (address))
	{
		editor_open_uri (address);
	}
	else
	{
		save_temp_source (embed);
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
	ephy_dialog_set_parent (dialog, GTK_WIDGET (window));

	ephy_dialog_show (dialog);
}

void
window_cmd_edit_prefs (GtkAction *action,
		       EphyWindow *window)
{
	EphyDialog *dialog;

	dialog = EPHY_DIALOG (ephy_shell_get_prefs_dialog (ephy_shell));
	ephy_dialog_set_parent (dialog, GTK_WIDGET (window));

	ephy_dialog_show (dialog);
}

static void
toolbar_editor_destroy_cb (GtkWidget *tbe,
			   EphyWindow *window)
{
	egg_editable_toolbar_set_edit_mode (EGG_EDITABLE_TOOLBAR
		(ephy_window_get_toolbar (window)), FALSE);
	egg_editable_toolbar_set_edit_mode (EGG_EDITABLE_TOOLBAR
		(ephy_window_get_bookmarksbar (window)), FALSE);
}

static void
toolbar_editor_response_cb (GtkDialog  *dialog,
			    gint response_id,
			    EggToolbarsModel *model)
{
	switch (response_id)
	{
	case GTK_RESPONSE_CLOSE:
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	case RESPONSE_ADD_TOOLBAR:
		egg_toolbars_model_add_toolbar (model, -1, "UserCreated");
		break;
	case GTK_RESPONSE_HELP:
		ephy_gui_help (GTK_WINDOW (dialog), "epiphany", "to-edit-toolbars");
		break;
	}
}

void
window_cmd_edit_toolbar (GtkAction *action,
			 EphyWindow *window)
{
	GtkWidget *editor;
	EggToolbarsModel *model;
	GtkWidget *t;
	GtkWidget *dialog;

	model = EGG_TOOLBARS_MODEL
		(ephy_shell_get_toolbars_model (ephy_shell, FALSE));
	t = ephy_window_get_toolbar (window);

	dialog = gtk_dialog_new ();
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Toolbar Editor"));
        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

	editor = egg_toolbar_editor_new (GTK_UI_MANAGER (window->ui_merge), model);
	egg_toolbar_editor_load_actions (EGG_TOOLBAR_EDITOR (editor),
					 ephy_file ("epiphany-toolbar.xml"));
	gtk_container_set_border_width (GTK_CONTAINER (EGG_TOOLBAR_EDITOR (editor)), 5);
	gtk_box_set_spacing (GTK_BOX (EGG_TOOLBAR_EDITOR (editor)), 5);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), editor);
	g_signal_connect (editor, "destroy",
			  G_CALLBACK (toolbar_editor_destroy_cb), window);
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
			  G_CALLBACK (toolbar_editor_response_cb), model);
	ephy_state_add_window (dialog,
			       "toolbar_editor",
		               500, 330,
			       EPHY_STATE_WINDOW_SAVE_SIZE);
	gtk_widget_show (dialog);

	egg_editable_toolbar_set_edit_mode (EGG_EDITABLE_TOOLBAR (t), TRUE);
	egg_editable_toolbar_set_edit_mode
		(EGG_EDITABLE_TOOLBAR (ephy_window_get_bookmarksbar (window)), TRUE);
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
	const char *icon_path;
	GdkPixbuf *icon = NULL;
	GtkIconTheme *icon_theme;
	GtkIconInfo *icon_info;

	static char *authors[] = {
		"Marco Pesenti Gritti <marco@gnome.org>",
		"Xan Lopez <xan@gnome.org>",
		"David Bordoley <bordoley@msu.edu>",
		"Christian Persch <chpe@gnome.org>",
		NULL
	};

	char *documenters[] = {
		"Patanjali Somayaji <patanjali@codito.com>",
		"David Bordoley <bordoley@msu.edu>",
		"Piers Cornwell <piers@gnome.org>", 
		NULL
	};

	/* Translator credits */
	char *translator_credits = _("translator-credits");

	if (about != NULL)
	{
		gdk_window_show(about->window);
		gdk_window_raise(about->window);
		return;
	}

	/* FIXME: use the icon theme for the correct screen, not for the default screen */
	icon_theme = gtk_icon_theme_get_default ();
	icon_info = gtk_icon_theme_lookup_icon (icon_theme, "web-browser", -1, 0);

	if (icon_info)
	{
		icon_path = gtk_icon_info_get_filename (icon_info);

		if (icon_path != NULL)
		{
			icon = gdk_pixbuf_new_from_file (icon_path, NULL);
		}
	}
	else
	{
		g_warning ("Web browser gnome icon not found");
	}

	about = gnome_about_new(
		       "Epiphany", VERSION,
		       "Copyright \xc2\xa9 2002-2004 Marco Pesenti Gritti",
		       _("A GNOME browser based on Mozilla"),
		       (const char **)authors,
		       (const char **)documenters,
		       strcmp (translator_credits, "translator-credits") != 0 ? translator_credits : NULL,
		       icon);

	gtk_window_set_icon (GTK_WINDOW (about), icon);

	if (icon != NULL)
	{
		g_object_unref (icon);
	}

	gtk_window_set_transient_for (GTK_WINDOW (about),
				      GTK_WINDOW (window));

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
	Toolbar *toolbar;
	const char *location;

	toolbar = EPHY_TOOLBAR (ephy_window_get_toolbar (window));
	location = toolbar_get_location (toolbar);

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
