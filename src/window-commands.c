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

#include <config.h>

#include "ephy-shell.h"
#include "window-commands.h"
#include "find-dialog.h"
#include "print-dialog.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-embed-utils.h"
#include "pdm-dialog.h"
#include "toolbar.h"
#include "ephy-toolbar-editor.h"
#include "ephy-bookmarks-editor.h"
#include "ephy-new-bookmark.h"

#include <string.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <bonobo/bonobo-i18n.h>
#include <libgnomeui/gnome-about.h>
#include <libgnome/gnome-help.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkeditable.h>

#define AVAILABLE_TOOLBAR_ITEMS \
	"back=navigation_button(direction=back,arrow=FALSE);" \
	"back_menu=navigation_button(direction=back,arrow=TRUE);" \
	"forward=navigation_button(direction=forward,arrow=FALSE);" \
	"forward_menu=navigation_button(direction=forward,arrow=TRUE);" \
	"up=navigation_button(direction=up,arrow=FALSE);" \
	"up_menu=navigation_button(direction=up,arrow=TRUE);" \
	"stop=std_toolitem(item=stop);" \
	"reload=std_toolitem(item=reload);" \
	"home=std_toolitem(item=home);" \
	"favicon=favicon;" \
	"location=location;" \
	"go=std_toolitem(item=go);" \
	"zoom=zoom;" \
	"spinner=spinner;" \
	"separator;"

void
window_cmd_edit_find (BonoboUIComponent *uic,
		      EphyWindow *window,
		      const char* verbname)
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
window_cmd_file_print (BonoboUIComponent *uic,
		       EphyWindow *window,
		       const char* verbname)
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
window_cmd_go_back (BonoboUIComponent *uic,
		    EphyWindow *window,
		    const char* verbname)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_go_back (embed);
}

void
window_cmd_go_up (BonoboUIComponent *uic,
		  EphyWindow *window,
		  const char* verbname)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_go_up (embed);
}

void
window_cmd_file_send_to	(BonoboUIComponent *uic,
			 EphyWindow *window,
			 const char* verbname)
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
window_cmd_go_forward (BonoboUIComponent *uic,
		       EphyWindow *window,
		       const char* verbname)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_go_forward (embed);
}

void
window_cmd_go_go (BonoboUIComponent *uic,
		  EphyWindow *window,
		  const char* verbname)
{
	Toolbar *tb;

	g_return_if_fail (IS_EPHY_WINDOW (window));

	tb = ephy_window_get_toolbar (window);

	if (tb)
	{
		char *location = toolbar_get_location (tb);
		ephy_window_load_url (window, location);
		g_free (location);
	}
}

void
window_cmd_go_home (BonoboUIComponent *uic,
		    EphyWindow *window,
		    const char* verbname)
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
window_cmd_go_myportal (BonoboUIComponent *uic,
		        EphyWindow *window,
		        const char* verbname)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_load_url (embed, "myportal:");
}

void
window_cmd_go_location (BonoboUIComponent *uic,
		        EphyWindow *window,
		        const char* verbname)
{
	ephy_window_activate_location (window);
}

void
window_cmd_go_stop (BonoboUIComponent *uic,
		    EphyWindow *window,
		    const char* verbname)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_stop_load (embed);
}

void
window_cmd_go_reload (BonoboUIComponent *uic,
		      EphyWindow *window,
		      const char* verbname)
{
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_reload (embed, EMBED_RELOAD_NORMAL);
}

void
window_cmd_new (BonoboUIComponent *uic,
		EphyWindow *window,
		const char* verbname)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);

	ephy_shell_new_tab (ephy_shell, window, tab, NULL,
			    EPHY_NEW_TAB_HOMEPAGE |
			    EPHY_NEW_TAB_JUMP);
}

void
window_cmd_new_window (BonoboUIComponent *uic,
		       EphyWindow *window,
		       const char* verbname)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);

	ephy_shell_new_tab (ephy_shell, NULL, tab, NULL,
			    EPHY_NEW_TAB_HOMEPAGE |
			    EPHY_NEW_TAB_IN_NEW_WINDOW |
			    EPHY_NEW_TAB_JUMP);
}

void
window_cmd_new_tab (BonoboUIComponent *uic,
		    EphyWindow *window,
		    const char* verbname)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);

	ephy_shell_new_tab (ephy_shell, window, tab, NULL,
			      EPHY_NEW_TAB_HOMEPAGE |
			      EPHY_NEW_TAB_IN_EXISTING_WINDOW |
			      EPHY_NEW_TAB_JUMP);
}

void
window_cmd_bookmarks_edit (BonoboUIComponent *uic,
			   EphyWindow *window,
			   const char* verbname)
{
	GtkWidget *dialog;
	EphyBookmarks *bookmarks;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	g_assert (bookmarks != NULL);
	dialog = ephy_bookmarks_editor_new (bookmarks, GTK_WINDOW (window));
	gtk_widget_show (dialog);
}

void
window_cmd_bookmarks_add_default (BonoboUIComponent *uic,
				  EphyWindow *window,
				  const char* verbname)
{
	EphyTab *tab;
	EphyEmbed *embed;
	EphyBookmarks *bookmarks;
	GtkWidget *new_bookmark;
	const char *location;
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

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	new_bookmark = ephy_new_bookmark_new
		(bookmarks, GTK_WINDOW (window), location);
	ephy_new_bookmark_set_title
		(EPHY_NEW_BOOKMARK (new_bookmark), title);
	gtk_widget_show (new_bookmark);
}

void
window_cmd_file_open (BonoboUIComponent *uic,
		      EphyWindow *window,
		      const char* verbname)
{
	gchar *dir, *retDir;
        gchar *file = NULL;
        GnomeVFSURI *uri;
	GtkWidget *wmain;
	EphyEmbedShell *embed_shell;
	gresult result;

	embed_shell = ephy_shell_get_embed_shell (ephy_shell);
	g_return_if_fail (embed_shell != NULL);

	wmain = GTK_WIDGET (window);
	g_return_if_fail (wmain != NULL);

        dir = eel_gconf_get_string (CONF_STATE_OPEN_DIR);

	result = ephy_embed_shell_show_file_picker
		(embed_shell, wmain,
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
        }

        g_free (dir);
        g_free (file);
}

void
window_cmd_file_save_as (BonoboUIComponent *uic,
			 EphyWindow *window,
		         const char* verbname)
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
window_cmd_file_close_tab (BonoboUIComponent *uic,
		           EphyWindow *window,
		           const char* verbname)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	ephy_window_remove_tab (window, tab);
}

void
window_cmd_file_close_window (BonoboUIComponent *uic,
		              EphyWindow *window,
		              const char* verbname)
{
	gtk_widget_destroy (GTK_WIDGET(window));
}

void
window_cmd_edit_cut (BonoboUIComponent *uic,
		     EphyWindow *window,
		     const char* verbname)
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
window_cmd_edit_copy (BonoboUIComponent *uic,
		      EphyWindow *window,
		      const char* verbname)
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
window_cmd_edit_paste (BonoboUIComponent *uic,
		       EphyWindow *window,
		       const char* verbname)
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
window_cmd_edit_select_all (BonoboUIComponent *uic,
			    EphyWindow *window,
			    const char* verbname)
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
window_cmd_edit_find_next (BonoboUIComponent *uic,
			   EphyWindow *window,
			   const char* verbname)
{
	EphyDialog *dialog;

	dialog = ephy_window_get_find_dialog (window);

	find_dialog_go_next (FIND_DIALOG(dialog), FALSE);

	ephy_window_update_control (window, FindControl);
}

void
window_cmd_edit_find_prev (BonoboUIComponent *uic,
			   EphyWindow *window,
			   const char* verbname)
{
	EphyDialog *dialog;

	dialog = ephy_window_get_find_dialog (window);

	find_dialog_go_prev (FIND_DIALOG(dialog), FALSE);

	ephy_window_update_control (window, FindControl);
}

void
window_cmd_view_zoom_in	(BonoboUIComponent *uic,
			 EphyWindow *window,
			 const char *verbname)
{
	EphyEmbed *embed;
	int zoom;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_zoom_get (embed, &zoom);
	ephy_window_set_zoom (window, zoom + 10);
}

void
window_cmd_view_zoom_out (BonoboUIComponent *uic,
			  EphyWindow *window,
		          const char* verbname)
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
window_cmd_view_zoom_normal (BonoboUIComponent *uic,
			     EphyWindow *window,
			     const char* verbname)
{
	ephy_window_set_zoom (window, 100);
}

void
window_cmd_view_page_source (BonoboUIComponent *uic,
			     EphyWindow *window,
			     const char* verbname)
{
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);
	g_return_if_fail (tab != NULL);

	ephy_shell_new_tab (ephy_shell, window, tab, NULL,
			      EPHY_NEW_TAB_VIEW_SOURCE);
}

void
window_cmd_tools_history (BonoboUIComponent *uic,
			  EphyWindow *window,
			  const char* verbname)
{
	ephy_window_show_history (window);
}

void
window_cmd_tools_pdm (BonoboUIComponent *uic,
		      EphyWindow *window,
		      const char* verbname)
{
	EphyDialog *dialog;

	dialog = pdm_dialog_new (GTK_WIDGET(window));

	ephy_dialog_show (dialog);
}

void
window_cmd_edit_prefs (BonoboUIComponent *uic,
		       EphyWindow *window,
		       const char* verbname)
{
	GtkDialog *dialog;

	dialog = prefs_dialog_new ();
	prefs_dialog_show_page (PREFS_DIALOG(dialog),
				PREFS_PAGE_GENERAL);
	gtk_window_set_transient_for (GTK_WINDOW (dialog),
				      GTK_WINDOW (window));
	gtk_widget_show (GTK_WIDGET(dialog));
}

static void
window_cmd_settings_toolbar_editor_revert_clicked_cb (GtkButton *b, EphyTbEditor *tbe)
{
	gchar *def;

	g_return_if_fail (EPHY_IS_TB_EDITOR (tbe));

	eel_gconf_unset (CONF_TOOLBAR_SETUP);
	def = eel_gconf_get_string (CONF_TOOLBAR_SETUP);
	if (def)
	{
		EphyToolbar *current;
		EphyToolbar *avail;
		current = ephy_tb_editor_get_toolbar (tbe);
		ephy_toolbar_parse (current, def);
		g_free (def);

		avail = ephy_tb_editor_get_available (tbe);
		g_object_ref (avail);
		ephy_toolbar_parse (avail, AVAILABLE_TOOLBAR_ITEMS);
		ephy_tb_editor_set_available (tbe, avail);
		g_object_unref (avail);
	}

}

static void
window_cmd_settings_toolbar_editor_current_changed_cb (EphyToolbar *tb, gpointer data)
{
	gchar *current_str;

	g_return_if_fail (EPHY_IS_TOOLBAR (tb));

	current_str = ephy_toolbar_to_string (tb);
	eel_gconf_set_string (CONF_TOOLBAR_SETUP, current_str);
	g_free (current_str);
}

void
window_cmd_settings_toolbar_editor (BonoboUIComponent *uic,
				    EphyWindow *window,
				    const char* verbname)
{
	static EphyTbEditor *tbe = NULL;
	EphyToolbar *avail;
	EphyToolbar *current;
	gchar *current_str;
	GtkButton *revert_button;

	avail = ephy_toolbar_new ();
	ephy_toolbar_parse (avail, AVAILABLE_TOOLBAR_ITEMS);

	current_str = eel_gconf_get_string (CONF_TOOLBAR_SETUP);
	current = ephy_toolbar_new ();
	if (current_str)
	{
		ephy_toolbar_parse (current, current_str);
		g_free (current_str);
	}

	if (!tbe)
	{
		tbe = ephy_tb_editor_new ();
		g_object_add_weak_pointer (G_OBJECT (tbe),
                                           (void **)&tbe);
		ephy_tb_editor_set_parent (tbe,
					  GTK_WIDGET(window));
	}
	else
	{
		ephy_tb_editor_show (tbe);
                return;
	}

	ephy_tb_editor_set_toolbar (tbe, current);
	ephy_tb_editor_set_available (tbe, avail);
	g_object_unref (avail);
	g_object_unref (current);

	g_signal_connect (current, "changed",
			  G_CALLBACK (window_cmd_settings_toolbar_editor_current_changed_cb), NULL);

	revert_button = ephy_tb_editor_get_revert_button (tbe);
	gtk_widget_show (GTK_WIDGET (revert_button));

	g_signal_connect (revert_button, "clicked",
			  G_CALLBACK (window_cmd_settings_toolbar_editor_revert_clicked_cb), tbe);

	ephy_tb_editor_show (tbe);
}

void
window_cmd_help_about (BonoboUIComponent *uic,
		       EphyWindow *window,
		       const char* verbname)
{
	static GtkWidget *about = NULL;

	static gchar *authors[] = {
		"Marco Pesenti Gritti <mpeseng@tin.it>",
		NULL
	};

	gchar *documenters[] = {
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
		       _("Epiphany"), VERSION,
		       /* Translators: Please change the (C) to a real
			* copyright character if your character set allows it
			* (Hint: iso-8859-1 is one of the character sets that
			* has this symbol). */
		       _("Copyright (C) 2002 Marco Pesenti Gritti"),
		       _("A GNOME browser based on Mozilla"),
		       (const char **)authors,
		       (const char **)documenters,
		       strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
		       NULL);

	gtk_window_set_transient_for (GTK_WINDOW (about),
				      GTK_WINDOW (window));
	g_object_add_weak_pointer (G_OBJECT (about), (gpointer *)&about);
	gtk_widget_show (about);
}

void
window_cmd_set_charset (BonoboUIComponent *uic,
			EncodingMenuData *data,
			const char* verbname)
{
	EphyWindow *window = data->data;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	g_print (data->encoding);
	ephy_embed_set_charset (embed, data->encoding);
}

void
window_cmd_tabs_next (BonoboUIComponent *uic,
		      EphyWindow *window,
		      const char* verbname)
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
window_cmd_tabs_previous (BonoboUIComponent *uic,
			  EphyWindow *window,
			  const char* verbname)
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
window_cmd_tabs_move_left  (BonoboUIComponent *uic,
			    EphyWindow *window,
			    const char* verbname)
{
}

void window_cmd_tabs_move_right (BonoboUIComponent *uic,
				 EphyWindow *window,
				 const char* verbname)
{
}

void
window_cmd_tabs_detach  (BonoboUIComponent *uic,
			 EphyWindow *window,
			 const char* verbname)
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
window_cmd_help_manual (BonoboUIComponent *uic,
			char *filename,
			const char* verbname)
{
	GError *error;
	GtkWidget *dialog;

	error = NULL;
	gnome_help_display ("Ephy.xml", NULL, &error);

	if (error)
	{
		dialog = gtk_message_dialog_new (NULL,
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("There was an error displaying help: \n%s"),
						 error->message);
		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}
