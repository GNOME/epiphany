/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti <mpeseng@tin.it>
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

#include "config.h"

#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkhpaned.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkactiongroup.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtktoggleaction.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreemodelsort.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkradioaction.h>
#include <gtk/gtkclipboard.h>
#include <gtk/gtkmain.h>
#include <gtk/gtktreemodelsort.h>
#include <gtk/gtkmessagedialog.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <string.h>

#include "ephy-bookmarks-editor.h"
#include "ephy-bookmarks-import.h"
#include "ephy-bookmarks-export.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-bookmark-action.h"
#include "ephy-topic-action.h"
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
#include "ephy-favicon-cache.h"
#include "eel-gconf-extensions.h"
#include "ephy-debug.h"
#include "egg-toolbars-model.h"
#include "ephy-prefs.h"

static const GtkTargetEntry topic_drag_dest_types [] =
{
	{ EPHY_DND_URI_LIST_TYPE,   0, 0 }
};

static const GtkTargetEntry bmk_drag_types [] =
{
	{ EPHY_DND_URL_TYPE,        0, 0 },
	{ EPHY_DND_URI_LIST_TYPE,   0, 1 },
	{ EPHY_DND_TEXT_TYPE,       0, 2 }
};

static const GtkTargetEntry topic_drag_types [] =
{
	{ EPHY_DND_TOPIC_TYPE,      0, 0 }
};

static const struct
{
	const char *name;
	const char *extension;
}
export_formats [] = 
/* Don't add or reoder those entries without updating export_dialog_response_cb too! */
{
	{ N_("Epiphany (RDF)"), "rdf" },
	{ N_("Mozilla (HTML)"), "html" }
};

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
static void cmd_toolbar			  (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_delete			  (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_bookmark_properties	  (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_bookmarks_import	  (GtkAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_bookmarks_export	  (GtkAction *action,
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

#define RESERVED_STRING N_("Remove from this topic")

struct _EphyBookmarksEditorPrivate
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

	GtkTreeViewColumn *title_col;
	GtkTreeViewColumn *address_col;
};

enum
{
	PROP_0,
	PROP_BOOKMARKS
};

static GObjectClass *parent_class = NULL;

static const GtkActionEntry ephy_bookmark_popup_entries [] = {
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
	{ "OpenInWindow", GTK_STOCK_OPEN, N_("Open in New _Window"), "<control>O",
	  N_("Open the selected bookmark in a new window"), 
	  G_CALLBACK (cmd_open_bookmarks_in_browser) },
	{ "OpenInTab", STOCK_NEW_TAB, N_("Open in New _Tab"), "<shift><control>O",
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
	{ "Export", NULL, N_("_Export Bookmarks..."), NULL,
	  N_("Export bookmarks to a file"), 
	  G_CALLBACK (cmd_bookmarks_export) },	  
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

static const GtkToggleActionEntry ephy_bookmark_toggle_entries [] = {
	{ "ShowOnToolbar", NULL, N_("_Show on Toolbar"), NULL,
	  N_("Show the selected bookmark on a toolbar"), 
	  G_CALLBACK (cmd_toolbar), FALSE },
};

enum
{
	VIEW_TITLE,
	VIEW_TITLE_AND_ADDRESS
};

static const GtkRadioActionEntry ephy_bookmark_radio_entries [] =
{
	/* View Menu */
	{ "ViewTitle", NULL, N_("_Title"), NULL,
	  N_("Show only the title column"), VIEW_TITLE },
	{ "ViewTitleAddress", NULL, N_("T_itle and Address"), NULL,
	  N_("Show both the title and address columns"),
	  VIEW_TITLE_AND_ADDRESS } 
};

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
	ephy_node_view_edit (EPHY_NODE_VIEW (editor->priv->key_view), TRUE);
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
		ephy_node_view_edit (EPHY_NODE_VIEW (editor->priv->bm_view), FALSE);
	}
	else if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->key_view)))
	{
		ephy_node_view_edit (EPHY_NODE_VIEW (editor->priv->key_view), FALSE);
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

static GtkWidget*
delete_topic_dialog_construct (GtkWindow *parent,
			       const char *topic)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_CANCEL,
					 _("Delete topic “%s”?"),
					 topic);
	
	gtk_window_set_title (GTK_WINDOW (dialog), _("Delete this topic?"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
			_("Deleting this topic will cause all its bookmarks to become "
			"uncategorized, unless they also belong to other topics. "
			"The bookmarks will not be deleted."));
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Delete Topic"), GTK_RESPONSE_ACCEPT);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

	gtk_window_group_add_window (GTK_WINDOW (parent)->group, GTK_WINDOW (dialog));

	return dialog;
}

static gint
get_bookmarks_bar (EggToolbarsModel *model)
{
	gint tpos;
	const char *tname;
	
	for (tpos = 0; tpos < egg_toolbars_model_n_toolbars (model); tpos++)
	{
		tname = egg_toolbars_model_toolbar_nth (model, tpos);

		if (tname != NULL &&
		    strcmp (tname, "BookmarksBar") == 0)
			break;
	}
	
	if (tpos == egg_toolbars_model_n_toolbars (model))
	{		
		tpos = egg_toolbars_model_add_toolbar (model, -1, "BookmarksBar");
	}
	
	return tpos;
}

static void
cmd_toolbar (GtkAction *action,
	     EphyBookmarksEditor *editor)
{
	EggToolbarsModel *model;
	EphyNode *node;
	gboolean show;
	gint flags, tpos = 0;
	GList *selection;
	GList *l;
	
	model = EGG_TOOLBARS_MODEL (ephy_shell_get_toolbars_model (ephy_shell_get_default (), FALSE));
		
	if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->bm_view)))
	{
		char name[EPHY_BOOKMARK_ACTION_NAME_BUFFER_SIZE];

		selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->bm_view));
		
		node = selection->data;

		EPHY_BOOKMARK_ACTION_NAME_PRINTF (name, node);

		flags = egg_toolbars_model_get_name_flags (model, name);
		show = ((flags & EGG_TB_MODEL_NAME_USED) == 0);
		
		if (show)
		{
			tpos = get_bookmarks_bar (model);
		}

		for (l = selection; l; l = l->next)
		{
			node = l->data;

			EPHY_BOOKMARK_ACTION_NAME_PRINTF (name, node);

			flags = egg_toolbars_model_get_name_flags (model, name);
			if(show && ((flags & EGG_TB_MODEL_NAME_USED) == 0))
			{
				egg_toolbars_model_add_item (model, tpos, -1, name);
			}
			else if(!show && ((flags & EGG_TB_MODEL_NAME_USED) != 0))
			{
				egg_toolbars_model_delete_item (model, name);
			}
		}

		g_list_free (selection);
	}
	else if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->key_view)))
	{
		char name[EPHY_TOPIC_ACTION_NAME_BUFFER_SIZE];

		selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->key_view));
	  
		node = selection->data;

		EPHY_TOPIC_ACTION_NAME_PRINTF (name, node);

		flags = egg_toolbars_model_get_name_flags (model, name);
		show = ((flags & EGG_TB_MODEL_NAME_USED) == 0);

		if (show)
		{
			tpos = get_bookmarks_bar (model);
		}

		for (l = selection; l; l = l->next)
		{
			node = l->data;

			EPHY_TOPIC_ACTION_NAME_PRINTF (name, node);

			flags = egg_toolbars_model_get_name_flags (model, name);
			if(show && ((flags & EGG_TB_MODEL_NAME_USED) == 0))
			{
				egg_toolbars_model_add_item (model, tpos, -1, name);
			}
			else if(!show && ((flags & EGG_TB_MODEL_NAME_USED) != 0))
			{
				egg_toolbars_model_delete_item (model, name);
			}
		}

		g_list_free (selection);
	}
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
			GtkWidget *dialog;
			const char *title;
			int response;
			GPtrArray *children;
			
			children = ephy_node_get_children(node);
			
			/* Do not warn if the topic is empty */
			if (children->len == 0)
			{
				ephy_node_view_remove (EPHY_NODE_VIEW (editor->priv->key_view));
			}
			else
			{
				title = ephy_node_get_property_string (node, EPHY_NODE_KEYWORD_PROP_NAME);
				dialog = delete_topic_dialog_construct (GTK_WINDOW (editor), title);
				
				response = gtk_dialog_run (GTK_DIALOG (dialog));

				gtk_widget_destroy (dialog);

				if (response == GTK_RESPONSE_ACCEPT)
				{
					ephy_node_view_remove (EPHY_NODE_VIEW (editor->priv->key_view));
				}
			}
		}
		g_list_free (selected);
	}
}

static GSList *
add_bookmarks_files (const char *dir,
		     const char *filename,
		     int max_depth)
{
	GSList *list;
	char *path;

	path = g_build_filename (g_get_home_dir (), dir, NULL);
	list = ephy_file_find  (path, filename, max_depth);
	g_free (path);

	return list;
}

static void
add_bookmarks_source (const char *file,
		      GtkListStore *store)
{
	GtkTreeIter iter;
	char **path;
	char *description = NULL;
	int len, i;

	path = g_strsplit (file, G_DIR_SEPARATOR_S, -1);
	g_return_if_fail (path != NULL);

	len = g_strv_length (path);

	for (i = len - 2; i >= 0 && description == NULL; --i)
	{
		const char *p = (const char *) path[i];

		g_return_if_fail (p != NULL);

		if (strcmp (p, "firefox") == 0)
		{
			const char *profile = NULL, *dot;

			if (path[i+1] != NULL)
			{
				dot = strchr (path[i+1], '.');
				profile = dot ? dot + 1 : path[i+1];
			}

			if (profile != NULL && strcmp (profile, "default") != 0)
			{
				/* FIXME: proper i18n after freeze */
				description = g_strdup_printf ("%s “%s”", _("Firefox"), profile);
			}
			else
			{
				description = g_strdup (_("Firefox"));
			}
		}
		else if (strcmp (p, ".firefox") == 0)
		{
			description = g_strdup (_("Firebird"));
		}
		else if (strcmp (p, ".phoenix") == 0)
		{
			description = g_strdup (_("Firebird"));
		}
		else if (strcmp (p, ".mozilla") == 0)
		{
			/* Translators: The %s is the name of a Mozilla profile. */
			description = g_strdup_printf (_("Mozilla “%s” profile"), path[i+1]);
		}
		else if (strcmp (p, ".galeon") == 0)
		{
			description = g_strdup (_("Galeon"));
		}
		else if (strcmp (p, "konqueror") == 0)
		{
			description = g_strdup (_("Konqueror"));
		}
	}

	if (description != NULL)
	{
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, description, 1, file, -1);

		g_free (description);
	}

	g_strfreev (path);
}

static void
import_bookmarks (EphyBookmarksEditor *editor,
		  const char *filename)
{
	if (ephy_bookmarks_import (editor->priv->bookmarks, filename) == FALSE)
	{
		GtkWidget *dialog;
		char *basename;

		basename = g_filename_display_basename (filename);
		dialog = gtk_message_dialog_new (GTK_WINDOW (editor),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("Import failed"));

		gtk_window_set_title (GTK_WINDOW (dialog), _("Import Failed"));
		gtk_message_dialog_format_secondary_text
			(GTK_MESSAGE_DIALOG (dialog),
			 _("The bookmarks from “%s” could not be imported "
			   "because the file is corrupted or of an "
			   "unsupported type."),
			 basename);

		gtk_window_group_add_window (GTK_WINDOW (editor)->group,
					     GTK_WINDOW (dialog));

		gtk_dialog_run (GTK_DIALOG (dialog));

		g_free (basename);
		gtk_widget_destroy (dialog);
	}
}

static void
import_from_file_response_cb (GtkWidget *dialog,
			      int response,
			      EphyBookmarksEditor *editor)
{
	char *filename;

	gtk_widget_hide (dialog);

	if (response == GTK_RESPONSE_ACCEPT)
	{
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

		if (filename != NULL)
		{
			import_bookmarks (editor, filename);			

			g_free (filename);
		}
	}

	gtk_widget_destroy (dialog);
}

static void
import_dialog_response_cb (GtkDialog *dialog,
			   int response,
			   EphyBookmarksEditor *editor)
{
	if (response == GTK_RESPONSE_OK)
	{
		GtkTreeIter iter;
		const char *filename;
		GtkWidget *combo;
		GtkTreeModel *model;
		GValue value = { 0, };

		combo = g_object_get_data (G_OBJECT (dialog), "combo_box");
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
		gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter);
		gtk_tree_model_get_value (model, &iter, 1, &value);
		filename = g_value_get_string (&value);

		if (filename == NULL)
		{
			EphyFileChooser *dialog;
			GtkFileFilter *filter;

			dialog = ephy_file_chooser_new (_("Import Bookmarks from File"),
							GTK_WIDGET (editor),
							GTK_FILE_CHOOSER_ACTION_OPEN,
							NULL, EPHY_FILE_FILTER_NONE);

			ephy_file_chooser_add_mime_filter
				(dialog,
				 _("Firefox/Mozilla bookmarks"),
				 "application/x-mozilla-bookmarks", NULL);

			ephy_file_chooser_add_mime_filter
				(dialog, _("Galeon/Konqueror bookmarks"),
				 "application/x-xbel", NULL);

			ephy_file_chooser_add_mime_filter
				(dialog, _("Epiphany bookmarks"),
				 "text/rdf", "application/rdf+xml", NULL);

			filter = ephy_file_chooser_add_pattern_filter (dialog,
							      _("All files"),
							      "*", NULL);

			gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog),
						     filter);

			g_signal_connect (dialog, "response",
					  G_CALLBACK (import_from_file_response_cb), editor);

			gtk_widget_show (GTK_WIDGET (dialog));
		}
		else
		{
			import_bookmarks (editor, filename);
		}

		g_value_unset (&value);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
export_format_combo_changed_cb (GtkComboBox *combo,
				GtkFileChooser *chooser)
{
	char *filename, *basename, *dot, *newname;
	int i, format;

	filename = gtk_file_chooser_get_filename (chooser);
	if (filename == NULL) return;

	basename = g_path_get_basename (filename);
	if (basename == NULL || basename[0] == '\0')
	{
		g_free (filename);
		g_free (basename);
		return;
	}

	format = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
	g_return_if_fail (format >= 0 && format < G_N_ELEMENTS (export_formats));

	dot = strrchr (basename, '.');
	if (dot != NULL)
	{
		for (i = 0; i < G_N_ELEMENTS (export_formats); ++i)
		{
			if (strcmp (dot + 1, export_formats[i].extension) == 0)
			{
				*dot = '\0';
				break;
			}
		}
	}

	newname = g_strconcat (basename, ".",
			       export_formats[format].extension,
			       NULL);

	gtk_file_chooser_set_current_name (chooser, newname);

	g_free (filename);
	g_free (basename);
	g_free (newname);
}

static void
export_dialog_response_cb (GtkWidget *dialog,
			   int response,
			   EphyBookmarksEditor *editor)
{
	GtkWidget *combo;
	char *filename;
	int format;

	if (response != GTK_RESPONSE_ACCEPT)
	{
		gtk_widget_destroy (dialog);
		return;
	}

	filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
	if (filename == NULL) return;
	
	if (!ephy_gui_check_location_writable (GTK_WIDGET (dialog), filename))
	{
		g_free (filename);
		return;
	}

	combo = g_object_get_data (G_OBJECT (dialog), "format-combo");
	g_return_if_fail (combo != NULL);

	format = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
	g_return_if_fail (format >= 0 && format < G_N_ELEMENTS (export_formats));

	gtk_widget_destroy (dialog);

	/* 0 for ephy RDF format, 1 for mozilla HTML format */
	if (format == 0)
	{
		ephy_bookmarks_export_rdf (editor->priv->bookmarks, filename);
	}
	else if (format == 1)
	{
		ephy_bookmarks_export_mozilla (editor->priv->bookmarks, filename);
	}

	g_free (filename);
}

static void
cmd_bookmarks_export (GtkAction *action,
		      EphyBookmarksEditor *editor)
{
	GtkWidget *dialog, *hbox, *label, *combo;
	int format;
	char *filename;

	dialog = GTK_WIDGET (ephy_file_chooser_new (_("Export Bookmarks"),
		GTK_WIDGET (editor),
		GTK_FILE_CHOOSER_ACTION_SAVE,
		NULL,
		EPHY_FILE_FILTER_NONE));

	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

	gtk_file_chooser_set_current_folder
		(GTK_FILE_CHOOSER (dialog), g_get_home_dir ());
	
	filename = g_strdup_printf ("%s.%s", _("Bookmarks"), export_formats[0].extension);
	gtk_file_chooser_set_current_name
		(GTK_FILE_CHOOSER (dialog), filename);
	g_free(filename);

	/* Make a format selection combo & label */
	label = gtk_label_new_with_mnemonic (_("File f_ormat:"));

	combo = gtk_combo_box_new_text ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

	for (format = 0; format < G_N_ELEMENTS (export_formats); ++format)
	{
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo),
					   _(export_formats[format].name));
	}

	g_object_set_data (G_OBJECT (dialog), "format-combo", combo);
	g_signal_connect (combo, "changed",
			  G_CALLBACK (export_format_combo_changed_cb), dialog);
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
	gtk_widget_show_all (hbox);
	
	gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dialog), hbox);

	gtk_window_group_add_window (GTK_WINDOW (editor)->group, GTK_WINDOW (dialog));

	g_signal_connect (dialog, "response",
			  G_CALLBACK (export_dialog_response_cb), editor);
	gtk_widget_show (dialog);
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
	GtkTreeModel *sortmodel;
	GSList *files;

	dialog = gtk_dialog_new_with_buttons (_("Import Bookmarks"),
					      GTK_WINDOW (editor),
					      GTK_DIALOG_DESTROY_WITH_PARENT |
					      GTK_DIALOG_NO_SEPARATOR,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      _("I_mport"),
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

	label = gtk_label_new (_("Import bookmarks from:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	store = GTK_LIST_STORE (gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING));

	files = add_bookmarks_files (FIREFOX_BOOKMARKS_DIR_0, "bookmarks.html", 2);
	files = g_slist_concat (add_bookmarks_files (FIREFOX_BOOKMARKS_DIR_1, "bookmarks.html", 2), files);
	/* FIREFOX_BOOKMARKS_DIR_2 is subdir of MOZILLA_BOOKMARKS_DIR, so don't search it twice */
	files = g_slist_concat (add_bookmarks_files (MOZILLA_BOOKMARKS_DIR, "bookmarks.html", 2), files);
	files = g_slist_concat (add_bookmarks_files (GALEON_BOOKMARKS_DIR, "bookmarks.xbel", 0), files);
	files = g_slist_concat (add_bookmarks_files (KDE_BOOKMARKS_DIR, "bookmarks.xml", 0), files);
	
	g_slist_foreach (files, (GFunc) add_bookmarks_source, store);
	g_slist_foreach (files, (GFunc) g_free, NULL);
	g_slist_free (files);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, _("File"), 1, NULL, -1);

	sortmodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
	g_object_unref (store);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sortmodel), 0, GTK_SORT_ASCENDING);

	combo = gtk_combo_box_new ();
	gtk_combo_box_set_model(GTK_COMBO_BOX (combo), sortmodel);
	g_object_set_data (G_OBJECT (dialog), "combo_box", combo);
	g_object_unref (sortmodel);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell,
					"text", 0,
					NULL);

	gtk_box_pack_start (GTK_BOX (vbox), combo, TRUE, TRUE, 0);
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

	gtk_widget_show (combo);

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

		ephy_bookmarks_ui_show_bookmark (node);
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
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
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

		type = g_type_register_static (GTK_TYPE_WINDOW,
					       "EphyBookmarksEditor",
					       &our_info, 0);
	}

	return type;
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
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY));

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
		GtkWidget **window = &editor->priv->window;
		g_object_remove_weak_pointer
			(G_OBJECT(editor->priv->window),
			 (gpointer *)window);
	}

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
	gboolean key_selection, bmk_selection, single_bmk_selected;
	gboolean key_normal = FALSE;
	gboolean cut, copy, paste, select_all;
	gboolean mutable = TRUE;
	gboolean showtoolbar = FALSE;
	gboolean ontoolbar = FALSE;

	EggToolbarsModel *model;
	GtkActionGroup *action_group;
	GtkAction *action;
	GList *selected;
	GtkWidget *focus_widget;
	int num_bmk_selected;

	LOG ("Update menu sensitivity");

	model = EGG_TOOLBARS_MODEL (ephy_shell_get_toolbars_model 
				    (ephy_shell_get_default(), FALSE));
	
	bmk_focus = ephy_node_view_is_target
		(EPHY_NODE_VIEW (editor->priv->bm_view));
	key_focus = ephy_node_view_is_target
		(EPHY_NODE_VIEW (editor->priv->key_view));
	focus_widget = gtk_window_get_focus (GTK_WINDOW (editor));

	num_bmk_selected = gtk_tree_selection_count_selected_rows
		 (gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->priv->bm_view)));
	bmk_selection = num_bmk_selected > 0;
	single_bmk_selected = num_bmk_selected == 1;

	key_selection = gtk_tree_selection_count_selected_rows
		 (gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->priv->key_view))) > 0;

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
		copy = (bmk_focus && single_bmk_selected);
		paste = FALSE;
		select_all = bmk_focus;
	}

	selected = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->key_view));
	if (key_focus && selected)
	{
		EphyNode *node = selected->data;
		EphyNodePriority priority;
		char name[EPHY_TOPIC_ACTION_NAME_BUFFER_SIZE];

		priority = ephy_node_get_property_int
			(node, EPHY_NODE_KEYWORD_PROP_PRIORITY);
		if (priority == -1) priority = EPHY_NODE_NORMAL_PRIORITY;
		key_normal = (priority == EPHY_NODE_NORMAL_PRIORITY);

		EPHY_TOPIC_ACTION_NAME_PRINTF (name, node);

		ontoolbar = ((egg_toolbars_model_get_name_flags (model, name)
			      & EGG_TB_MODEL_NAME_USED) != 0);

		g_list_free (selected);
	}

	selected = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->bm_view));
	if (bmk_focus && selected)
	{
		EphyNode *node = selected->data;
		char name[EPHY_BOOKMARK_ACTION_NAME_BUFFER_SIZE];

		g_return_if_fail (node != NULL);

		mutable = !ephy_node_get_property_boolean (node, EPHY_NODE_BMK_PROP_IMMUTABLE);
		
		EPHY_BOOKMARK_ACTION_NAME_PRINTF (name, node);

		ontoolbar = ((egg_toolbars_model_get_name_flags (model, name)
			      & EGG_TB_MODEL_NAME_USED) != 0);

		g_list_free (selected);
	}

	open_in_window_label = ngettext ("Open in New _Window",
					 "Open in New _Windows",
					 num_bmk_selected);
	open_in_tab_label = ngettext ("Open in New _Tab",
				      "Open in New _Tabs",
				      num_bmk_selected);

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
	rename = (bmk_focus && single_bmk_selected && mutable) ||
		 (key_selection && key_focus && key_normal);
	delete = (bmk_focus && bmk_selection && mutable) ||
		 (key_selection && key_focus && key_normal);
	properties = bmk_focus && single_bmk_selected && mutable;
	showtoolbar = (bmk_focus && bmk_selection) ||
	              (key_focus && key_selection);

	action_group = editor->priv->action_group;
	action = gtk_action_group_get_action (action_group, "OpenInWindow");
	gtk_action_set_sensitive (action, open_in_window);
	g_object_set (action, "label", open_in_window_label, NULL);
	action = gtk_action_group_get_action (action_group, "OpenInTab");
	gtk_action_set_sensitive (action, open_in_tab);
	g_object_set (action,  "label", open_in_tab_label, NULL);
	action = gtk_action_group_get_action (action_group, "Rename");
	gtk_action_set_sensitive (action, rename);
	action = gtk_action_group_get_action (action_group, "Delete");
	gtk_action_set_sensitive (action, delete);
	action = gtk_action_group_get_action (action_group, "Properties");
	gtk_action_set_sensitive (action, properties);
	action = gtk_action_group_get_action (action_group, "Cut");
	gtk_action_set_sensitive (action, cut);
	action = gtk_action_group_get_action (action_group, "Copy");
	gtk_action_set_sensitive (action, copy);
	g_object_set (action, "label", copy_label, NULL);
	action = gtk_action_group_get_action (action_group, "Paste");
	gtk_action_set_sensitive (action, paste);
	action = gtk_action_group_get_action (action_group, "SelectAll");
	g_object_set (action, "sensitive", select_all, NULL);
	action = gtk_action_group_get_action (action_group, "ShowOnToolbar");
	gtk_action_set_sensitive (action, showtoolbar);
	g_signal_handlers_block_by_func (action, cmd_toolbar, editor);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), ontoolbar);
	g_signal_handlers_unblock_by_func (action, cmd_toolbar, editor);
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
	ephy_node_view_popup (EPHY_NODE_VIEW (view), widget);

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
	EphyBookmarksEditorPrivate *priv = editor->priv;
	GtkAction *action;

	if (event->keyval == GDK_Delete || event->keyval == GDK_KP_Delete)
	{
		action = gtk_action_group_get_action (priv->action_group, "Delete");
		if (gtk_action_get_sensitive (action))
		{
			cmd_delete (NULL, editor);
			return TRUE;
		}
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
	ephy_node_view_popup (EPHY_NODE_VIEW (view), widget);

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
node_dropped_cb (EphyNodeView *view,
		 EphyNode *node,
		 const char * const *uris,
		 EphyBookmarksEditor *editor)
{
	EphyNode *bmk;
	int i;

	g_return_if_fail (uris != NULL);

	for (i = 0; uris[i] != NULL; i++)
	{
		bmk = ephy_bookmarks_find_bookmark (editor->priv->bookmarks, uris[i]);

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

	LOG ("Get favicon for %s", icon_location ? icon_location : "None");

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
	GtkAction *action;
	int col_id, url_col_id, title_col_id, details_value;

	ephy_gui_ensure_window_group (GTK_WINDOW (editor));

	gtk_window_set_title (GTK_WINDOW (editor), _("Bookmarks"));
	gtk_window_set_icon_name (GTK_WINDOW (editor), EPHY_STOCK_BOOKMARKS);

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
				      G_N_ELEMENTS (ephy_bookmark_popup_entries), editor);
	gtk_action_group_add_toggle_actions (action_group, ephy_bookmark_toggle_entries,
					     G_N_ELEMENTS (ephy_bookmark_toggle_entries), editor);

	details_value = get_details_value (editor);
	gtk_action_group_add_radio_actions (action_group,
					    ephy_bookmark_radio_entries,
					    G_N_ELEMENTS (ephy_bookmark_radio_entries),
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
	ephy_node_view_add_column (EPHY_NODE_VIEW (key_view), _("Topics"),
				   G_TYPE_STRING,
				   EPHY_NODE_KEYWORD_PROP_NAME,
				   EPHY_NODE_VIEW_SHOW_PRIORITY |
				   EPHY_NODE_VIEW_EDITABLE |
				   EPHY_NODE_VIEW_SEARCHABLE, NULL, NULL);
	ephy_node_view_enable_drag_source (EPHY_NODE_VIEW (key_view),
					   topic_drag_types,
					   G_N_ELEMENTS (topic_drag_types),
					   col_id, -1);
	ephy_node_view_enable_drag_dest (EPHY_NODE_VIEW (key_view),
					 topic_drag_dest_types,
					 G_N_ELEMENTS (topic_drag_dest_types));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (key_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	g_signal_connect (G_OBJECT (selection),
			  "changed",
			  G_CALLBACK (view_selection_changed_cb),
			  editor);
	ephy_node_view_set_priority (EPHY_NODE_VIEW (key_view),
				     EPHY_NODE_KEYWORD_PROP_PRIORITY);
	ephy_node_view_set_sort (EPHY_NODE_VIEW (key_view), G_TYPE_STRING,
				 EPHY_NODE_KEYWORD_PROP_NAME, GTK_SORT_ASCENDING);
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

	gtk_box_pack_start (GTK_BOX (editor->priv->main_vbox),
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
	title_col_id = ephy_node_view_add_column
				  (EPHY_NODE_VIEW (bm_view), _("Title"),
				   G_TYPE_STRING, EPHY_NODE_BMK_PROP_TITLE,
				   EPHY_NODE_VIEW_EDITABLE |
				   EPHY_NODE_VIEW_SEARCHABLE,
				   provide_favicon, &(editor->priv->title_col));
	url_col_id = ephy_node_view_add_column
				  (EPHY_NODE_VIEW (bm_view), _("Address"),
				   G_TYPE_STRING, EPHY_NODE_BMK_PROP_LOCATION,
				   0, NULL, &(editor->priv->address_col));
	ephy_node_view_enable_drag_source (EPHY_NODE_VIEW (bm_view),
					   bmk_drag_types,
					   G_N_ELEMENTS (bmk_drag_types),
					   url_col_id, title_col_id);
	ephy_node_view_set_sort (EPHY_NODE_VIEW (bm_view), G_TYPE_STRING,
				 EPHY_NODE_BMK_PROP_TITLE, GTK_SORT_ASCENDING);
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
			       450, 400, FALSE,
			       EPHY_STATE_WINDOW_SAVE_SIZE | EPHY_STATE_WINDOW_SAVE_POSITION);
	ephy_state_add_paned  (GTK_WIDGET (hpaned),
			       "bookmarks_paned",
			       130);

	set_columns_visibility (editor, details_value);

	/* Lockdown settings */
	action = gtk_action_group_get_action (action_group, "Export");
	gtk_action_set_sensitive (action,
				  eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_SAVE_TO_DISK) == FALSE);
}

void
ephy_bookmarks_editor_set_parent (EphyBookmarksEditor *ebe,
				  GtkWidget *window)
{
	GtkWidget **w;
	if (ebe->priv->window)
	{
		w = &ebe->priv->window;
		g_object_remove_weak_pointer
			(G_OBJECT(ebe->priv->window),
			 (gpointer *)w);
	}

	ebe->priv->window = window;
	w = &ebe->priv->window;

	g_object_add_weak_pointer
			(G_OBJECT(ebe->priv->window),
			 (gpointer *)w);

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
ephy_bookmarks_editor_init (EphyBookmarksEditor *editor)
{
	editor->priv = EPHY_BOOKMARKS_EDITOR_GET_PRIVATE (editor);
}
