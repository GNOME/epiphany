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
#include "ephy-gui.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-time-helpers.h"
#include "ephy-window.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <time.h>

#define NUM_RESULTS_LIMIT -1

struct _EphyHistoryWindowPrivate
{
	EphyHistoryService *history_service;
	GCancellable *cancellable;

	GtkWidget *treeview;
	GtkWidget *liststore;
	GtkWidget *date_column;
	GtkWidget *date_renderer;
	GtkWidget *remove_button;
	GtkWidget *open_button;
	GtkWidget *clear_button;
	GtkWidget *open_menuitem;
	GtkWidget *bookmark_menuitem;
	GtkWidget *delete_menuitem;
	GtkWidget *copy_location_menuitem;
	GtkWidget *treeview_popup_menu;

	char *search_text;
	GtkWidget *window;

	GtkWidget *confirmation_dialog;
};

G_DEFINE_TYPE_WITH_PRIVATE (EphyHistoryWindow, ephy_history_window, GTK_TYPE_DIALOG)

enum
{
	PROP_0,
	PROP_HISTORY_SERVICE,
};

typedef enum
{
	COLUMN_DATE,
	COLUMN_NAME,
	COLUMN_LOCATION
} EphyHistoryWindowColumns;

static void
add_urls (GtkListStore *store,
	  GList *urls)
{
	EphyHistoryURL *url;
	GList *iter;

	for (iter = urls; iter != NULL; iter = iter->next) {
		url = (EphyHistoryURL *)iter->data;
		gtk_list_store_insert_with_values (store,
						   NULL, G_MAXINT,
						   COLUMN_DATE, url->last_visit_time,
						   COLUMN_NAME, url->title,
						   COLUMN_LOCATION, url->url,
						   -1);
	}
}

static void
on_find_urls_cb (gpointer service,
		 gboolean success,
		 gpointer result_data,
		 gpointer user_data)
{
	EphyHistoryWindow *self = EPHY_HISTORY_WINDOW (user_data);
	GList *urls;

	if (success != TRUE)
		return;

	urls = (GList *)result_data;
	gtk_list_store_clear (GTK_LIST_STORE (self->priv->liststore));
	add_urls (GTK_LIST_STORE (self->priv->liststore), urls);
	g_list_free_full (urls, (GDestroyNotify)ephy_history_url_free);
}

static GList *
substrings_filter (EphyHistoryWindow *self)
{
	char **tokens, **p;
	GList *substrings = NULL;

	if (self->priv->search_text == NULL)
		return NULL;

	tokens = p = g_strsplit (self->priv->search_text, " ", -1);

	while (*p) {
		substrings = g_list_prepend (substrings, *p++);
	};
	g_free (tokens);

	return substrings;
}

static void
filter_now (EphyHistoryWindow *self)
{
	gint64 from, to;
	GList *substrings;

	substrings = substrings_filter (self);

	from = to = -1; /* all */
	ephy_history_service_find_urls (self->priv->history_service,
					from, to,
					NUM_RESULTS_LIMIT, 0,
					substrings,
					EPHY_HISTORY_SORT_MOST_RECENTLY_VISITED,
					self->priv->cancellable,
					(EphyHistoryJobCallback)on_find_urls_cb, self);
}

static void
confirmation_dialog_response_cb (GtkWidget *dialog,
				 int response,
				 EphyHistoryWindow *self)
{
	gtk_widget_destroy (dialog);

	if (response == GTK_RESPONSE_ACCEPT)
	{
		ephy_history_service_clear (self->priv->history_service,
					    NULL, NULL, NULL);
		filter_now (self);
	}
}

static GtkWidget *
confirmation_dialog_construct (EphyHistoryWindow *self)
{
	GtkWidget *dialog, *button;

	dialog = gtk_message_dialog_new
		(GTK_WINDOW (self),
		 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		 GTK_MESSAGE_WARNING,
		 GTK_BUTTONS_CANCEL,
		 _("Clear browsing history?"));

	gtk_message_dialog_format_secondary_text
		(GTK_MESSAGE_DIALOG (dialog),
		 _("Clearing the browsing history will cause all"
		   " history links to be permanently deleted."));

	gtk_window_group_add_window (ephy_gui_ensure_window_group (GTK_WINDOW (self)),
				     GTK_WINDOW (dialog));
	
	button = gtk_button_new_with_mnemonic (_("Cl_ear"));
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_ACCEPT);
	
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (confirmation_dialog_response_cb),
			  self);

	return dialog;
}

static void
clear_all_history (EphyHistoryWindow *self)
{
	if (self->priv->confirmation_dialog == NULL)
	{
		GtkWidget **confirmation_dialog;

		self->priv->confirmation_dialog = confirmation_dialog_construct (self);
		confirmation_dialog = &self->priv->confirmation_dialog;
		g_object_add_weak_pointer (G_OBJECT (self->priv->confirmation_dialog),
					   (gpointer *) confirmation_dialog);
	}

	gtk_widget_show (self->priv->confirmation_dialog);
}

static GtkWidget *
get_target_window (EphyHistoryWindow *self)
{
	if (self->priv->window)
	{
		return self->priv->window;
	}
	else
	{
		return GTK_WIDGET (gtk_application_get_active_window (GTK_APPLICATION (ephy_shell_get_default ())));
	}
}

static void
on_browse_history_deleted_cb (gpointer service,
			      gboolean success,
			      gpointer result_data,
			      gpointer user_data)
{
	EphyHistoryWindow *self = EPHY_HISTORY_WINDOW (user_data);

	if (success != TRUE)
		return;

	filter_now (self);
}

static EphyHistoryURL *
get_url_from_path (GtkTreeModel *model,
		   GtkTreePath *path)
{
	GtkTreeIter iter;

	EphyHistoryURL *url = ephy_history_url_new (NULL, NULL, 0, 0, 0);

	gtk_tree_model_get_iter (model, &iter, path);

	gtk_tree_model_get (model, &iter,
			    COLUMN_NAME, &url->title,
			    COLUMN_LOCATION, &url->url,
			    -1);
	return url;
}

static void
get_selection_foreach (GtkTreeModel *model,
		       GtkTreePath *path,
		       GtkTreeIter *iter,
		       gpointer *data)
{
	EphyHistoryURL *url;

	url = get_url_from_path (model, path);
	*data = g_list_prepend (*data, url);
}

static GList *
get_selection (EphyHistoryWindow *self)
{
	GtkTreeSelection *selection;
	GList *list = NULL;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->treeview));
	gtk_tree_selection_selected_foreach (selection,
					     (GtkTreeSelectionForeachFunc) get_selection_foreach,
					     &list);

	return g_list_reverse (list);
}

static void
delete_selected (EphyHistoryWindow *self)
{
	GList *selected;

	selected = get_selection (self);
	ephy_history_service_delete_urls (self->priv->history_service, selected, self->priv->cancellable,
					  (EphyHistoryJobCallback)on_browse_history_deleted_cb, self);
}

static void
open_selected (EphyHistoryWindow *self)
{
	GList *selection;
	GList *l;
	EphyWindow *window;

	selection = get_selection (self);

	window = EPHY_WINDOW (get_target_window (self));
	for (l = selection; l; l = l->next) {
		EphyHistoryURL *url = l->data;
		ephy_shell_new_tab (ephy_shell_get_default (),
				    window, NULL, url->url,
				    EPHY_NEW_TAB_OPEN_PAGE |
				    EPHY_NEW_TAB_IN_EXISTING_WINDOW);
	}

	g_list_free_full (selection, (GDestroyNotify) ephy_history_url_free);
}

static void
on_open_menuitem_activate (GtkMenuItem *menuitem,
			   EphyHistoryWindow *self)
{
	open_selected (self);
}

static void
on_copy_location_menuitem_activate (GtkMenuItem *menuitem,
				    EphyHistoryWindow *self)
{
	GList *selection;

	selection = get_selection (self);

	if (g_list_length (selection) == 1) {
		EphyHistoryURL *url = selection->data;
		g_message ("URL %s", url->url);
		gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), url->url, -1);
	}

	g_list_free_full (selection, (GDestroyNotify) ephy_history_url_free);
}

static void
on_bookmark_menuitem_activate (GtkMenuItem *menuitem,
			       EphyHistoryWindow *self)
{
	GList *selection;

	selection = get_selection (self);

	if (g_list_length (selection) == 1)
	{
		EphyHistoryURL *url;

		url = selection->data;

		ephy_bookmarks_ui_add_bookmark (GTK_WINDOW (self), url->url, url->title);
	}

	g_list_free_full (selection, (GDestroyNotify) ephy_history_url_free);
}

static void
on_delete_menuitem_activate (GtkMenuItem *menuitem,
			     EphyHistoryWindow *self)
{
	delete_selected (self);
}

static gboolean
on_treeview_key_press_event (GtkWidget         *widget,
			     GdkEventKey       *event,
			     EphyHistoryWindow *self)
{
	if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete)
	{
		delete_selected (self);

		return TRUE;
	}

	return FALSE;
}

static gboolean
on_treeview_button_press_event (GtkWidget         *widget,
				GdkEventButton    *event,
				EphyHistoryWindow *self)
{
	if (event->button == 3) {
		GtkTreeSelection *selection;
		int n;
		gboolean bookmarks_locked;

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->treeview));
		n = gtk_tree_selection_count_selected_rows (selection);
		if (n == 0)
			return FALSE;

		gtk_widget_set_sensitive (self->priv->open_menuitem, (n > 0));
		gtk_widget_set_sensitive (self->priv->copy_location_menuitem, (n > 0));
		gtk_widget_set_sensitive (self->priv->delete_menuitem, (n > 0));

		bookmarks_locked = g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
							   EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING);
		gtk_widget_set_sensitive (self->priv->bookmark_menuitem, (n == 1 && !bookmarks_locked));

		gtk_menu_popup (GTK_MENU (self->priv->treeview_popup_menu),
				NULL, NULL, NULL, NULL,
				event->button, event->time);
		return TRUE;
	}

	return FALSE;
}

static void
on_treeview_row_activated (GtkTreeView *view,
			   GtkTreePath *path,
			   GtkTreeViewColumn *col,
			   EphyHistoryWindow *self)
{
	EphyWindow *window;
	EphyHistoryURL *url;

	window = EPHY_WINDOW (get_target_window (self));
	url = get_url_from_path (gtk_tree_view_get_model (view),
				 path);
	g_return_if_fail (url != NULL);

	ephy_shell_new_tab (ephy_shell_get_default (),
			    window, NULL, url->url,
			    EPHY_NEW_TAB_OPEN_PAGE |
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW);
	ephy_history_url_free (url);
}

static void
on_search_entry_changed (GtkSearchEntry *entry,
			 EphyHistoryWindow *self)
{
	const char *text;

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	g_free (self->priv->search_text);
	self->priv->search_text = g_strdup (text);

	filter_now (self);
}

static void
on_treeview_selection_changed (GtkTreeSelection *selection,
			       EphyHistoryWindow *self)
{
	gboolean has_selection;

	has_selection = gtk_tree_selection_count_selected_rows (selection) > 0;

	gtk_widget_set_sensitive (self->priv->remove_button, has_selection);
	gtk_widget_set_sensitive (self->priv->open_button, has_selection);
}

static void
on_remove_button_clicked (GtkButton *button,
			  EphyHistoryWindow *self)
{
	delete_selected (self);

	/* Restore the focus to the button */
	gtk_widget_grab_focus (GTK_WIDGET (button));
}

static void
on_open_button_clicked (GtkButton *button,
			EphyHistoryWindow *self)
{
	open_selected (self);
}

static gboolean
on_urls_visited_cb (EphyHistoryService *service,
		    EphyHistoryWindow *self)
{
	filter_now (self);

	return FALSE;
}

static void
set_history_service (EphyHistoryWindow *self,
		     EphyHistoryService *history_service)
{
	if (history_service == self->priv->history_service)
		return;

	if (self->priv->history_service != NULL) {
		g_signal_handlers_disconnect_by_func (self->priv->history_service,
						      on_urls_visited_cb,
						      self);
		g_clear_object (&self->priv->history_service);
	}

	if (history_service != NULL) {
		self->priv->history_service = g_object_ref (history_service);
		g_signal_connect_after (self->priv->history_service,
					"urls-visited", G_CALLBACK (on_urls_visited_cb),
					self);
	}

	filter_now (self);
}

static void
ephy_history_window_set_property (GObject *object,
				  guint prop_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	EphyHistoryWindow *self = EPHY_HISTORY_WINDOW (object);

	switch (prop_id)
	{
	case PROP_HISTORY_SERVICE:
		set_history_service (self, g_value_get_object (value));
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
	EphyHistoryWindow *self = EPHY_HISTORY_WINDOW (object);

	switch (prop_id)
	{
	case PROP_HISTORY_SERVICE:
		g_value_set_object (value, self->priv->history_service);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ephy_history_window_dispose (GObject *object)
{
	EphyHistoryWindow *self = EPHY_HISTORY_WINDOW (object);

	g_free (self->priv->search_text);
	self->priv->search_text = NULL;

	if (self->priv->cancellable)
	{
		g_cancellable_cancel (self->priv->cancellable);
		g_clear_object (&self->priv->cancellable);
	}

	if (self->priv->history_service != NULL)
		g_signal_handlers_disconnect_by_func (self->priv->history_service,
						      on_urls_visited_cb,
						      self);
	g_clear_object (&self->priv->history_service);

	G_OBJECT_CLASS (ephy_history_window_parent_class)->dispose (object);
}

static void
ephy_history_window_finalize (GObject *object)
{
	EphyHistoryWindow *self = EPHY_HISTORY_WINDOW (object);

	if (self->priv->window)
	{
		GtkWidget **window = &self->priv->window;
		g_object_remove_weak_pointer
			(G_OBJECT(self->priv->window),
			 (gpointer *)window);
	}

	G_OBJECT_CLASS (ephy_history_window_parent_class)->finalize (object);
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

	g_object_class_install_property (object_class,
					 PROP_HISTORY_SERVICE,
					 g_param_spec_object ("history-service",
							      "History service",
							      "History Service",
							      EPHY_TYPE_HISTORY_SERVICE,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY));

	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/epiphany/history-dialog.ui");
	gtk_widget_class_bind_template_child_private (widget_class, EphyHistoryWindow, liststore);
	gtk_widget_class_bind_template_child_private (widget_class, EphyHistoryWindow, treeview);
	gtk_widget_class_bind_template_child_private (widget_class, EphyHistoryWindow, clear_button);
	gtk_widget_class_bind_template_child_private (widget_class, EphyHistoryWindow, remove_button);
	gtk_widget_class_bind_template_child_private (widget_class, EphyHistoryWindow, open_button);
	gtk_widget_class_bind_template_child_private (widget_class, EphyHistoryWindow, date_column);
	gtk_widget_class_bind_template_child_private (widget_class, EphyHistoryWindow, date_renderer);
	gtk_widget_class_bind_template_child_private (widget_class, EphyHistoryWindow, open_menuitem);
	gtk_widget_class_bind_template_child_private (widget_class, EphyHistoryWindow, copy_location_menuitem);
	gtk_widget_class_bind_template_child_private (widget_class, EphyHistoryWindow, bookmark_menuitem);
	gtk_widget_class_bind_template_child_private (widget_class, EphyHistoryWindow, delete_menuitem);
	gtk_widget_class_bind_template_child_private (widget_class, EphyHistoryWindow, treeview_popup_menu);

	gtk_widget_class_bind_template_callback (widget_class, on_treeview_row_activated);
	gtk_widget_class_bind_template_callback (widget_class, on_treeview_key_press_event);
	gtk_widget_class_bind_template_callback (widget_class, on_treeview_button_press_event);
	gtk_widget_class_bind_template_callback (widget_class, on_treeview_selection_changed);
	gtk_widget_class_bind_template_callback (widget_class, on_remove_button_clicked);
	gtk_widget_class_bind_template_callback (widget_class, on_open_button_clicked);
	gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);

	gtk_widget_class_bind_template_callback (widget_class, on_open_menuitem_activate);
	gtk_widget_class_bind_template_callback (widget_class, on_copy_location_menuitem_activate);
	gtk_widget_class_bind_template_callback (widget_class, on_bookmark_menuitem_activate);
	gtk_widget_class_bind_template_callback (widget_class, on_delete_menuitem_activate);
}

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
response_cb (GtkDialog *widget,
	     int response,
	     EphyHistoryWindow *self)
{
	if (response == GTK_RESPONSE_REJECT) {
		clear_all_history (self);
		return;
	}

	gtk_widget_destroy (GTK_WIDGET (self));
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
	EphyHistoryWindow *self;

	g_return_val_if_fail (history_service != NULL, NULL);

	self = g_object_new (EPHY_TYPE_HISTORY_WINDOW,
			     "use-header-bar" , TRUE,
			     "history-service", history_service,
			     NULL);

	return GTK_WIDGET (self);
}

static void
ephy_history_window_init (EphyHistoryWindow *self)
{
	self->priv = ephy_history_window_get_instance_private (self);
	gtk_widget_init_template (GTK_WIDGET (self));

	self->priv->cancellable = g_cancellable_new ();

	ephy_gui_ensure_window_group (GTK_WINDOW (self));

	gtk_tree_view_column_set_cell_data_func (GTK_TREE_VIEW_COLUMN (self->priv->date_column),
						 GTK_CELL_RENDERER (self->priv->date_renderer),
						 (GtkTreeCellDataFunc) convert_cell_data_func,
						 GINT_TO_POINTER (COLUMN_DATE),
						 NULL);

	g_signal_connect (self, "response",
			  G_CALLBACK (response_cb), self);
}
