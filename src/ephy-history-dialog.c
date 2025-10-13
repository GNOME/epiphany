/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti <mpeseng@tin.it>
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2012 Igalia S.L
 *  Copyright © 2018-2022 Jan-Michael Brummer
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
#include "ephy-embed-prefs.h"
#include "ephy-favicon-helpers.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-snapshot-service.h"
#include "ephy-uri-helpers.h"
#include "ephy-time-helpers.h"
#include "ephy-window.h"

#include <adwaita.h>
#include <ctype.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <time.h>

#define NUM_FETCH_LIMIT 15

struct _EphyHistoryDialog {
  AdwDialog parent_instance;

  EphySnapshotService *snapshot_service;
  EphyHistoryService *history_service;
  GCancellable *cancellable;

  /* UI Elements */
  GtkWidget *header_bars_stack;
  GtkWidget *window_header_bar;
  GtkWidget *search_button;
  GtkWidget *selection_button;
  GtkWidget *selection_header_bar;
  GtkWidget *search_bar;
  GtkWidget *search_entry;
  GtkWidget *toast_overlay;
  GtkWidget *history_presentation_stack;
  GtkWidget *history_scrolled_window;
  GtkWidget *listbox;
  GtkWidget *loading_spinner;
  GtkWidget *empty_history_message;
  GtkWidget *no_search_results_message;
  GtkWidget *clear_button;
  GtkWidget *action_bar_revealer;
  GtkWidget *select_all_button;
  GtkWidget *selection_delete_button;
  GtkWidget *selection_open_button;

  GtkWidget *confirmation_dialog;

  GActionGroup *action_group;

  GList *urls;
  guint sorter_source;

  EphyWindow *parent_window;
  gint num_fetch;
  gboolean shift_modifier_active;
  gboolean is_loading;
  gboolean selection_active;
  gboolean is_selection_empty;
  gboolean is_all_selected;
  gboolean can_clear;
  gboolean has_data;
  gboolean has_search_results;
};

G_DEFINE_FINAL_TYPE (EphyHistoryDialog, ephy_history_dialog, ADW_TYPE_DIALOG)

enum {
  PROP_0,
  PROP_HISTORY_SERVICE,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static gboolean add_urls_source (EphyHistoryDialog *self);
static void set_is_selection_empty (EphyHistoryDialog *self,
                                    gboolean           is_selection_empty);
static GList *get_checked_rows (EphyHistoryDialog *self);

static void
update_ui_state (EphyHistoryDialog *self)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  GtkStack *header_bars_stack = GTK_STACK (self->header_bars_stack);
  GtkStack *history_presentation_stack = GTK_STACK (self->history_presentation_stack);
  gboolean has_data = self->has_data;
  gboolean incognito_mode = (ephy_embed_shell_get_mode (shell) == EPHY_EMBED_SHELL_MODE_INCOGNITO);
  g_autoptr (GList) checked_rows = get_checked_rows (self);
  guint n_rows = g_list_length (checked_rows);

  set_is_selection_empty (self, n_rows == 0);

  if (self->is_loading) {
    gtk_stack_set_visible_child (history_presentation_stack, self->loading_spinner);
  } else {
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->search_button))) {
      if (has_data && self->has_search_results)
        gtk_stack_set_visible_child (history_presentation_stack, self->history_scrolled_window);
      else
        gtk_stack_set_visible_child (history_presentation_stack, self->no_search_results_message);
    } else {
      if (has_data)
        gtk_stack_set_visible_child (history_presentation_stack, self->history_scrolled_window);
      else
        gtk_stack_set_visible_child (history_presentation_stack, self->empty_history_message);
    }
  }

  if (self->selection_active) {
    gtk_stack_set_visible_child (header_bars_stack, self->selection_header_bar);
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->action_bar_revealer), TRUE);
  } else {
    gtk_stack_set_visible_child (header_bars_stack, self->window_header_bar);
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->action_bar_revealer), FALSE);
  }

  gtk_widget_set_sensitive (self->search_button, has_data);
  gtk_widget_set_sensitive (self->selection_button, has_data);
  gtk_widget_set_sensitive (self->clear_button, has_data && self->can_clear);
  gtk_widget_set_sensitive (self->selection_open_button, !self->is_selection_empty);
  gtk_widget_set_sensitive (self->selection_delete_button, !self->is_selection_empty && !incognito_mode);
}

static void
set_is_loading (EphyHistoryDialog *self,
                gboolean           is_loading)
{
  if (self->is_loading == is_loading)
    return;

  self->is_loading = is_loading;
}

static void
set_can_clear (EphyHistoryDialog *self,
               gboolean           can_clear)
{
  if (self->can_clear == can_clear)
    return;

  self->can_clear = can_clear;
}

static void
set_has_data (EphyHistoryDialog *self,
              gboolean           has_data)
{
  if (self->has_data == has_data)
    return;

  self->has_data = has_data;
}

static void
set_has_search_results (EphyHistoryDialog *self,
                        gboolean           has_search_results)
{
  if (self->has_search_results == has_search_results)
    return;

  self->has_search_results = has_search_results;
}

static void
set_selection_active (EphyHistoryDialog *self,
                      gboolean           selection_active)
{
  GtkListBoxRow *row;
  int i = 0;

  self->selection_active = selection_active;

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->listbox), i++))) {
    GtkWidget *check_button = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "check-button"));

    /* Uncheck all rows when toggling selection mode */
    gtk_check_button_set_active (GTK_CHECK_BUTTON (check_button), FALSE);

    /* Show/Hide row selection widgets (check_button) */
    gtk_widget_set_visible (check_button, selection_active);
  }

  update_ui_state (self);
}

EphyWindow *
ephy_history_dialog_get_parent_window (EphyHistoryDialog *self)
{
  return self->parent_window;
}

void
ephy_history_dialog_set_parent_window (EphyHistoryDialog *self,
                                       EphyWindow        *window)
{
  self->parent_window = window;
}

static void
on_selection_button_clicked (GtkButton         *button,
                             EphyHistoryDialog *self)
{
  set_selection_active (self, TRUE);
}

static void
on_selection_cancel_button_clicked (GtkButton         *button,
                                    EphyHistoryDialog *self)
{
  set_selection_active (self, FALSE);
}

static void
set_is_selection_empty (EphyHistoryDialog *self,
                        gboolean           is_selection_empty)
{
  if (is_selection_empty == self->is_selection_empty)
    return;

  self->is_selection_empty = is_selection_empty;
}

static void
set_is_all_selected (EphyHistoryDialog *self,
                     gboolean           is_all_selected)
{
  if (is_all_selected == self->is_all_selected)
    return;

  self->is_all_selected = is_all_selected;

  if (is_all_selected)
    gtk_button_set_label (GTK_BUTTON (self->select_all_button),
                          _("Select _None"));
  else
    gtk_button_set_label (GTK_BUTTON (self->select_all_button),
                          _("Select _All"));
}

static void
on_select_all_button_clicked (GtkButton         *button,
                              EphyHistoryDialog *self)
{
  GtkListBoxRow *row;
  int i = 0;

  set_is_all_selected (self, !self->is_all_selected);

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->listbox), i++))) {
    GtkWidget *check_button = GTK_WIDGET (g_object_get_data (G_OBJECT (row), "check-button"));

    gtk_check_button_set_active (GTK_CHECK_BUTTON (check_button), self->is_all_selected);
  }

  update_ui_state (self);
}

static EphyHistoryURL *
get_url_from_row (GtkListBoxRow *row)
{
  return ephy_history_url_new (adw_action_row_get_subtitle (ADW_ACTION_ROW (row)),
                               adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row)),
                               0,
                               0,
                               0);
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

  if (self->urls)
    ephy_history_url_list_free (self->urls);
  self->urls = ephy_history_url_list_copy (result_data);

  gtk_list_box_remove_all (GTK_LIST_BOX (self->listbox));

  self->num_fetch = NUM_FETCH_LIMIT;
  self->sorter_source = g_idle_add ((GSourceFunc)add_urls_source, self);
}

static GList *
substrings_filter (EphyHistoryDialog *self)
{
  const gchar *search_text = gtk_editable_get_text (GTK_EDITABLE (self->search_entry));
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
    ephy_history_url_list_free (self->urls);
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
get_checked_rows (EphyHistoryDialog *self)
{
  GList *checked_rows = NULL;
  GtkListBoxRow *row;
  int i = 0;

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->listbox), i++))) {
    GtkCheckButton *check_button =
      GTK_CHECK_BUTTON (g_object_get_data (G_OBJECT (row), "check-button"));

    if (gtk_check_button_get_active (check_button))
      checked_rows = g_list_prepend (checked_rows, row);
  }

  return checked_rows;
}

static void
on_browse_history_deleted_cb (gpointer service,
                              gboolean success,
                              gpointer result_data,
                              gpointer user_data)
{
  EphyHistoryDialog *self = EPHY_HISTORY_DIALOG (user_data);

  if (success) {
    g_autoptr (GList) checked_rows = get_checked_rows (self);
    GList *iter = NULL;

    for (iter = checked_rows; iter; iter = g_list_next (iter))
      gtk_list_box_remove (GTK_LIST_BOX (self->listbox), iter->data);

    if (!gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->listbox), 0)) {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->search_button), FALSE);
      set_has_data (self, FALSE);
    }
  }

  set_selection_active (self, FALSE);
}

static void
delete_checked_rows (EphyHistoryDialog *self)
{
  g_autoptr (GList) checked_rows = get_checked_rows (self);
  GList *deleted_urls = NULL;
  GList *iter = NULL;

  for (iter = checked_rows; iter; iter = g_list_next (iter)) {
    EphyHistoryURL *url = get_url_from_row (iter->data);

    deleted_urls = g_list_prepend (deleted_urls, url);
  }

  ephy_history_service_delete_urls (self->history_service, deleted_urls, self->cancellable,
                                    (EphyHistoryJobCallback)on_browse_history_deleted_cb, self);

  for (iter = deleted_urls; iter; iter = g_list_next (iter))
    ephy_snapshot_service_delete_snapshot_for_url (self->snapshot_service, ((EphyHistoryURL *)iter->data)->url);

  g_list_free_full (deleted_urls, (GDestroyNotify)ephy_history_url_free);
}

static GtkWidget *
get_target_window (EphyHistoryDialog *self)
{
  return GTK_WIDGET (gtk_application_get_active_window (GTK_APPLICATION (ephy_shell_get_default ())));
}

static void
row_copy_url_button_clicked (GtkWidget *button,
                             gpointer   user_data)
{
  EphyHistoryDialog *self = user_data;
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (gtk_widget_get_ancestor (button, GTK_TYPE_LIST_BOX_ROW));
  g_autoptr (EphyHistoryURL) url = get_url_from_row (row);

  if (url) {
    AdwToast *toast = adw_toast_new (_("Link copied"));

    gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)), url->url);
    adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (self->toast_overlay), toast);
  }
}

static void
row_check_button_toggled (GtkCheckButton    *check_button,
                          EphyHistoryDialog *self)
{
  g_autoptr (GList) checked_rows = get_checked_rows (self);
  g_autoptr (GList) total_rows = NULL;
  GtkListBoxRow *row;
  int i = 0;
  guint n_checked_rows = g_list_length (checked_rows);
  guint n_rows;

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->listbox), i++)))
    total_rows = g_list_prepend (total_rows, row);

  n_rows = g_list_length (total_rows);

  if (!gtk_check_button_get_active (check_button) && self->is_all_selected)
    set_is_all_selected (self, FALSE);
  else if (n_rows == n_checked_rows)
    set_is_all_selected (self, TRUE);

  set_is_selection_empty (self, n_checked_rows == 0);
  update_ui_state (self);
}

static void
ephy_history_dialog_row_favicon_loaded_cb (GObject      *source,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  g_autoptr (GtkWidget) icon = user_data;
  WebKitFaviconDatabase *database = WEBKIT_FAVICON_DATABASE (source);
  g_autoptr (GdkTexture) icon_texture = NULL;
  g_autoptr (GIcon) favicon = NULL;
  int scale;

  icon_texture = webkit_favicon_database_get_favicon_finish (database, result, NULL);
  if (!icon_texture)
    return;

  scale = gtk_widget_get_scale_factor (icon);
  favicon = ephy_favicon_get_from_texture_scaled (icon_texture, FAVICON_SIZE * scale, FAVICON_SIZE * scale);
  if (favicon && icon)
    gtk_image_set_from_gicon (GTK_IMAGE (icon), favicon);
}

static GtkWidget *
create_row (EphyHistoryDialog *self,
            EphyHistoryURL    *url)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  WebKitFaviconDatabase *database;
  GtkWidget *icon;
  GtkWidget *date;
  GtkWidget *row;
  GtkWidget *check_button;
  GtkWidget *copy_url_button;
  g_autofree char *title_escaped = g_markup_escape_text (url->title, -1);
  g_autofree char *subtitle_escaped = NULL;
  g_autofree char *decoded_url = ephy_uri_decode (url->url);

  if (!decoded_url)
    decoded_url = url->url;
  subtitle_escaped = g_markup_escape_text (decoded_url, -1);

  /* Row */
  row = adw_action_row_new ();
  adw_action_row_set_title_lines (ADW_ACTION_ROW (row), 1);
  adw_action_row_set_subtitle_lines (ADW_ACTION_ROW (row), 1);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title_escaped);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (row), subtitle_escaped);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
  gtk_widget_set_tooltip_text (row, decoded_url);

  /* Fav Icon */
  icon = gtk_image_new ();
  gtk_image_set_pixel_size (GTK_IMAGE (icon), 16);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), icon);

  database = ephy_embed_shell_get_favicon_database (shell);
  webkit_favicon_database_get_favicon (database,
                                       url->url,
                                       self->cancellable,
                                       (GAsyncReadyCallback)ephy_history_dialog_row_favicon_loaded_cb,
                                       g_object_ref (icon));

  /* Date */
  date = gtk_label_new (ephy_time_helpers_utf_friendly_time (url->last_visit_time / 1000000));
  gtk_label_set_ellipsize (GTK_LABEL (date), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (date), 0);

  /* CheckButton */
  check_button = gtk_check_button_new ();
  g_object_set_data (G_OBJECT (row), "check-button", check_button);
  gtk_widget_set_valign (check_button, GTK_ALIGN_CENTER);
  gtk_widget_set_tooltip_text (check_button, _("Remove the selected pages from history"));
  gtk_widget_add_css_class (check_button, "selection-mode");
  g_signal_connect (check_button, "toggled", G_CALLBACK (row_check_button_toggled), self);

  /* Copy URL button */
  copy_url_button = gtk_button_new_from_icon_name ("edit-copy-symbolic");
  gtk_widget_set_valign (copy_url_button, GTK_ALIGN_CENTER);
  gtk_widget_set_tooltip_text (copy_url_button, _("Copy URL"));
  gtk_widget_add_css_class (copy_url_button, "flat");
  g_signal_connect (copy_url_button, "clicked", G_CALLBACK (row_copy_url_button_clicked), self);

  adw_action_row_add_prefix (ADW_ACTION_ROW (row), check_button);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), date);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), copy_url_button);

  gtk_widget_set_sensitive (check_button, ephy_embed_shell_get_mode (shell) != EPHY_EMBED_SHELL_MODE_INCOGNITO);

  /* Hide the CheckButton if selection isn't active */
  if (!self->selection_active)
    gtk_widget_set_visible (check_button, FALSE);

  return row;
}

static gboolean
add_urls_source (EphyHistoryDialog *self)
{
  EphyHistoryURL *url;
  GList *element;
  GtkWidget *row;
  gboolean has_results;
  gboolean prev_is_loading = self->is_loading;
  gboolean prev_has_results = self->has_search_results;
  gboolean prev_has_data = self->has_data;

  set_is_loading (self, FALSE);

  has_results = !!gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->listbox), 0);
  set_has_search_results (self, has_results);

  if (!has_results)
    set_has_data (self, FALSE);

  if (!self->urls || !self->num_fetch) {
    self->sorter_source = 0;
    gtk_widget_queue_draw (self->listbox);

    if (prev_is_loading != self->is_loading ||
        prev_has_data != self->has_data ||
        prev_has_results != has_results)
      update_ui_state (self);

    return G_SOURCE_REMOVE;
  }

  element = self->urls;
  url = element->data;

  row = create_row (self, url);
  gtk_list_box_insert (GTK_LIST_BOX (self->listbox), row, -1);
  set_has_data (self, TRUE);
  set_is_all_selected (self, FALSE);

  self->urls = g_list_remove_link (self->urls, element);
  ephy_history_url_free (url);
  g_list_free_1 (element);

  self->num_fetch--;

  if (prev_is_loading != self->is_loading ||
      prev_has_data != self->has_data ||
      prev_has_results != has_results)
    update_ui_state (self);

  if (!self->num_fetch) {
    self->sorter_source = 0;
    return G_SOURCE_REMOVE;
  }

  return G_SOURCE_CONTINUE;
}

static void
confirmation_dialog_response_cb (EphyHistoryDialog *self)
{
  const char *search_text = gtk_editable_get_text (GTK_EDITABLE (self->search_entry));
  g_autoptr (GList) visible_rows = NULL;
  GtkListBoxRow *row;
  int i = 0;
  GList *deleted_urls = NULL;
  GList *iter = NULL;

  if (g_strcmp0 (search_text, "") == 0) {
    ephy_history_service_clear (self->history_service,
                                NULL, NULL, NULL);
    ephy_snapshot_service_delete_all_snapshots (self->snapshot_service);
  } else {
    while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->listbox), i++)))
      visible_rows = g_list_prepend (visible_rows, row);

    for (iter = visible_rows; iter; iter = g_list_next (iter)) {
      EphyHistoryURL *url = get_url_from_row (iter->data);

      deleted_urls = g_list_prepend (deleted_urls, url);
    }

    ephy_history_service_delete_urls (self->history_service, deleted_urls, self->cancellable,
                                      (EphyHistoryJobCallback)on_browse_history_deleted_cb, self);

    for (iter = deleted_urls; iter; iter = g_list_next (iter))
      ephy_snapshot_service_delete_snapshot_for_url (self->snapshot_service, ((EphyHistoryURL *)iter->data)->url);

    g_list_free_full (deleted_urls, (GDestroyNotify)ephy_history_url_free);
  }

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->search_button), FALSE);
  filter_now (self);
}

static void
on_search_entry_changed (GtkSearchEntry    *search_entry,
                         EphyHistoryDialog *self)
{
  set_is_all_selected (self, FALSE);
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
key_pressed_cb (EphyHistoryDialog *self,
                guint              keyval,
                guint              keycode,
                GdkModifierType    state)
{
  /* Keep track internally of the Shift modifier needed for the
   * interval selection logic */
  if (keyval == GDK_KEY_Shift_L || keyval == GDK_KEY_Shift_R)
    self->shift_modifier_active = TRUE;

  return GDK_EVENT_PROPAGATE;
}

static gboolean
key_released_cb (EphyHistoryDialog *self,
                 guint              keyval,
                 guint              keycode,
                 GdkModifierType    state)
{
  /* Keep track internally of the Shift modifier needed for the
   * interval selection logic */
  if (keyval == GDK_KEY_Shift_L || keyval == GDK_KEY_Shift_R)
    self->shift_modifier_active = FALSE;

  return GDK_EVENT_PROPAGATE;
}

static void
check_rows_interval (GtkListBox *listbox,
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
    GtkCheckButton *check_button = GTK_CHECK_BUTTON (g_object_get_data (G_OBJECT (row), "check-button"));

    gtk_check_button_set_active (check_button, TRUE);
  }
}

static void
handle_selection_row_activated_event (EphyHistoryDialog *self,
                                      GtkListBoxRow     *activated_row)
{
  g_autoptr (GList) checked_rows = get_checked_rows (self);
  GtkCheckButton *check_button = GTK_CHECK_BUTTON (g_object_get_data (G_OBJECT (activated_row), "check-button"));
  gboolean button_checked = gtk_check_button_get_active (check_button);

  /* If Shift modifier isn't active, event simply toggles the row's checkbox button */
  if (!self->shift_modifier_active) {
    gtk_check_button_set_active (check_button, !button_checked);
    return;
  }

  /* If Shift modifier is active, do the row interval logic */
  if (g_list_length (checked_rows) == 1) {
    /* If there's exactly one other row checked we check the interval between
     * that one and the currently clicked row */
    gint index_a = gtk_list_box_row_get_index (activated_row);
    gint index_b = gtk_list_box_row_get_index (checked_rows->data);

    check_rows_interval (GTK_LIST_BOX (self->listbox), index_a, index_b);
  } else {
    /* If there are zero or more than one other rows checked,
     * then we check the clicked row and uncheck all the others */
    GtkListBoxRow *row;
    int i = 0;

    while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->listbox), i++))) {
      GtkCheckButton *row_check_btn =
        GTK_CHECK_BUTTON (g_object_get_data (G_OBJECT (row), "check-button"));

      gtk_check_button_set_active (row_check_btn, FALSE);
    }

    gtk_check_button_set_active (check_button, TRUE);
  }
}

static void
on_listbox_row_activated (GtkListBox        *box,
                          GtkListBoxRow     *row,
                          EphyHistoryDialog *self)
{
  /* If a History row is activated outside of selection mode, we open the
   * row's web page in a new tab*/
  if (!self->selection_active) {
    EphyWindow *window = EPHY_WINDOW (get_target_window (self));
    g_autoptr (EphyHistoryURL) url = get_url_from_row (row);
    EphyEmbed *embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                           window, NULL, EPHY_NEW_TAB_JUMP);

    ephy_web_view_load_url (ephy_embed_get_web_view (embed), url->url);
    gtk_widget_grab_focus (GTK_WIDGET (row));
  } else {
    /* Selection mode is active, run selection logic */
    handle_selection_row_activated_event (self, row);
  }
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

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

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
on_clear_button_clicked (GtkButton         *button,
                         EphyHistoryDialog *self)
{
  AdwDialog *dialog;

  dialog = adw_alert_dialog_new (_("Clear Browsing History?"),
                                 _("All visible links will be permanently deleted"));

  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "cancel", _("_Cancel"),
                                  "clear", _("Cl_ear"),
                                  NULL);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
                                            "clear",
                                            ADW_RESPONSE_DESTRUCTIVE);

  g_signal_connect_swapped (dialog, "response::clear",
                            G_CALLBACK (confirmation_dialog_response_cb),
                            self);

  adw_dialog_present (dialog, GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self))));
}

static void
on_selection_delete_button_clicked (GtkButton         *button,
                                    EphyHistoryDialog *self)
{
  delete_checked_rows (self);
}

static void
on_selection_open_button_clicked (GtkWidget         *open_button,
                                  EphyHistoryDialog *self)
{
  /* Open checked rows URLs in new tabs */
  EphyWindow *window = EPHY_WINDOW (get_target_window (self));
  g_autoptr (GList) checked_rows = get_checked_rows (self);
  GList *iter = NULL;

  for (iter = checked_rows; iter; iter = g_list_next (iter)) {
    g_autoptr (EphyHistoryURL) url = get_url_from_row (iter->data);
    EphyEmbed *embed;

    embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                window, NULL, EPHY_NEW_TAB_JUMP);
    ephy_web_view_load_url (ephy_embed_get_web_view (embed), url->url);
  }
}

static gboolean
shift_activate_cb (EphyHistoryDialog *self)
{
  GtkWidget *focused_widget;

  if (!self->selection_active)
    return GDK_EVENT_PROPAGATE;

  focused_widget = adw_dialog_get_focus (ADW_DIALOG (self));

  if (GTK_IS_LIST_BOX_ROW (focused_widget)) {
    g_signal_emit_by_name (self->listbox, "row-activated", focused_widget, self);

    return GDK_EVENT_STOP;
  }

  return GDK_EVENT_PROPAGATE;
}

static gboolean
delete_selected_cb (EphyHistoryDialog *self)
{
  if (self->selection_active && !self->is_selection_empty) {
    delete_checked_rows (self);

    return GDK_EVENT_STOP;
  }

  /* TODO: Delete the focused row if not in selection mode? */

  return GDK_EVENT_PROPAGATE;
}

static gboolean
find_shortcut_cb (EphyHistoryDialog *self)
{
  gboolean search;

  search = gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (self->search_bar));

  if (!search && !self->has_data)
    return GDK_EVENT_PROPAGATE;

  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (self->search_bar), !search);

  return GDK_EVENT_STOP;
}

static gboolean
load_more_shortcut_cb (GtkWidget         *listbox,
                       GVariant          *args,
                       EphyHistoryDialog *self)
{
  GtkWidget *last = gtk_widget_get_last_child (listbox);
  GtkWidget *focus = gtk_widget_get_focus_child (listbox);

  if (focus == last)
    load_further_data (self);

  return GDK_EVENT_PROPAGATE;
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
                         NULL, NULL,
                         EPHY_TYPE_HISTORY_SERVICE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/history-dialog.ui");

  /* UI Elements */
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, header_bars_stack);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, window_header_bar);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, search_button);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, selection_button);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, selection_header_bar);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, search_bar);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, search_entry);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, toast_overlay);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, history_presentation_stack);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, history_scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, listbox);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, loading_spinner);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, empty_history_message);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, no_search_results_message);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, clear_button);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, action_bar_revealer);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, select_all_button);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, selection_delete_button);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, selection_open_button);

  gtk_widget_class_bind_template_callback (widget_class, key_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, key_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_cancel_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_edge_reached);
  gtk_widget_class_bind_template_callback (widget_class, on_clear_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_select_all_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_delete_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_open_button_clicked);

  gtk_widget_class_add_binding (widget_class, GDK_KEY_Return, GDK_SHIFT_MASK,
                                (GtkShortcutFunc)shift_activate_cb,
                                NULL);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_ISO_Enter, GDK_SHIFT_MASK,
                                (GtkShortcutFunc)shift_activate_cb,
                                NULL);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_KP_Enter, GDK_SHIFT_MASK,
                                (GtkShortcutFunc)shift_activate_cb,
                                NULL);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_space, GDK_SHIFT_MASK,
                                (GtkShortcutFunc)shift_activate_cb,
                                NULL);

  gtk_widget_class_add_binding (widget_class, GDK_KEY_Delete, 0,
                                (GtkShortcutFunc)delete_selected_cb,
                                NULL);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_KP_Delete, 0,
                                (GtkShortcutFunc)delete_selected_cb,
                                NULL);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_F, GDK_CONTROL_MASK,
                                (GtkShortcutFunc)find_shortcut_cb,
                                NULL);
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

static void
ephy_history_dialog_init (EphyHistoryDialog *self)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  const char *tooltip;
  GtkEventController *controller;
  GtkShortcut *shortcut;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->snapshot_service = ephy_snapshot_service_get_default ();
  self->cancellable = g_cancellable_new ();
  self->urls = NULL;
  self->sorter_source = 0;
  self->is_selection_empty = TRUE;
  self->is_all_selected = FALSE;

  gtk_search_bar_connect_entry (GTK_SEARCH_BAR (self->search_bar),
                                GTK_EDITABLE (self->search_entry));

  if (ephy_embed_shell_get_mode (shell) == EPHY_EMBED_SHELL_MODE_INCOGNITO) {
    tooltip = _("Unavailable in Incognito Mode");
    set_can_clear (self, FALSE);
  } else {
    tooltip = _("Clear History");
    set_can_clear (self, TRUE);
  }

  gtk_widget_set_tooltip_text (self->clear_button, tooltip);
  set_is_loading (self, TRUE);
  update_ui_state (self);

  adw_status_page_set_icon_name (ADW_STATUS_PAGE (self->empty_history_message),
                                 APPLICATION_ID "-symbolic");

  shortcut = gtk_shortcut_new (gtk_alternative_trigger_new (gtk_keyval_trigger_new (GDK_KEY_Down, 0),
                                                            gtk_keyval_trigger_new (GDK_KEY_Page_Down, 0)),
                               gtk_callback_action_new ((GtkShortcutFunc)load_more_shortcut_cb, self, NULL));

  controller = gtk_shortcut_controller_new ();
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);
  gtk_widget_add_controller (self->listbox, controller);
}
