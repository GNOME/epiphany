/* 
 *  Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org>
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
#include <gtk/gtkentry.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gdk/gdkkeysyms.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

#include "ephy-bookmarks-editor.h"
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

static void popup_cmd_open_bookmarks_in_tabs    (EggAction *action,
						 EphyBookmarksEditor *editor);
static void popup_cmd_open_bookmarks_in_browser (EggAction *action,
						 EphyBookmarksEditor *editor);
static void popup_cmd_remove_bookmarks          (EggAction *action,
						 EphyBookmarksEditor *editor);

struct EphyBookmarksEditorPrivate
{
	EphyBookmarks *bookmarks;
	EphyNodeView *bm_view;
	EphyNodeView *key_view;
	EphyNodeFilter *bookmarks_filter;
	GtkWidget *title_entry;
	GtkWidget *location_entry;
	GtkWidget *keywords_entry;
	GtkWidget *search_entry;
	GtkWidget *go_button;
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
	{ "FakeToplevel", (""), NULL, NULL, NULL, NULL, NULL },
	
	{ "OpenInWindow", N_("Open In _New Window"), GTK_STOCK_OPEN, NULL,
	  NULL, G_CALLBACK (popup_cmd_open_bookmarks_in_browser), NULL },

	{ "OpenInTab", N_("Open In New _Tab"), NULL, NULL,
	  NULL, G_CALLBACK (popup_cmd_open_bookmarks_in_tabs), NULL },

	{ "Remove", N_("_Remove"), GTK_STOCK_REMOVE, NULL,
	  NULL, G_CALLBACK (popup_cmd_remove_bookmarks), NULL },
};
static guint ephy_bookmark_popup_n_entries = G_N_ELEMENTS (ephy_bookmark_popup_entries);


static void
popup_cmd_open_bookmarks_in_tabs (EggAction *action,
				  EphyBookmarksEditor *editor)
{
	EphyWindow *window;
	GList *selection;
	GList *l;

	window = EPHY_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (editor)));
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
popup_cmd_open_bookmarks_in_browser (EggAction *action,
				    EphyBookmarksEditor *editor)
{
	EphyWindow *window;
	GList *selection;
	GList *l;

	window = EPHY_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (editor)));
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
popup_cmd_remove_bookmarks (EggAction *action,
			    EphyBookmarksEditor *editor)
{
	ephy_node_view_remove (editor->priv->bm_view);
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

		ephy_bookmarks_editor_type = g_type_register_static (GTK_TYPE_DIALOG,
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

	g_free (editor->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_bookmarks_editor_node_selected_cb (GtkWidget *view,
				        EphyNode *node,
					EphyBookmarksEditor *editor)
{
	const char *title;
	const char *keywords;
	const char *location;

	if (node != NULL)
	{
		g_assert (EPHY_IS_NODE (node));

		title = ephy_node_get_property_string
			(node, EPHY_NODE_BMK_PROP_TITLE);
		keywords = ephy_node_get_property_string
			(node, EPHY_NODE_BMK_PROP_KEYWORDS);
		location = ephy_node_get_property_string
			(node, EPHY_NODE_BMK_PROP_LOCATION);
		gtk_entry_set_text (GTK_ENTRY (editor->priv->title_entry),
				    title ? g_strdup (title) : "");
		gtk_entry_set_text (GTK_ENTRY (editor->priv->keywords_entry),
				    keywords ? g_strdup (keywords) : "");
		gtk_entry_set_text (GTK_ENTRY (editor->priv->location_entry),
				    location ? g_strdup (location) : "");
		gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->title_entry), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->keywords_entry), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->location_entry), TRUE);
		/* Activate the Go button */
		gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->go_button), TRUE);
	}
	else
	{
		gtk_entry_set_text (GTK_ENTRY (editor->priv->title_entry), "");
		gtk_entry_set_text (GTK_ENTRY (editor->priv->keywords_entry), "");
		gtk_entry_set_text (GTK_ENTRY (editor->priv->location_entry), "");
		gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->title_entry), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->keywords_entry), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->location_entry), FALSE);
	}
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

	window = gtk_window_get_transient_for (GTK_WINDOW (editor));
	g_return_if_fail (IS_EPHY_WINDOW (window));
	ephy_window_load_url (EPHY_WINDOW (window), location);
}

static void
ephy_bookmarks_editor_go_to_location (EphyBookmarksEditor *editor)
{
	GList *selection;
	const char *location;
	GtkWindow *window;

	selection = ephy_node_view_get_selection (editor->priv->bm_view);
	if (selection != NULL)
	{
		EphyNode *bm = EPHY_NODE (selection->data);

		location = ephy_node_get_property_string (bm,
				EPHY_NODE_BMK_PROP_LOCATION);
		g_return_if_fail (location != NULL);
		window = gtk_window_get_transient_for (GTK_WINDOW (editor));
		g_return_if_fail (IS_EPHY_WINDOW (window));
		ephy_window_load_url (EPHY_WINDOW (window), location);
		g_list_free (selection);
	}
}

static void
ephy_bookmarks_editor_response_cb (GtkDialog *dialog,
		                   int response_id,
				   EphyBookmarksEditor *editor)
{
	switch (response_id)
	{
		case GTK_RESPONSE_CLOSE:
			gtk_widget_destroy (GTK_WIDGET (dialog));
			break;
		case RESPONSE_REMOVE:
			ephy_node_view_remove (editor->priv->bm_view);
			break;
		case RESPONSE_GO:
			ephy_bookmarks_editor_go_to_location (editor);
			break;
	}
}

static void
update_prop_from_entry (EphyBookmarksEditor *editor,
		        GtkWidget *entry,
			guint id)
{
	GList *selection;
	GValue value = { 0, };

	selection = ephy_node_view_get_selection (editor->priv->bm_view);
	if (selection != NULL)
	{
		EphyNode *bm = EPHY_NODE (selection->data);
		char *tmp;

		tmp = gtk_editable_get_chars
			(GTK_EDITABLE (entry), 0, -1);
		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, tmp);
		ephy_node_set_property (bm, id, &value);
	        g_value_unset (&value);
		g_free (tmp);
		g_list_free (selection);
	}
}

static void
title_entry_changed_cb (GtkWidget *entry, EphyBookmarksEditor *editor)
{
	update_prop_from_entry (editor, editor->priv->title_entry,
				EPHY_NODE_BMK_PROP_TITLE);
}

static void
location_entry_changed_cb (GtkWidget *entry, EphyBookmarksEditor *editor)
{
	update_prop_from_entry (editor, editor->priv->location_entry,
				EPHY_NODE_BMK_PROP_LOCATION);
}

static void
keywords_changed_cb (GtkWidget *entry,
		     EphyBookmarksEditor *editor)
{
	EphyNode *node;
	GList *selection;
	char *keywords;

	selection = ephy_node_view_get_selection (editor->priv->bm_view);
	if (selection == NULL) return;
	node = EPHY_NODE (selection->data);
	g_list_free (selection);

	keywords = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	ephy_bookmarks_update_keywords (editor->priv->bookmarks,
					keywords, node);
	g_free (keywords);

	update_prop_from_entry (editor, editor->priv->keywords_entry,
				EPHY_NODE_BMK_PROP_KEYWORDS);
}

static GtkWidget *
build_editing_table (EphyBookmarksEditor *editor)
{
	GtkWidget *table, *label, *entry;
	char *str;

	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	gtk_widget_show (table);

	/* Title entry */
	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	str = g_strconcat ("<b>", _("Title:"), "</b>", NULL);
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_show (label);
	entry = gtk_entry_new ();
	editor->priv->title_entry = entry;
	gtk_widget_set_sensitive (GTK_WIDGET (entry), FALSE);
	gtk_widget_show (entry);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), entry, 1, 2, 0, 1);
	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (title_entry_changed_cb),
			  editor);

	/* Location entry */
	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	str = g_strconcat ("<b>", _("Location:"), "</b>", NULL);
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_show (label);
	entry = gtk_entry_new ();
	editor->priv->location_entry = entry;
	gtk_widget_set_sensitive (GTK_WIDGET (entry), FALSE);
	gtk_widget_show (entry);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), entry, 1, 2, 1, 2);
	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (location_entry_changed_cb),
			  editor);

	/* Keywords entry */
	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	str = g_strconcat ("<b>", _("Keywords:"), "</b>", NULL);
	gtk_label_set_markup (GTK_LABEL (label), str);
	g_free (str);
	gtk_widget_show (label);
	entry = ephy_keywords_entry_new ();
	ephy_keywords_entry_set_bookmarks (EPHY_KEYWORDS_ENTRY (entry),
					   editor->priv->bookmarks);
	editor->priv->keywords_entry = entry;
	gtk_widget_set_sensitive (GTK_WIDGET (entry), FALSE);
	gtk_widget_show (entry);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), entry, 1, 2, 2, 3);
	g_signal_connect (G_OBJECT (entry), "keywords_changed",
			  G_CALLBACK (keywords_changed_cb),
			  editor);

	return table;
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
		/* Desactivate the Go button */
		gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->go_button),
				FALSE);
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
	int i;
	
	for (i = 0; i < ephy_bookmark_popup_n_entries; i++)
	{
		ephy_bookmark_popup_entries[i].user_data = editor;
	}

	ui_merge = egg_menu_merge_new ();
	action_group = egg_action_group_new ("PopupActions");
	egg_action_group_add_actions (action_group, ephy_bookmark_popup_entries,
				      ephy_bookmark_popup_n_entries);
	egg_menu_merge_insert_action_group (ui_merge,
					    action_group, 0);
	egg_menu_merge_add_ui_from_file (ui_merge,
				ephy_file ("epiphany-bookmark-editor-ui.xml"),
				NULL);
	editor->priv->ui_merge = ui_merge;
	editor->priv->action_group = action_group;
	

	gtk_window_set_title (GTK_WINDOW (editor), _("Bookmarks"));

	gtk_dialog_set_has_separator (GTK_DIALOG (editor), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (editor), 6);
	gtk_widget_set_size_request (GTK_WIDGET (editor), 500, 400);
	g_signal_connect (G_OBJECT (editor),
			  "response",
			  G_CALLBACK (ephy_bookmarks_editor_response_cb),
			  editor);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (editor)->vbox),
			    hbox, TRUE, TRUE, 0);
	gtk_widget_show (hbox);

	g_assert (editor->priv->bookmarks);

	node = ephy_bookmarks_get_keywords (editor->priv->bookmarks);
	
	/* Keywords View */
	key_view = ephy_node_view_new (node, NULL);
	ephy_node_view_set_browse_mode (key_view);
	ephy_node_view_add_column (key_view, _("Keywords"),
			           EPHY_TREE_MODEL_NODE_COL_KEYWORD, TRUE);
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
	ephy_node_view_enable_drag_source (bm_view);
	ephy_node_view_add_icon_column (bm_view, EPHY_TREE_MODEL_NODE_COL_ICON);
	ephy_node_view_add_column (bm_view, _("Title"),
				   EPHY_TREE_MODEL_NODE_COL_BOOKMARK, TRUE);
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
			  "node_selected",
			  G_CALLBACK (ephy_bookmarks_editor_node_selected_cb),
			  editor);
	g_signal_connect (G_OBJECT (bm_view),
			  "show_popup",
			  G_CALLBACK (ephy_bookmarks_editor_show_popup_cb),
			  editor);

	gtk_box_pack_start (GTK_BOX (vbox),
			    build_editing_table (editor),
			    FALSE, FALSE, 0);

	editor->priv->go_button = gtk_dialog_add_button (GTK_DIALOG (editor),
				GTK_STOCK_JUMP_TO,
				RESPONSE_GO);
	/* The Go button is insensitive by default */
	gtk_widget_set_sensitive (GTK_WIDGET (editor->priv->go_button), FALSE);

	gtk_dialog_add_button (GTK_DIALOG (editor),
			       GTK_STOCK_REMOVE,
			       RESPONSE_REMOVE);
	gtk_dialog_add_button (GTK_DIALOG (editor),
			       GTK_STOCK_CLOSE,
			       GTK_RESPONSE_CLOSE);
	gtk_dialog_set_default_response (GTK_DIALOG (editor), GTK_RESPONSE_CLOSE);

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

GtkWidget *
ephy_bookmarks_editor_new (EphyBookmarks *bookmarks,
			   GtkWindow *parent)
{
	EphyBookmarksEditor *editor;

	g_assert (bookmarks != NULL);

	editor = EPHY_BOOKMARKS_EDITOR (g_object_new
			(EPHY_TYPE_BOOKMARKS_EDITOR,
			 "bookmarks", bookmarks,
			 NULL));

	if (parent)
	{
		gtk_window_set_transient_for (GTK_WINDOW (editor), parent);
	}

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
