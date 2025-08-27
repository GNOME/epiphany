/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2025 Jan-Michael Brummer
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

#include "ephy-passwords-view.h"

#include "ephy-shell.h"

#include "preferences/ephy-data-view.h"

struct _EphyPasswordsView {
  AdwDialog parent_instance;

  GtkWidget *remember_switch;

  EphyPasswordManager *manager;
  EphyDataView *data_view;
  GList *records;
  GtkWidget *toast_overlay;
  GtkWidget *listbox;
  GtkWidget *confirmation_dialog;

  GActionGroup *action_group;
  GCancellable *cancellable;
  GMenu *options_menu;
};

G_DEFINE_FINAL_TYPE (EphyPasswordsView, ephy_passwords_view, ADW_TYPE_DIALOG)

static void populate_model (EphyPasswordsView *passwords_view);

static void
on_search_text_changed (EphyPasswordsView *passwords_view)
{
  ephy_data_view_set_has_search_results (EPHY_DATA_VIEW (passwords_view->data_view), FALSE);
  gtk_list_box_invalidate_filter (GTK_LIST_BOX (passwords_view->listbox));
}

static void
forget_operation_finished_cb (GObject      *source_object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  EphyPasswordsView *passwords_view = user_data;
  g_autoptr (GError) error = NULL;

  if (!ephy_password_manager_forget_finish (EPHY_PASSWORD_MANAGER (source_object), result, &error)) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning ("Failed to forget password: %s", error->message);
    return;
  }

  /* Repopulate the list */
  ephy_data_view_set_has_data (EPHY_DATA_VIEW (passwords_view->data_view), FALSE);
  populate_model (passwords_view);
}

static void
forget_clicked (GtkWidget *button,
                gpointer   user_data)
{
  EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (user_data);
  EphyPasswordsView *passwords_view = g_object_get_data (G_OBJECT (record), "passwords-view");

  ephy_password_manager_forget (passwords_view->manager,
                                ephy_password_record_get_id (record),
                                passwords_view->cancellable,
                                forget_operation_finished_cb,
                                passwords_view);

  /* Clear internal state */
  gtk_list_box_remove_all (GTK_LIST_BOX (passwords_view->listbox));
  g_list_free_full (passwords_view->records, g_object_unref);
  passwords_view->records = NULL;

  /* Present loading spinner while waiting for the async forget op to finish */
  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (passwords_view->data_view), TRUE);
}


static void
copy_password_clicked (GtkWidget *button,
                       gpointer   user_data)
{
  const char *password = user_data;

  if (password) {
    EphyPasswordsView *self = EPHY_PASSWORDS_VIEW (gtk_widget_get_ancestor (button, EPHY_TYPE_PASSWORDS_VIEW));
    AdwToastOverlay *toast_overlay = ephy_data_view_get_toast_overlay (self->data_view);
    AdwToast *toast = adw_toast_new (_("Password copied"));

    gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)), password);
    adw_toast_overlay_add_toast (toast_overlay, toast);
  }
}

static void
copy_username_clicked (GtkWidget *button,
                       gpointer   user_data)
{
  const char *username = user_data;

  if (username)
    gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)), username);
}

static void
ephy_passwords_view_class_init (EphyPasswordsViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/passwords-view.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsView, listbox);
  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsView, data_view);
  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsView, options_menu);
  gtk_widget_class_bind_template_callback (widget_class, on_search_text_changed);
}

static void
confirmation_dialog_response_cb (EphyPasswordsView *self)
{
  ephy_password_manager_forget_all (self->manager);

  gtk_list_box_remove_all (GTK_LIST_BOX (self->listbox));
  ephy_data_view_set_has_data (EPHY_DATA_VIEW (self->data_view), FALSE);

  g_list_free_full (self->records, g_object_unref);
  self->records = NULL;
}

static GtkWidget *
confirmation_dialog_construct (EphyPasswordsView *self)
{
  AdwDialog *dialog;

  dialog = adw_alert_dialog_new (_("Delete All Passwords?"),
                                 _("This will clear all locally stored passwords, and can not be undone."));

  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "cancel", _("_Cancel"),
                                  "delete", _("_Delete"),
                                  NULL);
  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
                                            "delete",
                                            ADW_RESPONSE_DESTRUCTIVE);

  g_signal_connect_swapped (dialog, "response::delete",
                            G_CALLBACK (confirmation_dialog_response_cb),
                            self);

  return GTK_WIDGET (dialog);
}

static void
forget_all (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       user_data)
{
  EphyPasswordsView *self = EPHY_PASSWORDS_VIEW (user_data);

  if (!self->confirmation_dialog) {
    GtkWidget **confirmation_dialog;

    self->confirmation_dialog = confirmation_dialog_construct (self);
    confirmation_dialog = &self->confirmation_dialog;
    g_object_add_weak_pointer (G_OBJECT (self->confirmation_dialog),
                               (gpointer *)confirmation_dialog);
  }

  adw_dialog_present (ADW_DIALOG (self->confirmation_dialog), GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self))));
}

static void
on_username_apply (AdwEntryRow *row,
                   gpointer     user_data)
{
  EphyPasswordsView *self = EPHY_PASSWORDS_VIEW (user_data);
  GtkWidget *expander_row = gtk_widget_get_ancestor (GTK_WIDGET (row), ADW_TYPE_EXPANDER_ROW);
  EphyPasswordRecord *record = g_object_get_data (G_OBJECT (expander_row), "record");

  ephy_password_manager_save (self->manager,
                              ephy_password_record_get_origin (record),
                              ephy_password_record_get_target_origin (record),
                              ephy_password_record_get_username (record),
                              gtk_editable_get_text (GTK_EDITABLE (row)),
                              ephy_password_record_get_password (record),
                              ephy_password_record_get_username_field (record),
                              ephy_password_record_get_password_field (record),
                              FALSE);
}

static void
on_password_apply (AdwEntryRow *row,
                   gpointer     user_data)
{
  EphyPasswordsView *self = EPHY_PASSWORDS_VIEW (user_data);
  GtkWidget *expander_row = gtk_widget_get_ancestor (GTK_WIDGET (row), ADW_TYPE_EXPANDER_ROW);
  EphyPasswordRecord *record = g_object_get_data (G_OBJECT (expander_row), "record");

  ephy_password_manager_save (self->manager,
                              ephy_password_record_get_origin (record),
                              ephy_password_record_get_target_origin (record),
                              ephy_password_record_get_username (record),
                              ephy_password_record_get_username (record),
                              gtk_editable_get_text (GTK_EDITABLE (row)),
                              ephy_password_record_get_username_field (record),
                              ephy_password_record_get_password_field (record),
                              FALSE);
}

static void
populate_model_cb (GList    *records,
                   gpointer  user_data)
{
  EphyPasswordsView *passwords_view = EPHY_PASSWORDS_VIEW (user_data);

  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (passwords_view->data_view), FALSE);

  for (GList *l = records; l && l->data; l = l->next) {
    EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (l->data);
    GtkWidget *row;
    GtkWidget *sub_row;
    GtkWidget *button;
    const char *text;

    row = adw_expander_row_new ();
    g_object_set_data (G_OBJECT (row), "record", record);
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), ephy_password_record_get_origin (record));
    adw_expander_row_set_subtitle (ADW_EXPANDER_ROW (row), ephy_password_record_get_username (record));
    adw_expander_row_set_show_enable_switch (ADW_EXPANDER_ROW (row), FALSE);

    button = gtk_button_new_from_icon_name ("edit-copy-symbolic");
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text (button, _("Copy password"));
    gtk_widget_add_css_class (button, "flat");
    adw_expander_row_add_suffix (ADW_EXPANDER_ROW (row), button);
    g_signal_connect (button, "clicked", G_CALLBACK (copy_password_clicked), (void *)(ephy_password_record_get_password (record)));

    /* Username */
    sub_row = adw_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (sub_row), _("Username"));
    adw_entry_row_set_show_apply_button (ADW_ENTRY_ROW (sub_row), TRUE);
    g_signal_connect (sub_row, "apply", G_CALLBACK (on_username_apply), passwords_view);
    adw_expander_row_add_row (ADW_EXPANDER_ROW (row), sub_row);

    text = ephy_password_record_get_username (record);
    if (text)
      gtk_editable_set_text (GTK_EDITABLE (sub_row), text);

    button = gtk_button_new_from_icon_name ("edit-copy-symbolic");
    g_signal_connect (button, "clicked", G_CALLBACK (copy_username_clicked), (void *)(ephy_password_record_get_username (record)));
    gtk_widget_set_tooltip_text (button, _("Copy username"));
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (button, "flat");
    adw_entry_row_add_suffix (ADW_ENTRY_ROW (sub_row), button);

    /* Password */
    sub_row = adw_password_entry_row_new ();
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (sub_row), _("Password"));
    adw_entry_row_set_show_apply_button (ADW_ENTRY_ROW (sub_row), TRUE);
    g_signal_connect (sub_row, "apply", G_CALLBACK (on_password_apply), passwords_view);
    adw_expander_row_add_row (ADW_EXPANDER_ROW (row), sub_row);

    text = ephy_password_record_get_password (record);
    if (text)
      gtk_editable_set_text (GTK_EDITABLE (sub_row), text);

    /* Remove button */
    sub_row = adw_action_row_new ();
    adw_expander_row_add_row (ADW_EXPANDER_ROW (row), sub_row);

    button = gtk_button_new_with_label (_("Remove Password"));
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (button, "destructive-action");
    g_signal_connect (button, "clicked", G_CALLBACK (forget_clicked), record);
    adw_action_row_add_suffix (ADW_ACTION_ROW (sub_row), button);

    g_object_set_data (G_OBJECT (record), "passwords-view", passwords_view);

    gtk_list_box_append (GTK_LIST_BOX (passwords_view->listbox), row);
  }

  if (g_list_length (records))
    ephy_data_view_set_has_data (EPHY_DATA_VIEW (passwords_view->data_view), TRUE);

  g_assert (!passwords_view->records);
  passwords_view->records = g_list_copy_deep (records, (GCopyFunc)g_object_ref, NULL);
}

static void
populate_model (EphyPasswordsView *self)
{
  g_assert (EPHY_IS_PASSWORDS_VIEW (self));
  g_assert (!ephy_data_view_get_has_data (EPHY_DATA_VIEW (self->data_view)));

  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (self->data_view), TRUE);
  /* Ask for all password records. */
  ephy_password_manager_query (self->manager,
                               NULL, NULL, NULL, NULL, NULL, NULL,
                               populate_model_cb, self);
}

static GActionGroup *
create_action_group (EphyPasswordsView *self)
{
  const GActionEntry entries[] = {
    { "forget-all", forget_all },
  };

  GSimpleActionGroup *group;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), self);

  return G_ACTION_GROUP (group);
}

static gboolean
password_filter (GtkListBoxRow *row,
                 gpointer       user_data)
{
  EphyPasswordsView *passwords_view = EPHY_PASSWORDS_VIEW (user_data);
  EphyPasswordRecord *record = g_object_get_data (G_OBJECT (row), "record");
  const char *username;
  const char *origin;
  gboolean visible = FALSE;
  const char *search_text = ephy_data_view_get_search_text (EPHY_DATA_VIEW (passwords_view->data_view));

  if (!search_text) {
    gtk_widget_set_visible (GTK_WIDGET (row), TRUE);

    return TRUE;
  }

  origin = ephy_password_record_get_origin (record);
  username = ephy_password_record_get_username (record);

  if (origin && g_strrstr (origin, search_text))
    visible = TRUE;
  else if (username && g_strrstr (username, search_text))
    visible = TRUE;

  if (visible)
    ephy_data_view_set_has_search_results (EPHY_DATA_VIEW (passwords_view->data_view), TRUE);

  gtk_widget_set_visible (GTK_WIDGET (row), visible);

  return visible;
}

static void
ephy_passwords_view_init (EphyPasswordsView *self)
{
  self->manager = ephy_embed_shell_get_password_manager (EPHY_EMBED_SHELL (ephy_shell_get_default ()));

  gtk_widget_init_template (GTK_WIDGET (self));

  self->action_group = create_action_group (self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "passwords", self->action_group);

  self->cancellable = g_cancellable_new ();

  gtk_list_box_set_filter_func (GTK_LIST_BOX (self->listbox), password_filter, self, NULL);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->listbox), GTK_SELECTION_NONE);

  populate_model (self);

  ephy_data_view_set_options_menu (EPHY_DATA_VIEW (self->data_view), G_MENU_MODEL (self->options_menu));
}

void
ephy_passwords_show (EphyWindow *window)
{
  GtkWidget *dialog = g_object_new (EPHY_TYPE_PASSWORDS_VIEW, NULL);

  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (window));
}
