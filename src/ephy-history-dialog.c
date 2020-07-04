/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti <mpeseng@tin.it>
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2012 Igalia S.L
 *  Copyright © 2018 Jan-Michael Brummer
 *  Copyright © 2019 Purism SPC
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
#include "ephy-snapshot-service.h"
#include "ephy-uri-helpers.h"
#include "ephy-time-helpers.h"
#include "ephy-window.h"

#include <ctype.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#define HANDY_USE_UNSTABLE_API
#include <handy.h>
#include <string.h>
#include <time.h>

#define NUM_FETCH_LIMIT 15

struct _EphyHistoryDialog {
  EphyDataDialog parent_instance;

  EphySnapshotService *snapshot_service;
  EphyHistoryService *history_service;
  GCancellable *cancellable;

  GtkWidget *listbox;
  GtkWidget *forget_all_button;
  GtkWidget *popup_menu;

  GActionGroup *action_group;

  GList *urls;
  guint sorter_source;

  gint num_fetch;

  GtkWidget *confirmation_dialog;
};

G_DEFINE_TYPE (EphyHistoryDialog, ephy_history_dialog, EPHY_TYPE_DATA_DIALOG)

enum {
  PROP_0,
  PROP_HISTORY_SERVICE,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static gboolean add_urls_source (EphyHistoryDialog *self);

static EphyHistoryURL *
get_url_from_row (GtkListBoxRow *row)
{
  return ephy_history_url_new (hdy_action_row_get_subtitle (HDY_ACTION_ROW (row)),
                               hdy_action_row_get_title (HDY_ACTION_ROW (row)),
                               0,
                               0,
                               0);
}

static void
clear_listbox (GtkWidget *listbox)
{
  GList *children, *iter;

  children = gtk_container_get_children (GTK_CONTAINER (listbox));

  for (iter = children; iter; iter = g_list_next (iter)) {
    gtk_widget_destroy (GTK_WIDGET (iter->data));
  }

  g_list_free (children);
}

static void
on_find_urls_cb (gpointer service,
                 gboolean success,
                 gpointer result_data,
                 gpointer user_data)
{
  EphyHistoryDialog *self = EPHY_HISTORY_DIALOG (user_data);

  if (!success)
    return;

  self->urls = ephy_history_url_list_copy (result_data);

  clear_listbox (self->listbox);

  self->num_fetch = NUM_FETCH_LIMIT;
  self->sorter_source = g_idle_add ((GSourceFunc)add_urls_source, self);
}

static GList *
substrings_filter (EphyHistoryDialog *self)
{
  const gchar *search_text = ephy_data_dialog_get_search_text (EPHY_DATA_DIALOG (self));
  char **tokens, **p;
  GList *substrings = NULL;

  if (!search_text)
    return NULL;

  tokens = p = g_strsplit (search_text, " ", -1);

  while (*p) {
    substrings = g_list_prepend (substrings, *p++);
  }

  g_free (tokens);

  return substrings;
}

static void
remove_pending_sorter_source (EphyHistoryDialog *self,
                              gboolean           free_urls)
{
  g_clear_handle_id (&self->sorter_source, g_source_remove);

  if (free_urls && self->urls) {
    ephy_history_host_list_free (self->urls);
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

  type = EPHY_HISTORY_SORT_MOST_RECENTLY_VISITED;

  remove_pending_sorter_source (self, TRUE);

  ephy_history_service_find_urls (self->history_service,
                                  from, to,
                                  -1, 0,
                                  substrings,
                                  type,
                                  self->cancellable,
                                  (EphyHistoryJobCallback)on_find_urls_cb, self);
}

static GList *
get_selection (EphyHistoryDialog *self)
{
  GList *selected_rows = gtk_list_box_get_selected_rows (GTK_LIST_BOX (self->listbox));
  GList *list = NULL;
  GList *tmp;

  for (tmp = selected_rows; tmp; tmp = tmp->next) {
    EphyHistoryURL *url = get_url_from_row (tmp->data);

    if (!url) {
      continue;
    }

    list = g_list_append (list, url);
  }

  return g_list_reverse (list);
}

static void
on_browse_history_deleted_cb (gpointer service,
                              gboolean success,
                              gpointer result_data,
                              gpointer user_data)
{
  EphyHistoryDialog *self = EPHY_HISTORY_DIALOG (user_data);

  if (!success)
    return;

  filter_now (self);
}

static void
delete_selected (EphyHistoryDialog *self)
{
  GList *selected;

  selected = get_selection (self);
  ephy_history_service_delete_urls (self->history_service, selected, self->cancellable,
                                    (EphyHistoryJobCallback)on_browse_history_deleted_cb, self);

  for (GList *l = selected; l; l = l->next)
    ephy_snapshot_service_delete_snapshot_for_url (self->snapshot_service, ((EphyHistoryURL *)l->data)->url);
}

static void
row_check_button_toggled (GtkCheckButton *check_button,
                          GtkListBoxRow  *row)
{
  GtkListBox *listbox = GTK_LIST_BOX (gtk_widget_get_parent (GTK_WIDGET (row)));
  gboolean row_selected = gtk_list_box_row_is_selected (row);
  gboolean button_checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_button));

  if (button_checked == row_selected)
    return;

  /* If the user ticks a checkbox, then the corresponding row should get selected */
  if (button_checked)
    gtk_list_box_select_row (listbox, row);
  else
    gtk_list_box_unselect_row (listbox, row);
}

static GtkWidget *
create_row (EphyHistoryDialog *self,
            EphyHistoryURL    *url)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  GtkWidget *date;
  GtkWidget *row;
  GtkWidget *separator;
  GtkWidget *check_button;

  /* Row */
  row = hdy_action_row_new ();
  hdy_action_row_set_title (HDY_ACTION_ROW (row), url->title);
  hdy_action_row_set_subtitle (HDY_ACTION_ROW (row), url->url);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
  gtk_widget_set_tooltip_text (row, url->url);

  /* Date */
  date = gtk_label_new (ephy_time_helpers_utf_friendly_time (url->last_visit_time / 1000000));
  gtk_label_set_ellipsize (GTK_LABEL (date), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (date), 0);

  /* Separator */
  separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_widget_set_margin_top (separator, 8);
  gtk_widget_set_margin_bottom (separator, 8);

  /* CheckButton */
  check_button = gtk_check_button_new ();
  g_object_set_data (G_OBJECT (row), "check-button", check_button);
  gtk_widget_set_valign (check_button, GTK_ALIGN_CENTER);
  gtk_widget_set_tooltip_text (check_button, _("Remove the selected pages from history"));
  gtk_button_set_relief (GTK_BUTTON (check_button), GTK_RELIEF_NONE);
  g_signal_connect (check_button, "toggled", G_CALLBACK (row_check_button_toggled), row);

  /* Separator and CheckButton should be visible only during selection mode */
  g_object_bind_property (self, "selection-active",
                          separator, "visible",
                          G_BINDING_DEFAULT);

  g_object_bind_property (self, "selection-active",
                          check_button, "visible",
                          G_BINDING_DEFAULT);

  /* Added in reverse order because actions are packed from the end. */
  gtk_container_add (GTK_CONTAINER (row), date);
  gtk_container_add (GTK_CONTAINER (row), separator);
  gtk_container_add (GTK_CONTAINER (row), check_button);

  gtk_widget_set_sensitive (check_button, ephy_embed_shell_get_mode (shell) != EPHY_EMBED_SHELL_MODE_INCOGNITO);

  gtk_widget_show_all (row);

  /* Hide the Separator and CheckButton if selection isn't active */
  if (!ephy_data_dialog_get_selection_active (EPHY_DATA_DIALOG (self))) {
    gtk_widget_set_visible (separator, FALSE);
    gtk_widget_set_visible (check_button, FALSE);
  }

  return row;
}

static gboolean
add_urls_source (EphyHistoryDialog *self)
{
  EphyHistoryURL *url;
  GList *element;
  GtkWidget *row;
  GList *children;

  ephy_data_dialog_set_is_loading (EPHY_DATA_DIALOG (self), FALSE);

  children = gtk_container_get_children (GTK_CONTAINER (self->listbox));
  ephy_data_dialog_set_has_search_results (EPHY_DATA_DIALOG (self), !!children);
  if (!children)
    ephy_data_dialog_set_has_data (EPHY_DATA_DIALOG (self), FALSE);
  g_list_free (children);

  if (!self->urls || !self->num_fetch) {
    self->sorter_source = 0;
    gtk_widget_queue_draw (self->listbox);
    return G_SOURCE_REMOVE;
  }

  element = self->urls;
  url = element->data;

  row = create_row (self, url);
  gtk_list_box_insert (GTK_LIST_BOX (self->listbox), row, -1);
  ephy_data_dialog_set_has_data (EPHY_DATA_DIALOG (self), TRUE);

  self->urls = g_list_remove_link (self->urls, element);
  ephy_history_url_free (url);
  g_list_free_1 (element);

  self->num_fetch--;

  if (!self->num_fetch) {
    self->sorter_source = 0;
    return G_SOURCE_REMOVE;
  }

  return G_SOURCE_CONTINUE;
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

    ephy_snapshot_service_delete_all_snapshots (self->snapshot_service);
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

  if (!self->confirmation_dialog) {
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
    gtk_clipboard_set_text (gtk_clipboard_get_default (gdk_display_get_default ()),
                            url->url, -1);
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
on_listbox_key_press_event (GtkWidget         *widget,
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

static void
on_search_text_changed (EphyHistoryDialog *self)
{
  filter_now (self);
}

static void
load_further_data (EphyHistoryDialog *self)
{
  remove_pending_sorter_source (self, FALSE);

  self->num_fetch += NUM_FETCH_LIMIT;
  self->sorter_source = g_idle_add ((GSourceFunc)add_urls_source, self);
}

static gboolean
on_key_press_event (EphyHistoryDialog *self,
                    GdkEvent          *event,
                    gpointer           user_data)
{
  GdkEventKey *key = (GdkEventKey *)event;

  if (key->keyval == GDK_KEY_Down || key->keyval == GDK_KEY_Page_Down) {
    GList *childrens = gtk_container_get_children (GTK_CONTAINER (self->listbox));
    GtkWidget *last = g_list_last (childrens)->data;
    GtkWidget *focus = gtk_container_get_focus_child (GTK_CONTAINER (self->listbox));

    if (focus == last) {
      load_further_data (self);

      return GDK_EVENT_STOP;
    }
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
on_listbox_row_selected (GtkListBox        *box,
                         GtkListBoxRow     *row,
                         EphyHistoryDialog *self)
{
  update_selection_actions (self->action_group, !!row);
}

static void
on_listbox_row_activated (GtkListBox        *box,
                          GtkListBoxRow     *row,
                          EphyHistoryDialog *self)
{
  EphyWindow *window;
  EphyHistoryURL *url;
  EphyEmbed *embed;

  window = EPHY_WINDOW (get_target_window (self));
  url = get_url_from_row (row);
  g_assert (url);

  embed = ephy_shell_new_tab (ephy_shell_get_default (),
                              window, NULL, EPHY_NEW_TAB_JUMP);
  ephy_web_view_load_url (ephy_embed_get_web_view (embed), url->url);
  ephy_history_url_free (url);
}

static void
on_listbox_selected_rows_changed (GtkListBox        *listbox,
                                  EphyHistoryDialog *self)
{
  GList *selected_rows = gtk_list_box_get_selected_rows (listbox);
  GList *rows = gtk_container_get_children (GTK_CONTAINER (listbox));
  GList *iter = NULL;

  for (iter = rows; iter != NULL; iter = g_list_next (iter)) {
    GtkWidget *row = iter->data;
    GtkCheckButton *check_button = GTK_CHECK_BUTTON (g_object_get_data (G_OBJECT (row), "check-button"));
    gboolean row_selected = gtk_list_box_row_is_selected (GTK_LIST_BOX_ROW (row));
    gboolean button_checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_button));

    /* Ensure selected rows have their checkboxes ticked */
    if (button_checked == row_selected)
      continue;

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button), row_selected);
  }

  if (selected_rows == NULL)
    ephy_data_dialog_set_selection_delete_sensitive (EPHY_DATA_DIALOG (self), FALSE);
  else
    ephy_data_dialog_set_selection_delete_sensitive (EPHY_DATA_DIALOG (self), TRUE);

  g_list_free (selected_rows);
  g_list_free (rows);
}

static void
select_list_index_interval (GtkListBox *listbox,
                            gint        index_a,
                            gint        index_b)
{
  gint start = 0;
  gint end = 0;
  gint index = 0;

  if (index_a < index_b) {
    start = index_a;
    end = index_b;
  } else {
    start = index_b;
    end = index_a;
  }

  for (index = start; index <= end; index++) {
    GtkListBoxRow *row = gtk_list_box_get_row_at_index (listbox, index);

    gtk_list_box_select_row (listbox, row);
  }
}

static void
handle_selection_click_event (EphyHistoryDialog *self,
                              GtkListBoxRow     *clicked_row,
                              GdkEventButton    *event)
{
  GList *selected_rows = NULL;
  GtkListBox *listbox = GTK_LIST_BOX (self->listbox);
  gboolean shift_pressed = event->state & GDK_SHIFT_MASK;

  /* If Shift isn't pressed, event simply toggles the row's selection */
  if (!shift_pressed) {
    if (gtk_list_box_row_is_selected (clicked_row))
      gtk_list_box_unselect_row (listbox, clicked_row);
    else
      gtk_list_box_select_row (listbox, clicked_row);
    return;
  }

  /* If Shift key is pressed, do the interval selection logic */
  selected_rows = gtk_list_box_get_selected_rows (listbox);

  if (g_list_length (selected_rows) == 1) {
    /* If there's exactly one other row selected we select the interval between
     * that one and the currently clicked row */
    gint index_a = gtk_list_box_row_get_index (clicked_row);
    gint index_b = gtk_list_box_row_get_index (selected_rows->data);

    select_list_index_interval (listbox, index_a, index_b);
  } else {
    /* If there are zero or more than one other rows selected,
     * then we select the clicked row and unselect all the others */
    gtk_list_box_unselect_all (listbox);
    gtk_list_box_select_row (listbox, clicked_row);
  }

  g_list_free (selected_rows);
}

static gboolean
on_listbox_button_press_event (GtkWidget         *widget,
                               GdkEventButton    *event,
                               EphyHistoryDialog *self)
{
  GtkListBoxRow *row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (self->listbox), event->y);
  gboolean selection_active = ephy_data_dialog_get_selection_active (EPHY_DATA_DIALOG (self));

  if (!row)
    return GDK_EVENT_PROPAGATE;

  if (event->button == GDK_BUTTON_SECONDARY) {
    GList *rows = NULL;
    int n;

    /* If selection mode is active, secondary button has no effect */
    if (selection_active)
      return GDK_EVENT_STOP;

    if (!gtk_list_box_row_is_selected (row))
      gtk_list_box_unselect_all (GTK_LIST_BOX (self->listbox));

    gtk_list_box_select_row (GTK_LIST_BOX (self->listbox), row);
    rows = gtk_list_box_get_selected_rows (GTK_LIST_BOX (self->listbox));
    n = g_list_length (rows);
    g_list_free (rows);

    update_popup_menu_actions (self->action_group, n == 1);

    gtk_menu_popup_at_pointer (GTK_MENU (self->popup_menu), (GdkEvent *)event);

    return GDK_EVENT_STOP;
  } else if (event->button == GDK_BUTTON_PRIMARY) {
    /* If selection mode isn't active, primary button has no effect */
    if (!selection_active)
      return GDK_EVENT_STOP;

    handle_selection_click_event (self, row, event);

    return GDK_EVENT_STOP;
  }

  return GDK_EVENT_PROPAGATE;
}

static void
set_history_service (EphyHistoryDialog  *self,
                     EphyHistoryService *history_service)
{
  if (history_service == self->history_service)
    return;

  g_clear_object (&self->history_service);

  if (history_service)
    self->history_service = g_object_ref (history_service);

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

  if (self->cancellable) {
    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
  }

  g_clear_object (&self->history_service);

  remove_pending_sorter_source (self, TRUE);

  G_OBJECT_CLASS (ephy_history_dialog_parent_class)->dispose (object);
}

static void
on_edge_reached (GtkScrolledWindow *scrolled,
                 GtkPositionType    pos,
                 gpointer           user_data)
{
  EphyHistoryDialog *self = EPHY_HISTORY_DIALOG (user_data);

  if (pos == GTK_POS_BOTTOM) {
    load_further_data (self);
  }
}

static void
on_dialog_selection_active_toggled (EphyHistoryDialog *self)
{
  gtk_list_box_unselect_all (GTK_LIST_BOX (self->listbox));
}

static void
on_dialog_selection_delete_clicked (EphyHistoryDialog *self)
{
  delete_selected (self);
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
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, listbox);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, popup_menu);

  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_selected);
  gtk_widget_class_bind_template_callback (widget_class, on_listbox_selected_rows_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_listbox_button_press_event);
  gtk_widget_class_bind_template_callback (widget_class, on_listbox_key_press_event);
  gtk_widget_class_bind_template_callback (widget_class, on_key_press_event);
  gtk_widget_class_bind_template_callback (widget_class, on_search_text_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_dialog_selection_active_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_dialog_selection_delete_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_edge_reached);
}

GtkWidget *
ephy_history_dialog_new (EphyHistoryService *history_service)
{
  EphyHistoryDialog *self;

  g_assert (history_service);

  self = g_object_new (EPHY_TYPE_HISTORY_DIALOG,
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

  self->snapshot_service = ephy_snapshot_service_get_default ();
  self->cancellable = g_cancellable_new ();

  self->urls = NULL;
  self->sorter_source = 0;

  ephy_gui_ensure_window_group (GTK_WINDOW (self));

  gtk_menu_attach_to_widget (GTK_MENU (self->popup_menu), GTK_WIDGET (self), NULL);

  self->action_group = create_action_group (self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "history", self->action_group);

  if (ephy_embed_shell_get_mode (shell) == EPHY_EMBED_SHELL_MODE_INCOGNITO) {
    tooltip = _("It is not possible to modify history when in incognito mode.");
    ephy_data_dialog_set_clear_all_description (EPHY_DATA_DIALOG (self), tooltip);

    action = g_action_map_lookup_action (G_ACTION_MAP (self->action_group), "forget-all");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

    update_selection_actions (self->action_group, FALSE);
  } else {
    ephy_data_dialog_set_can_clear (EPHY_DATA_DIALOG (self), TRUE);
  }
  ephy_data_dialog_set_is_loading (EPHY_DATA_DIALOG (self), TRUE);
}
