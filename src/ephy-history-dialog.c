/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti <mpeseng@tin.it>
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2012 Igalia S.L
 *  Copyright © 2018 Jan-Michael Brummer
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
#include <string.h>
#include <time.h>

#define NUM_FETCH_LIMIT 15

struct _EphyHistoryDialog {
  GtkWindow parent_instance;

  EphySnapshotService *snapshot_service;
  EphyHistoryService *history_service;
  GCancellable *cancellable;

  GtkWidget *listbox;
  GtkWidget *forget_all_button;
  GtkWidget *popup_menu;
  GtkWidget *search_bar;
  GtkWidget *search_button;

  GActionGroup *action_group;

  GList *urls;
  guint sorter_source;

  char *search_text;

  gint num_fetch;

  GtkWidget *confirmation_dialog;
};

G_DEFINE_TYPE (EphyHistoryDialog, ephy_history_dialog, GTK_TYPE_WINDOW)

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
  return ephy_history_url_new (g_object_get_data (G_OBJECT (row), "url"),
                               g_object_get_data (G_OBJECT (row), "title"),
                               0,
                               0,
                               0);
}

static void
clear_listbox (GtkWidget *listbox)
{
  GList *children, *iter;

  children = gtk_container_get_children (GTK_CONTAINER (listbox));

  for (iter = children; iter != NULL; iter = g_list_next (iter)) {
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

  if (success != TRUE)
    return;


  self->urls = (GList *)result_data;

  clear_listbox (self->listbox);

  self->num_fetch = NUM_FETCH_LIMIT;
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

  g_free (tokens);

  return substrings;
}

static void
remove_pending_sorter_source (EphyHistoryDialog *self,
                              gboolean           free_urls)
{
  if (self->sorter_source != 0) {
    g_source_remove (self->sorter_source);
    self->sorter_source = 0;
  }

  if (free_urls && self->urls != NULL) {
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

  for (tmp = selected_rows; tmp != NULL; tmp = tmp->next) {
    EphyHistoryURL *url = get_url_from_row (tmp->data);

    if (url == NULL) {
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

  if (success != TRUE)
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
forget_clicked (GtkButton *button,
                gpointer   user_data)
{
  EphyHistoryDialog *self = EPHY_HISTORY_DIALOG (user_data);
  GtkListBoxRow *row = g_object_get_data (G_OBJECT (button), "row");

  gtk_list_box_select_row (GTK_LIST_BOX (self->listbox), row);

  delete_selected (self);
}

static GtkWidget *
create_row (EphyHistoryDialog *self,
            EphyHistoryURL    *url)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  GtkWidget *grid;
  GtkWidget *title;
  GtkWidget *address;
  GtkWidget *date;
  GtkWidget *row;
  GtkWidget *separator;
  GtkWidget *button;
  PangoAttrList *attrlist;
  PangoAttribute *attr;

  /* Row */
  row = gtk_list_box_row_new ();
  g_object_set_data (G_OBJECT (row), "title", g_strdup (url->title));
  g_object_set_data (G_OBJECT (row), "url", g_strdup (url->url));

  /* Grid */
  grid = gtk_grid_new ();
  gtk_widget_set_margin_start (grid, 6);
  gtk_widget_set_margin_end (grid, 6);
  gtk_widget_set_margin_top (grid, 6);
  gtk_widget_set_margin_bottom (grid, 6);
  gtk_grid_set_column_spacing (GTK_GRID(grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID(grid), 6);
  gtk_widget_set_tooltip_text (grid, url->url);

  /* Title */
  title = gtk_label_new (url->title);
  gtk_label_set_ellipsize (GTK_LABEL(title), PANGO_ELLIPSIZE_END);
  gtk_widget_set_hexpand (title, TRUE);
  gtk_label_set_xalign (GTK_LABEL(title), 0);

  attrlist = pango_attr_list_new ();
  attr = pango_attr_weight_new (PANGO_WEIGHT_SEMIBOLD);
  pango_attr_list_insert (attrlist, attr);
  gtk_label_set_attributes (GTK_LABEL (title), attrlist);
  pango_attr_list_unref (attrlist);

  gtk_grid_attach (GTK_GRID (grid), title, 0, 0, 1, 1);

  /* Address */
  address = gtk_label_new (url->url);
  gtk_label_set_ellipsize (GTK_LABEL(address), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL(address), 0);
  gtk_widget_set_sensitive (address, FALSE);

  gtk_grid_attach (GTK_GRID (grid), address, 0, 1, 1, 1);

  /* Date */
  date = gtk_label_new (ephy_time_helpers_utf_friendly_time (url->last_visit_time / 1000000));
  gtk_label_set_ellipsize (GTK_LABEL(date), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (date), 0);

  gtk_grid_attach (GTK_GRID (grid), date, 0, 2, 1, 1);

  /* Separator */
  separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_grid_attach (GTK_GRID (grid), separator, 1, 0, 1, 3);

  /* Button */
  button = gtk_button_new_from_icon_name ("user-trash-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  g_object_set_data (G_OBJECT (button), "row", row);
  gtk_widget_set_tooltip_text (button, _("Remove the selected pages from history"));
  g_signal_connect (button, "clicked", G_CALLBACK (forget_clicked), self);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_grid_attach (GTK_GRID (grid), button, 2, 0, 1, 3);

  gtk_widget_set_sensitive (button, ephy_embed_shell_get_mode (shell) != EPHY_EMBED_SHELL_MODE_INCOGNITO);

  gtk_container_add (GTK_CONTAINER (row), grid);
  gtk_widget_show_all (row);

  return row;
}

static gboolean
add_urls_source (EphyHistoryDialog *self)
{
  EphyHistoryURL *url;
  GList *element;
  GtkWidget *row;

  if (self->urls == NULL || !self->num_fetch) {
    self->sorter_source = 0;
    gtk_widget_queue_draw (self->listbox);
    return G_SOURCE_REMOVE;
  }

  element = self->urls;
  url = element->data;

  row = create_row (self, url);
  gtk_list_box_insert (GTK_LIST_BOX(self->listbox), row, -1);

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
on_key_press_event (EphyHistoryDialog *self,
                    GdkEvent          *event,
                    gpointer           user_data)
{
  GdkEventKey *key = (GdkEventKey *)event;
  gint ret;

  ret = gtk_search_bar_handle_event (GTK_SEARCH_BAR (self->search_bar), event);

  if (ret != GDK_EVENT_STOP) {
    if (key->keyval == GDK_KEY_Escape) {
      if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->search_button)))
        gtk_widget_destroy (GTK_WIDGET (self));
      else
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->search_button), FALSE);
    }
    else if (isprint (key->keyval))
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->search_button), TRUE);
  }

  return ret;
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
  update_selection_actions (self->action_group, row != NULL);
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
  g_assert (url != NULL);

  embed = ephy_shell_new_tab (ephy_shell_get_default (),
                              window, NULL, EPHY_NEW_TAB_JUMP);
  ephy_web_view_load_url (ephy_embed_get_web_view (embed), url->url);
  ephy_history_url_free (url);
}

static gboolean
on_listbox_button_press_event (GtkWidget         *widget,
                               GdkEventButton    *event,
                               EphyHistoryDialog *self)
{
  if (event->button == GDK_BUTTON_SECONDARY) {
    GtkListBoxRow *row = gtk_list_box_get_row_at_y (GTK_LIST_BOX (self->listbox), event->y);
    GList *rows = NULL;
    guint state = event->state & gtk_accelerator_get_default_mod_mask ();
    int n;

    if (!row)
      return GDK_EVENT_PROPAGATE;

    if (state != GDK_CONTROL_MASK)
      gtk_list_box_unselect_all (GTK_LIST_BOX (self->listbox));

    gtk_list_box_select_row (GTK_LIST_BOX (self->listbox), row);
    rows = gtk_list_box_get_selected_rows (GTK_LIST_BOX (self->listbox));
    n = g_list_length (rows);
    g_list_free (rows);

    update_popup_menu_actions (self->action_group, n == 1);

    gtk_menu_popup_at_pointer (GTK_MENU (self->popup_menu), (GdkEvent *)event);

    return GDK_EVENT_STOP;
  }

  return GDK_EVENT_PROPAGATE;
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

  remove_pending_sorter_source (self, TRUE);

  G_OBJECT_CLASS (ephy_history_dialog_parent_class)->dispose (object);
}

static void
box_header_func (GtkListBoxRow *row,
                 GtkListBoxRow *before,
                 gpointer       user_data)
{
  GtkWidget *current;

  if (!before) {
    gtk_list_box_row_set_header (row, NULL);
    return;
  }

  current = gtk_list_box_row_get_header (row);
  if (!current) {
    current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_show (current);
    gtk_list_box_row_set_header (row, current);
  }
}

static void
on_edge_reached (GtkScrolledWindow *scrolled,
                 GtkPositionType    pos,
                 gpointer           user_data)
{
  EphyHistoryDialog *self = EPHY_HISTORY_DIALOG (user_data);

  if (pos == GTK_POS_BOTTOM) {
    remove_pending_sorter_source (self, FALSE);

    self->num_fetch += NUM_FETCH_LIMIT;
    self->sorter_source = g_idle_add ((GSourceFunc)add_urls_source, self);
  }
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
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, forget_all_button);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, popup_menu);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, search_bar);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialog, search_button);

  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_selected);
  gtk_widget_class_bind_template_callback (widget_class, on_listbox_button_press_event);
  gtk_widget_class_bind_template_callback (widget_class, on_listbox_key_press_event);
  gtk_widget_class_bind_template_callback (widget_class, on_key_press_event);
  gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_edge_reached);
}

GtkWidget *
ephy_history_dialog_new (EphyHistoryService *history_service)
{
  EphyHistoryDialog *self;

  g_assert (history_service != NULL);

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

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->listbox), box_header_func, NULL, NULL);
  ephy_gui_ensure_window_group (GTK_WINDOW (self));

  gtk_menu_attach_to_widget (GTK_MENU (self->popup_menu), GTK_WIDGET (self), NULL);

  self->action_group = create_action_group (self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "history", self->action_group);

  if (ephy_embed_shell_get_mode (shell) == EPHY_EMBED_SHELL_MODE_INCOGNITO) {
    tooltip = _("It is not possible to modify history when in incognito mode.");
    gtk_widget_set_tooltip_text (self->forget_all_button, tooltip);

    action = g_action_map_lookup_action (G_ACTION_MAP (self->action_group), "forget-all");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

    update_selection_actions (self->action_group, FALSE);
  }
}

