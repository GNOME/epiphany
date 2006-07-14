/*
 *  Copyright (C) 2003 Marco Pesenti Gritti <mpeseng@tin.it>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkhpaned.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkactiongroup.h>
#include <gtk/gtktoggleaction.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtktoggleaction.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <string.h>

#include "ephy-bookmarks-editor.h"
#include "ephy-bookmark-properties.h"
#include "ephy-bookmarks-import.h"
#include "ephy-node-common.h"
#include "ephy-node-view.h"
#include "ephy-window.h"
#include "ephy-dnd.h"
#include "ephy-shell.h"
#include "ephy-session.h"
#include "ephy-file-helpers.h"
#include "ephy-file-chooser.h"
#include "popup-commands.h"
#include "ephy-state.h"
#include "window-commands.h"
#include "ephy-gui.h"
#include "ephy-stock-icons.h"
#include "ephy-search-entry.h"
#include "ephy-toolbars-model.h"
#include "ephy-favicon-cache.h"
#include "eel-gconf-extensions.h"
#include "ephy-debug.h"

static GtkTargetEntry topic_drag_dest_types [] =
{
	{ EPHY_DND_URI_LIST_TYPE,   0, 0 }
};

static int n_topic_drag_dest_types = G_N_ELEMENTS (topic_drag_dest_types);

static GtkTargetEntry bmk_drag_types [] =
{
        { EPHY_DND_URI_LIST_TYPE,   0, 0 },
        { EPHY_DND_TEXT_TYPE,       0, 1 },
        { EPHY_DND_URL_TYPE,        0, 2 }
};
static int n_bmk_drag_types = G_N_ELEMENTS (bmk_drag_types);

static GtkTargetEntry topic_drag_types [] =
{
	{ EPHY_DND_TOPIC_TYPE,      0, 0 }
};
static int n_topic_drag_types = G_N_ELEMENTS (topic_drag_types);

static void ephy_bookmarks_editor_class_init (EphyBookmarksEditorClass *klass);
static void ephy_bookmarks_editor_init (EphyBookmarksEditor *editor);
static void ephy_bookmarks_editor_finalize (GObject *object);
static void ephy_bookmarks_editor_dispose  (GObject *object);
static void ephy_bookmarks_editor_set_property (GObject *object,
		                                guint prop_id,
		                                const GValue *value,
		                                GParamSpec *pspec);
static void ephy_bookmarks_editor_get_property (GObject *object,
						guint prop_id,
						GValue *value,
						GParamSpec *pspec);
static void ephy_bookmarks_editor_update_menu  (EphyBookmarksEditor *editor);

static void cmd_open_bookmarks_in_tabs    (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_open_bookmarks_in_browser (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_show_in_bookmarks_bar     (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_delete			  (GtkAction *action,
				           EphyBookmarksEditor *editor);
static void cmd_bookmark_properties	  (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_bookmarks_import	  (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_add_topic		  (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_rename			  (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_close			  (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_cut			  (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_copy			  (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_paste			  (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_select_all		  (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_help_contents		  (GtkAction *action,
					   EphyBookmarksEditor *editor);

#define EPHY_BOOKMARKS_EDITOR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_BOOKMARKS_EDITOR, EphyBookmarksEditorPrivate))

#define CONF_BOOKMARKS_VIEW_DETAILS "/apps/epiphany/dialogs/bookmarks_view_details"

struct EphyBookmarksEditorPrivate
{
	EphyBookmarks *bookmarks;
	GtkWidget *bm_view;
	GtkWidget *key_view;
	EphyNodeFilter *bookmarks_filter;
	GtkWidget *search_entry;
	GtkWidget *main_vbox;
	GtkWidget *window;
	GtkUIManager *ui_merge;
	GtkActionGroup *action_group;
	int priority_col;
	EphyToolbarsModel *tb_model;
	GHashTable *props_dialogs;

	GtkTreeViewColumn *title_col;
	GtkTreeViewColumn *address_col;
};

enum
{
	PROP_0,
	PROP_BOOKMARKS
};

static GObjectClass *parent_class = NULL;

static GtkActionEntry ephy_bookmark_popup_entries [] = {
	/* Toplevel */
	{ "File", NULL, N_("_File") },
	{ "Edit", NULL, N_("_Edit") },
	{ "View", NULL, N_("_View") },
	{ "Help", NULL, N_("_Help") },
	{ "PopupAction", NULL, "" },

	/* File Menu*/
	{ "NewTopic", GTK_STOCK_NEW, N_("_New Topic"), "<control>N",
	  N_("Create a new topic"), 
	  G_CALLBACK (cmd_add_topic) },
	{ "OpenInWindow", GTK_STOCK_OPEN, N_("_Open in New Window"), "<control>O",
	  N_("Open the selected bookmark in a new window"), 
	  G_CALLBACK (cmd_open_bookmarks_in_browser) },
	{ "OpenInTab", NULL, N_("Open in New _Tab"), "<shift><control>O",
	  N_("Open the selected bookmark in a new tab"), 
	  G_CALLBACK (cmd_open_bookmarks_in_tabs) },
	{ "Rename", NULL, N_("_Rename..."), "F2",
	  N_("Rename the selected bookmark or topic"), G_CALLBACK (cmd_rename) },
	{ "Delete", GTK_STOCK_DELETE, N_("_Delete"), NULL,
	  N_("Delete the selected bookmark or topic"), 
	  G_CALLBACK (cmd_delete) },
	{ "Properties", GTK_STOCK_PROPERTIES, N_("_Properties"), "<alt>Return",
	  N_("View or modify the properties of the selected bookmark"), 
	  G_CALLBACK (cmd_bookmark_properties) },
	{ "Import", NULL, N_("_Import Bookmarks..."), NULL,
	  N_("Import bookmarks from another browser or a bookmarks file"), 
	  G_CALLBACK (cmd_bookmarks_import) },
	{ "Close", GTK_STOCK_CLOSE, N_("_Close"), "<control>W",
	  N_("Close the bookmarks window"), 
	  G_CALLBACK (cmd_close) },

	/* Edit Menu */
	{ "Cut", GTK_STOCK_CUT, N_("Cu_t"), "<control>X",
	  N_("Cut the selection"), 
	  G_CALLBACK (cmd_cut) },
	{ "Copy", GTK_STOCK_COPY, N_("_Copy"), "<control>C",
	  N_("Copy the selection"), 
	  G_CALLBACK (cmd_copy) },
	{ "Paste", GTK_STOCK_PASTE, N_("_Paste"), "<control>V",
	  N_("Paste the clipboard"), 
	  G_CALLBACK (cmd_paste) },
	{ "SelectAll", NULL, N_("Select _All"), "<control>A",
	  N_("Select all bookmarks or text"), 
	  G_CALLBACK (cmd_select_all) },
	
	/* Help Menu */	
	{ "HelpContents", GTK_STOCK_HELP, N_("_Contents"), "F1",
	  N_("Display bookmarks help"), 
	  G_CALLBACK (cmd_help_contents) },
	{ "HelpAbout", GNOME_STOCK_ABOUT, N_("_About"), NULL,
	  N_("Display credits for the web browser creators"),
	  G_CALLBACK (window_cmd_help_about) },
};
static guint ephy_bookmark_popup_n_entries = G_N_ELEMENTS (ephy_bookmark_popup_entries);

static GtkToggleActionEntry ephy_bookmark_popup_toggle_entries [] =
{
	/* File Menu */
	{ "ShowInBookmarksBar", NULL, N_("_Show in Bookmarks Bar"), NULL,
	  N_("Show the selected bookmark or topic in the bookmarks bar"), 
	  G_CALLBACK (cmd_show_in_bookmarks_bar), FALSE }
};
static guint ephy_bookmark_popup_n_toggle_entries = G_N_ELEMENTS (ephy_bookmark_popup_toggle_entries);

enum
{
	VIEW_TITLE,
	VIEW_TITLE_AND_ADDRESS
};

static GtkRadioActionEntry ephy_bookmark_radio_entries [] =
{
	/* View Menu */
	{ "ViewTitle", NULL, N_("_Title"), NULL,
	  N_("Show only the title column"), VIEW_TITLE },
	{ "ViewTitleAddress", NULL, N_("T_itle and Address"), NULL,
	  N_("Show both the title and address columns"),
	  VIEW_TITLE_AND_ADDRESS } 
};
static guint ephy_bookmark_n_radio_entries = G_N_ELEMENTS (ephy_bookmark_radio_entries);

static void
entry_selection_changed_cb (GtkWidget *widget, GParamSpec *pspec, EphyBookmarksEditor *editor)
{
	ephy_bookmarks_editor_update_menu (editor);
}

static void
add_entry_monitor (EphyBookmarksEditor *editor, GtkWidget *entry)
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

static void
add_text_renderer_monitor (EphyBookmarksEditor *editor)
{
	GtkWidget *entry;

	entry = gtk_window_get_focus (GTK_WINDOW (editor));
	g_return_if_fail (GTK_IS_EDITABLE (entry));

	add_entry_monitor (editor, entry);
}

static void
cmd_add_topic (GtkAction *action,
	       EphyBookmarksEditor *editor)
{
	EphyNode *node;

	node = ephy_bookmarks_add_keyword (editor->priv->bookmarks,
				           _("Type a topic"));
	ephy_node_view_select_node (EPHY_NODE_VIEW (editor->priv->key_view), node);
	ephy_node_view_edit (EPHY_NODE_VIEW (editor->priv->key_view));
	add_text_renderer_monitor (editor);
}

static void
cmd_close (GtkAction *action,
	   EphyBookmarksEditor *editor)
{
	gtk_widget_hide (GTK_WIDGET (editor));
}

static void
cmd_rename (GtkAction *action,
	    EphyBookmarksEditor *editor)
{
	if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->bm_view)))
	{
		ephy_node_view_edit (EPHY_NODE_VIEW (editor->priv->bm_view));
	}
	else if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->key_view)))
	{
		ephy_node_view_edit (EPHY_NODE_VIEW (editor->priv->key_view));
	}
	add_text_renderer_monitor (editor);
}

static GtkWidget *
get_target_window (EphyBookmarksEditor *editor)
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
cmd_show_in_bookmarks_bar (GtkAction *action,
		           EphyBookmarksEditor *editor)
{
	EphyNode *node;
	GList *selection;
	gboolean state, topic;
	gulong id;

	if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->bm_view)))
	{
		selection = ephy_node_view_get_selection
			(EPHY_NODE_VIEW (editor->priv->bm_view));
		topic = FALSE;
	}
	else if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->key_view)))
	{
		selection = ephy_node_view_get_selection
			(EPHY_NODE_VIEW (editor->priv->key_view));
		topic = TRUE;
	}
	else
	{
		return;
	}

	node = selection->data;
	id = ephy_node_get_id (node);
	state = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	if (state)
	{
		ephy_toolbars_model_add_bookmark
			(editor->priv->tb_model, topic, id);
	}
	else
	{
		ephy_toolbars_model_remove_bookmark
			(editor->priv->tb_model, id);
	}

	g_list_free (selection);
}

static void
cmd_open_bookmarks_in_tabs (GtkAction *action,
			    EphyBookmarksEditor *editor)
{
	EphyWindow *window;
	GList *selection;
	GList *l;

	window = EPHY_WINDOW (get_target_window (editor));
	selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->bm_view));

	for (l = selection; l; l = l->next)
	{
		EphyNode *node = l->data;
		EphyTab *new_tab;
		const char *location;

		location = ephy_node_get_property_string (node,
						EPHY_NODE_BMK_PROP_LOCATION);

		new_tab = ephy_shell_new_tab (ephy_shell, window, NULL, location,
					      EPHY_NEW_TAB_OPEN_PAGE |
					      EPHY_NEW_TAB_IN_EXISTING_WINDOW);
		/* if there was no target window, a new one was opened. Get it
		 * from the new tab so we open the remaining links in the
		 * same window. See bug 138343.
		 */
		if (window == NULL)
		{
			window = EPHY_WINDOW
				(gtk_widget_get_toplevel (GTK_WIDGET (new_tab)));
		}
	}

	g_list_free (selection);
}

static void
cmd_open_bookmarks_in_browser (GtkAction *action,
			       EphyBookmarksEditor *editor)
{
	EphyWindow *window;
	GList *selection;
	GList *l;

	window = EPHY_WINDOW (get_target_window (editor));
	selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->bm_view));

	for (l = selection; l; l = l->next)
	{
		EphyNode *node = l->data;
		const char *location;

		location = ephy_node_get_property_string (node,
						EPHY_NODE_BMK_PROP_LOCATION);

		ephy_shell_new_tab (ephy_shell, window, NULL, location,
				    EPHY_NEW_TAB_OPEN_PAGE | EPHY_NEW_TAB_IN_NEW_WINDOW);
	}

	g_list_free (selection);
}

static void
cmd_delete (GtkAction *action,
	    EphyBookmarksEditor *editor)
{
	if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->bm_view)))
	{
		ephy_node_view_remove (EPHY_NODE_VIEW (editor->priv->bm_view));
	}
	else if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->key_view)))
	{
		EphyNodePriority priority;
		GList *selected;
		EphyNode *node;

		selected = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->key_view));
		node = selected->data;
		priority = ephy_node_get_property_int (node, EPHY_NODE_KEYWORD_PROP_PRIORITY);

		if (priority == -1) priority = EPHY_NODE_NORMAL_PRIORITY;

		if (priority == EPHY_NODE_NORMAL_PRIORITY)
		{
			ephy_node_view_remove (EPHY_NODE_VIEW (editor->priv->key_view));
		}
		g_list_free (selected);
	}
}

static void
prop_dialog_destroy_cb (GtkWidget *dialog, EphyBookmarksEditor *editor)
{
	EphyNode *node;

	node = ephy_bookmark_properties_get_node (EPHY_BOOKMARK_PROPERTIES (dialog));
	g_hash_table_remove (editor->priv->props_dialogs, node);
}

static void
show_properties_dialog (EphyBookmarksEditor *editor, EphyNode *node)
{
	GtkWidget *dialog;

	dialog = g_hash_table_lookup (editor->priv->props_dialogs, node);

	if (!dialog)
	{
		dialog = ephy_bookmark_properties_new
			(editor->priv->bookmarks, node, GTK_WINDOW (editor));
		g_signal_connect (dialog, "destroy",
				  G_CALLBACK (prop_dialog_destroy_cb), editor);
		g_hash_table_insert (editor->priv->props_dialogs, node, dialog);
	}

	gtk_window_present (GTK_WINDOW (dialog));
}

static void
add_bookmarks_source (GtkListStore *store,
		      const char *desc,
		      const char *dir,
		      const char *filename,
		      int max_depth)
{
	GtkTreeIter iter;
	GSList *l;
	char *path;

	path = g_build_filename (g_get_home_dir (), dir, NULL);
	l = ephy_file_find  (path, filename, max_depth);
	g_free (path);

	if (l)
	{
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, desc, 1, l->data, -1);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);
}

static void
import_from_file_response_cb (GtkDialog *dialog, gint response,
			      EphyBookmarksEditor *editor)
{
	char *filename;

	if (response == GTK_RESPONSE_ACCEPT)
	{
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

		if (filename != NULL)
		{
			ephy_bookmarks_import (editor->priv->bookmarks, filename);			

			g_free (filename);
		}
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
import_dialog_response_cb (GtkDialog *dialog, gint response,
			   EphyBookmarksEditor *editor)
{
	if (response == GTK_RESPONSE_OK)
	{
		GtkTreeIter iter;
		char *filename;
		GtkWidget *combo;
		GtkTreeModel *model;
		int active;
		GValue value = { 0, };

		combo = g_object_get_data (G_OBJECT (dialog), "combo_box");
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
		active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
		gtk_tree_model_iter_nth_child (model, &iter, NULL, active);
		gtk_tree_model_get_value (model, &iter, 1, &value);
		filename = g_strdup (g_value_get_string (&value));
		g_value_unset (&value);

		if (filename == NULL)
		{
			EphyFileChooser *dialog;

			dialog = ephy_file_chooser_new (_("Import bookmarks from file"),
							GTK_WIDGET (editor),
							GTK_FILE_CHOOSER_ACTION_OPEN,
							NULL);
			/* FIXME: set up some filters perhaps ? */
			g_signal_connect (dialog, "response",
					  G_CALLBACK (import_from_file_response_cb), editor);

			gtk_widget_show (GTK_WIDGET (dialog));
		}
		else
		{
			ephy_bookmarks_import (editor->priv->bookmarks, filename);
		}

		g_free (filename);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
cmd_bookmarks_import (GtkAction *action,
		      EphyBookmarksEditor *editor)
{
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *combo;
	GtkCellRenderer *cell;
	GtkListStore *store;
	GtkTreeIter iter;

	dialog = gtk_dialog_new_with_buttons (_("Import Bookmarks"),
					     GTK_WINDOW (editor),
					     GTK_DIALOG_DESTROY_WITH_PARENT |
					     GTK_DIALOG_NO_SEPARATOR,
					     GTK_STOCK_CANCEL,
					     GTK_RESPONSE_CANCEL,
					     GTK_STOCK_OK,
					     GTK_RESPONSE_OK,
					     NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox,
			    TRUE, TRUE, 0);

	label = gtk_label_new (_("Choose the bookmarks source:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

        store = GTK_LIST_STORE (gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING));
	combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	gtk_widget_show (combo); 
	g_object_set_data (G_OBJECT (dialog), "combo_box", combo);
	g_object_unref (store);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell,
                                        "text", 0,
                                        NULL);

	add_bookmarks_source (store, _("Mozilla bookmarks"),
			      MOZILLA_BOOKMARKS_DIR, "bookmarks.html", 4);
	add_bookmarks_source (store, _("Firebird bookmarks"),
			      FIREBIRD_BOOKMARKS_DIR, "bookmarks.html", 4);
	add_bookmarks_source (store, _("Galeon bookmarks"),
			      GALEON_BOOKMARKS_DIR, "bookmarks.xbel", 0);
	add_bookmarks_source (store, _("Konqueror bookmarks"),
			      KDE_BOOKMARKS_DIR, "bookmarks.xml", 0);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, _("Import from a file"), 1, NULL, -1);

	gtk_box_pack_start (GTK_BOX (vbox), combo, TRUE, TRUE, 0);
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (import_dialog_response_cb),
			  editor);

	gtk_widget_show (dialog);
}

static void
cmd_bookmark_properties (GtkAction *action,
			 EphyBookmarksEditor *editor)
{
	GList *selection;
	GList *l;

	selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->bm_view));

	for (l = selection; l; l = l->next)
	{
		EphyNode *node = l->data;
		show_properties_dialog (editor, node);
	}

	g_list_free (selection);
}

static void
cmd_cut (GtkAction *action,
	 EphyBookmarksEditor *editor)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (editor));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_cut_clipboard (GTK_EDITABLE (widget));
	}
}

static void
cmd_copy (GtkAction *action,
	  EphyBookmarksEditor *editor)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (editor));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_copy_clipboard (GTK_EDITABLE (widget));
	}

	else if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->bm_view)))
	{
		GList *selection;

		selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->bm_view));

		if (g_list_length (selection) == 1)
		{
			const char *tmp;
			EphyNode *node = selection->data;
			tmp = ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_LOCATION);
			gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), tmp, -1);
		}

		g_list_free (selection);
	}
}

static void
cmd_paste (GtkAction *action,
	   EphyBookmarksEditor *editor)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (editor));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_paste_clipboard (GTK_EDITABLE (widget));
	}
}

static void
cmd_select_all (GtkAction *action,
		EphyBookmarksEditor *editor)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (editor));
	GtkWidget *bm_view = editor->priv->bm_view;

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
	}
	else if (ephy_node_view_is_target (EPHY_NODE_VIEW (bm_view)))
	{
		GtkTreeSelection *sel;

		sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (bm_view));
		gtk_tree_selection_select_all (sel);
	}
}

static void
cmd_help_contents (GtkAction *action,
		   EphyBookmarksEditor *editor)
{
	ephy_gui_help (GTK_WINDOW (editor), 
		       "epiphany", 
		       "ephy-managing-bookmarks");
}

static void
set_columns_visibility (EphyBookmarksEditor *editor, int value)
{
	switch (value)
	{
		case VIEW_TITLE:
			gtk_tree_view_column_set_visible (editor->priv->title_col, TRUE);
			gtk_tree_view_column_set_visible (editor->priv->address_col, FALSE);
			break;
		case VIEW_TITLE_AND_ADDRESS:
			gtk_tree_view_column_set_visible (editor->priv->title_col, TRUE);
			gtk_tree_view_column_set_visible (editor->priv->address_col, TRUE);
			break;
	}
}

static void
cmd_view_columns (GtkAction *action,
		  GtkRadioAction *current,
		  EphyBookmarksEditor *editor)
{
	int value;
	GSList *svalues = NULL;

	g_return_if_fail (EPHY_IS_BOOKMARKS_EDITOR (editor));

	value = gtk_radio_action_get_current_value (current);
	set_columns_visibility (editor, value);

	switch (value)
	{
		case VIEW_TITLE:
			svalues = g_slist_append (svalues, (gpointer)"title");
			break;
		case VIEW_TITLE_AND_ADDRESS:
			svalues = g_slist_append (svalues, (gpointer)"title");
			svalues = g_slist_append (svalues, (gpointer)"address");
			break;
	}

	eel_gconf_set_string_list (CONF_BOOKMARKS_VIEW_DETAILS, svalues);
	g_slist_free (svalues);
}

GType
ephy_bookmarks_editor_get_type (void)
{
	static GType ephy_bookmarks_editor_type = 0;

	if (ephy_bookmarks_editor_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyBookmarksEditorClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_bookmarks_editor_class_init,
			NULL,
			NULL,
			sizeof (EphyBookmarksEditor),
			0,
			(GInstanceInitFunc) ephy_bookmarks_editor_init
		};

		ephy_bookmarks_editor_type = g_type_register_static (GTK_TYPE_WINDOW,
							             "EphyBookmarksEditor",
							             &our_info, 0);
	}

	return ephy_bookmarks_editor_type;
}

static void
ephy_bookmarks_editor_show (GtkWidget *widget)
{
	EphyBookmarksEditor *editor = EPHY_BOOKMARKS_EDITOR (widget);

	gtk_widget_grab_focus (editor->priv->search_entry);

	GTK_WIDGET_CLASS (parent_class)->show (widget);
}

static void
ephy_bookmarks_editor_class_init (EphyBookmarksEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_bookmarks_editor_finalize;
	object_class->dispose  = ephy_bookmarks_editor_dispose;

	object_class->set_property = ephy_bookmarks_editor_set_property;
	object_class->get_property = ephy_bookmarks_editor_get_property;

	widget_class->show = ephy_bookmarks_editor_show;

	g_object_class_install_property (object_class,
					 PROP_BOOKMARKS,
					 g_param_spec_object ("bookmarks",
							      "Bookmarks set",
							      "Bookmarks set",
							      EPHY_TYPE_BOOKMARKS,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(EphyBookmarksEditorPrivate));
}

static void
ephy_bookmarks_editor_finalize (GObject *object)
{
	EphyBookmarksEditor *editor = EPHY_BOOKMARKS_EDITOR (object);

	g_object_unref (G_OBJECT (editor->priv->bookmarks_filter));

	g_object_unref (editor->priv->action_group);
	g_object_unref (editor->priv->ui_merge);

	if (editor->priv->window)
	{
		g_object_remove_weak_pointer
                        (G_OBJECT(editor->priv->window),
                         (gpointer *)&editor->priv->window);
	}

	g_hash_table_destroy (editor->priv->props_dialogs);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_bookmarks_editor_node_activated_cb (GtkWidget *view,
				         EphyNode *node,
					 EphyBookmarksEditor *editor)
{
	const char *location;

	location = ephy_node_get_property_string
		(node, EPHY_NODE_BMK_PROP_LOCATION);
	g_return_if_fail (location != NULL);

	ephy_shell_new_tab (ephy_shell, NULL, NULL, location,
			    EPHY_NEW_TAB_OPEN_PAGE);
}

static void
ephy_bookmarks_editor_update_menu (EphyBookmarksEditor *editor)
{
	gboolean open_in_window, open_in_tab,
		 rename, delete, properties;
	const gchar *open_in_window_label, *open_in_tab_label, *copy_label;
	gboolean bmk_focus, key_focus;
	gboolean key_selection, bmk_selection;
	gboolean key_normal = FALSE;
	gboolean bmk_multiple_selection;
	gboolean cut, copy, paste, select_all;
	gboolean can_show_in_bookmarks_bar, show_in_bookmarks_bar = FALSE;
	GtkActionGroup *action_group;
	GtkAction *action;
	GList *selected;
	GtkWidget *focus_widget;

	LOG ("Update menu sensitivity")

	bmk_focus = ephy_node_view_is_target
		(EPHY_NODE_VIEW (editor->priv->bm_view));
	key_focus = ephy_node_view_is_target
		(EPHY_NODE_VIEW (editor->priv->key_view));
	focus_widget = gtk_window_get_focus (GTK_WINDOW (editor));
	bmk_selection = ephy_node_view_has_selection
		(EPHY_NODE_VIEW (editor->priv->bm_view),
		 &bmk_multiple_selection);
	key_selection = ephy_node_view_has_selection
		(EPHY_NODE_VIEW (editor->priv->key_view), NULL);

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
		copy = (bmk_focus && !bmk_multiple_selection && bmk_selection);
		paste = FALSE;
		select_all = bmk_focus;
	}

	selected = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->key_view));
	if (key_focus && selected)
	{
		EphyNode *node = selected->data;
		EphyNodePriority priority;
		gulong id;

		id = ephy_node_get_id (node);
		show_in_bookmarks_bar = ephy_toolbars_model_has_bookmark
			(editor->priv->tb_model, id);

		priority = ephy_node_get_property_int
			(node, EPHY_NODE_KEYWORD_PROP_PRIORITY);
		if (priority == -1) priority = EPHY_NODE_NORMAL_PRIORITY;
		key_normal = (priority == EPHY_NODE_NORMAL_PRIORITY);

		g_list_free (selected);
	}

	selected = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->bm_view));
	if (bmk_focus && selected)
	{
		EphyNode *node = selected->data;
		gulong id;

		g_return_if_fail (node != NULL);

		id = ephy_node_get_id (node);
		show_in_bookmarks_bar = ephy_toolbars_model_has_bookmark
			(editor->priv->tb_model, id);

		g_list_free (selected);
	}

	if (bmk_multiple_selection)
	{
		open_in_window_label = N_("_Open in New Windows");
		open_in_tab_label = N_("Open in New _Tabs");
	}
	else
	{
		open_in_window_label = _("_Open in New Window");
		open_in_tab_label = _("Open in New _Tab");
	}

	if (bmk_focus)
	{
		copy_label = _("_Copy Address");
	}
	else
	{
		copy_label = _("_Copy");
	}

	open_in_window = (bmk_focus && bmk_selection);
	open_in_tab = (bmk_focus && bmk_selection);
	rename = (bmk_focus && bmk_selection && !bmk_multiple_selection) ||
		 (key_selection && key_focus && key_normal);
	delete = (bmk_focus && bmk_selection) ||
		 (key_selection && key_focus && key_normal);
	properties = (bmk_focus && bmk_selection && !bmk_multiple_selection);
	can_show_in_bookmarks_bar = (bmk_focus && bmk_selection && !bmk_multiple_selection) ||
		 (key_selection && key_focus);

	action_group = editor->priv->action_group;
	action = gtk_action_group_get_action (action_group, "OpenInWindow");
	g_object_set (action, "sensitive", open_in_window, NULL);
	g_object_set (action, "label", open_in_window_label, NULL);
	action = gtk_action_group_get_action (action_group, "OpenInTab");
	g_object_set (action, "sensitive", open_in_tab, NULL);
	g_object_set (action, "label", open_in_tab_label, NULL);
	action = gtk_action_group_get_action (action_group, "Rename");
	g_object_set (action, "sensitive", rename, NULL);
	action = gtk_action_group_get_action (action_group, "Delete");
	g_object_set (action, "sensitive", delete, NULL);
	action = gtk_action_group_get_action (action_group, "Properties");
	g_object_set (action, "sensitive", properties, NULL);
	action = gtk_action_group_get_action (action_group, "Cut");
	g_object_set (action, "sensitive", cut, NULL);
	action = gtk_action_group_get_action (action_group, "Copy");
	g_object_set (action, "sensitive", copy, NULL);
	g_object_set (action, "label", copy_label, NULL);
	action = gtk_action_group_get_action (action_group, "Paste");
	g_object_set (action, "sensitive", paste, NULL);
	action = gtk_action_group_get_action (action_group, "SelectAll");
	g_object_set (action, "sensitive", select_all, NULL);
	action = gtk_action_group_get_action (action_group, "ShowInBookmarksBar");
	g_object_set (action, "sensitive", can_show_in_bookmarks_bar, NULL);

	g_signal_handlers_block_by_func
		(G_OBJECT (GTK_TOGGLE_ACTION (action)),
		 G_CALLBACK (cmd_show_in_bookmarks_bar),
		 editor);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), show_in_bookmarks_bar);
	g_signal_handlers_unblock_by_func
		(G_OBJECT (GTK_TOGGLE_ACTION (action)),
		 G_CALLBACK (cmd_show_in_bookmarks_bar),
		 editor);
}

static gboolean
view_focus_cb (EphyNodeView *view,
              GdkEventFocus *event,
              EphyBookmarksEditor *editor)
{
       ephy_bookmarks_editor_update_menu (editor);

       return FALSE;
}

static void
add_focus_monitor (EphyBookmarksEditor *editor, GtkWidget *widget)
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
remove_focus_monitor (EphyBookmarksEditor *editor, GtkWidget *widget)
{
       g_signal_handlers_disconnect_by_func (G_OBJECT (widget),
                                             G_CALLBACK (view_focus_cb),
                                             editor);
}

static gboolean
ephy_bookmarks_editor_show_popup_cb (GtkWidget *view,
				     EphyBookmarksEditor *editor)
{
	GtkWidget *widget;

	widget = gtk_ui_manager_get_widget (editor->priv->ui_merge,
					    "/EphyBookmarkEditorPopup");
	gtk_menu_popup (GTK_MENU (widget), NULL, NULL, NULL, NULL, 2,
			gtk_get_current_event_time ());

	return TRUE;
}

static void
ephy_bookmarks_editor_dispose (GObject *object)
{
	EphyBookmarksEditor *editor;
	GList *selection;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EPHY_IS_BOOKMARKS_EDITOR (object));

	editor = EPHY_BOOKMARKS_EDITOR (object);

	g_return_if_fail (editor->priv != NULL);

	if (editor->priv->key_view != NULL)
	{
		remove_focus_monitor (editor, editor->priv->key_view);
		remove_focus_monitor (editor, editor->priv->bm_view);
		remove_focus_monitor (editor, editor->priv->search_entry);

		selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->key_view));
		if (selection == NULL || selection->data == NULL)
		{
			editor->priv->key_view = NULL;
			G_OBJECT_CLASS (parent_class)->dispose (object);
			return;
		}

		g_list_free (selection);

		editor->priv->key_view = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
bookmarks_filter (EphyBookmarksEditor *editor,
	          EphyNode *keyword)
{
	ephy_node_filter_empty (editor->priv->bookmarks_filter);
	ephy_node_filter_add_expression (editor->priv->bookmarks_filter,
				         ephy_node_filter_expression_new (EPHY_NODE_FILTER_EXPRESSION_HAS_PARENT,
								          keyword),
				         0);
	ephy_node_filter_done_changing (editor->priv->bookmarks_filter);
}

static gboolean
key_pressed_cb (EphyNodeView *view,
		GdkEventKey *event,
		EphyBookmarksEditor *editor)
{
	if (event->keyval == GDK_Delete || event->keyval == GDK_KP_Delete)
	{
		cmd_delete (NULL, editor);
		return TRUE;
	} 

	return FALSE;
}

static void
keyword_node_selected_cb (EphyNodeView *view,
			  EphyNode *node,
			  EphyBookmarksEditor *editor)
{
	EphyNode *bookmarks;

	if (node == NULL)
	{
		bookmarks = ephy_bookmarks_get_bookmarks (editor->priv->bookmarks);
		ephy_node_view_select_node (EPHY_NODE_VIEW (editor->priv->key_view), bookmarks);
	}
	else
	{
		ephy_search_entry_clear (EPHY_SEARCH_ENTRY (editor->priv->search_entry));
		bookmarks_filter (editor, node);
	}
}

static gboolean
keyword_node_show_popup_cb (GtkWidget *view, EphyBookmarksEditor *editor)
{
	GtkWidget *widget;

	widget = gtk_ui_manager_get_widget (editor->priv->ui_merge,
					   "/EphyBookmarkKeywordPopup");
	gtk_menu_popup (GTK_MENU (widget), NULL, NULL, NULL, NULL, 2,
			gtk_get_current_event_time ());

	return TRUE;
}

static void
search_entry_search_cb (GtkWidget *entry, const char *search_text, EphyBookmarksEditor *editor)
{
	EphyNode *all;

	g_signal_handlers_block_by_func
		(G_OBJECT (editor->priv->key_view),
		 G_CALLBACK (keyword_node_selected_cb),
		 editor);
	all = ephy_bookmarks_get_bookmarks (editor->priv->bookmarks);
	ephy_node_view_select_node (EPHY_NODE_VIEW (editor->priv->key_view),
				    all);
	g_signal_handlers_unblock_by_func
		(G_OBJECT (editor->priv->key_view),
		 G_CALLBACK (keyword_node_selected_cb),
		 editor);

	ephy_node_filter_empty (editor->priv->bookmarks_filter);
	ephy_node_filter_add_expression (editor->priv->bookmarks_filter,
				         ephy_node_filter_expression_new (EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_CONTAINS,
								          EPHY_NODE_BMK_PROP_TITLE,
								          search_text),
				         0);
	ephy_node_filter_add_expression (editor->priv->bookmarks_filter,
				         ephy_node_filter_expression_new (EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_CONTAINS,
								          EPHY_NODE_BMK_PROP_KEYWORDS,
								          search_text),
				         0);
	ephy_node_filter_done_changing (editor->priv->bookmarks_filter);
}

static GtkWidget *
build_search_box (EphyBookmarksEditor *editor)
{
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *entry;
	char *str;

	box = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (box), 6);
	gtk_widget_show (box);

	entry = ephy_search_entry_new ();
	editor->priv->search_entry = entry;
	gtk_widget_show (entry);
	g_signal_connect (G_OBJECT (entry), "search",
			  G_CALLBACK (search_entry_search_cb),
			  editor);
	add_entry_monitor (editor, entry);
	add_focus_monitor (editor, entry);

	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	str = g_strconcat ("<b>", _("_Search:"), "</b>", NULL);
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), str);
	g_free (str);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (box),
			    label, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (box),
			    entry, TRUE, TRUE, 0);

	return box;
}

static void
add_widget (GtkUIManager *merge, GtkWidget *widget, EphyBookmarksEditor *editor)
{
	gtk_box_pack_start (GTK_BOX (editor->priv->main_vbox),
			    widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
}

static gboolean
delete_event_cb (EphyBookmarksEditor *editor)
{
	gtk_widget_hide (GTK_WIDGET (editor));

	return TRUE;
}

static void
node_dropped_cb (EphyNodeView *view, EphyNode *node,
		 GList *nodes, EphyBookmarksEditor *editor)
{
	GList *l;

	for (l = nodes; l != NULL; l = l->next)
	{
		const char *url = (const char *) l->data;
		EphyNode *bmk;

		bmk = ephy_bookmarks_find_bookmark (editor->priv->bookmarks, url);

		if (bmk != NULL)
		{
			ephy_bookmarks_set_keyword (editor->priv->bookmarks, node, bmk);
		}
	}
}

static void
provide_favicon (EphyNode *node, GValue *value, gpointer user_data)
{
	EphyFaviconCache *cache;
	const char *icon_location;
	GdkPixbuf *pixbuf = NULL;

	cache = EPHY_FAVICON_CACHE
		(ephy_embed_shell_get_favicon_cache (EPHY_EMBED_SHELL (ephy_shell)));
	icon_location = ephy_node_get_property_string
		(node, EPHY_NODE_BMK_PROP_ICON);

	LOG ("Get favicon for %s", icon_location ? icon_location : "None")

	if (icon_location)
	{
		pixbuf = ephy_favicon_cache_get (cache, icon_location);
	}

	g_value_init (value, GDK_TYPE_PIXBUF);
	g_value_take_object (value, pixbuf);
}

static void
view_selection_changed_cb (GtkWidget *view, EphyBookmarksEditor *editor)
{
	ephy_bookmarks_editor_update_menu (editor);
}

static void
provide_keyword_uri (EphyNode *node, GValue *value, gpointer data)
{
	EphyBookmarks *bookmarks = EPHY_BOOKMARKS_EDITOR (data)->priv->bookmarks;
	char *uri;

	uri = ephy_bookmarks_get_topic_uri (bookmarks, node);

	g_value_init (value, G_TYPE_STRING);
	g_value_set_string (value, uri);
	g_free (uri);
}

static int
get_details_value (EphyBookmarksEditor *editor)
{
	int value;
	GSList *svalues;

	svalues = eel_gconf_get_string_list (CONF_BOOKMARKS_VIEW_DETAILS);

	if (svalues &&
	    g_slist_find_custom (svalues, "title", (GCompareFunc)strcmp) &&
	    g_slist_find_custom (svalues, "address", (GCompareFunc)strcmp))
	{
		value = VIEW_TITLE_AND_ADDRESS;
	}
	else
	{
		value = VIEW_TITLE;
	}

	g_slist_foreach (svalues, (GFunc) g_free, NULL);
	g_slist_free (svalues);

	return value;
}

static void
ephy_bookmarks_editor_construct (EphyBookmarksEditor *editor)
{
	GtkTreeSelection *selection;
	GtkWidget *hpaned, *vbox;
	GtkWidget *bm_view, *key_view;
	GtkWidget *scrolled_window;
	EphyNode *node;
	GtkUIManager *ui_merge;
	GtkActionGroup *action_group;
	GdkPixbuf *icon;
	int col_id, details_value;

	gtk_window_set_title (GTK_WINDOW (editor), _("Bookmarks"));

	icon = gtk_widget_render_icon (GTK_WIDGET (editor), 
				       EPHY_STOCK_BOOKMARKS,
				       GTK_ICON_SIZE_MENU,
				       NULL);
	gtk_window_set_icon (GTK_WINDOW(editor), icon);

	g_signal_connect (editor, "delete_event",
			  G_CALLBACK (delete_event_cb), NULL);

	editor->priv->main_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (editor->priv->main_vbox);
	gtk_container_add (GTK_CONTAINER (editor), editor->priv->main_vbox);

	ui_merge = gtk_ui_manager_new ();
	g_signal_connect (ui_merge, "add_widget", G_CALLBACK (add_widget), editor);
	action_group = gtk_action_group_new ("PopupActions");
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group, ephy_bookmark_popup_entries,
				      ephy_bookmark_popup_n_entries, editor);
	gtk_action_group_add_toggle_actions (action_group,
					     ephy_bookmark_popup_toggle_entries,
					     ephy_bookmark_popup_n_toggle_entries,
					     editor);

	details_value = get_details_value (editor);
	gtk_action_group_add_radio_actions (action_group,
					    ephy_bookmark_radio_entries,
					    ephy_bookmark_n_radio_entries,
					    details_value,
					    G_CALLBACK (cmd_view_columns), 
					    editor);
	gtk_ui_manager_insert_action_group (ui_merge,
					    action_group, 0);
	gtk_ui_manager_add_ui_from_file (ui_merge,
				         ephy_file ("epiphany-bookmark-editor-ui.xml"),
				         NULL);
	gtk_window_add_accel_group (GTK_WINDOW (editor), 
				    gtk_ui_manager_get_accel_group (ui_merge));
	gtk_ui_manager_ensure_update (ui_merge);

	editor->priv->ui_merge = ui_merge;
	editor->priv->action_group = action_group;

	hpaned = gtk_hpaned_new ();
	gtk_container_set_border_width (GTK_CONTAINER (hpaned), 0);
	gtk_box_pack_end (GTK_BOX (editor->priv->main_vbox), hpaned,
			  TRUE, TRUE, 0);
	gtk_widget_show (hpaned);

	g_assert (editor->priv->bookmarks);

	node = ephy_bookmarks_get_keywords (editor->priv->bookmarks);

	scrolled_window = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
					"hadjustment", NULL,
					"vadjustment", NULL,
					"hscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"vscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"shadow_type", GTK_SHADOW_IN,
					NULL);
	gtk_paned_pack1 (GTK_PANED (hpaned), GTK_WIDGET (scrolled_window), FALSE, TRUE);
	gtk_widget_show (scrolled_window);

	/* Keywords View */
	key_view = ephy_node_view_new (node, NULL);
	add_focus_monitor (editor, key_view);
	col_id = ephy_node_view_add_data_column (EPHY_NODE_VIEW (key_view),
					         G_TYPE_STRING, -1,
						 provide_keyword_uri, editor);
	ephy_node_view_enable_drag_source (EPHY_NODE_VIEW (key_view),
					   topic_drag_types,
				           n_topic_drag_types, col_id);
	ephy_node_view_enable_drag_dest (EPHY_NODE_VIEW (key_view),
			                 topic_drag_dest_types,
			                 n_topic_drag_dest_types);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (key_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	g_signal_connect (G_OBJECT (selection),
			  "changed",
			  G_CALLBACK (view_selection_changed_cb),
			  editor);
	ephy_node_view_add_column (EPHY_NODE_VIEW (key_view), _("Topics"),
				   G_TYPE_STRING,
				   EPHY_NODE_KEYWORD_PROP_NAME,
				   EPHY_NODE_KEYWORD_PROP_PRIORITY,
				   EPHY_NODE_VIEW_AUTO_SORT |
				   EPHY_NODE_VIEW_EDITABLE |
				   EPHY_NODE_VIEW_SEARCHABLE, NULL);
	gtk_container_add (GTK_CONTAINER (scrolled_window), key_view);
	gtk_widget_set_size_request (key_view, 130, -1);
	gtk_widget_show (key_view);
	editor->priv->key_view = key_view;
	g_signal_connect (G_OBJECT (key_view),
			  "key_press_event",
			  G_CALLBACK (key_pressed_cb),
			  editor);
	g_signal_connect (G_OBJECT (key_view),
			  "node_selected",
			  G_CALLBACK (keyword_node_selected_cb),
			  editor);
	g_signal_connect (G_OBJECT (key_view),
			  "node_dropped",
			  G_CALLBACK (node_dropped_cb),
			  editor);
	g_signal_connect (G_OBJECT (key_view),
			  "popup_menu",
			  G_CALLBACK (keyword_node_show_popup_cb),
			  editor);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_paned_pack2 (GTK_PANED (hpaned), vbox, TRUE, TRUE);
	gtk_widget_show (vbox);

	gtk_box_pack_start (GTK_BOX (vbox),
			    build_search_box (editor),
			    FALSE, FALSE, 0);
	add_focus_monitor (editor, editor->priv->search_entry);

	scrolled_window = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
					"hadjustment", NULL,
					"vadjustment", NULL,
					"hscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"vscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"shadow_type", GTK_SHADOW_IN,
					NULL);
	gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show (scrolled_window);

	node = ephy_bookmarks_get_bookmarks (editor->priv->bookmarks);
	editor->priv->bookmarks_filter = ephy_node_filter_new ();

	/* Bookmarks View */
	bm_view = ephy_node_view_new (node, editor->priv->bookmarks_filter);
	add_focus_monitor (editor, bm_view);
	col_id = ephy_node_view_add_data_column (EPHY_NODE_VIEW (bm_view),
					         G_TYPE_STRING,
					         EPHY_NODE_BMK_PROP_LOCATION,
						 NULL, NULL);
	ephy_node_view_enable_drag_source (EPHY_NODE_VIEW (bm_view),
					   bmk_drag_types,
				           n_bmk_drag_types,
					   col_id);
	editor->priv->title_col = ephy_node_view_add_column
				  (EPHY_NODE_VIEW (bm_view), _("Title"),
				   G_TYPE_STRING, EPHY_NODE_BMK_PROP_TITLE, -1,
				   EPHY_NODE_VIEW_AUTO_SORT |
				   EPHY_NODE_VIEW_EDITABLE |
				   EPHY_NODE_VIEW_SEARCHABLE,
				   provide_favicon);
	editor->priv->address_col = ephy_node_view_add_column
				  (EPHY_NODE_VIEW (bm_view), _("Address"),
				   G_TYPE_STRING, EPHY_NODE_BMK_PROP_LOCATION,
				   -1, 0, NULL);
	gtk_container_add (GTK_CONTAINER (scrolled_window), bm_view);
	gtk_widget_show (bm_view);
	editor->priv->bm_view = bm_view;
	g_signal_connect (G_OBJECT (bm_view),
			  "key_press_event",
			  G_CALLBACK (key_pressed_cb),
			  editor);
	g_signal_connect (G_OBJECT (bm_view),
			  "node_activated",
			  G_CALLBACK (ephy_bookmarks_editor_node_activated_cb),
			  editor);
	g_signal_connect (G_OBJECT (bm_view),
			  "popup_menu",
			  G_CALLBACK (ephy_bookmarks_editor_show_popup_cb),
			  editor);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (bm_view));
	g_signal_connect (G_OBJECT (selection),
			  "changed",
			  G_CALLBACK (view_selection_changed_cb),
			  editor);

	ephy_state_add_window (GTK_WIDGET(editor),
			       "bookmarks_editor",
		               450, 400,
			       EPHY_STATE_WINDOW_SAVE_SIZE | EPHY_STATE_WINDOW_SAVE_POSITION);
	ephy_state_add_paned  (GTK_WIDGET (hpaned),
			       "bookmarks_paned",
		               130);

	set_columns_visibility (editor, details_value);
}

void
ephy_bookmarks_editor_set_parent (EphyBookmarksEditor *ebe,
				  GtkWidget *window)
{
	if (ebe->priv->window)
	{
		g_object_remove_weak_pointer
                        (G_OBJECT(ebe->priv->window),
                         (gpointer *)&ebe->priv->window);
	}

	ebe->priv->window = window;

	g_object_add_weak_pointer
                        (G_OBJECT(ebe->priv->window),
                         (gpointer *)&ebe->priv->window);

}

GtkWidget *
ephy_bookmarks_editor_new (EphyBookmarks *bookmarks)
{
	EphyBookmarksEditor *editor;

	g_assert (bookmarks != NULL);

	editor = EPHY_BOOKMARKS_EDITOR (g_object_new
			(EPHY_TYPE_BOOKMARKS_EDITOR,
			 "bookmarks", bookmarks,
			 NULL));

	ephy_bookmarks_editor_construct (editor);

	return GTK_WIDGET (editor);
}

static void
ephy_bookmarks_editor_set_property (GObject *object,
		                    guint prop_id,
		                    const GValue *value,
		                    GParamSpec *pspec)
{
	EphyBookmarksEditor *editor = EPHY_BOOKMARKS_EDITOR (object);

	switch (prop_id)
	{
	case PROP_BOOKMARKS:
		editor->priv->bookmarks = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_bookmarks_editor_get_property (GObject *object,
		                    guint prop_id,
		                    GValue *value,
		                    GParamSpec *pspec)
{
	EphyBookmarksEditor *editor = EPHY_BOOKMARKS_EDITOR (object);

	switch (prop_id)
	{
	case PROP_BOOKMARKS:
		g_value_set_object (value, editor->priv->bookmarks);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
toolbar_items_changed_cb (EphyToolbarsModel *model,
			  int toolbar_position,
			  int position,
			  EphyBookmarksEditor *editor)
{
	ephy_bookmarks_editor_update_menu (editor);
}

static void
ephy_bookmarks_editor_init (EphyBookmarksEditor *editor)
{
	editor->priv = EPHY_BOOKMARKS_EDITOR_GET_PRIVATE (editor);

	editor->priv->props_dialogs = g_hash_table_new (g_direct_hash,
                                                        g_direct_equal);
	editor->priv->tb_model = EPHY_TOOLBARS_MODEL
		(ephy_shell_get_toolbars_model (ephy_shell, FALSE));

	g_signal_connect (editor->priv->tb_model, "item_added",
			  G_CALLBACK (toolbar_items_changed_cb), editor);
	g_signal_connect (editor->priv->tb_model, "item_removed",
			  G_CALLBACK (toolbar_items_changed_cb), editor);
}
