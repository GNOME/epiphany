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
 */

#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gdk/gdkkeysyms.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <string.h>

#include "ephy-bookmarks-editor.h"
#include "ephy-bookmark-properties.h"
#include "ephy-node-view.h"
#include "ephy-window.h"
#include "ephy-dnd.h"
#include "ephy-prefs.h"
#include "ephy-shell.h"
#include "eel-gconf-extensions.h"
#include "ephy-file-helpers.h"
#include "egg-action-group.h"
#include "egg-toggle-action.h"
#include "egg-menu-merge.h"
#include "egg-toggle-action.h"
#include "popup-commands.h"
#include "ephy-state.h"
#include "window-commands.h"
#include "ephy-debug.h"

static GtkTargetEntry topic_drag_dest_types [] =
{
	{ EPHY_DND_BOOKMARK_TYPE,   0, 0 }
};

static int n_topic_drag_dest_types = G_N_ELEMENTS (topic_drag_dest_types);

static GtkTargetEntry bmk_drag_types [] =
{
        { EPHY_DND_URI_LIST_TYPE,   0, 0 },
        { EPHY_DND_TEXT_TYPE,       0, 1 },
        { EPHY_DND_URL_TYPE,        0, 2 },
	{ EPHY_DND_BOOKMARK_TYPE,   0, 3 }
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

static void search_entry_changed_cb       (GtkWidget *entry,
					   EphyBookmarksEditor *editor);
static void cmd_open_bookmarks_in_tabs    (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_open_bookmarks_in_browser (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_show_in_the_toolbar       (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_delete			  (EggAction *action,
				           EphyBookmarksEditor *editor);
static void cmd_bookmark_properties	  (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_add_topic		  (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_rename			  (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_close			  (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_cut			  (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_copy			  (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_paste			  (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_select_all		  (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_help_contents		  (EggAction *action,
					   EphyBookmarksEditor *editor);

struct EphyBookmarksEditorPrivate
{
	EphyBookmarks *bookmarks;
	GtkWidget *bm_view;
	GtkWidget *key_view;
	EphyNodeFilter *bookmarks_filter;
	GtkWidget *search_entry;
	GtkWidget *menu_dock;
	GtkWidget *window;
	EggMenuMerge *ui_merge;
	EggActionGroup *action_group;
	int priority_col;
};

enum
{
	PROP_0,
	PROP_BOOKMARKS
};

static GObjectClass *parent_class = NULL;

static EggActionGroupEntry ephy_bookmark_popup_entries [] = {
	/* Toplevel */
	{ "File", N_("_File"), NULL, NULL, NULL, NULL, NULL },
	{ "Edit", N_("_Edit"), NULL, NULL, NULL, NULL, NULL },
	{ "View", N_("_View"), NULL, NULL, NULL, NULL, NULL },
	{ "Help", N_("_Help"), NULL, NULL, NULL, NULL, NULL },
	{ "FakeToplevel", (""), NULL, NULL, NULL, NULL, NULL },

	/* File Menu*/
	{ "NewTopic", N_("_New Topic"), GTK_STOCK_NEW, "<control>N",
	  N_("Create a new topic"), 
	  G_CALLBACK (cmd_add_topic), NULL },
	{ "OpenInWindow", N_("_Open in New Window"), GTK_STOCK_OPEN, "<control>O",
	  N_("Open the selected bookmark in a new window"), 
	  G_CALLBACK (cmd_open_bookmarks_in_browser), NULL },
	{ "OpenInTab", N_("Open in New _Tab"), NULL, "<shift><control>O",
	  N_("Open the selected bookmark in a new tab"), 
	  G_CALLBACK (cmd_open_bookmarks_in_tabs), NULL },
	{ "Rename", N_("_Rename"), NULL, "F2",
	  N_("Rename the selected bookmark or topic"), G_CALLBACK (cmd_rename), NULL },
	{ "Delete", N_("_Delete"), GTK_STOCK_DELETE, NULL,
	  N_("Delete the selected bookmark or topic"), 
	  G_CALLBACK (cmd_delete), NULL },
	{ "ShowInToolbar", N_("_Show in the Toolbar"), NULL, NULL,
	  N_("Show the selected bookmark or topic in the bookmarks toolbar"), 
	  G_CALLBACK (cmd_show_in_the_toolbar), NULL, TOGGLE_ACTION },
	{ "Properties", N_("_Properties"), GTK_STOCK_PROPERTIES, "<alt>Return",
	  N_("View or modify the properties of the selected bookmark"), 
	  G_CALLBACK (cmd_bookmark_properties), NULL },
	{ "Close", N_("_Close"), GTK_STOCK_CLOSE, "<control>W",
	  N_("Close the bookmarks window"), 
	  G_CALLBACK (cmd_close), NULL },

	/* Edit Menu */
	{ "Cut", N_("Cu_t"), GTK_STOCK_CUT, "<control>X",
	  N_("Cut the selection"), 
	  G_CALLBACK (cmd_cut), NULL },
	{ "Copy", N_("_Copy"), GTK_STOCK_COPY, "<control>C",
	  N_("Copy the selection"), 
	  G_CALLBACK (cmd_copy), NULL },
	{ "Paste", N_("_Paste"), GTK_STOCK_PASTE, "<control>V",
	  N_("Paste the clipboard"), 
	  G_CALLBACK (cmd_paste), NULL },
	{ "SelectAll", N_("Select _All"), NULL, "<control>A",
	  N_("Select all bookmarks or text"), 
	  G_CALLBACK (cmd_select_all), NULL },
	
	/* View Menu */
	{ "ViewTitle", N_("_Title"), NULL, NULL,
	  N_("Show only the title column"), 
	  NULL, NULL, RADIO_ACTION, NULL },
	{ "ViewLocation", N_("_Location"), NULL, NULL,
	  N_("Show only the location column"), 
	  NULL, NULL, RADIO_ACTION, "ViewTitle" },
	{ "ViewTitleLocation", N_("T_itle and Location"), NULL, NULL,
	  N_("Show both the title and location columns"), 
	  NULL, NULL, RADIO_ACTION, "ViewTitle" },	

	/* Help Menu */	
	{ "HelpContents", N_("_Contents"), GTK_STOCK_HELP, "F1",
	  N_("Display bookmarks help"), 
	  G_CALLBACK (cmd_help_contents), NULL },
	{ "HelpAbout", N_("_About"), GNOME_STOCK_ABOUT, NULL,
	  N_("Display credits for the web browser creators"),
	  G_CALLBACK (window_cmd_help_about), NULL },
};
static guint ephy_bookmark_popup_n_entries = G_N_ELEMENTS (ephy_bookmark_popup_entries);

static void
cmd_add_topic (EggAction *action,
	       EphyBookmarksEditor *editor)
{
	EphyNode *node;

	node = ephy_bookmarks_add_keyword (editor->priv->bookmarks,
				           _("Type a topic"));
	ephy_node_view_select_node (EPHY_NODE_VIEW (editor->priv->key_view), node);
	ephy_node_view_edit (EPHY_NODE_VIEW (editor->priv->key_view));
}

static void
cmd_close (EggAction *action,
	   EphyBookmarksEditor *editor)
{
	gtk_widget_hide (GTK_WIDGET (editor));
}

static void
cmd_rename (EggAction *action,
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
		return GTK_WIDGET (ephy_shell_get_active_window (ephy_shell));
	}
}

static void
cmd_show_in_the_toolbar (EggAction *action,
		         EphyBookmarksEditor *editor)
{
	EphyNode *node;
	GList *selection;
	GValue value = { 0, };
	gboolean state;

	if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->bm_view)))
	{
		selection = ephy_node_view_get_selection
			(EPHY_NODE_VIEW (editor->priv->bm_view));
	}
	else if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->key_view)))
	{
		selection = ephy_node_view_get_selection
			(EPHY_NODE_VIEW (editor->priv->key_view));
	}
	else
	{
		return;
	}

	node = EPHY_NODE (selection->data);

	state = EGG_TOGGLE_ACTION (action)->active;

	g_value_init (&value, G_TYPE_BOOLEAN);
	g_value_set_boolean (&value, state);
	ephy_node_set_property (node,
			        EPHY_NODE_BMK_PROP_SHOW_IN_TOOLBAR,
			        &value);
	g_value_unset (&value);

	g_list_free (selection);
}

static void
cmd_open_bookmarks_in_tabs (EggAction *action,
			    EphyBookmarksEditor *editor)
{
	EphyWindow *window;
	GList *selection;
	GList *l;

	window = EPHY_WINDOW (get_target_window (editor));
	selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->bm_view));

	for (l = selection; l; l = l->next)
	{
		EphyNode *node = EPHY_NODE (l->data);
		const char *location;

		location = ephy_node_get_property_string (node,
						EPHY_NODE_BMK_PROP_LOCATION);

		ephy_shell_new_tab (ephy_shell, window, NULL, location,
			EPHY_NEW_TAB_APPEND|EPHY_NEW_TAB_IN_EXISTING_WINDOW);
	}

	g_list_free (selection);
}

static void
cmd_open_bookmarks_in_browser (EggAction *action,
			       EphyBookmarksEditor *editor)
{
	EphyWindow *window;
	GList *selection;
	GList *l;

	window = EPHY_WINDOW (get_target_window (editor));
	selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->bm_view));

	for (l = selection; l; l = l->next)
	{
		EphyNode *node = EPHY_NODE (l->data);
		const char *location;

		location = ephy_node_get_property_string (node,
						EPHY_NODE_BMK_PROP_LOCATION);

		ephy_shell_new_tab (ephy_shell, window, NULL, location,
				    EPHY_NEW_TAB_IN_NEW_WINDOW);
	}

	g_list_free (selection);
}

static void
cmd_delete (EggAction *action,
	    EphyBookmarksEditor *editor)
{
	if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->bm_view)))
	{
		ephy_node_view_remove (EPHY_NODE_VIEW (editor->priv->bm_view));
	}
	else if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->key_view)))
	{
		EphyNodeViewPriority priority;
		GList *selected;
		EphyNode *node; 
		
		selected = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->key_view));
		node = EPHY_NODE (selected->data);
		priority = ephy_node_get_property_int (node, EPHY_NODE_KEYWORD_PROP_PRIORITY);

		if (priority == -1) priority = EPHY_NODE_VIEW_NORMAL_PRIORITY;
		
		if (priority == EPHY_NODE_VIEW_NORMAL_PRIORITY)
		{
			ephy_node_view_remove (EPHY_NODE_VIEW (editor->priv->key_view));
		}
	}
}

static void
cmd_bookmark_properties (EggAction *action,
			 EphyBookmarksEditor *editor)
{
	GtkWidget *dialog;
	GList *selection;
	GList *l;

	selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->bm_view));

	for (l = selection; l; l = l->next)
	{
		EphyNode *node = EPHY_NODE (l->data);
		dialog = ephy_bookmark_properties_new (editor->priv->bookmarks, node, GTK_WINDOW (editor));
		gtk_widget_show (GTK_WIDGET (dialog));
	}

	g_list_free (selection);
}

static void
cmd_cut (EggAction *action,
	 EphyBookmarksEditor *editor)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (editor));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_cut_clipboard (GTK_EDITABLE (widget));
	}
}

static void
cmd_copy (EggAction *action,
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
			EphyNode *node = EPHY_NODE (selection->data);
			tmp = ephy_node_get_property_string (node, EPHY_NODE_BMK_PROP_LOCATION);
			gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), tmp, -1);
		}

		g_list_free (selection);
	}
}

static void
cmd_paste (EggAction *action,
	   EphyBookmarksEditor *editor)
{
	GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (editor));

	if (GTK_IS_EDITABLE (widget))
	{
		gtk_editable_paste_clipboard (GTK_EDITABLE (widget));
	}
}

static void
cmd_select_all (EggAction *action,
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
cmd_help_contents (EggAction *action,
		   EphyBookmarksEditor *editor)
{
	/*FIXME: Implement help.*/
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
ephy_bookmarks_editor_class_init (EphyBookmarksEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_bookmarks_editor_finalize;
	object_class->dispose  = ephy_bookmarks_editor_dispose;

	object_class->set_property = ephy_bookmarks_editor_set_property;
	object_class->get_property = ephy_bookmarks_editor_get_property;

	g_object_class_install_property (object_class,
					 PROP_BOOKMARKS,
					 g_param_spec_object ("bookmarks",
							      "Bookmarks set",
							      "Bookmarks set",
							      EPHY_BOOKMARKS_TYPE,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
ephy_bookmarks_editor_finalize (GObject *object)
{
	EphyBookmarksEditor *editor;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EPHY_IS_BOOKMARKS_EDITOR (object));

	editor = EPHY_BOOKMARKS_EDITOR (object);

	g_return_if_fail (editor->priv != NULL);

	g_object_unref (G_OBJECT (editor->priv->bookmarks_filter));

	g_object_unref (editor->priv->action_group);
	egg_menu_merge_remove_action_group (editor->priv->ui_merge,
					    editor->priv->action_group);
	g_object_unref (editor->priv->ui_merge);

	if (editor->priv->window)
	{
		g_object_remove_weak_pointer
                        (G_OBJECT(editor->priv->window),
                         (gpointer *)&editor->priv->window);
	}

	g_free (editor->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_bookmarks_editor_node_activated_cb (GtkWidget *view,
				         EphyNode *node,
					 EphyBookmarksEditor *editor)
{
	const char *location;
	EphyWindow *window;

	g_return_if_fail (EPHY_IS_NODE (node));
	location = ephy_node_get_property_string
		(node, EPHY_NODE_BMK_PROP_LOCATION);
	g_return_if_fail (location != NULL);

	window = EPHY_WINDOW (get_target_window (editor));
	if (window != NULL)
	{
		ephy_window_load_url (EPHY_WINDOW (window), location);
		gtk_window_present (GTK_WINDOW (window));
	}
	else
	{
		/* We have to create a browser window */
		ephy_shell_new_tab (ephy_shell, NULL, NULL, location,
				    EPHY_NEW_TAB_IN_NEW_WINDOW);
	}
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
	gboolean can_show_in_toolbar, show_in_toolbar = FALSE;
	EggActionGroup *action_group;
	EggAction *action;
	GList *selected;
	GtkWidget *focus_widget;

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
		gboolean clipboard_contains_text;

		has_selection = gtk_editable_get_selection_bounds
			(GTK_EDITABLE (focus_widget), NULL, NULL);
		clipboard_contains_text = gtk_clipboard_wait_is_text_available
			(gtk_clipboard_get (GDK_SELECTION_CLIPBOARD));

		cut = has_selection;
		copy = has_selection;
		paste = clipboard_contains_text;
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
		EphyNode *node = EPHY_NODE (selected->data);
		EphyNodeViewPriority priority;

		priority = ephy_node_get_property_int
			(node, EPHY_NODE_KEYWORD_PROP_PRIORITY);
		if (priority == -1) priority = EPHY_NODE_VIEW_NORMAL_PRIORITY;
		key_normal = (priority == EPHY_NODE_VIEW_NORMAL_PRIORITY);

		show_in_toolbar = ephy_node_get_property_boolean
			(node, EPHY_NODE_BMK_PROP_SHOW_IN_TOOLBAR);

		g_list_free (selected);
	}

	selected = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->bm_view));
	if (bmk_focus && selected)
	{
		EphyNode *node = EPHY_NODE (selected->data);

		show_in_toolbar = ephy_node_get_property_boolean
			(node, EPHY_NODE_BMK_PROP_SHOW_IN_TOOLBAR);

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
		copy_label = _("_Copy Location");
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
	can_show_in_toolbar = (bmk_focus && bmk_selection && !bmk_multiple_selection) ||
		 (key_selection && key_focus);

	action_group = editor->priv->action_group;
	action = egg_action_group_get_action (action_group, "OpenInWindow");
	g_object_set (action, "sensitive", open_in_window, NULL);
	g_object_set (action, "label", open_in_window_label, NULL);
	action = egg_action_group_get_action (action_group, "OpenInTab");
	g_object_set (action, "sensitive", open_in_tab, NULL);
	g_object_set (action, "label", open_in_tab_label, NULL);
	action = egg_action_group_get_action (action_group, "Rename");
	g_object_set (action, "sensitive", rename, NULL);
	action = egg_action_group_get_action (action_group, "Delete");
	g_object_set (action, "sensitive", delete, NULL);
	action = egg_action_group_get_action (action_group, "Properties");
	g_object_set (action, "sensitive", properties, NULL);
	action = egg_action_group_get_action (action_group, "Cut");
	g_object_set (action, "sensitive", cut, NULL);
	action = egg_action_group_get_action (action_group, "Copy");
	g_object_set (action, "sensitive", copy, NULL);
	g_object_set (action, "label", copy_label, NULL);
	action = egg_action_group_get_action (action_group, "Paste");
	g_object_set (action, "sensitive", paste, NULL);
	action = egg_action_group_get_action (action_group, "SelectAll");
	g_object_set (action, "sensitive", select_all, NULL);
	action = egg_action_group_get_action (action_group, "ShowInToolbar");
	g_object_set (action, "sensitive", can_show_in_toolbar, NULL);
	egg_toggle_action_set_active (EGG_TOGGLE_ACTION (action), show_in_toolbar);
}

static void
ephy_bookmarks_editor_show_popup_cb (GtkWidget *view,
				     EphyBookmarksEditor *editor)
{
	GtkWidget *widget;

	widget = egg_menu_merge_get_widget (editor->priv->ui_merge,
					    "/popups/EphyBookmarkEditorPopup");
	ephy_bookmarks_editor_update_menu (editor);
	gtk_menu_popup (GTK_MENU (widget), NULL, NULL, NULL, NULL, 2,
			gtk_get_current_event_time ());
}

static void
ephy_bookmarks_editor_dispose (GObject *object)
{
	EphyBookmarksEditor *editor;
	long selected_id;
	char *selected_id_str;
	GList *selection;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EPHY_IS_BOOKMARKS_EDITOR (object));

	editor = EPHY_BOOKMARKS_EDITOR (object);

	g_return_if_fail (editor->priv != NULL);

	if (editor->priv->key_view != NULL)
	{
		selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->key_view));
		if (selection == NULL || selection->data == NULL)
		{
			editor->priv->key_view = NULL;
			G_OBJECT_CLASS (parent_class)->dispose (object);
			return;
		}

		selected_id = ephy_node_get_id (EPHY_NODE (selection->data));
		if (selected_id >= 0)
		{
			selected_id_str = g_strdup_printf ("%ld", selected_id);
			eel_gconf_set_string (CONF_BOOKMARKS_SELECTED_NODE,
					      selected_id_str);
			g_free (selected_id_str);
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
	switch (event->keyval)
	{
	case GDK_Delete:
	case GDK_KP_Delete:
		cmd_delete (NULL, editor);
		return TRUE;

	default:
		break;
	}

	return FALSE;
}

static void
reset_search_entry (EphyBookmarksEditor *editor)
{
	g_signal_handlers_block_by_func
		(G_OBJECT (editor->priv->search_entry),
		 G_CALLBACK (search_entry_changed_cb),
		 editor);
	gtk_entry_set_text (GTK_ENTRY (editor->priv->search_entry), "");
	g_signal_handlers_unblock_by_func
		(G_OBJECT (editor->priv->search_entry),
		 G_CALLBACK (search_entry_changed_cb),
		 editor);
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
		reset_search_entry (editor);
		bookmarks_filter (editor, node);
	}
}

static void
keyword_node_show_popup_cb (GtkWidget *view, EphyBookmarksEditor *editor)
{
	GtkWidget *widget;

	widget = egg_menu_merge_get_widget (editor->priv->ui_merge,
					   "/popups/EphyBookmarkKeywordPopup");
	ephy_bookmarks_editor_update_menu (editor);
	gtk_menu_popup (GTK_MENU (widget), NULL, NULL, NULL, NULL, 2,
			gtk_get_current_event_time ());
}

static void
search_entry_changed_cb (GtkWidget *entry, EphyBookmarksEditor *editor)
{
	EphyNode *all;
	char *search_text;

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

	search_text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);

	GDK_THREADS_ENTER ();

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

	GDK_THREADS_LEAVE ();

	g_free (search_text);
}

static GtkWidget *
build_search_box (EphyBookmarksEditor *editor)
{
	GtkWidget *box;
	GtkWidget *label;
	GtkWidget *entry;
	char *str;

	box = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (box);

	entry = gtk_entry_new ();
	editor->priv->search_entry = entry;
	gtk_widget_show (entry);
	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (search_entry_changed_cb),
			  editor);

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
add_widget (EggMenuMerge *merge, GtkWidget *widget, EphyBookmarksEditor *editor)
{
	gtk_box_pack_start (GTK_BOX (editor->priv->menu_dock),
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

	g_return_if_fail (EPHY_IS_NODE (node));

	for (l = nodes; l != NULL; l = l->next)
	{
		EphyNode *bmk = EPHY_NODE (l->data);

		ephy_bookmarks_set_keyword (editor->priv->bookmarks, node, bmk);
	}
}

static void
menu_activate_cb (EphyNodeView *view,
	          EphyBookmarksEditor *editor)
{
	ephy_bookmarks_editor_update_menu (editor);
}

static void
provide_favicon (EphyNode *node, GValue *value, gpointer user_data)
{
	EphyFaviconCache *cache;
	const char *icon_location;
	GdkPixbuf *pixbuf = NULL;

	cache = ephy_embed_shell_get_favicon_cache (EPHY_EMBED_SHELL (ephy_shell));
	icon_location = ephy_node_get_property_string
		(node, EPHY_NODE_BMK_PROP_ICON);

	LOG ("Get favicon for %s", icon_location ? icon_location : "None")

	if (icon_location)
	{
		pixbuf = ephy_favicon_cache_get (cache, icon_location);
	}

	g_value_init (value, GDK_TYPE_PIXBUF);
	g_value_set_object (value, pixbuf);
}

static void
ephy_bookmarks_editor_construct (EphyBookmarksEditor *editor)
{
	GtkTreeSelection *selection;
	GtkWidget *hbox, *vbox;
	GtkWidget *bm_view, *key_view;
	GtkWidget *scrolled_window;
	GtkWidget *menu;
	EphyNode *node;
	long selected_id;
	EphyNode *selected_node;
	char *selected_id_str;
	EggMenuMerge *ui_merge;
	EggActionGroup *action_group;
	EggAction *action;
	const char *icon_path;
	int i;

	ephy_state_add_window (GTK_WIDGET(editor),
			       "bookmarks_editor",
		               450, 400);

	gtk_window_set_title (GTK_WINDOW (editor), _("Bookmarks"));

	icon_path = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP,
					       "epiphany-bookmarks.png", TRUE, NULL);
	gtk_window_set_icon_from_file (GTK_WINDOW (editor), icon_path, NULL);

	g_signal_connect (editor, "delete_event",
			  G_CALLBACK (delete_event_cb), NULL);

	for (i = 0; i < ephy_bookmark_popup_n_entries; i++)
	{
		ephy_bookmark_popup_entries[i].user_data = editor;
	}

	editor->priv->menu_dock = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (editor->priv->menu_dock);
	gtk_container_add (GTK_CONTAINER (editor), editor->priv->menu_dock);

	ui_merge = egg_menu_merge_new ();
	g_signal_connect (ui_merge, "add_widget", G_CALLBACK (add_widget), editor);
	action_group = egg_action_group_new ("PopupActions");
	egg_action_group_add_actions (action_group, ephy_bookmark_popup_entries,
				      ephy_bookmark_popup_n_entries);
	egg_menu_merge_insert_action_group (ui_merge,
					    action_group, 0);
	egg_menu_merge_add_ui_from_file (ui_merge,
				         ephy_file ("epiphany-bookmark-editor-ui.xml"),
				         NULL);
	gtk_window_add_accel_group (GTK_WINDOW (editor), ui_merge->accel_group);
	egg_menu_merge_ensure_update (ui_merge);
	editor->priv->ui_merge = ui_merge;
	editor->priv->action_group = action_group;

	/* Update menu sensitivity before showing them */
	menu = egg_menu_merge_get_widget (ui_merge, "/menu/FileMenu");
	g_signal_connect (menu, "activate", G_CALLBACK (menu_activate_cb), editor);
	menu = egg_menu_merge_get_widget (ui_merge, "/menu/EditMenu");
	g_signal_connect (menu, "activate", G_CALLBACK (menu_activate_cb), editor);

	/* Fixme: We should implement gconf prefs for monitoring this setting */
	action = egg_action_group_get_action (action_group, "ViewTitle");
	egg_toggle_action_set_active (EGG_TOGGLE_ACTION (action), TRUE);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
	gtk_container_add (GTK_CONTAINER (editor->priv->menu_dock), hbox);
	gtk_widget_show (hbox);

	g_assert (editor->priv->bookmarks);

	node = ephy_bookmarks_get_keywords (editor->priv->bookmarks);

	scrolled_window = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
					"hadjustment", NULL,
					"vadjustment", NULL,
					"hscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"vscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"shadow_type", GTK_SHADOW_IN,
					NULL);
	gtk_box_pack_start (GTK_BOX (hbox), scrolled_window, FALSE, TRUE, 0);
	gtk_widget_show (scrolled_window);

	/* Keywords View */
	key_view = ephy_node_view_new (node, NULL);
	ephy_node_view_enable_drag_source (EPHY_NODE_VIEW (key_view),
					   topic_drag_types,
				           n_topic_drag_types,
					   -1);
	ephy_node_view_enable_drag_dest (EPHY_NODE_VIEW (key_view),
			                 topic_drag_dest_types,
			                 n_topic_drag_dest_types);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (key_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	ephy_node_view_add_column (EPHY_NODE_VIEW (key_view), _("Topics"),
				   G_TYPE_STRING,
				   EPHY_NODE_KEYWORD_PROP_NAME,
				   EPHY_NODE_KEYWORD_PROP_PRIORITY,
				   EPHY_NODE_VIEW_AUTO_SORT |
				   EPHY_NODE_VIEW_EDITABLE);
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
			  "show_popup",
			  G_CALLBACK (keyword_node_show_popup_cb),
			  editor);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (hbox),
			    vbox, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	gtk_box_pack_start (GTK_BOX (vbox),
			    build_search_box (editor),
			    FALSE, FALSE, 0);

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
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (bm_view), TRUE);
	ephy_node_view_enable_drag_source (EPHY_NODE_VIEW (bm_view),
					   bmk_drag_types,
				           n_bmk_drag_types,
					   EPHY_NODE_BMK_PROP_LOCATION);
	ephy_node_view_add_icon_column (EPHY_NODE_VIEW (bm_view), provide_favicon);
	ephy_node_view_add_column (EPHY_NODE_VIEW (bm_view), _("Title"),
				   G_TYPE_STRING, EPHY_NODE_BMK_PROP_TITLE, -1,
				   EPHY_NODE_VIEW_AUTO_SORT |
				   EPHY_NODE_VIEW_EDITABLE);
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
			  "show_popup",
			  G_CALLBACK (ephy_bookmarks_editor_show_popup_cb),
			  editor);

	selected_id_str = eel_gconf_get_string (CONF_BOOKMARKS_SELECTED_NODE);
	selected_id = g_ascii_strtoull (selected_id_str, NULL, 10);
	if (selected_id <= 0)
	{
		g_free (selected_id_str);
		return;
	}

	selected_node = ephy_node_get_from_id (selected_id);
	if (selected_node != NULL)
	{
		ephy_node_view_select_node (EPHY_NODE_VIEW (key_view), selected_node);
	}

	g_free (selected_id_str);
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
ephy_bookmarks_editor_init (EphyBookmarksEditor *editor)
{
	editor->priv = g_new0 (EphyBookmarksEditorPrivate, 1);
}
