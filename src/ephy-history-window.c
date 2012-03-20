/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti <mpeseng@tin.it>
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2012 Igalia S.L
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
 */

#include "config.h"
#include "ephy-history-window.h"

#include "ephy-bookmarks-ui.h"
#include "ephy-debug.h"
#include "ephy-dnd.h"
#include "ephy-file-helpers.h"
#include "ephy-gui.h"
#include "ephy-hosts-store.h"
#include "ephy-hosts-view.h"
#include "ephy-prefs.h"
#include "ephy-search-entry.h"
#include "ephy-session.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-state.h"
#include "ephy-time-helpers.h"
#include "ephy-urls-store.h"
#include "ephy-urls-view.h"
#include "ephy-window.h"
#include "window-commands.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <time.h>

static const GtkTargetEntry page_drag_types [] =
{
	{ EPHY_DND_URL_TYPE,	    0, 0 },
	{ EPHY_DND_URI_LIST_TYPE,   0, 1 },
	{ EPHY_DND_TEXT_TYPE,	    0, 2 }
};

static void ephy_history_window_constructed (GObject *object);
static void ephy_history_window_class_init (EphyHistoryWindowClass *klass);
static void ephy_history_window_init (EphyHistoryWindow *editor);
static void ephy_history_window_finalize (GObject *object);
static void ephy_history_window_set_property (GObject *object,
					      guint prop_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void ephy_history_window_get_property (GObject *object,
					      guint prop_id,
					      GValue *value,
					      GParamSpec *pspec);
static void ephy_history_window_dispose	     (GObject *object);

static void cmd_open_bookmarks_in_tabs	  (GtkAction *action,
					   EphyHistoryWindow *editor);
static void cmd_open_bookmarks_in_browser (GtkAction *action,
					   EphyHistoryWindow *editor);
static void cmd_delete			  (GtkAction *action,
					   EphyHistoryWindow *editor);
static void cmd_bookmark_link		  (GtkAction *action,
					   EphyHistoryWindow *editor);
static void cmd_clear			  (GtkAction *action,
					   EphyHistoryWindow *editor);
static void cmd_close			  (GtkAction *action,
					   EphyHistoryWindow *editor);
static void cmd_cut			  (GtkAction *action,
					   EphyHistoryWindow *editor);
static void cmd_copy			  (GtkAction *action,
					   EphyHistoryWindow *editor);
static void cmd_paste			  (GtkAction *action,
					   EphyHistoryWindow *editor);
static void cmd_select_all		  (GtkAction *action,
					   EphyHistoryWindow *editor);
static void cmd_help_contents		  (GtkAction *action,
					   EphyHistoryWindow *editor);
static void search_entry_search_cb	  (GtkWidget *entry,
					   char *search_text,
					   EphyHistoryWindow *editor);
static void
filter_now (EphyHistoryWindow *editor, gboolean hosts, gboolean pages);

#define EPHY_HISTORY_WINDOW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_HISTORY_WINDOW, EphyHistoryWindowPrivate))

struct _EphyHistoryWindowPrivate
{
	EphyHistoryService *history_service;
	GtkWidget *hosts_view;
	GtkWidget *pages_view;
	EphyURLsStore *urls_store;
	EphyHostsStore *hosts_store;
	GtkWidget *time_combo;
	GtkWidget *search_entry;
	GtkWidget *main_vbox;
	GtkWidget *window;
	GtkUIManager *ui_merge;
	GtkActionGroup *action_group;
	GtkWidget *confirmation_dialog;
	GtkTreeViewColumn *title_col;
	GtkTreeViewColumn *address_col;
	GtkTreeViewColumn *datetime_col;
};

enum
{
	PROP_0,
	PROP_HISTORY_SERVICE,
};

static const GtkActionEntry ephy_history_ui_entries [] = {
	/* Toplevel */
	{ "File", NULL, N_("_File") },
	{ "Edit", NULL, N_("_Edit") },
	{ "View", NULL, N_("_View") },
	{ "Help", NULL, N_("_Help") },
	{ "PopupAction", NULL, "" },

	/* File Menu */
	{ "OpenInWindow", NULL, N_("Open in New _Window"), "<control>O",
	  N_("Open the selected history link in a new window"),
	  G_CALLBACK (cmd_open_bookmarks_in_browser) },
	{ "OpenInTab", NULL, N_("Open in New _Tab"), "<shift><control>O",
	  N_("Open the selected history link in a new tab"),
	  G_CALLBACK (cmd_open_bookmarks_in_tabs) },
	{ "BookmarkLink", NULL, N_("Add _Bookmark…"), "<control>D",
	  N_("Bookmark the selected history link"),
	  G_CALLBACK (cmd_bookmark_link) },
	{ "Close", NULL, N_("_Close"), "<control>W",
	  N_("Close the history window"),
	  G_CALLBACK (cmd_close) },

	/* Edit Menu */
	{ "Cut", NULL, N_("Cu_t"), "<control>X",
	  N_("Cut the selection"),
	  G_CALLBACK (cmd_cut) },
	{ "Copy", NULL, N_("_Copy"), "<control>C",
	  N_("Copy the selection"),
	  G_CALLBACK (cmd_copy) },
	{ "Paste", NULL, N_("_Paste"), "<control>V",
	  N_("Paste the clipboard"),
	  G_CALLBACK (cmd_paste) },
	{ "Delete", NULL, N_("_Delete"), "<control>T",
	  N_("Delete the selected history link"),
	  G_CALLBACK (cmd_delete) },
	{ "SelectAll", NULL, N_("Select _All"), "<control>A",
	  N_("Select all history links or text"),
	  G_CALLBACK (cmd_select_all) },
	{ "Clear", NULL, N_("Clear _History"), NULL,
	  N_("Clear your browsing history"),
	  G_CALLBACK (cmd_clear) },

	/* Help Menu */
	{ "HelpContents", NULL, N_("_Contents"), "F1",
	  N_("Display history help"),
	  G_CALLBACK (cmd_help_contents) },
	{ "HelpAbout", NULL, N_("_About"), NULL,
	  N_("Display credits for the web browser creators"),
	  G_CALLBACK (window_cmd_help_about) },
};

typedef enum
{
	VIEW_TITLE = 1 << 0,
	VIEW_ADDRESS = 1 << 1,
	VIEW_DATETIME = 1 << 2
} EphyHistoryWindowColumns;

static const GtkToggleActionEntry ephy_history_toggle_entries [] =
{
	/* View Menu */
	{ "ViewTitle", NULL, N_("_Title"), NULL,
	  N_("Show the title column"), NULL, TRUE },
	{ "ViewAddress", NULL, N_("_Address"), NULL,
	  N_("Show the address column"), NULL, TRUE },
	{ "ViewDateTime", NULL, N_("_Date and Time"), NULL,
	  N_("Show the date and time column"), NULL, TRUE }
};

static void
confirmation_dialog_response_cb (GtkWidget *dialog,
				 int response,
				 EphyHistoryWindow *editor)
{
	gtk_widget_destroy (dialog);

	if (response == GTK_RESPONSE_ACCEPT)
	{
		ephy_history_service_clear (editor->priv->history_service,
					    NULL, NULL, NULL);
		filter_now (editor, TRUE, TRUE);
	}
}

static GtkWidget *
confirmation_dialog_construct (EphyHistoryWindow *editor)
{
	GtkWidget *dialog, *button, *image;

	dialog = gtk_message_dialog_new
		(GTK_WINDOW (editor),
		 GTK_DIALOG_DESTROY_WITH_PARENT,
		 GTK_MESSAGE_WARNING,
		 GTK_BUTTONS_CANCEL,
		 _("Clear browsing history?"));

	gtk_message_dialog_format_secondary_text
		(GTK_MESSAGE_DIALOG (dialog),
		 _("Clearing the browsing history will cause all"
		   " history links to be permanently deleted."));

	gtk_window_group_add_window (ephy_gui_ensure_window_group (GTK_WINDOW (editor)),
				     GTK_WINDOW (dialog));
	
	button = gtk_button_new_with_mnemonic (_("Cl_ear"));
	image = gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (button), image);
	/* don't show the image! see bug #307818 */
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_ACCEPT);
	
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Clear History"));

	g_signal_connect (dialog, "response",
			  G_CALLBACK (confirmation_dialog_response_cb),
			  editor);

	return dialog;
}

static EphyHistoryHost *
get_selected_host (EphyHistoryWindow *editor)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeModel *model;
	EphyHistoryHost *host = NULL;

	EphyHistoryWindowPrivate *priv = editor->priv;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->hosts_view));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		path = gtk_tree_model_get_path (model, &iter);
		host = ephy_hosts_store_get_host_from_path (priv->hosts_store,
							    path);
		gtk_tree_path_free (path);
	}
	return host;
}

static void
cmd_clear (GtkAction *action,
	   EphyHistoryWindow *editor)
{
	if (editor->priv->confirmation_dialog == NULL)
	{
		GtkWidget **confirmation_dialog;

		editor->priv->confirmation_dialog = confirmation_dialog_construct (editor);
		confirmation_dialog = &editor->priv->confirmation_dialog;
		g_object_add_weak_pointer (G_OBJECT (editor->priv->confirmation_dialog),
					   (gpointer *) confirmation_dialog);
	}

	gtk_widget_show (editor->priv->confirmation_dialog);
}

static void
cmd_close (GtkAction *action,
	   EphyHistoryWindow *editor)
{
	if (editor->priv->confirmation_dialog != NULL)
	{
		gtk_widget_destroy (editor->priv->confirmation_dialog);
	}
	gtk_widget_hide (GTK_WIDGET (editor));
}

static GtkWidget *
get_target_window (EphyHistoryWindow *editor)
{
	if (editor->priv->window)
	{
		return editor->priv->window;
	}
	else
	{
		EphySession *session;

		session = EPHY_SESSION (ephy_shell_get_session (ephy_shell));
		return GTK_WIDGET (ephy_session_get_active_window (session));
	}
}

static void
cmd_open_bookmarks_in_tabs (GtkAction *action,
			    EphyHistoryWindow *editor)
{
	EphyWindow *window;
	GList *selection;
	GList *l;

	window = EPHY_WINDOW (get_target_window (editor));
	selection = ephy_urls_view_get_selection (EPHY_URLS_VIEW (editor->priv->pages_view));

	for (l = selection; l; l = l->next)
	{
		EphyHistoryURL *url = l->data;
		ephy_shell_new_tab (ephy_shell, window, NULL, url->url,
			EPHY_NEW_TAB_OPEN_PAGE | EPHY_NEW_TAB_IN_EXISTING_WINDOW);
	}

	g_list_free_full (selection, (GDestroyNotify) ephy_history_url_free);
}

static void
cmd_open_bookmarks_in_browser (GtkAction *action,
			       EphyHistoryWindow *editor)
{
	EphyWindow *window;
	GList *selection;
	GList *l;

	window = EPHY_WINDOW (get_target_window (editor));
	selection = ephy_urls_view_get_selection (EPHY_URLS_VIEW (editor->priv->pages_view));

	for (l = selection; l; l = l->next)
	{
		EphyHistoryURL *url = l->data;
		ephy_shell_new_tab (ephy_shell, window, NULL, url->url,
				    EPHY_NEW_TAB_OPEN_PAGE |
				    EPHY_NEW_TAB_IN_NEW_WINDOW);
	}

	g_list_free_full (selection, (GDestroyNotify) ephy_history_url_free);
}

static void
cmd_cut (GtkAction *action,
	 EphyHistoryWindow *editor)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (editor));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_cut_clipboard (GTK_EDITABLE (widget));
	}
}

static void
cmd_copy (GtkAction *action,
	  EphyHistoryWindow *editor)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (editor));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_copy_clipboard (GTK_EDITABLE (widget));
	}

	else if (gtk_widget_is_focus (editor->priv->pages_view))
	{
		GList *selection;

		selection = ephy_urls_view_get_selection (EPHY_URLS_VIEW (editor->priv->pages_view));

		if (g_list_length (selection) == 1)
		{
			EphyHistoryURL *url = selection->data;
			gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), url->url, -1);
		}

		g_list_free_full (selection, (GDestroyNotify) ephy_history_url_free);
	}
}

static void
cmd_paste (GtkAction *action,
	   EphyHistoryWindow *editor)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (editor));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_paste_clipboard (GTK_EDITABLE (widget));
	}
}

static void
cmd_select_all (GtkAction *action,
		EphyHistoryWindow *editor)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (editor));
	GtkWidget *pages_view = editor->priv->pages_view;

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
	}
	else if (gtk_widget_is_focus (pages_view))
	{
		GtkTreeSelection *sel;

		sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (pages_view));
		gtk_tree_selection_select_all (sel);
	}
}

static void
on_browse_history_deleted_cb (gpointer service,
			      gboolean success,
			      gpointer result_data,
			      gpointer user_data)
{
	EphyHistoryWindow *editor = EPHY_HISTORY_WINDOW (user_data);

	if (success != TRUE)
		return;

	filter_now (editor, TRUE, TRUE);
}

static void
on_host_deleted_cb (gpointer service,
		    gboolean success,
		    gpointer result_data,
		    gpointer user_data)
{
	EphyHistoryWindow *editor = EPHY_HISTORY_WINDOW (user_data);

	if (success != TRUE)
		return;

	filter_now (editor, TRUE, TRUE);
}

static void
cmd_delete (GtkAction *action,
	    EphyHistoryWindow *editor)
{
	if (gtk_widget_is_focus (editor->priv->pages_view))
	{
		GList *selected;
		selected = ephy_urls_view_get_selection (EPHY_URLS_VIEW (editor->priv->pages_view));
		ephy_history_service_delete_urls (editor->priv->history_service, selected, NULL,
						  (EphyHistoryJobCallback)on_browse_history_deleted_cb, editor);
	} else if (gtk_widget_is_focus (editor->priv->hosts_view)) {
		EphyHistoryHost *host = get_selected_host (editor);
		if (host) {
			ephy_history_service_delete_host (editor->priv->history_service,
							  host, NULL,
							  (EphyHistoryJobCallback)on_host_deleted_cb,
							  editor);
			ephy_history_host_free (host);
		}
	}
}

static void
cmd_bookmark_link (GtkAction *action,
		   EphyHistoryWindow *editor)
{
	GList *selection;

	selection = ephy_urls_view_get_selection (EPHY_URLS_VIEW (editor->priv->pages_view));

	if (g_list_length (selection) == 1)
	{
		EphyHistoryURL *url;

		url = selection->data;

		ephy_bookmarks_ui_add_bookmark (GTK_WINDOW (editor), url->url, url->title);
	}

	g_list_free_full (selection, (GDestroyNotify) ephy_history_url_free);
}

static void
cmd_help_contents (GtkAction *action,
		   EphyHistoryWindow *editor)
{
	ephy_gui_help (GTK_WIDGET (editor), "ephy-managing-history");
}

G_DEFINE_TYPE (EphyHistoryWindow, ephy_history_window, GTK_TYPE_WINDOW)

static void
ephy_history_window_show (GtkWidget *widget)
{
	EphyHistoryWindow *window = EPHY_HISTORY_WINDOW (widget);

	gtk_widget_grab_focus (window->priv->search_entry);

	GTK_WIDGET_CLASS (ephy_history_window_parent_class)->show (widget);
}

static void
ephy_history_window_class_init (EphyHistoryWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = ephy_history_window_finalize;

	object_class->set_property = ephy_history_window_set_property;
	object_class->get_property = ephy_history_window_get_property;
	object_class->dispose  = ephy_history_window_dispose;
	object_class->constructed = ephy_history_window_constructed;

	widget_class->show = ephy_history_window_show;

	g_object_class_install_property (object_class,
					 PROP_HISTORY_SERVICE,
					 g_param_spec_object ("history-service",
							      "History service",
							      "History Service",
							      EPHY_TYPE_HISTORY_SERVICE,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(EphyHistoryWindowPrivate));
}

static void
ephy_history_window_finalize (GObject *object)
{
	EphyHistoryWindow *editor = EPHY_HISTORY_WINDOW (object);

	g_object_unref (editor->priv->action_group);
	g_object_unref (editor->priv->ui_merge);

	if (editor->priv->window)
	{
		GtkWidget **window = &editor->priv->window;
		g_object_remove_weak_pointer
			(G_OBJECT(editor->priv->window),
			 (gpointer *)window);
	}

	G_OBJECT_CLASS (ephy_history_window_parent_class)->finalize (object);
}

static void
ephy_history_window_row_activated_cb (GtkTreeView *view,
				      GtkTreePath *path,
				      GtkTreeViewColumn *col,
				      EphyHistoryWindow *editor)
{
	EphyHistoryURL *url;

	url = ephy_urls_store_get_url_from_path (EPHY_URLS_STORE (gtk_tree_view_get_model (view)),
						 path);
	g_return_if_fail (url != NULL);

	ephy_shell_new_tab (ephy_shell, NULL, NULL, url->url,
			    EPHY_NEW_TAB_OPEN_PAGE);
	ephy_history_url_free (url);
}

static void
ephy_history_window_row_middle_clicked_cb (EphyHistoryView *view,
					   GtkTreePath *path,
					   EphyHistoryWindow *editor)
{
	EphyWindow *window;
	EphyHistoryURL *url;

	window = EPHY_WINDOW (get_target_window (editor));
	url = ephy_urls_store_get_url_from_path (editor->priv->urls_store, path);
	g_return_if_fail (url != NULL);

	ephy_shell_new_tab (ephy_shell, window, NULL, url->url,
			    EPHY_NEW_TAB_OPEN_PAGE |
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW);
	ephy_history_url_free (url);
}

static void
ephy_history_window_update_menu (EphyHistoryWindow *editor)
{
	gboolean open_in_window, open_in_tab;
	gboolean cut, copy, paste, select_all;
	gboolean pages_focus, pages_selection, single_page_selected;
	gboolean delete, bookmark_page;
	gboolean bookmarks_locked;
	int num_pages_selected;
	GtkActionGroup *action_group;
	GtkAction *action;
	char *open_in_window_label, *open_in_tab_label, *copy_label;
	GtkWidget *focus_widget;

	pages_focus = gtk_widget_is_focus (editor->priv->pages_view);
	num_pages_selected = gtk_tree_selection_count_selected_rows
		 (gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->priv->pages_view)));
	pages_selection = num_pages_selected > 0;
	single_page_selected = num_pages_selected == 1;

	focus_widget = gtk_window_get_focus (GTK_WINDOW (editor));

	if (GTK_IS_EDITABLE (focus_widget))
	{
		gboolean has_selection;

		has_selection = gtk_editable_get_selection_bounds
			(GTK_EDITABLE (focus_widget), NULL, NULL);

		cut = has_selection;
		copy = has_selection;
		paste = TRUE;
		select_all = TRUE;
	}
	else
	{
		cut = FALSE;
		copy = (pages_focus && single_page_selected);
		paste = FALSE;
		select_all = pages_focus;
	}

	open_in_window_label = ngettext ("Open in New _Window",
					 "Open in New _Windows",
					 num_pages_selected);
	open_in_tab_label = ngettext ("Open in New _Tab",
				      "Open in New _Tabs",
				      num_pages_selected);

	if (pages_focus)
	{
		copy_label = _("_Copy Address");
	}
	else
	{
		copy_label = _("_Copy");
	}

	open_in_window = (pages_focus && pages_selection);
	open_in_tab = (pages_focus && pages_selection);
	delete = (pages_focus && pages_selection);
	bookmarks_locked = g_settings_get_boolean
				(EPHY_SETTINGS_LOCKDOWN,
				 EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING);
	bookmark_page = (pages_focus && single_page_selected) && !bookmarks_locked;

	action_group = editor->priv->action_group;
	action = gtk_action_group_get_action (action_group, "OpenInWindow");
	gtk_action_set_sensitive (action, open_in_window);
	g_object_set (action, "label", open_in_window_label, NULL);
	action = gtk_action_group_get_action (action_group, "OpenInTab");
	gtk_action_set_sensitive (action, open_in_tab);
	g_object_set (action, "label", open_in_tab_label, NULL);
	action = gtk_action_group_get_action (action_group, "Cut");
	gtk_action_set_sensitive (action, cut);
	action = gtk_action_group_get_action (action_group, "Copy");
	gtk_action_set_sensitive (action, copy);
	g_object_set (action, "label", copy_label, NULL);
	action = gtk_action_group_get_action (action_group, "Paste");
	gtk_action_set_sensitive (action, paste);
	action = gtk_action_group_get_action (action_group, "SelectAll");
	gtk_action_set_sensitive (action, select_all);
	action = gtk_action_group_get_action (action_group, "Delete");
	gtk_action_set_sensitive (action, delete);
	action = gtk_action_group_get_action (action_group, "BookmarkLink");
	gtk_action_set_sensitive (action, bookmark_page);

}

static void
entry_selection_changed_cb (GtkWidget *widget, GParamSpec *pspec, EphyHistoryWindow *editor)
{
	ephy_history_window_update_menu (editor);
	filter_now (editor, FALSE, TRUE);
}

static void
add_entry_monitor (EphyHistoryWindow *editor, GtkWidget *entry)
{
	g_signal_connect (G_OBJECT (entry),
			  "notify::selection-bound",
			  G_CALLBACK (entry_selection_changed_cb),
			  editor);
	g_signal_connect (G_OBJECT (entry),
			  "notify::cursor-position",
			  G_CALLBACK (entry_selection_changed_cb),
			  editor);
}

static gboolean
view_focus_cb (GtkWidget *view,
	       GdkEventFocus *event,
	       EphyHistoryWindow *editor)
{
       ephy_history_window_update_menu (editor);

       return FALSE;
}

static void
add_focus_monitor (EphyHistoryWindow *editor, GtkWidget *widget)
{
       g_signal_connect (G_OBJECT (widget),
			 "focus_in_event",
			 G_CALLBACK (view_focus_cb),
			 editor);
       g_signal_connect (G_OBJECT (widget),
			 "focus_out_event",
			 G_CALLBACK (view_focus_cb),
			 editor);
}

static void
remove_focus_monitor (EphyHistoryWindow *editor, GtkWidget *widget)
{
       g_signal_handlers_disconnect_by_func (G_OBJECT (widget),
					     G_CALLBACK (view_focus_cb),
					     editor);
}

static gboolean
ephy_history_window_show_popup_cb (GtkWidget *view,
				   EphyHistoryWindow *editor)
{
	GtkWidget *widget;

	widget = gtk_ui_manager_get_widget (editor->priv->ui_merge,
					    "/EphyHistoryWindowPopup");
	ephy_history_view_popup (EPHY_HISTORY_VIEW (view), widget);

	return TRUE;
}

static gboolean
key_pressed_cb (GtkWidget *view,
		GdkEventKey *event,
		EphyHistoryWindow *editor)
{
	switch (event->keyval)
	{
	case GDK_KEY_Delete:
	case GDK_KEY_KP_Delete:
		cmd_delete (NULL, editor);
		return TRUE;

	default:
		break;
	}

	return FALSE;
}

static void
search_entry_search_cb (GtkWidget *entry, char *search_text, EphyHistoryWindow *editor)
{
	filter_now (editor, FALSE, TRUE);
}

static void
time_combo_changed_cb (GtkWidget *combo, EphyHistoryWindow *editor)
{
	filter_now (editor, TRUE, TRUE);
}

static GtkWidget *
build_search_box (EphyHistoryWindow *editor)
{
	GtkWidget *box, *label, *entry;
	GtkWidget *combo;
	char *str;

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (box), 6);
	gtk_widget_show (box);

	entry = ephy_search_entry_new ();
	add_focus_monitor (editor, entry);
	add_entry_monitor (editor, entry);
	editor->priv->search_entry = entry;
    
	gtk_widget_show_all (entry);

	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	str = g_strconcat ("<b>", _("_Search:"), "</b>", NULL);
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), str);
	g_free (str);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show (label);

	combo = gtk_combo_box_text_new ();
	gtk_widget_show (combo);

	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Last 30 minutes"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("Today"));

	str = g_strdup_printf (ngettext ("Last %d day", "Last %d days", 2), 2);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), str);
	g_free (str);

	str = g_strdup_printf (ngettext ("Last %d day", "Last %d days", 3), 3);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), str);
	g_free (str);

	/* keep this in sync with embed/ephy-history.c's
	 * HISTORY_PAGE_OBSOLETE_DAYS */
	str = g_strdup_printf (ngettext ("Last %d day", "Last %d days", 10), 10);
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), str);
	g_free (str);

	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), _("All history"));

	g_settings_bind (EPHY_SETTINGS_STATE,
			 EPHY_PREFS_STATE_HISTORY_DATE_FILTER,
			 combo, "active",
			 G_SETTINGS_BIND_DEFAULT);

	editor->priv->time_combo = combo;

	gtk_box_pack_start (GTK_BOX (box),
			    label, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (box),
			    entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (box),
			    combo, FALSE, TRUE, 0);

	g_signal_connect (combo, "changed",
			  G_CALLBACK (time_combo_changed_cb),
			  editor);
	g_signal_connect (G_OBJECT (entry), "search",
			  G_CALLBACK (search_entry_search_cb),
			  editor);

	return box;
}

static void
add_widget (GtkUIManager *merge, GtkWidget *widget, EphyHistoryWindow *editor)
{
	gtk_box_pack_start (GTK_BOX (editor->priv->main_vbox),
			    widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
}

static gboolean
delete_event_cb (EphyHistoryWindow *editor)
{
	gtk_widget_hide (GTK_WIDGET (editor));

	return TRUE;
}

#if 0
static void
provide_favicon (EphyNode *node, GValue *value, gpointer user_data)
{
	const char *page_location;
	GdkPixbuf *pixbuf = NULL;

	page_location = ephy_node_get_property_string
		(node, EPHY_NODE_PAGE_PROP_LOCATION);

	LOG ("Get favicon for %s", page_location ? page_location : "None");

	if (page_location)
        {
		/* No need to use the async version as this function will be
		called many times by the treeview. */
		WebKitFaviconDatabase *database = webkit_get_favicon_database ();
		pixbuf = webkit_favicon_database_get_favicon_pixbuf (database, page_location,
								     FAVICON_SIZE, FAVICON_SIZE);
        }

	g_value_init (value, GDK_TYPE_PIXBUF);
	g_value_take_object (value, pixbuf);
}
#endif

static void
convert_cell_data_func (GtkTreeViewColumn *column,
			GtkCellRenderer *renderer,
			GtkTreeModel *model,
			GtkTreeIter *iter,
			gpointer user_data)
{
	int col_id = GPOINTER_TO_INT (user_data);
	int value;
	time_t time;
	char *friendly;
	
	gtk_tree_model_get (model, iter,
			    col_id,	
			    &value,
			    -1);
	time = (time_t) value;
	
	friendly = ephy_time_helpers_utf_friendly_time (time);
	g_object_set (renderer, "text", friendly, NULL);
	g_free (friendly);
}

static void
parse_time_into_date (GtkTreeViewColumn *column,
		      int column_id)
{
	GList *renderers_list;
	GtkCellRenderer *renderer;
	
	renderers_list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
	renderer = GTK_CELL_RENDERER (renderers_list->data);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 (GtkTreeCellDataFunc) convert_cell_data_func,
						 GINT_TO_POINTER (column_id),
						 NULL);
	g_list_free (renderers_list);
}
static void
view_selection_changed_cb (GtkWidget *view, EphyHistoryWindow *editor)
{
	ephy_history_window_update_menu (editor);
	filter_now (editor, FALSE, TRUE);
}

static void
setup_time_filters (EphyHistoryWindow *editor,
		    gint64 *from, gint64 *to)
{
	time_t now, midnight, cmp_time = 0;
	struct tm btime;
	int time_range, days = 0;

	time_range = gtk_combo_box_get_active
		(GTK_COMBO_BOX (editor->priv->time_combo));

	*from = *to = -1;

	/* no need to setup a new filter */
	if (time_range == EPHY_PREFS_STATE_HISTORY_DATE_FILTER_EVER) return;

	now = time (NULL);
	if (localtime_r (&now, &btime) == NULL) return;

	/* get start of day */
	btime.tm_sec = 0;
	btime.tm_min = 0;
	btime.tm_hour = 0;
	midnight = mktime (&btime);

	switch (time_range)
	{
		case EPHY_PREFS_STATE_HISTORY_DATE_FILTER_LAST_HALF_HOUR:
			cmp_time = now - 30 * 60;
			break;
		case EPHY_PREFS_STATE_HISTORY_DATE_FILTER_TODAY:
			cmp_time = midnight;
			break;
		case EPHY_PREFS_STATE_HISTORY_DATE_FILTER_LAST_TWO_DAYS:
			days = 1;
			cmp_time = midnight;
			break;
		case EPHY_PREFS_STATE_HISTORY_DATE_FILTER_LAST_THREE_DAYS:
			days = 2;
			cmp_time = midnight;
			break;
		case EPHY_PREFS_STATE_HISTORY_DATE_FILTER_LAST_TEN_DAYS:
			days = 9;
			cmp_time = midnight;
			break;
		default:
			g_return_if_reached ();
			break;
	}

	while (--days >= 0)
	{
		/* subtract 1 day */
		cmp_time -= 43200;
		localtime_r (&cmp_time, &btime);
		btime.tm_sec = 0;
		btime.tm_min = 0;
		btime.tm_hour = 0;
		cmp_time = mktime (&btime);
	}

	*from = cmp_time;
}

static GList *
substrings_filter (EphyHistoryWindow *editor)
{
	const char *search_text;
	char **tokens, **p;
	GList *substrings = NULL;

	search_text = gtk_entry_get_text (GTK_ENTRY (editor->priv->search_entry));
	tokens = p = g_strsplit (search_text, " ", -1);

	while (*p) {
		substrings = g_list_prepend (substrings, *p++);
	};
	substrings = g_list_reverse (substrings);
	g_free (tokens);

	return substrings;
}

static void
on_get_hosts_cb (gpointer service,
		 gboolean success,
		 gpointer result_data,
		 gpointer user_data)
{
	EphyHistoryWindow *window = EPHY_HISTORY_WINDOW (user_data);
	EphyHistoryHost *selected_host;
	GList *hosts = NULL;

	if (success != TRUE)
		goto out;

	hosts = (GList *) result_data;
	selected_host = get_selected_host (window);
	ephy_hosts_store_clear (EPHY_HOSTS_STORE (window->priv->hosts_store));
	ephy_hosts_store_add_hosts (window->priv->hosts_store, hosts);
	ephy_hosts_view_select_host (EPHY_HOSTS_VIEW (window->priv->hosts_view),
				     selected_host);
	ephy_history_host_free (selected_host);
out:
	g_list_free_full (hosts, (GDestroyNotify)ephy_history_host_free);
}

static void
on_find_urls_cb (gpointer service,
		 gboolean success,
		 gpointer result_data,
		 gpointer user_data)
{
	EphyHistoryWindow *window = EPHY_HISTORY_WINDOW (user_data);
	GList *urls;
	
	if (success != TRUE)
		return;

	urls = (GList *)result_data;
	gtk_list_store_clear (GTK_LIST_STORE (window->priv->urls_store));
	ephy_urls_store_add_urls (window->priv->urls_store, urls);
	g_list_free_full (urls, (GDestroyNotify)ephy_history_url_free);
}

static void
filter_now (EphyHistoryWindow *editor,
	    gboolean hosts,
	    gboolean pages)
{
	gint64 from, to;
	EphyHistoryHost *host;
	GList *substrings;

	setup_time_filters (editor, &from, &to);
	substrings = substrings_filter (editor);

	if (hosts)
	{
		ephy_history_service_find_hosts (editor->priv->history_service,
						 from, to, NULL,
						(EphyHistoryJobCallback) on_get_hosts_cb, editor);
	}

	if (pages)
	{
		host = get_selected_host (editor);
		ephy_history_service_find_urls (editor->priv->history_service,
						from, to,
						0, host ? host->id : 0,
						substrings, NULL,
						(EphyHistoryJobCallback)on_find_urls_cb, editor);
		ephy_history_host_free (host);
	}
}

static gboolean
on_visit_url_cb (EphyHistoryService *service,
		 gchar *url,
		 EphyHistoryPageVisitType visit_type,
		 EphyHistoryWindow *editor)
{
	filter_now (editor, TRUE, TRUE);

	return FALSE;
}

static void
ephy_history_window_constructed (GObject *object)
{
	GtkTreeViewColumn *col;
	GtkTreeSelection *selection;
	GtkWidget *vbox, *hpaned;
	GtkWidget *pages_view, *hosts_view;
	GtkWidget *scrolled_window;
	EphyURLsStore *urls_store;
	EphyHostsStore *hosts_store;
	GtkUIManager *ui_merge;
	GtkActionGroup *action_group;
	GtkAction *action;
	EphyHistoryWindow *editor = EPHY_HISTORY_WINDOW (object);

	ephy_gui_ensure_window_group (GTK_WINDOW (editor));

	gtk_window_set_title (GTK_WINDOW (editor), _("History"));

	g_signal_connect (editor, "delete_event",
			  G_CALLBACK (delete_event_cb), NULL);

	editor->priv->main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (editor->priv->main_vbox);
	gtk_container_add (GTK_CONTAINER (editor), editor->priv->main_vbox);

	ui_merge = gtk_ui_manager_new ();
	g_signal_connect (ui_merge, "add_widget", G_CALLBACK (add_widget), editor);
	gtk_window_add_accel_group (GTK_WINDOW (editor),
				    gtk_ui_manager_get_accel_group (ui_merge));
	action_group = gtk_action_group_new ("PopupActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group, ephy_history_ui_entries,
				      G_N_ELEMENTS (ephy_history_ui_entries), editor);

	gtk_action_group_add_toggle_actions (action_group,
					     ephy_history_toggle_entries,
					     G_N_ELEMENTS (ephy_history_toggle_entries),
					     editor);

	gtk_ui_manager_insert_action_group (ui_merge,
					    action_group, 0);
	gtk_ui_manager_add_ui_from_file (ui_merge,
					 ephy_file ("epiphany-history-window-ui.xml"),
					 NULL);
	gtk_ui_manager_ensure_update (ui_merge);
	editor->priv->ui_merge = ui_merge;
	editor->priv->action_group = action_group;

	hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_container_set_border_width (GTK_CONTAINER (hpaned), 0);
	gtk_box_pack_end (GTK_BOX (editor->priv->main_vbox), hpaned,
			  TRUE, TRUE, 0);
	gtk_widget_show (hpaned);

	/* Sites View */
	scrolled_window = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
					"hadjustment", NULL,
					"vadjustment", NULL,
					"hscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"vscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"shadow_type", GTK_SHADOW_IN,
					NULL);
	gtk_paned_pack1 (GTK_PANED (hpaned), scrolled_window, TRUE, FALSE);
	gtk_widget_show (scrolled_window);
	hosts_store = ephy_hosts_store_new ();
	hosts_view = ephy_hosts_view_new ();
	gtk_tree_view_set_model (GTK_TREE_VIEW (hosts_view),
				 GTK_TREE_MODEL (hosts_store));
	add_focus_monitor (editor, hosts_view);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (hosts_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	gtk_container_add (GTK_CONTAINER (scrolled_window), hosts_view);
	gtk_widget_show (hosts_view);
	editor->priv->hosts_view = hosts_view;
	g_signal_connect (G_OBJECT (hosts_view),
			  "key_press_event",
			  G_CALLBACK (key_pressed_cb),
			  editor);
	g_signal_connect (G_OBJECT (selection),
			  "changed",
			  G_CALLBACK (view_selection_changed_cb),
			  editor);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_paned_pack2 (GTK_PANED (hpaned), vbox, TRUE, FALSE);
	gtk_widget_show (vbox);

	gtk_box_pack_start (GTK_BOX (vbox),
			    build_search_box (editor),
			    FALSE, FALSE, 0);

	/* Pages View */
	scrolled_window = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
					"hadjustment", NULL,
					"vadjustment", NULL,
					"hscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"vscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"shadow_type", GTK_SHADOW_IN,
					NULL);
	gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show (scrolled_window);
	editor->priv->pages_view = pages_view = ephy_urls_view_new ();
	urls_store = ephy_urls_store_new ();
	gtk_tree_view_set_model (GTK_TREE_VIEW (pages_view), GTK_TREE_MODEL (urls_store));
	add_focus_monitor (editor, pages_view);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pages_view));
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (pages_view), TRUE);

	/* These three blocks should most likely go into
	   EphyHistoryView. */
	col = gtk_tree_view_get_column (GTK_TREE_VIEW (pages_view), 0);
	gtk_tree_view_column_set_min_width (col, 300);
	gtk_tree_view_column_set_resizable (col, TRUE);
	editor->priv->title_col = col;

	col = gtk_tree_view_get_column (GTK_TREE_VIEW (pages_view), 1);
	gtk_tree_view_column_set_min_width (col, 300);
	gtk_tree_view_column_set_resizable (col, TRUE);
	editor->priv->address_col = col;

	col = gtk_tree_view_get_column (GTK_TREE_VIEW (pages_view), 2);
	editor->priv->datetime_col = col;
	parse_time_into_date (editor->priv->datetime_col, 2);

	gtk_container_add (GTK_CONTAINER (scrolled_window), pages_view);
	gtk_widget_show (pages_view);
	editor->priv->pages_view = pages_view;
	editor->priv->urls_store = urls_store;
	editor->priv->hosts_store = hosts_store;

	action = gtk_action_group_get_action (action_group, "ViewTitle");
	g_settings_bind (EPHY_SETTINGS_STATE,
			 EPHY_PREFS_STATE_HISTORY_VIEW_TITLE,
			 action, "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (EPHY_SETTINGS_STATE,
			 EPHY_PREFS_STATE_HISTORY_VIEW_TITLE,
			 editor->priv->title_col, "visible",
			 G_SETTINGS_BIND_DEFAULT);

	action = gtk_action_group_get_action (action_group, "ViewAddress");
	g_settings_bind (EPHY_SETTINGS_STATE,
			 EPHY_PREFS_STATE_HISTORY_VIEW_ADDRESS,
			 action, "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (EPHY_SETTINGS_STATE,
			 EPHY_PREFS_STATE_HISTORY_VIEW_ADDRESS,
			 editor->priv->address_col, "visible",
			 G_SETTINGS_BIND_DEFAULT);

	action = gtk_action_group_get_action (action_group, "ViewDateTime");
	g_settings_bind (EPHY_SETTINGS_STATE,
			 EPHY_PREFS_STATE_HISTORY_VIEW_DATE,
			 action, "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_settings_bind (EPHY_SETTINGS_STATE,
			 EPHY_PREFS_STATE_HISTORY_VIEW_DATE,
			 editor->priv->datetime_col, "visible",
			 G_SETTINGS_BIND_DEFAULT);

	g_signal_connect (G_OBJECT (pages_view),
			  "row-activated",
			  G_CALLBACK (ephy_history_window_row_activated_cb),
			  editor);
	g_signal_connect (pages_view, "row-middle-clicked",
			  G_CALLBACK (ephy_history_window_row_middle_clicked_cb),
			  editor);
	g_signal_connect (G_OBJECT (pages_view),
			  "popup_menu",
			  G_CALLBACK (ephy_history_window_show_popup_cb),
			  editor);
	g_signal_connect (G_OBJECT (pages_view),
			  "key_press_event",
			  G_CALLBACK (key_pressed_cb),
			  editor);

	ephy_state_add_window (GTK_WIDGET (editor),
			       "history_window",
			       450, 400, FALSE,
			       EPHY_STATE_WINDOW_SAVE_SIZE | EPHY_STATE_WINDOW_SAVE_POSITION);
	ephy_state_add_paned  (GTK_WIDGET (hpaned),
			       "history_paned",
			       130);

	filter_now (editor, TRUE, TRUE);

	g_signal_connect_after (editor->priv->history_service,
				"visit-url", G_CALLBACK (on_visit_url_cb),
				editor);

	if (G_OBJECT_CLASS (ephy_history_window_parent_class)->constructed)
		G_OBJECT_CLASS (ephy_history_window_parent_class)->constructed (object);
}

void
ephy_history_window_set_parent (EphyHistoryWindow *ebe,
				GtkWidget *window)
{
	GtkWidget **widget;
	if (ebe->priv->window)
	{
		widget = &ebe->priv->window;
		g_object_remove_weak_pointer
			(G_OBJECT(ebe->priv->window),
			 (gpointer *)widget);
	}

	ebe->priv->window = window;
	widget = &ebe->priv->window;

	g_object_add_weak_pointer
			(G_OBJECT(ebe->priv->window),
			(gpointer *)widget);

}

GtkWidget *
ephy_history_window_new (EphyHistoryService *history_service)
{
	EphyHistoryWindow *editor;

	g_return_val_if_fail (history_service != NULL, NULL);

	editor = g_object_new (EPHY_TYPE_HISTORY_WINDOW,
			       "history-service", history_service,
			       NULL);

	return GTK_WIDGET (editor);
}

static void
ephy_history_window_set_property (GObject *object,
				  guint prop_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	EphyHistoryWindow *editor = EPHY_HISTORY_WINDOW (object);

	switch (prop_id)
	{
	case PROP_HISTORY_SERVICE:
		editor->priv->history_service = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_history_window_get_property (GObject *object,
				  guint prop_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	EphyHistoryWindow *editor = EPHY_HISTORY_WINDOW (object);

	switch (prop_id)
	{
	case PROP_HISTORY_SERVICE:
		g_value_set_object (value, editor->priv->history_service);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_history_window_init (EphyHistoryWindow *editor)
{
	editor->priv = EPHY_HISTORY_WINDOW_GET_PRIVATE (editor);
}

static void
ephy_history_window_dispose (GObject *object)
{
	EphyHistoryWindow *editor = EPHY_HISTORY_WINDOW (object);

	if (editor->priv->hosts_view != NULL)
	{
		remove_focus_monitor (editor, editor->priv->pages_view);
		remove_focus_monitor (editor, editor->priv->hosts_view);
		remove_focus_monitor (editor, editor->priv->search_entry);

		editor->priv->hosts_view = NULL;
	}

	G_OBJECT_CLASS (ephy_history_window_parent_class)->dispose (object);
}
