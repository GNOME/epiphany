/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti <mpeseng@tin.it>
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2012 Igalia S.L
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-history-dialog.h"

#include "ephy-debug.h"
#include "ephy-gui.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-uri-helpers.h"
#include "ephy-time-helpers.h"
#include "ephy-window.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <time.h>

#define NUM_RESULTS_LIMIT -1
/* 3/5 of gdkframeclockidle.c's FRAME_INTERVAL (16667 microsecs) */
#define GTK_TREE_VIEW_TIME_MS_PER_IDLE 10

struct _EphyHistoryDialog {
  GtkDialog parent_instance;

  EphyHistoryService *history_service;
  GCancellable *cancellable;

  GtkWidget *treeview;
  GtkTreeSelection *tree_selection;
  GtkWidget *liststore;
  GtkTreeViewColumn *date_column;
  GtkTreeViewColumn *name_column;
  GtkTreeViewColumn *location_column;
  GtkWidget *date_renderer;
  GtkWidget *location_renderer;
  GMenuModel *treeview_popup_menu_model;

  GtkWidget *forget_all_button;
  GtkWidget *forget_button;

  GActionGroup *action_group;

  GList *urls;
  guint sorter_source;

  char *search_text;

  gboolean sort_ascending;
  gint sort_column;

  GtkWidget *confirmation_dialog;
};

G_DEFINE_TYPE (EphyHistoryDialog, ephy_history_dialog, GTK_TYPE_DIALOG)

enum {
  PROP_0,
  PROP_HISTORY_SERVICE,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

typedef enum {
  COLUMN_DATE,
  COLUMN_NAME,
  COLUMN_LOCATION
} EphyHistoryDialogColumns;

static gboolean
add_urls_source (EphyHistoryDialog *self)
{
  EphyHistoryURL *url;
  GTimer *timer;
  GList *element;

  if (self->urls == NULL) {
    self->sorter_source = 0;
    return G_SOURCE_REMOVE;
  }

  timer = g_timer_new ();
  g_timer_start (timer);

  do {
    element = self->urls;
    url = element->data;
    gtk_list_store_insert_with_values (GTK_LIST_STORE (self->liststore),
                                       NULL, G_MAXINT,
                                       COLUMN_DATE, url->last_visit_time,
                                       COLUMN_NAME, url->title,
                                       COLUMN_LOCATION, url->url,
                                       -1);
    self->urls = g_list_remove_link (self->urls, element);
    ephy_history_url_free (url);
    g_list_free_1 (element);
  } while (self->urls &&
           g_timer_elapsed (timer, NULL) < GTK_TREE_VIEW_TIME_MS_PER_IDLE / 1000.);

  g_timer_destroy (timer);

  return G_SOURCE_CONTINUE;
}

static void
on_find_urls_cb (gpointer service,
                 gboolean success,
                 gpointer result_data,
                 gpointer user_data)
{
  EphyHistoryDialog *self = EPHY_HISTORY_DIALOG (user_data);
  GtkTreeViewColumn *column;

  if (success != TRUE)
    return;

  self->urls = (GList *)result_data;

  gtk_tree_view_set_model (GTK_TREE_VIEW (self->treeview), NULL);
  gtk_list_store_clear (GTK_LIST_STORE (self->liststore));
  gtk_tree_view_set_model (GTK_TREE_VIEW (self->treeview), GTK_TREE_MODEL (self->liststore));

  column = gtk_tree_view_get_column (GTK_TREE_VIEW (self->treeview), self->sort_column);
  gtk_tree_view_column_set_sort_order (column, self->sort_ascending ? GTK_SORT_ASCENDING : GTK_SORT_DESCENDING);
  gtk_tree_view_column_set_sort_indicator (column, TRUE);

  self->sorter_source = g_idle_add ((GSourceFunc)add_urls_source, self);
}

static GList *
substrings_filter (EphyHistoryDialog *self)
{
  char **tokens, **p;
  GList *substrings = NULL;

  if (self->search_text == NULL)
    return NULL;

  tokens = p = g_strsplit (self->search_text, " ", -1);

  while (*p) {
    substrings = g_list_prepend (substrings, *p++);
  }
  ;
  g_free (tokens);

  return substrings;
}

static void
remove_pending_sorter_source (EphyHistoryDialog *self)
{
  if (self->sorter_source != 0) {
    g_source_remove (self->sorter_source);
    self->sorter_source = 0;
  }

  if (self->urls != NULL) {
    g_list_free_full (self->urls, (GDestroyNotify)ephy_history_url_free);
    self->urls = NULL;
  }
}

static void
filter_now (EphyHistoryDialog *self)
{
  gint64 from, to;
  GList *substrings;
  EphyHistorySortType type;

  substrings = substrings_filter (self);

  from = to = -1;       /* all */

  switch (self->sort_column) {
    case COLUMN_DATE:
      type = self->sort_ascending ? EPHY_HISTORY_SORT_LEAST_RECENTLY_VISITED : EPHY_HISTORY_SORT_MOST_RECENTLY_VISITED;
      break;
    case COLUMN_NAME:
      type = self->sort_ascending ? EPHY_HISTORY_SORT_TITLE_ASCENDING : EPHY_HISTORY_SORT_TITLE_DESCENDING;
      break;
    case COLUMN_LOCATION:
      type = self->sort_ascending ? EPHY_HISTORY_SORT_URL_ASCENDING : EPHY_HISTORY_SORT_URL_DESCENDING;
      break;
    default:
      type = EPHY_HISTORY_SORT_MOST_RECENTLY_VISITED;
  }

  remove_pending_sorter_source (self);

  ephy_history_service_find_urls (self->history_service,
                                  from, to,
                                  NUM_RESULTS_LIMIT, 0,
                                  substrings,
                                  type,
                                  self->cancellable,
                                  (EphyHistoryJobCallback)on_find_urls_cb, self);
}

static void
confirmation_dialog_response_cb (GtkWidget         *dialog,
                                 int                response,
                                 EphyHistoryDialog *self)
{
  gtk_widget_destroy (dialog);

  if (response == GTK_RESPONSE_ACCEPT) {
    ephy_history_service_clear (self->history_service,
                                NULL, NULL, NULL);
    filter_now (self);
  }
}

static GtkWidget *
confirmation_dialog_construct (EphyHistoryDialog *self)
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
forget_all (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       user_data)
{
  EphyHistoryDialog *self = EPHY_HISTORY_DIALOG (user_data);

  if (self->confirmation_dialog == NULL) {
    GtkWidget **confirmation_dialog;

    self->confirmation_dialog = confirmation_dialog_construct (self);
    confirmation_dialog = &self->confirmation_dialog;
    g_object_add_weak_pointer (G_OBJECT (self->confirmation_dialog),
                               (gpointer *)confirmation_dialog);
  }

  gtk_widget_show (self->confirmation_dialog);
}

static GtkWidget *
get_target_window (EphyHistoryDialog *self)
{
    return GTK_WIDGET (gtk_application_get_active_window (GTK_APPLICATION (ephy_shell_get_default ())));
}

static void
on_browse_history_deleted_cb (gpointer service,
                              gboolean success,
                              gpointer result_data,
                              gpointer user_data)
{
  EphyHistoryDialog *self = EPHY_HISTORY_DIALOG (user_data);

  if (success != TRUE)
    return;

  filter_now (self);
}

static EphyHistoryURL *
get_url_from_path (GtkTreeModel *model,
                   GtkTreePath  *path)
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
                       GtkTreePath  *path,
                       GtkTreeIter  *iter,
                       gpointer     *data)
{
  EphyHistoryURL *url;

  url = get_url_from_path (model, path);
  *data = g_list_prepend (*data, url);
}

static GList *
get_selection (EphyHistoryDialog *self)
{
  GList *list = NULL;

  gtk_tree_selection_selected_foreach (self->tree_selection,
                                       (GtkTreeSelectionForeachFunc)get_selection_foreach,
                                       &list);

  return g_list_reverse (list);
}

static void
delete_selected (EphyHistoryDialog *self)
{
  GList *selected;

  selected = get_selection (self);
  ephy_history_service_delete_urls (self->history_service, selected, self->cancellable,
                                    (EphyHistoryJobCallback)on_browse_history_deleted_cb, self);
}

static void
open_selection (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  EphyHistoryDialog *self = EPHY_HISTORY_DIALOG (user_data);
  EphyWindow *window;
  GList *selection;
  GList *l;

  selection = get_selection (self);

  window = EPHY_WINDOW (get_target_window (self));
  for (l = selection; l; l = l->next) {
    EphyHistoryURL *url = l->data;
    EphyEmbed *embed;

    embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                window, NULL, EPHY_NEW_TAB_JUMP);
    ephy_web_view_load_url (ephy_embed_get_web_view (embed), url->url);
  }

  g_list_free_full (selection, (GDestroyNotify)ephy_history_url_free);
}

static void
copy_url (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
  EphyHistoryDialog *self = EPHY_HISTORY_DIALOG (user_data);
  GList *selection;

  selection = get_selection (self);

  if (g_list_length (selection) == 1) {
    EphyHistoryURL *url = selection->data;
    g_message ("URL %s", url->url);
    gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), url->url, -1);
  }

  g_list_free_full (selection, (GDestroyNotify)ephy_history_url_free);
}

static void
forget (GSimpleAction *action,
        GVariant      *parameter,
        gpointer       user_data)
{
  EphyHistoryDialog *self = EPHY_HISTORY_DIALOG (user_data);

  delete_selected (self);
}

static gboolean
on_treeview_key_press_event (GtkWidget         *widget,
                             GdkEventKey       *event,
                             EphyHistoryDialog *self)
{
  if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete) {
    delete_selected (self);

    return TRUE;
  }

  return FALSE;
}

static void
update_popup_menu_actions (GActionGroup *action_group,
                           gboolean      only_one_selected_item)
{
  GAction *copy_url_action;

  copy_url_action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "copy-url");

  g_simple_action_set_enabled (G_SIMPLE_ACTION (copy_url_action), only_one_selected_item);
}

static gboolean
on_treeview_button_press_event (GtkWidget         *widget,
                                GdkEventButton    *event,
                                EphyHistoryDialog *self)
{
  if (event->button == 3) {
    int n;
    GtkWidget *menu;

    n = gtk_tree_selection_count_selected_rows (self->tree_selection);
    if (n <= 0)
      return FALSE;

    update_popup_menu_actions (self->action_group, (n == 1));

    menu = gtk_menu_new_from_model (self->treeview_popup_menu_model);
    gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (self), NULL);
    gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *)event);
    return TRUE;
  }

  return FALSE;
}

static void
on_treeview_row_activated (GtkTreeView       *view,
                           GtkTreePath       *path,
                           GtkTreeViewColumn *col,
                           EphyHistoryDialog *self)
{
  EphyWindow *window;
  EphyHistoryURL *url;
  EphyEmbed *embed;

  window = EPHY_WINDOW (get_target_window (self));
  url = get_url_from_path (gtk_tree_view_get_model (view),
                           path);
  g_return_if_fail (url != NULL);

  embed = ephy_shell_new_tab (ephy_shell_get_default (),
                              window, NULL, EPHY_NEW_TAB_JUMP);
  ephy_web_view_load_url (ephy_embed_get_web_view (embed), url->url);
  ephy_history_url_free (url);
}

static void
on_search_entry_changed (GtkSearchEntry    *entry,
                         EphyHistoryDialog *self)
{
  const char *text;

  text = gtk_entry_get_text (GTK_ENTRY (entry));
  g_free (self->search_text);
  self->search_text = g_strdup (text);

  filter_now (self);
}

static gboolean
on_search_key_press_event (GtkWidget        *widget,
                          GdkEventKey       *event,
                          EphyHistoryDialog *self)
{
  if (event->keyval == GDK_KEY_Escape) {
    g_signal_emit_by_name (self, "close", NULL);
    return GDK_EVENT_STOP;
  }

  return GDK_EVENT_PROPAGATE;
}

static void
update_selection_actions (GActionGroup *action_group,
                          gboolean      has_selection)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  GAction *forget_action;
  GAction *open_selection_action;

  if (ephy_embed_shell_get_mode (shell) != EPHY_EMBED_SHELL_MODE_INCOGNITO) {
    forget_action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "forget");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (forget_action), has_selection);
  }

  open_selection_action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "open-selection");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (open_selection_action), has_selection);
}

static void
on_treeview_selection_changed (GtkTreeSelection  *selection,
                               EphyHistoryDialog *self)
{
  update_selection_actions (self->action_group,
                            gtk_tree_selection_count_selected_rows (selection) > 0);
}

static void
on_treeview_column_clicked_event (GtkTreeViewColumn *column,
                                  EphyHistoryDialog *self)
{
  GtkTreeViewColumn *previous_sortby;
  gint new_sort_column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (column), "column"));

  if (new_sort_column == self->sort_column) {
    self->sort_ascending = !(self->sort_ascending);
  } else {
    previous_sortby = gtk_tree_view_get_column (GTK_TREE_VIEW (self->treeview), self->sort_column);
    gtk_tree_view_column_set_sort_indicator (previous_sortby, FALSE);

    self->sort_column = new_sort_column;
    self->sort_ascending = self->sort_column == COLUMN_DATE ? FALSE : TRUE;
  }

  gtk_tree_view_column_set_sort_order (column, self->sort_ascending ? GTK_SORT_ASCENDING : GTK_SORT_DESCENDING);
  gtk_tree_view_column_set_sort_indicator (column, TRUE);
  filter_now (self);
}

static gboolean
on_urls_visited_cb (EphyHistoryService *service,
                    EphyHistoryDialog  *self)
{
  filter_now (self);

  return FALSE;
}

static void
set_history_service (EphyHistoryDialog  *self,
                     EphyHistoryService *history_service)
{
  if (history_service == self->history_service)
    return;

  if (self->history_service != NULL) {
    g_signal_handlers_disconnect_by_func (self->history_service,
                                          on_urls_visited_cb,
                                          self);
    g_clear_object (&self->history_service);
  }

  if (history_service != NULL) {
    self->history_service = g_object_ref (history_service);
    g_signal_connect_after (self->history_service,
                            "urls-visited", G_CALLBACK (on_urls_visited_cb),
                            self);
  }

  filter_now (self);
}

static void
ephy_history_dialog_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  EphyHistoryDialog *self = EPHY_HISTORY_DIALOG (object);

  switch (prop_id) {
    case PROP_HISTORY_SERVICE:
      set_history_service (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_history_dialog_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  EphyHistoryDialog *self = EPHY_HISTORY_DIALOG (object);

  switch (prop_id) {
    case PROP_HISTORY_SERVICE:
      g_value_set_object (value, self->history_service);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_history_dialog_dispose (GObject *object)
{
  EphyHistoryDialog *self = EPHY_HISTORY_DIALOG (object);

  g_free (self->search_text);
  self->search_text = NULL;

  if (self->cancellable) {
    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
  }

  if (self->history_service != NULL)
    g_signal_handlers_disconnect_by_func (self->history_service,
                                          on_urls_visited_cb,
                                          self);
  g_clear_object (&self->history_service);

  remove_pending_sorter_source (self);

  G_OBJECT_CLASS (ephy_history_dialog_parent_class)->dispose (object);
}

static void
ephy_history_dialog_class_init (EphyHistoryDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ephy_history_dialog_set_property;
  object_class->get_property = ephy_history_dialog_get_property;
  object_class->dispose = ephy_history_dialog_dispose;

  obj_properties[PROP_HISTORY_SERVICE] =
    g_param_spec_object ("history-service",
                         "History service",
                         "History Service",
                         EPHY_TYPE_HISTORY_SERVICE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/history-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, liststore);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, treeview);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, tree_selection);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, date_column);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, name_column);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, location_column);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, date_renderer);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, location_renderer);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, treeview_popup_menu_model);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, forget_all_button);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, forget_button);

  gtk_widget_class_bind_template_callback (widget_class, on_treeview_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_treeview_key_press_event);
  gtk_widget_class_bind_template_callback (widget_class, on_treeview_button_press_event);
  gtk_widget_class_bind_template_callback (widget_class, on_treeview_selection_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_treeview_column_clicked_event);
  gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_search_key_press_event);
}

static void
convert_date_data_func (GtkTreeViewColumn *column,
                        GtkCellRenderer   *renderer,
                        GtkTreeModel      *model,
                        GtkTreeIter       *iter,
                        gpointer           user_data)
{
  int col_id = GPOINTER_TO_INT (user_data);
  gint64 value;
  char *friendly;

  gtk_tree_model_get (model, iter,
                      col_id,
                      &value,
                      -1);

  /* Convert back to seconds. */
  friendly = ephy_time_helpers_utf_friendly_time (value / 1000000);
  g_object_set (renderer, "text", friendly, NULL);
  g_free (friendly);
}

static void
convert_location_data_func (GtkTreeViewColumn *column,
                            GtkCellRenderer   *renderer,
                            GtkTreeModel      *model,
                            GtkTreeIter       *iter,
                            gpointer           user_data)
{
  int col_id = GPOINTER_TO_INT (user_data);
  char *url;
  char *decoded_url;

  gtk_tree_model_get (model, iter,
                      col_id,
                      &url,
                      -1);
  decoded_url = ephy_uri_decode (url);

  g_object_set (renderer, "text", decoded_url, NULL);

  g_free (url);
  g_free (decoded_url);
}

GtkWidget *
ephy_history_dialog_new (EphyHistoryService *history_service)
{
  EphyHistoryDialog *self;

  g_return_val_if_fail (history_service != NULL, NULL);

  self = g_object_new (EPHY_TYPE_HISTORY_DIALOG,
                       "use-header-bar", TRUE,
                       "history-service", history_service,
                       NULL);

  return GTK_WIDGET (self);
}

static GActionGroup *
create_action_group (EphyHistoryDialog *self)
{
  const GActionEntry entries[] = {
    { "open-selection", open_selection },
    { "copy-url", copy_url },
    { "forget", forget },
    { "forget-all", forget_all }
  };
  GSimpleActionGroup *group;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), self);

  return G_ACTION_GROUP (group);
}

static void
ephy_history_dialog_init (EphyHistoryDialog *self)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  const char *tooltip;
  GAction *action;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();

  self->urls = NULL;
  self->sort_ascending = FALSE;
  self->sort_column = COLUMN_DATE;
  self->sorter_source = 0;

  ephy_gui_ensure_window_group (GTK_WINDOW (self));

  g_object_set_data (G_OBJECT (self->date_column),
                     "column", GINT_TO_POINTER (COLUMN_DATE));
  g_object_set_data (G_OBJECT (self->name_column),
                     "column", GINT_TO_POINTER (COLUMN_NAME));
  g_object_set_data (G_OBJECT (self->location_column),
                     "column", GINT_TO_POINTER (COLUMN_LOCATION));

  gtk_tree_view_column_set_cell_data_func (GTK_TREE_VIEW_COLUMN (self->date_column),
                                           GTK_CELL_RENDERER (self->date_renderer),
                                           (GtkTreeCellDataFunc)convert_date_data_func,
                                           GINT_TO_POINTER (COLUMN_DATE),
                                           NULL);

  gtk_tree_view_column_set_cell_data_func (GTK_TREE_VIEW_COLUMN (self->location_column),
                                           GTK_CELL_RENDERER (self->location_renderer),
                                           (GtkTreeCellDataFunc)convert_location_data_func,
                                           GINT_TO_POINTER (COLUMN_LOCATION),
                                           NULL);

  self->action_group = create_action_group (self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "history", self->action_group);

  if (ephy_embed_shell_get_mode (shell) == EPHY_EMBED_SHELL_MODE_INCOGNITO) {
    tooltip = _("It is not possible to modify history when in incognito mode.");
    gtk_widget_set_tooltip_text (self->forget_all_button, tooltip);
    gtk_widget_set_tooltip_text (self->forget_button, tooltip);

    action = g_action_map_lookup_action (G_ACTION_MAP (self->action_group), "forget");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

    action = g_action_map_lookup_action (G_ACTION_MAP (self->action_group), "forget-all");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
  }

  update_selection_actions (self->action_group, FALSE);
}
