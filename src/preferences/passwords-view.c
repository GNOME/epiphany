/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2013 Red Hat, Inc.
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

#include <dazzle.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <handy.h>
#include <string.h>

#define SECRET_API_SUBJECT_TO_CHANGE
#include <libsecret/secret.h>

#include "ephy-shell.h"
#include "ephy-gui.h"
#include "ephy-uri-helpers.h"
#include "passwords-view.h"

struct _EphyPasswordsView {
  EphyDataView parent_instance;

  EphyPasswordManager *manager;
  GList *records;
  GtkWidget *listbox;
  GtkWidget *confirmation_dialog;

  GActionGroup *action_group;
  GCancellable *cancellable;
};

G_DEFINE_TYPE (EphyPasswordsView, ephy_passwords_view, EPHY_TYPE_DATA_VIEW)

static void populate_model (EphyPasswordsView *passwords_view);

static void
ephy_passwords_view_dispose (GObject *object)
{
  EphyPasswordsView *passwords_view = EPHY_PASSWORDS_VIEW (object);

  g_list_free_full (passwords_view->records, g_object_unref);
  passwords_view->records = NULL;

  g_cancellable_cancel (passwords_view->cancellable);
  g_clear_object (&passwords_view->cancellable);

  G_OBJECT_CLASS (ephy_passwords_view_parent_class)->dispose (object);
}

static void
clear_listbox (GtkWidget *listbox)
{
  GtkListBoxRow *row;

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (listbox), 0)))
    gtk_container_remove (GTK_CONTAINER (listbox), GTK_WIDGET (row));
}

static void
on_search_text_changed (EphyPasswordsView *passwords_view)
{
  ephy_data_view_set_has_search_results (EPHY_DATA_VIEW (passwords_view), FALSE);
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
  ephy_data_view_set_has_data (EPHY_DATA_VIEW (passwords_view), FALSE);
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
  clear_listbox (passwords_view->listbox);
  g_list_free_full (passwords_view->records, g_object_unref);
  passwords_view->records = NULL;

  /* Present loading spinner while waiting for the async forget op to finish */
  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (passwords_view), TRUE);
}


static void
copy_password_clicked (GtkWidget *button,
                       gpointer   user_data)
{
  const char *password = user_data;

  if (password)
    gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button), GDK_SELECTION_CLIPBOARD), password, -1);
}

static void
copy_username_clicked (GtkWidget *button,
                       gpointer   user_data)
{
  const char *username = user_data;

  if (username)
    gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button), GDK_SELECTION_CLIPBOARD), username, -1);
}

static void
ephy_passwords_view_class_init (EphyPasswordsViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ephy_passwords_view_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/passwords-view.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsView, listbox);
  gtk_widget_class_bind_template_callback (widget_class, on_search_text_changed);
}

static void
confirmation_dialog_response_cb (GtkWidget         *dialog,
                                 int                response,
                                 EphyPasswordsView *self)
{
  gtk_widget_destroy (dialog);

  if (response == GTK_RESPONSE_ACCEPT) {
    ephy_password_manager_forget_all (self->manager);

    clear_listbox (self->listbox);
    ephy_data_view_set_has_data (EPHY_DATA_VIEW (self), FALSE);

    g_list_free_full (self->records, g_object_unref);
    self->records = NULL;
  }
}

static GtkWidget *
confirmation_dialog_construct (EphyPasswordsView *self)
{
  GtkWidget *dialog;
  GtkWidget *button;
  GtkWidget *window;

  window = gtk_widget_get_toplevel (GTK_WIDGET (self));

  dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_CANCEL,
                                   _("Delete All Passwords?"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("This will clear all locally stored passwords, and can not be undone."));

  gtk_window_group_add_window (ephy_gui_ensure_window_group (GTK_WINDOW (window)),
                               GTK_WINDOW (dialog));

  button = gtk_button_new_with_mnemonic (_("_Delete"));
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
  EphyPasswordsView *self = EPHY_PASSWORDS_VIEW (user_data);

  if (!self->confirmation_dialog) {
    GtkWidget **confirmation_dialog;

    self->confirmation_dialog = confirmation_dialog_construct (self);
    confirmation_dialog = &self->confirmation_dialog;
    g_object_add_weak_pointer (G_OBJECT (self->confirmation_dialog),
                               (gpointer *)confirmation_dialog);
  }

  gtk_widget_show (self->confirmation_dialog);
}

static void
populate_model_cb (GList    *records,
                   gpointer  user_data)
{
  EphyPasswordsView *passwords_view = EPHY_PASSWORDS_VIEW (user_data);

  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (passwords_view), FALSE);

  for (GList *l = records; l && l->data; l = l->next) {
    EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (l->data);
    GtkWidget *row;
    GtkWidget *sub_row;
    GtkWidget *separator;
    GtkWidget *button;
    GtkWidget *image;
    GtkWidget *entry;
    const char *text;

    row = hdy_expander_row_new ();
    g_object_set_data (G_OBJECT (row), "record", record);
    hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (row), ephy_password_record_get_origin (record));
    hdy_expander_row_set_subtitle (HDY_EXPANDER_ROW (row), ephy_password_record_get_username (record));
    hdy_expander_row_set_show_enable_switch (HDY_EXPANDER_ROW (row), FALSE);

    button = gtk_button_new_from_icon_name ("edit-copy-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text (button, _("Copy password"));
    hdy_expander_row_add_action (HDY_EXPANDER_ROW (row), button);
    g_signal_connect (button, "clicked", G_CALLBACK (copy_password_clicked), (void *)(ephy_password_record_get_password (record)));

    /* Username */
    sub_row = hdy_action_row_new ();
    hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (sub_row), _("Username"));
    gtk_container_add (GTK_CONTAINER (row), sub_row);

    entry = gtk_entry_new ();
    gtk_widget_set_hexpand (entry, TRUE);
    gtk_widget_set_valign (entry, GTK_ALIGN_CENTER);
    gtk_editable_set_editable (GTK_EDITABLE (entry), FALSE);
    gtk_entry_set_alignment (GTK_ENTRY (entry), 1.0f);
    gtk_entry_set_has_frame (GTK_ENTRY (entry), FALSE);

    text = ephy_password_record_get_username (record);
    if (text)
      gtk_entry_set_text (GTK_ENTRY (entry), text);

    gtk_container_add (GTK_CONTAINER (sub_row), entry);

    separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_top (separator, 8);
    gtk_widget_set_margin_bottom (separator, 8);
    gtk_container_add (GTK_CONTAINER (sub_row), separator);

    button = gtk_button_new_from_icon_name ("edit-copy-symbolic", GTK_ICON_SIZE_BUTTON);
    g_signal_connect (button, "clicked", G_CALLBACK (copy_username_clicked), (void *)(ephy_password_record_get_username (record)));
    gtk_widget_set_tooltip_text (button, _("Copy username"));
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_container_add (GTK_CONTAINER (sub_row), button);

    /* Password */
    sub_row = hdy_action_row_new ();
    hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (sub_row), _("Password"));
    gtk_container_add (GTK_CONTAINER (row), sub_row);

    entry = gtk_entry_new ();
    gtk_widget_set_hexpand (entry, TRUE);
    gtk_widget_set_valign (entry, GTK_ALIGN_CENTER);
    gtk_editable_set_editable (GTK_EDITABLE (entry), FALSE);
    gtk_entry_set_alignment (GTK_ENTRY (entry), 1.0f);
    gtk_entry_set_has_frame (GTK_ENTRY (entry), FALSE);
    gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);

    text = ephy_password_record_get_password (record);
    if (text)
      gtk_entry_set_text (GTK_ENTRY (entry), text);

    gtk_container_add (GTK_CONTAINER (sub_row), entry);

    separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_top (separator, 8);
    gtk_widget_set_margin_bottom (separator, 8);
    gtk_container_add (GTK_CONTAINER (sub_row), separator);

    button = gtk_toggle_button_new ();
    image = gtk_image_new_from_icon_name ("dialog-password-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_tooltip_text (button, _("Reveal password"));
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);

    g_object_bind_property (G_OBJECT (button), "active", G_OBJECT (entry), "visibility", G_BINDING_DEFAULT);
    gtk_container_add (GTK_CONTAINER (sub_row), button);

    /* Remove button */
    sub_row = hdy_action_row_new ();
    gtk_container_add (GTK_CONTAINER (row), sub_row);

    button = gtk_button_new_with_label (_("Remove Password"));
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    dzl_gtk_widget_add_style_class (button, "destructive-action");
    g_signal_connect (button, "clicked", G_CALLBACK (forget_clicked), record);
    gtk_container_add (GTK_CONTAINER (sub_row), button);

    g_object_set_data (G_OBJECT (record), "passwords-view", passwords_view);

    gtk_list_box_insert (GTK_LIST_BOX (passwords_view->listbox), row, -1);
  }

  if (g_list_length (records)) {
    ephy_data_view_set_has_data (EPHY_DATA_VIEW (passwords_view), TRUE);
    gtk_widget_show_all (passwords_view->listbox);
  }

  g_assert (!passwords_view->records);
  passwords_view->records = g_list_copy_deep (records, (GCopyFunc)g_object_ref, NULL);
}

static void
populate_model (EphyPasswordsView *passwords_view)
{
  g_assert (EPHY_IS_PASSWORDS_VIEW (passwords_view));
  g_assert (!ephy_data_view_get_has_data (EPHY_DATA_VIEW (passwords_view)));

  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (passwords_view), TRUE);
  /* Ask for all password records. */
  ephy_password_manager_query (passwords_view->manager,
                               NULL, NULL, NULL, NULL, NULL, NULL,
                               populate_model_cb, passwords_view);
}

static GActionGroup *
create_action_group (EphyPasswordsView *passwords_view)
{
  const GActionEntry entries[] = {
    { "forget-all", forget_all },
  };

  GSimpleActionGroup *group;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), passwords_view);

  return G_ACTION_GROUP (group);
}

static void
show_dialog_cb (GtkWidget *widget,
                gpointer   user_data)
{
  EphyPasswordsView *passwords_view = EPHY_PASSWORDS_VIEW (widget);

  populate_model (passwords_view);
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
  const char *search_text = ephy_data_view_get_search_text (EPHY_DATA_VIEW (passwords_view));

  if (search_text == NULL) {
    gtk_widget_show (GTK_WIDGET (row));

    return TRUE;
  }

  origin = ephy_password_record_get_origin (record);
  username = ephy_password_record_get_username (record);

  if (origin != NULL && g_strrstr (origin, search_text) != NULL)
    visible = TRUE;
  else if (username != NULL && g_strrstr (username, search_text) != NULL)
    visible = TRUE;

  if (visible)
    ephy_data_view_set_has_search_results (EPHY_DATA_VIEW (passwords_view), TRUE);

  gtk_widget_set_visible (GTK_WIDGET (row), visible);

  return visible;
}

static void
ephy_passwords_view_init (EphyPasswordsView *passwords_view)
{
  passwords_view->manager = ephy_embed_shell_get_password_manager (EPHY_EMBED_SHELL (ephy_shell_get_default ()));

  gtk_widget_init_template (GTK_WIDGET (passwords_view));

  passwords_view->action_group = create_action_group (passwords_view);
  gtk_widget_insert_action_group (GTK_WIDGET (passwords_view), "passwords", passwords_view->action_group);

  passwords_view->cancellable = g_cancellable_new ();

  g_signal_connect (GTK_WIDGET (passwords_view), "show", G_CALLBACK (show_dialog_cb), NULL);

  gtk_list_box_set_filter_func (GTK_LIST_BOX (passwords_view->listbox), password_filter, passwords_view, NULL);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (passwords_view->listbox), GTK_SELECTION_NONE);
}
