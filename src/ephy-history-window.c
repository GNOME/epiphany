/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti <mpeseng@tin.it>
 *  Copyright © 2003, 2004 Christian Persch
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
 *  $Id$
 */

#include "config.h"

#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkradioaction.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkhpaned.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkclipboard.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkactiongroup.h>
#include <gtk/gtktoggleaction.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkuimanager.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <string.h>
#include <time.h>

#include "ephy-window.h"
#include "ephy-history-window.h"
#include "ephy-shell.h"
#include "ephy-dnd.h"
#include "ephy-state.h"
#include "window-commands.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"
#include "ephy-stock-icons.h"
#include "ephy-gui.h"
#include "ephy-stock-icons.h"
#include "ephy-search-entry.h"
#include "ephy-session.h"
#include "ephy-favicon-cache.h"
#include "eel-gconf-extensions.h"
#include "ephy-node.h"
#include "ephy-node-common.h"
#include "ephy-node-view.h"
#include "ephy-bookmarks-ui.h"
#include "ephy-prefs.h"
#include "ephy-gui.h"
#include "ephy-time-helpers.h"

static const GtkTargetEntry page_drag_types [] =
{
        { EPHY_DND_URL_TYPE,        0, 0 },
        { EPHY_DND_URI_LIST_TYPE,   0, 1 },
        { EPHY_DND_TEXT_TYPE,       0, 2 }
};

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
static void ephy_history_window_dispose      (GObject *object);

static void cmd_open_bookmarks_in_tabs    (GtkAction *action,
					   EphyHistoryWindow *editor);
static void cmd_open_bookmarks_in_browser (GtkAction *action,
					   EphyHistoryWindow *editor);
static void cmd_delete                    (GtkAction *action,
                                           EphyHistoryWindow *editor);
static void cmd_bookmark_link             (GtkAction *action,
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
static void cmd_view_columns		  (GtkAction *action,
					   EphyHistoryWindow *view);
static void search_entry_search_cb 	  (GtkWidget *entry,
					   char *search_text,
					   EphyHistoryWindow *editor);

#define EPHY_HISTORY_WINDOW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_HISTORY_WINDOW, EphyHistoryWindowPrivate))

#define CONF_HISTORY_DATE_FILTER "/apps/epiphany/dialogs/history_date_filter"
#define CONF_HISTORY_VIEW_DETAILS "/apps/epiphany/dialogs/history_view_details"

struct _EphyHistoryWindowPrivate
{
	EphyHistory *history;
	GtkWidget *sites_view;
	GtkWidget *pages_view;
	EphyNodeFilter *pages_filter;
	EphyNodeFilter *sites_filter;
	GtkWidget *time_combo;
	GtkWidget *search_entry;
	GtkWidget *main_vbox;
	GtkWidget *window;
	GtkUIManager *ui_merge;
	GtkActionGroup *action_group;
	GtkWidget *confirmation_dialog;
	EphyNode *selected_site;
	GtkTreeViewColumn *title_col;
	GtkTreeViewColumn *address_col;
	GtkTreeViewColumn *datetime_col;
};

enum
{
	PROP_0,
	PROP_HISTORY
};

enum
{
	TIME_LAST_HALF_HOUR,
	TIME_TODAY,
	TIME_LAST_TWO_DAYS,
	TIME_LAST_THREE_DAYS,
	TIME_EVER
};

#define TIME_LAST_HALF_HOUR_STRING "last_half_hour"
#define TIME_EVER_STRING "ever"
#define TIME_TODAY_STRING "today"
#define TIME_LAST_TWO_DAYS_STRING "last_two_days"
#define TIME_LAST_THREE_DAYS_STRING "last_three_days"

static GObjectClass *parent_class = NULL;

static const GtkActionEntry ephy_history_ui_entries [] = {
	/* Toplevel */
	{ "File", NULL, N_("_File") },
	{ "Edit", NULL, N_("_Edit") },
	{ "View", NULL, N_("_View") },
	{ "Help", NULL, N_("_Help") },
	{ "PopupAction", NULL, "" },

	/* File Menu */
	{ "OpenInWindow", GTK_STOCK_OPEN, N_("Open in New _Window"), "<control>O",
	  N_("Open the selected history link in a new window"),
	  G_CALLBACK (cmd_open_bookmarks_in_browser) },
	{ "OpenInTab", STOCK_NEW_TAB, N_("Open in New _Tab"), "<shift><control>O",
	  N_("Open the selected history link in a new tab"),
	  G_CALLBACK (cmd_open_bookmarks_in_tabs) },
	{ "BookmarkLink", STOCK_ADD_BOOKMARK, N_("Add _Bookmark…"), "<control>D",
	  N_("Bookmark the selected history link"),
	  G_CALLBACK (cmd_bookmark_link) },
	{ "Close", GTK_STOCK_CLOSE, N_("_Close"), "<control>W",
	  N_("Close the history window"),
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
	{ "Delete", GTK_STOCK_DELETE, N_("_Delete"), "<control>T",
	  N_("Delete the selected history link"),
	  G_CALLBACK (cmd_delete) },
	{ "SelectAll", GTK_STOCK_SELECT_ALL, N_("Select _All"), "<control>A",
	  N_("Select all history links or text"),
	  G_CALLBACK (cmd_select_all) },
	{ "Clear", GTK_STOCK_CLEAR, N_("Clear _History"), NULL,
	  N_("Clear your browsing history"),
	  G_CALLBACK (cmd_clear) },

	/* Help Menu */
	{ "HelpContents", GTK_STOCK_HELP, N_("_Contents"), "F1",
	  N_("Display history help"),
	  G_CALLBACK (cmd_help_contents) },
	{ "HelpAbout", GTK_STOCK_ABOUT, N_("_About"), NULL,
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
	  N_("Show the title column"), G_CALLBACK (cmd_view_columns), TRUE },
	{ "ViewAddress", NULL, N_("_Address"), NULL,
	  N_("Show the address column"), G_CALLBACK (cmd_view_columns), TRUE },
	{ "ViewDateTime", NULL, N_("_Date and Time"), NULL,
	  N_("Show the date and time column"), G_CALLBACK (cmd_view_columns), TRUE }
};

static void
confirmation_dialog_response_cb (GtkWidget *dialog,
				 int response,
				 EphyHistoryWindow *editor)
{
	gtk_widget_destroy (dialog);

	if (response == GTK_RESPONSE_ACCEPT)
	{
		ephy_history_clear (editor->priv->history);
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
	
	button = gtk_button_new_with_label (_("Cl_ear"));
	image = gtk_image_new_from_stock (GTK_STOCK_CLEAR, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image (GTK_BUTTON (button), image);
	/* don't show the image! see bug #307818 */
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_ACCEPT);
	
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Clear History"));
	gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (confirmation_dialog_response_cb),
			  editor);

	return dialog;
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
	selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->pages_view));

	for (l = selection; l; l = l->next)
	{
		EphyNode *node = l->data;
		const char *location;

		location = ephy_node_get_property_string (node,
						EPHY_NODE_PAGE_PROP_LOCATION);

		ephy_shell_new_tab (ephy_shell, window, NULL, location,
			EPHY_NEW_TAB_OPEN_PAGE | EPHY_NEW_TAB_IN_EXISTING_WINDOW);
	}

	g_list_free (selection);
}

static void
cmd_open_bookmarks_in_browser (GtkAction *action,
			       EphyHistoryWindow *editor)
{
	EphyWindow *window;
	GList *selection;
	GList *l;

	window = EPHY_WINDOW (get_target_window (editor));
	selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->pages_view));

	for (l = selection; l; l = l->next)
	{
		EphyNode *node = l->data;
		const char *location;

		location = ephy_node_get_property_string (node,
						EPHY_NODE_PAGE_PROP_LOCATION);

		ephy_shell_new_tab (ephy_shell, window, NULL, location,
				    EPHY_NEW_TAB_OPEN_PAGE |
				    EPHY_NEW_TAB_IN_NEW_WINDOW);
	}

	g_list_free (selection);
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

	else if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->pages_view)))
	{
		GList *selection;

		selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->pages_view));

		if (g_list_length (selection) == 1)
		{
			const char *tmp;
			EphyNode *node = selection->data;
			tmp = ephy_node_get_property_string (node, EPHY_NODE_PAGE_PROP_LOCATION);
			gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), tmp, -1);
		}

		g_list_free (selection);
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
	else if (ephy_node_view_is_target (EPHY_NODE_VIEW (pages_view)))
	{
		GtkTreeSelection *sel;

		sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (pages_view));
		gtk_tree_selection_select_all (sel);
	}
}

static void
cmd_delete (GtkAction *action,
            EphyHistoryWindow *editor)
{
	if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->pages_view)))
	{
		ephy_node_view_remove (EPHY_NODE_VIEW (editor->priv->pages_view));
	}
	else if (ephy_node_view_is_target (EPHY_NODE_VIEW (editor->priv->sites_view)))
	{
		EphyNodePriority priority;
		GList *selected;
		EphyNode *node;

		selected = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->sites_view));
		node = selected->data;
		priority = ephy_node_get_property_int (node, EPHY_NODE_KEYWORD_PROP_PRIORITY);

		if (priority == -1) priority = EPHY_NODE_NORMAL_PRIORITY;

		if (priority == EPHY_NODE_NORMAL_PRIORITY)
		{
			ephy_node_view_remove (EPHY_NODE_VIEW (editor->priv->sites_view));
		}
		
		g_list_free (selected);
	}
}

static void
cmd_bookmark_link (GtkAction *action,
                   EphyHistoryWindow *editor)
{
        GList *selection;

        selection = ephy_node_view_get_selection (EPHY_NODE_VIEW (editor->priv->pages_view));

	if (g_list_length (selection) == 1)
	{
		const char *location;
		const char *title;
		EphyNode *node;

		node = selection->data;
		location = ephy_node_get_property_string (node, EPHY_NODE_PAGE_PROP_LOCATION);
		title = ephy_node_get_property_string (node, EPHY_NODE_PAGE_PROP_TITLE);

		ephy_bookmarks_ui_add_bookmark (GTK_WINDOW (editor), location, title);
	}

	g_list_free (selection);
}

static void
cmd_help_contents (GtkAction *action,
		   EphyHistoryWindow *editor)
{
	ephy_gui_help (GTK_WINDOW (editor),
		       "epiphany", 
		       "ephy-managing-history");
}

static void
set_column_visibility (EphyHistoryWindow *view,
		       const char *action_name,
		       gboolean active)
{
	if (strcmp (action_name, "ViewTitle") == 0)
	{
		gtk_tree_view_column_set_visible (view->priv->title_col, active);
	}
	if (strcmp (action_name, "ViewAddress") == 0)
	{
		gtk_tree_view_column_set_visible (view->priv->address_col, active);
	}
	if (strcmp (action_name, "ViewDateTime") == 0)
	{
		gtk_tree_view_column_set_visible (view->priv->datetime_col, active);
	}
}

static void
set_all_columns_visibility (EphyHistoryWindow *view,
			    EphyHistoryWindowColumns details_value)
{
	GtkActionGroup *action_group;
	GtkAction *action_title, *action_address, *action_datetime;

	action_group = view->priv->action_group;
	action_title = gtk_action_group_get_action (action_group, "ViewTitle");
	action_address = gtk_action_group_get_action (action_group, "ViewAddress");
	action_datetime = gtk_action_group_get_action (action_group, "ViewDateTime");
	
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action_title), (details_value & VIEW_TITLE));
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action_address), (details_value & VIEW_ADDRESS));
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action_datetime), (details_value & VIEW_DATETIME));
}

static void
cmd_view_columns (GtkAction *action,
		  EphyHistoryWindow *view)
{
	gboolean active;
	const char *action_name;
	GSList *svalues = NULL;

	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	action_name = gtk_action_get_name (action);
	set_column_visibility (view, action_name, active);
	
	svalues = eel_gconf_get_string_list (CONF_HISTORY_VIEW_DETAILS);

	if (active)
	{
		if (!g_slist_find_custom (svalues, (gpointer) action_name, (GCompareFunc) strcmp))
		{
			svalues = g_slist_append (svalues, (gpointer) action_name);
		}
	}
	else
	{
		GSList *delete;
		delete = g_slist_find_custom (svalues, (gpointer) action_name, (GCompareFunc) strcmp);
		if (delete)
		{
			svalues = g_slist_delete_link (svalues, delete);
		}
	}

	eel_gconf_set_string_list (CONF_HISTORY_VIEW_DETAILS, svalues);
	g_slist_free (svalues);
}

GType
ephy_history_window_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
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

		type = g_type_register_static (GTK_TYPE_WINDOW,
					       "EphyHistoryWindow",
					       &our_info, 0);
	}

	return type;
}

static void
ephy_history_window_show (GtkWidget *widget)
{
	EphyHistoryWindow *window = EPHY_HISTORY_WINDOW (widget);

	gtk_widget_grab_focus (window->priv->search_entry);

	GTK_WIDGET_CLASS (parent_class)->show (widget);
}

static void
ephy_history_window_class_init (EphyHistoryWindowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_history_window_finalize;

	object_class->set_property = ephy_history_window_set_property;
	object_class->get_property = ephy_history_window_get_property;
	object_class->dispose  = ephy_history_window_dispose;

	widget_class->show = ephy_history_window_show;

	g_object_class_install_property (object_class,
					 PROP_HISTORY,
					 g_param_spec_object ("history",
							      "Global history",
							      "Global History",
							      EPHY_TYPE_HISTORY,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(EphyHistoryWindowPrivate));
}

static void
ephy_history_window_finalize (GObject *object)
{
	EphyHistoryWindow *editor = EPHY_HISTORY_WINDOW (object);

	g_object_unref (G_OBJECT (editor->priv->pages_filter));
	g_object_unref (G_OBJECT (editor->priv->sites_filter));

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
ephy_history_window_node_activated_cb (GtkWidget *view,
				         EphyNode *node,
					 EphyHistoryWindow *editor)
{
	const char *location;

	location = ephy_node_get_property_string
		(node, EPHY_NODE_PAGE_PROP_LOCATION);
	g_return_if_fail (location != NULL);

	ephy_shell_new_tab (ephy_shell, NULL, NULL, location,
			    EPHY_NEW_TAB_OPEN_PAGE);
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

	pages_focus = ephy_node_view_is_target
		(EPHY_NODE_VIEW (editor->priv->pages_view));
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
	bookmarks_locked = eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_BOOKMARK_EDITING);
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
view_focus_cb (EphyNodeView *view,
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
	ephy_node_view_popup (EPHY_NODE_VIEW (view), widget);

	return TRUE;
}

static gboolean
key_pressed_cb (EphyNodeView *view,
		GdkEventKey *event,
		EphyHistoryWindow *editor)
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
add_by_site_filter (EphyHistoryWindow *editor, EphyNodeFilter *filter, int level)
{
	if (editor->priv->selected_site == NULL) return;

	ephy_node_filter_add_expression
		(filter, ephy_node_filter_expression_new
				(EPHY_NODE_FILTER_EXPRESSION_HAS_PARENT,
				 editor->priv->selected_site),
		 level);
}

static void
add_by_date_filter (EphyHistoryWindow *editor,
		    EphyNodeFilter *filter,
		    int level,
		    EphyNode *equals)
{
	time_t now, midnight, cmp_time = 0;
	struct tm btime;
	int time_range, days = 0;

	time_range = gtk_combo_box_get_active
		(GTK_COMBO_BOX (editor->priv->time_combo));

	/* no need to setup a new filter */
	if (time_range == TIME_EVER) return;

	now = time (NULL);
	if (localtime_r (&now, &btime) == NULL) return;

	/* get start of day */
	btime.tm_sec = 0;
	btime.tm_min = 0;
	btime.tm_hour = 0;
	midnight = mktime (&btime);

	switch (time_range)
	{
		case TIME_LAST_HALF_HOUR:
			cmp_time = now - 30 * 60;
			break;
		case TIME_LAST_THREE_DAYS:
			days++;
			/* fall-through */
		case TIME_LAST_TWO_DAYS:
			days++;
			/* fall-through */
		case TIME_TODAY:
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

	ephy_node_filter_add_expression
		(filter, ephy_node_filter_expression_new
				(EPHY_NODE_FILTER_EXPRESSION_INT_PROP_BIGGER_THAN,
				 EPHY_NODE_PAGE_PROP_LAST_VISIT, cmp_time),
		 level);

	if (equals == NULL) return;

	ephy_node_filter_add_expression
		(filter, ephy_node_filter_expression_new
				(EPHY_NODE_FILTER_EXPRESSION_EQUALS, equals),
		 0);
}

static void
add_by_word_filter (EphyHistoryWindow *editor, EphyNodeFilter *filter, int level)
{
	const char *search_text;

	search_text = gtk_entry_get_text (GTK_ENTRY (ephy_icon_entry_get_entry
						     (EPHY_ICON_ENTRY (editor->priv->search_entry))));
	if (search_text == NULL) return;

	ephy_node_filter_add_expression
		(filter, ephy_node_filter_expression_new
				(EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_CONTAINS,
				 EPHY_NODE_PAGE_PROP_TITLE, search_text),
		 level);

	ephy_node_filter_add_expression
		(filter, ephy_node_filter_expression_new
				(EPHY_NODE_FILTER_EXPRESSION_STRING_PROP_CONTAINS,
				 EPHY_NODE_PAGE_PROP_LOCATION, search_text),
		 level);
}

static void
setup_filters (EphyHistoryWindow *editor,
	       gboolean pages, gboolean sites)
{
	LOG ("Setup filters for pages %d and sites %d", pages, sites);

	if (pages)
	{
		ephy_node_filter_empty (editor->priv->pages_filter);

		add_by_date_filter (editor, editor->priv->pages_filter, 0, NULL);
		add_by_word_filter (editor, editor->priv->pages_filter, 1);
		add_by_site_filter (editor, editor->priv->pages_filter, 2);

		ephy_node_filter_done_changing (editor->priv->pages_filter);
	}

	if (sites)
	{
		ephy_node_filter_empty (editor->priv->sites_filter);

		add_by_date_filter (editor, editor->priv->sites_filter, 0,
				    ephy_history_get_pages (editor->priv->history));

		ephy_node_filter_done_changing (editor->priv->sites_filter);
	}
}

static void
site_node_selected_cb (EphyNodeView *view,
		       EphyNode *node,
		       EphyHistoryWindow *editor)
{
	EphyNode *pages;

	if (editor->priv->selected_site == node) return;

	editor->priv->selected_site = node;

	if (node == NULL)
	{
		pages = ephy_history_get_pages (editor->priv->history);
		ephy_node_view_select_node (EPHY_NODE_VIEW (editor->priv->sites_view), pages);
	}
	else
	{
		g_signal_handlers_block_by_func (EPHY_SEARCH_ENTRY (editor->priv->search_entry),
						 G_CALLBACK (search_entry_search_cb),
						 editor);
		ephy_search_entry_clear (EPHY_SEARCH_ENTRY (editor->priv->search_entry));
		g_signal_handlers_unblock_by_func (EPHY_SEARCH_ENTRY (editor->priv->search_entry),
						   G_CALLBACK (search_entry_search_cb),
						   editor);
		setup_filters (editor, TRUE, FALSE);
	}
}

static void
search_entry_search_cb (GtkWidget *entry, char *search_text, EphyHistoryWindow *editor)
{
	EphyNode *all;

	g_signal_handlers_block_by_func
		(G_OBJECT (editor->priv->sites_view),
		 G_CALLBACK (site_node_selected_cb),
		 editor);
	all = ephy_history_get_pages (editor->priv->history);
	editor->priv->selected_site = all;
	ephy_node_view_select_node (EPHY_NODE_VIEW (editor->priv->sites_view),
				    all);
	g_signal_handlers_unblock_by_func
		(G_OBJECT (editor->priv->sites_view),
		 G_CALLBACK (site_node_selected_cb),
		 editor);

	setup_filters (editor, TRUE, FALSE);
}

static void
time_combo_changed_cb (GtkWidget *combo, EphyHistoryWindow *editor)
{
	setup_filters (editor, TRUE, TRUE);
}

static gboolean
search_entry_clear_cb (GtkWidget *ebox,
		       GdkEventButton *event,
		       GtkWidget *entry)
{
	guint state = event->state & gtk_accelerator_get_default_mod_mask ();
	
	if (event->type == GDK_BUTTON_RELEASE && 
	    event->button == 1 /* left */ && 
	    state == 0)
	{	
		ephy_search_entry_clear (EPHY_SEARCH_ENTRY (entry));
		
		return TRUE;
	}
	
	return FALSE;
}

static GtkWidget *
build_search_box (EphyHistoryWindow *editor)
{
	GtkWidget *box, *label, *entry;
	GtkWidget *combo;
	GtkWidget *cleaner, *ebox;
	char *str;
	int time_range;

	box = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (box), 6);
	gtk_widget_show (box);

	entry = ephy_search_entry_new ();
	add_focus_monitor (editor, entry);
	add_entry_monitor (editor, entry);
	editor->priv->search_entry = entry;
    
	cleaner = gtk_image_new_from_stock (GTK_STOCK_CLEAR,
					    GTK_ICON_SIZE_MENU);
	ebox = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);
	
	gtk_widget_add_events (ebox, GDK_BUTTON_PRESS_MASK |
			       GDK_BUTTON_RELEASE_MASK);
	g_signal_connect (ebox , "button-release-event",
			  G_CALLBACK (search_entry_clear_cb), 
			  entry);
	gtk_widget_set_tooltip_text (ebox,
			             _("Clear"));
	gtk_container_add (GTK_CONTAINER (ebox), cleaner);
	ephy_icon_entry_pack_widget ((EPHY_ICON_ENTRY (entry)), ebox, FALSE);
    
	gtk_widget_show_all (entry);

	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	str = g_strconcat ("<b>", _("_Search:"), "</b>", NULL);
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), str);
	g_free (str);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show (label);

	combo = gtk_combo_box_new_text ();
	gtk_widget_show (combo);

	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Last 30 minutes"));
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Today"));
	str = g_strdup_printf (ngettext ("Last %d day", "Last %d days", 2), 2);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), str);
	g_free (str);
	str = g_strdup_printf (ngettext ("Last %d day", "Last %d days", 3), 3);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), str);
	g_free (str);
	/* keep this in sync with embed/ephy-history.c's HISTORY_PAGE_OBSOLETE_DAYS */
	str = g_strdup_printf (ngettext ("Last %d day", "Last %d days", 10), 10);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), str);
	g_free (str);

	str = eel_gconf_get_string (CONF_HISTORY_DATE_FILTER);
	if (str && strcmp (TIME_LAST_HALF_HOUR_STRING, str) == 0)
	{
		time_range = TIME_LAST_HALF_HOUR;
	}
	if (str && strcmp (TIME_TODAY_STRING, str) == 0)
	{
		time_range = TIME_TODAY;
	}
	else if (str && strcmp (TIME_LAST_TWO_DAYS_STRING, str) == 0)
	{
		time_range = TIME_LAST_TWO_DAYS;
	}
	else if (str && strcmp (TIME_LAST_THREE_DAYS_STRING, str) == 0)
	{
		time_range = TIME_LAST_THREE_DAYS;
	}
	else
	{
		time_range = TIME_EVER;
	}
	g_free (str);

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo),
				  time_range);

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

static void
provide_favicon (EphyNode *node, GValue *value, gpointer user_data)
{
	EphyFaviconCache *cache;
	const char *icon_location;
	GdkPixbuf *pixbuf = NULL;

	cache = EPHY_FAVICON_CACHE
		(ephy_embed_shell_get_favicon_cache (EPHY_EMBED_SHELL (ephy_shell)));
	icon_location = ephy_node_get_property_string
		(node, EPHY_NODE_PAGE_PROP_ICON);

	LOG ("Get favicon for %s", icon_location ? icon_location : "None");

	if (icon_location)
	{
		pixbuf = ephy_favicon_cache_get (cache, icon_location);
	}

	g_value_init (value, GDK_TYPE_PIXBUF);
	g_value_take_object (value, pixbuf);
}

static void
convert_cell_data_func (GtkTreeViewColumn *column,
			GtkCellRenderer *renderer,
			GtkTreeModel *model,
			GtkTreeIter *iter,
			gpointer user_data)
{
	int col_id = (int) user_data;
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
	
	renderers_list = gtk_tree_view_column_get_cell_renderers (column);
	renderer = GTK_CELL_RENDERER (renderers_list->data);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 (GtkTreeCellDataFunc) convert_cell_data_func,
						 (gpointer) column_id,
						 NULL);
	g_list_free (renderers_list);
}
static void
view_selection_changed_cb (GtkWidget *view, EphyHistoryWindow *editor)
{
	ephy_history_window_update_menu (editor);
}

static EphyHistoryWindowColumns
get_details_value (void)
{
	guint value = 0;
	GSList *svalues;

	svalues = eel_gconf_get_string_list (CONF_HISTORY_VIEW_DETAILS);
	if (svalues == NULL) 
	{
		svalues = g_slist_append (svalues, (gpointer) "ViewAddress");
		svalues = g_slist_append (svalues, (gpointer) "ViewTitle");
		eel_gconf_set_string_list (CONF_HISTORY_VIEW_DETAILS, svalues);
		return (VIEW_ADDRESS | VIEW_TITLE);
	}

	if (g_slist_find_custom (svalues, "ViewTitle", (GCompareFunc)strcmp))
	{
		value |= VIEW_TITLE;
	}
	if (g_slist_find_custom (svalues, "ViewAddress", (GCompareFunc)strcmp))
	{
		value |= VIEW_ADDRESS;
	}
	if (g_slist_find_custom (svalues, "ViewDateTime", (GCompareFunc)strcmp))
	{
		value |= VIEW_DATETIME;
	}

	g_slist_foreach (svalues, (GFunc) g_free, NULL);
	g_slist_free (svalues);

	return value;
}

static void
ephy_history_window_construct (EphyHistoryWindow *editor)
{
	GtkTreeViewColumn *col;
	GtkTreeSelection *selection;
	GtkWidget *vbox, *hpaned;
	GtkWidget *pages_view, *sites_view;
	GtkWidget *scrolled_window;
	EphyNode *node;
	GtkUIManager *ui_merge;
	GtkActionGroup *action_group;
	int url_col_id, title_col_id, datetime_col_id;
	EphyHistoryWindowColumns details_value;

	ephy_gui_ensure_window_group (GTK_WINDOW (editor));

	gtk_window_set_title (GTK_WINDOW (editor), _("History"));
	gtk_window_set_icon_name (GTK_WINDOW (editor), EPHY_STOCK_HISTORY);

	g_signal_connect (editor, "delete_event",
			  G_CALLBACK (delete_event_cb), NULL);

	editor->priv->main_vbox = gtk_vbox_new (FALSE, 0);
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

	details_value = get_details_value ();
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

	hpaned = gtk_hpaned_new ();
	gtk_container_set_border_width (GTK_CONTAINER (hpaned), 0);
	gtk_box_pack_end (GTK_BOX (editor->priv->main_vbox), hpaned,
			  TRUE, TRUE, 0);
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
	gtk_paned_pack1 (GTK_PANED (hpaned), scrolled_window, TRUE, FALSE);
	gtk_widget_show (scrolled_window);
	editor->priv->sites_filter = ephy_node_filter_new ();
	sites_view = ephy_node_view_new (node, editor->priv->sites_filter);
	add_focus_monitor (editor, sites_view);
	url_col_id = ephy_node_view_add_data_column (EPHY_NODE_VIEW (sites_view),
					             G_TYPE_STRING,
					             EPHY_NODE_PAGE_PROP_LOCATION,
						     NULL, NULL);
	title_col_id = ephy_node_view_add_column (EPHY_NODE_VIEW (sites_view), _("Sites"),
						  G_TYPE_STRING,
						  EPHY_NODE_PAGE_PROP_TITLE,
						  EPHY_NODE_VIEW_SEARCHABLE |
						  EPHY_NODE_VIEW_SHOW_PRIORITY,
						  provide_favicon,
						  NULL);
	ephy_node_view_enable_drag_source (EPHY_NODE_VIEW (sites_view),
					   page_drag_types,
				           G_N_ELEMENTS (page_drag_types),
					   url_col_id,
					   title_col_id);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (sites_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	ephy_node_view_set_priority (EPHY_NODE_VIEW (sites_view),
				     EPHY_NODE_PAGE_PROP_PRIORITY);
	ephy_node_view_set_sort (EPHY_NODE_VIEW (sites_view), G_TYPE_STRING,
				 EPHY_NODE_PAGE_PROP_TITLE,
				 GTK_SORT_ASCENDING);
	gtk_container_add (GTK_CONTAINER (scrolled_window), sites_view);
	gtk_widget_show (sites_view);
	editor->priv->sites_view = sites_view;
	editor->priv->selected_site = ephy_history_get_pages (editor->priv->history);
	ephy_node_view_select_node (EPHY_NODE_VIEW (sites_view),
				    editor->priv->selected_site);

	g_signal_connect (G_OBJECT (sites_view),
			  "node_selected",
			  G_CALLBACK (site_node_selected_cb),
			  editor);
	g_signal_connect (G_OBJECT (sites_view),
			  "key_press_event",
			  G_CALLBACK (key_pressed_cb),
			  editor);
	g_signal_connect (G_OBJECT (selection),
			  "changed",
			  G_CALLBACK (view_selection_changed_cb),
			  editor);

	vbox = gtk_vbox_new (FALSE, 0);
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
	node = ephy_history_get_pages (editor->priv->history);
	editor->priv->pages_filter = ephy_node_filter_new ();
	pages_view = ephy_node_view_new (node, editor->priv->pages_filter);
	add_focus_monitor (editor, pages_view);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (pages_view));
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (pages_view), TRUE);
	title_col_id = ephy_node_view_add_column (EPHY_NODE_VIEW (pages_view), _("Title"),
				                  G_TYPE_STRING, EPHY_NODE_PAGE_PROP_TITLE,
				                  EPHY_NODE_VIEW_SORTABLE |
					          EPHY_NODE_VIEW_SEARCHABLE |
						  EPHY_NODE_VIEW_ELLIPSIZED, NULL, &col);
	gtk_tree_view_column_set_min_width (col, 300);
	gtk_tree_view_column_set_resizable (col, TRUE);
	editor->priv->title_col = col;
	
	url_col_id = ephy_node_view_add_column (EPHY_NODE_VIEW (pages_view), _("Address"),
				                G_TYPE_STRING, EPHY_NODE_PAGE_PROP_LOCATION,
				                EPHY_NODE_VIEW_SORTABLE |
						EPHY_NODE_VIEW_ELLIPSIZED, NULL, &col);
	gtk_tree_view_column_set_min_width (col, 300);
	gtk_tree_view_column_set_resizable (col, TRUE);
	editor->priv->address_col = col;

	datetime_col_id = ephy_node_view_add_column (EPHY_NODE_VIEW (pages_view), _("Date"),
						     G_TYPE_INT, EPHY_NODE_PAGE_PROP_LAST_VISIT,
						     EPHY_NODE_VIEW_SORTABLE, NULL, &col);
	editor->priv->datetime_col = col;
	parse_time_into_date (editor->priv->datetime_col, datetime_col_id);

	ephy_node_view_enable_drag_source (EPHY_NODE_VIEW (pages_view),
					   page_drag_types,
				           G_N_ELEMENTS (page_drag_types),
					   url_col_id, title_col_id);
	ephy_node_view_set_sort (EPHY_NODE_VIEW (pages_view), G_TYPE_INT,
				 EPHY_NODE_PAGE_PROP_LAST_VISIT,
				 GTK_SORT_DESCENDING);
	gtk_container_add (GTK_CONTAINER (scrolled_window), pages_view);
	gtk_widget_show (pages_view);
	editor->priv->pages_view = pages_view;

	g_signal_connect (G_OBJECT (pages_view),
			  "node_activated",
			  G_CALLBACK (ephy_history_window_node_activated_cb),
			  editor);
	g_signal_connect (G_OBJECT (pages_view),
			  "popup_menu",
			  G_CALLBACK (ephy_history_window_show_popup_cb),
			  editor);
	g_signal_connect (G_OBJECT (pages_view),
			  "key_press_event",
			  G_CALLBACK (key_pressed_cb),
			  editor);
	g_signal_connect (G_OBJECT (selection),
			  "changed",
			  G_CALLBACK (view_selection_changed_cb),
			  editor);

	ephy_state_add_window (GTK_WIDGET (editor),
			       "history_window",
		               450, 400, FALSE,
			       EPHY_STATE_WINDOW_SAVE_SIZE | EPHY_STATE_WINDOW_SAVE_POSITION);
	ephy_state_add_paned  (GTK_WIDGET (hpaned),
			       "history_paned",
		               130);

	set_all_columns_visibility (editor, details_value);
	setup_filters (editor, TRUE, TRUE);
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
	editor->priv = EPHY_HISTORY_WINDOW_GET_PRIVATE (editor);
}

static void
save_date_filter (EphyHistoryWindow *editor)
{
	const char *time_string = NULL;
	int time_range;

	time_range = gtk_combo_box_get_active
		(GTK_COMBO_BOX (editor->priv->time_combo));

	switch (time_range)
	{
		case TIME_LAST_HALF_HOUR:
			time_string = TIME_LAST_HALF_HOUR_STRING;
			break;
		case TIME_EVER:
			time_string = TIME_EVER_STRING;
			break;
		case TIME_TODAY:
			time_string = TIME_TODAY_STRING;
			break;
		case TIME_LAST_TWO_DAYS:
			time_string = TIME_LAST_TWO_DAYS_STRING;
			break;
		case TIME_LAST_THREE_DAYS:
			time_string = TIME_LAST_THREE_DAYS_STRING;
			break;
	}

	eel_gconf_set_string (CONF_HISTORY_DATE_FILTER, time_string);
}

static void
ephy_history_window_dispose (GObject *object)
{
	EphyHistoryWindow *editor;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EPHY_IS_HISTORY_WINDOW (object));

	editor = EPHY_HISTORY_WINDOW (object);

	if (editor->priv->sites_view != NULL)
	{
		remove_focus_monitor (editor, editor->priv->pages_view);
		remove_focus_monitor (editor, editor->priv->sites_view);
		remove_focus_monitor (editor, editor->priv->search_entry);

		editor->priv->sites_view = NULL;

		save_date_filter (editor);
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}
