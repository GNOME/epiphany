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
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gdk/gdkkeysyms.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

#include "ephy-bookmarks-editor.h"
#include "ephy-bookmark-properties.h"
#include "ephy-node-view.h"
#include "ephy-window.h"
#include "ephy-keywords-entry.h"
#include "ephy-dnd.h"
#include "ephy-prefs.h"
#include "ephy-shell.h"
#include "eel-gconf-extensions.h"
#include "ephy-file-helpers.h"
#include "egg-action-group.h"
#include "egg-menu-merge.h"
#include "popup-commands.h"

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

static void cmd_open_bookmarks_in_tabs    (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_open_bookmarks_in_browser (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_remove_bookmarks          (EggAction *action,
				           EphyBookmarksEditor *editor);
static void cmd_bookmark_properties	  (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_add_topic		  (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_remove_topic		  (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_rename_bookmark		  (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_rename_topic		  (EggAction *action,
					   EphyBookmarksEditor *editor);
static void cmd_close			  (EggAction *action,
					   EphyBookmarksEditor *editor);

struct EphyBookmarksEditorPrivate
{
	EphyBookmarks *bookmarks;
	EphyNodeView *bm_view;
	EphyNodeView *key_view;
	EphyNodeFilter *bookmarks_filter;
	GtkWidget *search_entry;
	GtkWidget *menu_dock;
	GtkWidget *window;
	EggMenuMerge *ui_merge;
	EggActionGroup *action_group;
};

enum
{
	PROP_0,
	PROP_BOOKMARKS
};

enum
{
	RESPONSE_REMOVE,
	RESPONSE_GO
};

static GObjectClass *parent_class = NULL;

static EggActionGroupEntry ephy_bookmark_popup_entries [] = {
	/* Toplevel */
	{ "File", N_("_File"), NULL, NULL, NULL, NULL, NULL },
	{ "Edit", N_("_Edit"), NULL, NULL, NULL, NULL, NULL },
	{ "FakeToplevel", (""), NULL, NULL, NULL, NULL, NULL },

	{ "NewTopic", N_("_New Topic"), GTK_STOCK_NEW, "<control>N",
	  NULL, G_CALLBACK (cmd_add_topic), NULL },

	{ "OpenInWindow", N_("_Open In New Window"), GTK_STOCK_OPEN, "<control>O",
	  NULL, G_CALLBACK (cmd_open_bookmarks_in_browser), NULL },

	{ "OpenInTab", N_("Open In New _Tab"), NULL, "<shift><control>O",
	  NULL, G_CALLBACK (cmd_open_bookmarks_in_tabs), NULL },

	{ "RenameBookmark", N_("_Rename Bookmark"), NULL, NULL,
	  NULL, G_CALLBACK (cmd_rename_bookmark), NULL },

	{ "RenameTopic", N_("R_ename Topic"), NULL, NULL,
	  NULL, G_CALLBACK (cmd_rename_topic), NULL },

	{ "RemoveBookmark", N_("_Delete Bookmark"), GTK_STOCK_DELETE, NULL,
	  NULL, G_CALLBACK (cmd_remove_bookmarks), NULL },

	{ "RemoveTopic", N_("D_elete Topic"), NULL, NULL,
          NULL, G_CALLBACK (cmd_remove_topic), NULL },

	{ "Properties", N_("_Properties"), GTK_STOCK_PROPERTIES, "<alt>Return",
	  NULL, G_CALLBACK (cmd_bookmark_properties), NULL },

	{ "Close", N_("_Close"), GTK_STOCK_CLOSE, "<control>W",
	  NULL, G_CALLBACK (cmd_close), NULL },
};
static guint ephy_bookmark_popup_n_entries = G_N_ELEMENTS (ephy_bookmark_popup_entries);

static void
cmd_add_topic (EggAction *action,
	       EphyBookmarksEditor *editor)
{
	EphyNode *node;

	node = ephy_bookmarks_add_keyword (editor->priv->bookmarks,
				           _("Type a topic"));
	ephy_node_view_select_node (editor->priv->key_view, node);
	ephy_node_view_edit (editor->priv->key_view);
}

static void
cmd_remove_topic (EggAction *action,
		  EphyBookmarksEditor *editor)
{
	GList *selection;

	selection = ephy_node_view_get_selection (editor->priv->key_view);
	if (selection)
	{
		EphyNode *node = EPHY_NODE (selection->data);
		ephy_bookmarks_remove_keyword (editor->priv->bookmarks, node);
		g_list_free (selection);
	}
}

static void
cmd_close (EggAction *action,
	   EphyBookmarksEditor *editor)
{
	gtk_widget_hide (GTK_WIDGET (editor));
}

static void
cmd_rename_topic (EggAction *action,
		  EphyBookmarksEditor *editor)
{
	ephy_node_view_edit (editor->priv->key_view);
}

static void
cmd_rename_bookmark (EggAction *action,
		     EphyBookmarksEditor *editor)
{
	ephy_node_view_edit (editor->priv->bm_view);
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
cmd_open_bookmarks_in_tabs (EggAction *action,
			    EphyBookmarksEditor *editor)
{
	EphyWindow *window;
	GList *selection;
	GList *l;

	window = EPHY_WINDOW (get_target_window (editor));
	selection = ephy_node_view_get_selection (editor->priv->bm_view);

	for (l = selection; l; l = l->next)
	{
		EphyNode *node = EPHY_NODE (l->data);
		const char *location;

		location = ephy_node_get_property_string (node,
						EPHY_NODE_BMK_PROP_LOCATION);

		ephy_shell_new_tab (ephy_shell, window, NULL, location,
			EPHY_NEW_TAB_APPEND|EPHY_NEW_TAB_IN_EXISTING_WINDOW);
	}

	if (selection)
	{
		g_list_free (selection);
	}
}

static void
cmd_open_bookmarks_in_browser (EggAction *action,
			       EphyBookmarksEditor *editor)
{
	EphyWindow *window;
	GList *selection;
	GList *l;

	window = EPHY_WINDOW (get_target_window (editor));
	selection = ephy_node_view_get_selection (editor->priv->bm_view);

	for (l = selection; l; l = l->next)
	{
		EphyNode *node = EPHY_NODE (l->data);
		const char *location;

		location = ephy_node_get_property_string (node,
						EPHY_NODE_BMK_PROP_LOCATION);

		ephy_shell_new_tab (ephy_shell, window, NULL, location,
				    EPHY_NEW_TAB_IN_NEW_WINDOW);
	}

	if (selection)
	{
		g_list_free (selection);
	}
}

static void
cmd_remove_bookmarks (EggAction *action,
		      EphyBookmarksEditor *editor)
{
	ephy_node_view_remove (editor->priv->bm_view);
}

static void
cmd_bookmark_properties (EggAction *action,
			 EphyBookmarksEditor *editor)
{
	GtkWidget *dialog;
	GList *selection;
	GList *l;

	selection = ephy_node_view_get_selection (editor->priv->bm_view);

	for (l = selection; l; l = l->next)
	{
		EphyNode *node = EPHY_NODE (l->data);
		dialog = ephy_bookmark_properties_new (editor->priv->bookmarks, node);
		gtk_widget_show (GTK_WIDGET (dialog));
	}

	if (selection)
	{
		g_list_free (selection);
	}
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
		selection = ephy_node_view_get_selection (editor->priv->key_view);
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
ephy_bookmarks_editor_show_popup_cb (GtkWidget *view,
				     EphyBookmarksEditor *editor)
{
	GtkWidget *widget;

	widget = egg_menu_merge_get_widget (editor->priv->ui_merge,
					    "/popups/EphyBookmarkEditorPopup");
	gtk_menu_popup (GTK_MENU (widget), NULL, NULL, NULL, NULL, 2,
			gtk_get_current_event_time ());
}

static void
ephy_bookmarks_editor_key_pressed_cb (GtkWidget *view,
				      GdkEventKey *event,
				      EphyBookmarksEditor *editor)
{
	switch (event->keyval)
	{
	case GDK_Delete:
		ephy_node_view_remove (editor->priv->bm_view);
		break;
	default:
		break;
	}
}

static void
ephy_bookmarks_editor_node_activated_cb (GtkWidget *view,
				         EphyNode *node,
					 EphyBookmarksEditor *editor)
{
	const char *location;
	GtkWindow *window;

	g_return_if_fail (EPHY_IS_NODE (node));
	location = ephy_node_get_property_string
		(node, EPHY_NODE_BMK_PROP_LOCATION);
	g_return_if_fail (location != NULL);

	window = GTK_WINDOW (get_target_window (editor));
	g_return_if_fail (IS_EPHY_WINDOW (window));
	ephy_window_load_url (EPHY_WINDOW (window), location);
	gtk_window_present (GTK_WINDOW (window));
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

static void
keyword_node_selected_cb (EphyNodeView *view,
			  EphyNode *node,
			  EphyBookmarksEditor *editor)
{
	if (node == NULL)
	{
		ephy_node_view_select_node
			(editor->priv->key_view,
			 ephy_bookmarks_get_bookmarks
			 (editor->priv->bookmarks));
	}
	else
	{
		bookmarks_filter (editor, node);
	}
}

static void
search_entry_changed_cb (GtkWidget *entry, EphyBookmarksEditor *editor)
{
	char *search_text;

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

	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	str = g_strconcat ("<b>", _("Search:"), "</b>", NULL);
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (box),
			    label, FALSE, TRUE, 0);

	entry = gtk_entry_new ();
	editor->priv->search_entry = entry;
	gtk_widget_show (entry);
	gtk_box_pack_start (GTK_BOX (box),
			    entry, TRUE, TRUE, 0);
	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (search_entry_changed_cb),
			  editor);
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
ephy_bookmarks_editor_construct (EphyBookmarksEditor *editor)
{
	GtkWidget *hbox, *vbox;
	EphyNodeView *bm_view, *key_view;
	EphyNode *node;
	long selected_id;
	EphyNode *selected_node;
	char *selected_id_str;
	EggMenuMerge *ui_merge;
	EggActionGroup *action_group;
	const char *icon_path;
	int i;

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
	egg_menu_merge_ensure_update (ui_merge);
	editor->priv->ui_merge = ui_merge;
	editor->priv->action_group = action_group;

	icon_path =  ephy_file ("epiphany-bookmarks.png");
	gtk_window_set_icon_from_file (GTK_WINDOW (editor), icon_path, NULL);
	gtk_window_set_title (GTK_WINDOW (editor), _("Bookmarks"));
	gtk_widget_set_size_request (GTK_WIDGET (editor), 500, 450);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
	gtk_container_add (GTK_CONTAINER (editor->priv->menu_dock), hbox);
	gtk_widget_show (hbox);

	g_assert (editor->priv->bookmarks);

	node = ephy_bookmarks_get_keywords (editor->priv->bookmarks);

	/* Keywords View */
	key_view = ephy_node_view_new (node, NULL);
	ephy_node_view_enable_drag_source (key_view);
	ephy_node_view_set_browse_mode (key_view);
	ephy_node_view_add_column (key_view, _("Topics"),
			           EPHY_TREE_MODEL_NODE_COL_KEYWORD, TRUE, TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (key_view), FALSE, TRUE, 0);
	gtk_widget_set_size_request (GTK_WIDGET (key_view), 130, -1);
	gtk_widget_show (GTK_WIDGET (key_view));
	editor->priv->key_view = key_view;
	g_signal_connect (G_OBJECT (key_view),
			  "node_selected",
			  G_CALLBACK (keyword_node_selected_cb),
			  editor);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (hbox),
			    vbox, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	gtk_box_pack_start (GTK_BOX (vbox),
			    build_search_box (editor),
			    FALSE, FALSE, 0);

	node = ephy_bookmarks_get_bookmarks (editor->priv->bookmarks);
	editor->priv->bookmarks_filter = ephy_node_filter_new ();

	/* Bookmarks View */
	bm_view = ephy_node_view_new (node, editor->priv->bookmarks_filter);
	ephy_node_view_set_hinted (bm_view, TRUE);
	ephy_node_view_enable_drag_source (bm_view);
	ephy_node_view_add_icon_column (bm_view, EPHY_TREE_MODEL_NODE_COL_ICON);
	ephy_node_view_add_column (bm_view, _("Bookmarks"),
				   EPHY_TREE_MODEL_NODE_COL_BOOKMARK, TRUE, TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (bm_view), TRUE, TRUE, 0);
	gtk_widget_show (GTK_WIDGET (bm_view));
	editor->priv->bm_view = bm_view;
	g_signal_connect (G_OBJECT (bm_view),
			  "key_press_event",
			  G_CALLBACK (ephy_bookmarks_editor_key_pressed_cb),
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
		ephy_node_view_select_node (key_view, selected_node);
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
