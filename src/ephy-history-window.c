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
#include <libgnomeui/gnome-stock-icons.h>
#include <string.h>

#include "ephy-node-view.h"
#include "ephy-window.h"
#include "ephy-history-window.h"
#include "ephy-shell.h"
#include "ephy-dnd.h"
#include "ephy-prefs.h"
#include "egg-action-group.h"
#include "egg-menu-merge.h"
#include "ephy-state.h"
#include "window-commands.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"

static GtkTargetEntry page_drag_types [] =
{
        { EPHY_DND_URI_LIST_TYPE,   0, 0 },
        { EPHY_DND_TEXT_TYPE,       0, 1 },
        { EPHY_DND_URL_TYPE,        0, 2 },
	{ EPHY_DND_BOOKMARK_TYPE,   0, 3 }
};
static int n_page_drag_types = G_N_ELEMENTS (page_drag_types);

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

static void search_entry_changed_cb       (GtkWidget *entry,
					   EphyHistoryWindow *editor);
static void cmd_open_bookmarks_in_tabs    (EggAction *action,
					   EphyHistoryWindow *editor);
static void cmd_open_bookmarks_in_browser (EggAction *action,
					   EphyHistoryWindow *editor);
static void cmd_clear			  (EggAction *action,
				           EphyHistoryWindow *editor);
static void cmd_close			  (EggAction *action,
					   EphyHistoryWindow *editor);
static void cmd_help_contents		  (EggAction *action,
					   EphyHistoryWindow *editor);

struct EphyHistoryWindowPrivate
{
	EphyHistory *history;
	GtkWidget *sites_view;
	GtkWidget *pages_view;
	EphyNodeFilter *pages_filter;
	GtkWidget *search_entry;
	GtkWidget *menu_dock;
	GtkWidget *window;
	EggMenuMerge *ui_merge;
	EggActionGroup *action_group;
};

enum
{
	PROP_0,
	PROP_HISTORY
};

static GObjectClass *parent_class = NULL;

static EggActionGroupEntry ephy_history_ui_entries [] = {
	/* Toplevel */
	{ "File", N_("_File"), NULL, NULL, NULL, NULL, NULL },
	{ "Help", N_("_Help"), NULL, NULL, NULL, NULL, NULL },
	{ "FakeToplevel", (""), NULL, NULL, NULL, NULL, NULL },

	{ "OpenInWindow", N_("_Open in New Window"), GTK_STOCK_OPEN, "<control>O",
	  NULL, G_CALLBACK (cmd_open_bookmarks_in_browser), NULL },

	{ "OpenInTab", N_("Open in New _Tab"), NULL, "<shift><control>O",
	  NULL, G_CALLBACK (cmd_open_bookmarks_in_tabs), NULL },

	{ "Clear", N_("Clea_r"), GTK_STOCK_CLEAR, NULL,
	  NULL, G_CALLBACK (cmd_clear), NULL },

	{ "Close", N_("_Close"), GTK_STOCK_CLOSE, "<control>W",
	  NULL, G_CALLBACK (cmd_close), NULL },

	{ "HelpContents", N_("_Contents"), GTK_STOCK_HELP, "F1",
	  NULL, G_CALLBACK (cmd_help_contents), NULL },

	{ "HelpAbout", N_("_About"), GNOME_STOCK_ABOUT, NULL,
	  NULL, G_CALLBACK (window_cmd_help_about), NULL },

};
static guint ephy_history_ui_n_entries = G_N_ELEMENTS (ephy_history_ui_entries);

static void
cmd_clear (EggAction *action,
	   EphyHistoryWindow *editor)
{
	ephy_history_clear (editor->priv->history);
}

static void
cmd_close (EggAction *action,
	   EphyHistoryWindow *editor)
{
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
		return GTK_WIDGET (ephy_shell_get_active_window (ephy_shell));
	}
}

static void
cmd_open_bookmarks_in_tabs (EggAction *action,
			    EphyHistoryWindow *editor)
{
	EphyWindow *window;
	GList *selection;
	GList *l;

	window = EPHY_WINDOW (get_target_window (editor));
	selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->pages_view));

	for (l = selection; l; l = l->next)
	{
		EphyNode *node = EPHY_NODE (l->data);
		const char *location;

		location = ephy_node_get_property_string (node,
						EPHY_NODE_PAGE_PROP_LOCATION);

		ephy_shell_new_tab (ephy_shell, window, NULL, location,
			EPHY_NEW_TAB_APPEND|EPHY_NEW_TAB_IN_EXISTING_WINDOW);
	}

	g_list_free (selection);
}

static void
cmd_open_bookmarks_in_browser (EggAction *action,
			       EphyHistoryWindow *editor)
{
	EphyWindow *window;
	GList *selection;
	GList *l;

	window = EPHY_WINDOW (get_target_window (editor));
	selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->pages_view));

	for (l = selection; l; l = l->next)
	{
		EphyNode *node = EPHY_NODE (l->data);
		const char *location;

		location = ephy_node_get_property_string (node,
						EPHY_NODE_PAGE_PROP_LOCATION);

		ephy_shell_new_tab (ephy_shell, window, NULL, location,
				    EPHY_NEW_TAB_IN_NEW_WINDOW);
	}

	g_list_free (selection);
}

static void
cmd_help_contents (EggAction *action,
		   EphyHistoryWindow *editor)
{
	/*FIXME: Implement help.*/
}

GType
ephy_history_window_get_type (void)
{
	static GType ephy_history_window_type = 0;

	if (ephy_history_window_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyHistoryWindowClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_history_window_class_init,
			NULL,
			NULL,
			sizeof (EphyHistoryWindow),
			0,
			(GInstanceInitFunc) ephy_history_window_init
		};

		ephy_history_window_type = g_type_register_static (GTK_TYPE_WINDOW,
							             "EphyHistoryWindow",
							             &our_info, 0);
	}

	return ephy_history_window_type;
}

static void
ephy_history_window_class_init (EphyHistoryWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_history_window_finalize;

	object_class->set_property = ephy_history_window_set_property;
	object_class->get_property = ephy_history_window_get_property;

	g_object_class_install_property (object_class,
					 PROP_HISTORY,
					 g_param_spec_object ("history",
							      "Global history",
							      "Global History",
							      EPHY_HISTORY_TYPE,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
ephy_history_window_finalize (GObject *object)
{
	EphyHistoryWindow *editor;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EPHY_IS_HISTORY_WINDOW (object));

	editor = EPHY_HISTORY_WINDOW (object);

	g_return_if_fail (editor->priv != NULL);

	g_object_unref (G_OBJECT (editor->priv->pages_filter));

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
ephy_history_window_node_activated_cb (GtkWidget *view,
				         EphyNode *node,
					 EphyHistoryWindow *editor)
{
	const char *location;
	EphyWindow *window;

	g_return_if_fail (EPHY_IS_NODE (node));
	location = ephy_node_get_property_string
		(node, EPHY_NODE_PAGE_PROP_LOCATION);
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
ephy_history_window_update_menu (EphyHistoryWindow *editor)
{
	gboolean open_in_window, open_in_tab;
	gboolean pages_focus, pages_selection;
	gboolean pages_multiple_selection;
	EggActionGroup *action_group;
	EggAction *action;
	char *open_in_window_label, *open_in_tab_label;

	pages_focus = ephy_node_view_is_target
		(EPHY_NODE_VIEW (editor->priv->pages_view));
	pages_selection = ephy_node_view_has_selection
		(EPHY_NODE_VIEW (editor->priv->pages_view),
		 &pages_multiple_selection);

	if (pages_multiple_selection)
	{
		open_in_window_label = N_("_Open in New Windows");
		open_in_tab_label = N_("Open in New _Tabs");
	}
	else
	{
		open_in_window_label = _("_Open in New Window");
		open_in_tab_label = _("Open in New _Tab");
	}

	open_in_window = (pages_focus && pages_selection);
	open_in_tab = (pages_focus && pages_selection);

	action_group = editor->priv->action_group;
	action = egg_action_group_get_action (action_group, "OpenInWindow");
	g_object_set (action, "sensitive", open_in_window, NULL);
	g_object_set (action, "label", open_in_window_label, NULL);
	action = egg_action_group_get_action (action_group, "OpenInTab");
	g_object_set (action, "sensitive", open_in_tab, NULL);
	g_object_set (action, "label", open_in_tab_label, NULL);
}

static void
ephy_history_window_show_popup_cb (GtkWidget *view,
				   EphyHistoryWindow *editor)
{
	GtkWidget *widget;

	widget = egg_menu_merge_get_widget (editor->priv->ui_merge,
					    "/popups/EphyHistoryWindowPopup");
	ephy_history_window_update_menu (editor);
	gtk_menu_popup (GTK_MENU (widget), NULL, NULL, NULL, NULL, 2,
			gtk_get_current_event_time ());
}

static void
pages_filter (EphyHistoryWindow *editor,
	      EphyNode *site)
{
	ephy_node_filter_empty (editor->priv->pages_filter);
	ephy_node_filter_add_expression (editor->priv->pages_filter,
				         ephy_node_filter_expression_new (EPHY_NODE_FILTER_EXPRESSION_HAS_PARENT,
								          site),
				         0);
	ephy_node_filter_done_changing (editor->priv->pages_filter);
}

static void
reset_search_entry (EphyHistoryWindow *editor)
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
site_node_selected_cb (EphyNodeView *view,
		       EphyNode *node,
		       EphyHistoryWindow *editor)
{
	EphyNode *pages;

	if (node == NULL)
	{
		pages = ephy_history_get_pages (editor->priv->history);
		ephy_node_view_select_node (EPHY_NODE_VIEW (editor->priv->sites_view), pages);
	}
	else
	{
		reset_search_entry (editor);
		pages_filter (editor, node);
	}
}

static void
search_entry_changed_cb (GtkWidget *entry, EphyHistoryWindow *editor)
{
	EphyNode *all;
	char *search_text;

	g_signal_handlers_block_by_func
		(G_OBJECT (editor->priv->sites_view),
		 G_CALLBACK (site_node_selected_cb),
		 editor);
	all = ephy_history_get_pages (editor->priv->history);
	ephy_node_view_select_node (EPHY_NODE_VIEW (editor->priv->sites_view),
				    all);
	g_signal_handlers_unblock_by_func
		(G_OBJECT (editor->priv->sites_view),
		 G_CALLBACK (site_node_selected_cb),
		 editor);

	search_text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);

	GDK_THREADS_ENTER ();

	ephy_node_filter_empty (editor->priv->pages_filter);
	ephy_node_filter_add_expression (editor->priv->pages_filter,
				         ephy_node_filter_expression_new (EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_CONTAINS,
								          EPHY_NODE_PAGE_PROP_TITLE,
								          search_text),
				         0);
	ephy_node_filter_add_expression (editor->priv->pages_filter,
				         ephy_node_filter_expression_new (EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_CONTAINS,
								          EPHY_NODE_PAGE_PROP_LOCATION,
								          search_text),
				         0);
	ephy_node_filter_done_changing (editor->priv->pages_filter);

	GDK_THREADS_LEAVE ();

	g_free (search_text);
}

static GtkWidget *
build_search_box (EphyHistoryWindow *editor)
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
add_widget (EggMenuMerge *merge, GtkWidget *widget, EphyHistoryWindow *editor)
{
	gtk_box_pack_start (GTK_BOX (editor->priv->menu_dock),
			    widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
}

static gboolean
delete_event_cb (EphyHistoryWindow *editor)
{
	gtk_widget_hide (GTK_WIDGET (editor));

	return TRUE;
}

static void
menu_activate_cb (EphyNodeView *view,
	          EphyHistoryWindow *editor)
{
	ephy_history_window_update_menu (editor);
}

static void
provide_favicon (EphyNode *node, GValue *value, gpointer user_data)
{
	EphyFaviconCache *cache;
	const char *icon_location;
	GdkPixbuf *pixbuf = NULL;

	cache = ephy_embed_shell_get_favicon_cache (EPHY_EMBED_SHELL (ephy_shell));
	icon_location = ephy_node_get_property_string
		(node, EPHY_NODE_PAGE_PROP_ICON);

	LOG ("Get favicon for %s", icon_location ? icon_location : "None")

	if (icon_location)
	{
		pixbuf = ephy_favicon_cache_get (cache, icon_location);
	}

	g_value_init (value, GDK_TYPE_PIXBUF);
	g_value_set_object (value, pixbuf);
}

static void
ephy_history_window_construct (EphyHistoryWindow *editor)
{
	GtkTreeViewColumn *col;
	GtkTreeSelection *selection;
	GtkWidget *vbox, *hpaned;
	GtkWidget *pages_view, *sites_view;
	GtkWidget *scrolled_window;
	GtkWidget *menu;
	EphyNode *node;
	EggMenuMerge *ui_merge;
	EggActionGroup *action_group;
	const char *icon_path;
	int i;

	gtk_window_set_title (GTK_WINDOW (editor), _("History"));

	icon_path = ephy_file ("epiphany-history.png");
	gtk_window_set_icon_from_file (GTK_WINDOW (editor), icon_path, NULL);

	g_signal_connect (editor, "delete_event",
			  G_CALLBACK (delete_event_cb), NULL);

	for (i = 0; i < ephy_history_ui_n_entries; i++)
	{
		ephy_history_ui_entries[i].user_data = editor;
	}

	editor->priv->menu_dock = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (editor->priv->menu_dock);
	gtk_container_add (GTK_CONTAINER (editor), editor->priv->menu_dock);

	ui_merge = egg_menu_merge_new ();
	g_signal_connect (ui_merge, "add_widget", G_CALLBACK (add_widget), editor);
	action_group = egg_action_group_new ("PopupActions");
	egg_action_group_add_actions (action_group, ephy_history_ui_entries,
				      ephy_history_ui_n_entries);
	egg_menu_merge_insert_action_group (ui_merge,
					    action_group, 0);
	egg_menu_merge_add_ui_from_file (ui_merge,
				         ephy_file ("epiphany-history-window-ui.xml"),
				         NULL);
	gtk_window_add_accel_group (GTK_WINDOW (editor), ui_merge->accel_group);
	egg_menu_merge_ensure_update (ui_merge);
	editor->priv->ui_merge = ui_merge;
	editor->priv->action_group = action_group;

	/* Update menu sensitivity before showing them */
	menu = egg_menu_merge_get_widget (ui_merge, "/menu/FileMenu");
	g_signal_connect (menu, "activate", G_CALLBACK (menu_activate_cb), editor);

	hpaned = gtk_hpaned_new ();
	gtk_container_set_border_width (GTK_CONTAINER (hpaned), 6);
	gtk_container_add (GTK_CONTAINER (editor->priv->menu_dock), hpaned);
	gtk_widget_show (hpaned);

	g_assert (editor->priv->history);

	/* Sites View */
	node = ephy_history_get_hosts (editor->priv->history);
	scrolled_window = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
					"hadjustment", NULL,
					"vadjustment", NULL,
					"hscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"vscrollbar_policy", GTK_POLICY_AUTOMATIC,
					"shadow_type", GTK_SHADOW_IN,
					NULL);
	gtk_paned_add1 (GTK_PANED (hpaned), scrolled_window);
	gtk_widget_show (scrolled_window);
	sites_view = ephy_node_view_new (node, NULL);
	ephy_node_view_select_node (EPHY_NODE_VIEW (sites_view),
				    ephy_history_get_pages (editor->priv->history));
	ephy_node_view_enable_drag_source (EPHY_NODE_VIEW (sites_view),
					   page_drag_types,
				           n_page_drag_types,
					   EPHY_NODE_PAGE_PROP_LOCATION);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (sites_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	ephy_node_view_add_icon_column (EPHY_NODE_VIEW (sites_view), provide_favicon);
	ephy_node_view_add_column (EPHY_NODE_VIEW (sites_view), _("Sites"),
				   G_TYPE_STRING,
				   EPHY_NODE_PAGE_PROP_TITLE,
				   EPHY_NODE_PAGE_PROP_PRIORITY,
				   EPHY_NODE_VIEW_AUTO_SORT);
	gtk_container_add (GTK_CONTAINER (scrolled_window), sites_view);
	gtk_widget_show (sites_view);
	editor->priv->sites_view = sites_view;
	g_signal_connect (G_OBJECT (sites_view),
			  "node_selected",
			  G_CALLBACK (site_node_selected_cb),
			  editor);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_paned_add2 (GTK_PANED (hpaned), vbox);
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
	node = ephy_history_get_pages (editor->priv->history);
	editor->priv->pages_filter = ephy_node_filter_new ();
	pages_view = ephy_node_view_new (node, editor->priv->pages_filter);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (pages_view), TRUE);
	ephy_node_view_enable_drag_source (EPHY_NODE_VIEW (pages_view),
					   page_drag_types,
				           n_page_drag_types,
					   EPHY_NODE_PAGE_PROP_LOCATION);
	col = ephy_node_view_add_column (EPHY_NODE_VIEW (pages_view), _("Title"),
				         G_TYPE_STRING, EPHY_NODE_PAGE_PROP_TITLE,
				         -1, EPHY_NODE_VIEW_USER_SORT);
	gtk_tree_view_column_set_max_width (col, 250);
	col = ephy_node_view_add_column (EPHY_NODE_VIEW (pages_view), _("Location"),
				         G_TYPE_STRING, EPHY_NODE_PAGE_PROP_LOCATION,
				         -1, EPHY_NODE_VIEW_USER_SORT);
	gtk_tree_view_column_set_max_width (col, 200);
/*	col = ephy_node_view_add_column (EPHY_NODE_VIEW (pages_view), _("Last Visit"),
				         G_TYPE_STRING, EPHY_NODE_PAGE_PROP_LAST_VISIT,
				         -1, EPHY_NODE_VIEW_USER_SORT);*/
	gtk_container_add (GTK_CONTAINER (scrolled_window), pages_view);
	gtk_widget_show (pages_view);
	editor->priv->pages_view = pages_view;
	g_signal_connect (G_OBJECT (pages_view),
			  "node_activated",
			  G_CALLBACK (ephy_history_window_node_activated_cb),
			  editor);
	g_signal_connect (G_OBJECT (pages_view),
			  "show_popup",
			  G_CALLBACK (ephy_history_window_show_popup_cb),
			  editor);

	ephy_state_add_window (GTK_WIDGET (editor),
			       "history_window",
		               450, 400);
	ephy_state_add_paned  (GTK_WIDGET (hpaned),
			       "history_paned",
		               130);
}

void
ephy_history_window_set_parent (EphyHistoryWindow *ebe,
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
ephy_history_window_new (EphyHistory *history)
{
	EphyHistoryWindow *editor;

	g_assert (history != NULL);

	editor = EPHY_HISTORY_WINDOW (g_object_new
			(EPHY_TYPE_HISTORY_WINDOW,
			 "history", history,
			 NULL));

	ephy_history_window_construct (editor);

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
	case PROP_HISTORY:
		editor->priv->history = g_value_get_object (value);
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
	case PROP_HISTORY:
		g_value_set_object (value, editor->priv->history);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_history_window_init (EphyHistoryWindow *editor)
{
	editor->priv = g_new0 (EphyHistoryWindowPrivate, 1);
}
