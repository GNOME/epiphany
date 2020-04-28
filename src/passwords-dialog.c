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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#define HANDY_USE_UNSTABLE_API
#include <handy.h>

#include <dazzle.h>

#define SECRET_API_SUBJECT_TO_CHANGE
#include <libsecret/secret.h>

#include "ephy-gui.h"
#include "ephy-uri-helpers.h"
#include "passwords-dialog.h"

struct _EphyPasswordsDialog {
  EphyDataDialog parent_instance;

  EphyPasswordManager *manager;
  GList *records;
  GtkWidget *listbox;
  GtkWidget *confirmation_dialog;

  GActionGroup *action_group;
};

G_DEFINE_TYPE (EphyPasswordsDialog, ephy_passwords_dialog, EPHY_TYPE_DATA_DIALOG)

enum {
  PROP_0,
  PROP_PASSWORD_MANAGER,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void populate_model (EphyPasswordsDialog *dialog);

static void
ephy_passwords_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (object);

  switch (prop_id) {
    case PROP_PASSWORD_MANAGER:
      if (dialog->manager)
        g_object_unref (dialog->manager);
      dialog->manager = g_object_ref (g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_passwords_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (object);

  switch (prop_id) {
    case PROP_PASSWORD_MANAGER:
      g_value_set_object (value, dialog->manager);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_passwords_dialog_dispose (GObject *object)
{
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (object);

  g_clear_object (&dialog->manager);

  g_list_free_full (dialog->records, g_object_unref);
  dialog->records = NULL;

  G_OBJECT_CLASS (ephy_passwords_dialog_parent_class)->dispose (object);
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
on_search_text_changed (EphyPasswordsDialog *dialog)
{
  gtk_list_box_invalidate_filter (GTK_LIST_BOX (dialog->listbox));
}


static void
forget_clicked (GtkWidget *button,
                gpointer   user_data)
{
  EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (user_data);
  EphyPasswordsDialog *dialog = g_object_get_data (G_OBJECT (record), "dialog");

  ephy_password_manager_forget (dialog->manager, ephy_password_record_get_id (record));
  clear_listbox (dialog->listbox);

  g_list_free_full (dialog->records, g_object_unref);
  dialog->records = NULL;

  ephy_data_dialog_set_has_data (EPHY_DATA_DIALOG (dialog), FALSE);

  populate_model (dialog);
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
ephy_passwords_dialog_class_init (EphyPasswordsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ephy_passwords_dialog_set_property;
  object_class->get_property = ephy_passwords_dialog_get_property;
  object_class->dispose = ephy_passwords_dialog_dispose;

  obj_properties[PROP_PASSWORD_MANAGER] =
    g_param_spec_object ("password-manager",
                         "Password manager",
                         "Password Manager",
                         EPHY_TYPE_PASSWORD_MANAGER,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/passwords-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, listbox);
  gtk_widget_class_bind_template_callback (widget_class, on_search_text_changed);
}

static void
confirmation_dialog_response_cb (GtkWidget           *dialog,
                                 int                  response,
                                 EphyPasswordsDialog *self)
{
  gtk_widget_destroy (dialog);

  if (response == GTK_RESPONSE_ACCEPT) {
    ephy_password_manager_forget_all (self->manager);

    clear_listbox (self->listbox);
    ephy_data_dialog_set_has_data (EPHY_DATA_DIALOG (self), FALSE);

    g_list_free_full (self->records, g_object_unref);
    self->records = NULL;
  }
}

static GtkWidget *
confirmation_dialog_construct (EphyPasswordsDialog *self)
{
  GtkWidget *dialog, *button;

  dialog = gtk_message_dialog_new (GTK_WINDOW (self),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_CANCEL,
                                   _("Delete All Passwords?"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("This will clear all locally stored passwords, and can not be undone."));

  gtk_window_group_add_window (ephy_gui_ensure_window_group (GTK_WINDOW (self)),
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
  EphyPasswordsDialog *self = EPHY_PASSWORDS_DIALOG (user_data);

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
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (user_data);

  ephy_data_dialog_set_is_loading (EPHY_DATA_DIALOG (dialog), FALSE);

  for (GList *l = records; l && l->data; l = l->next) {
    EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (l->data);
    GtkWidget *row;
    GtkWidget *sub_row;
    GtkWidget *separator;
    GtkWidget *button;
    GtkWidget *image;
    GtkWidget *entry;
    const char *text;

    row = GTK_WIDGET (hdy_expander_row_new ());
    g_object_set_data (G_OBJECT (row), "record", record);
    hdy_action_row_set_title (HDY_ACTION_ROW (row), ephy_password_record_get_origin (record));
    hdy_action_row_set_subtitle (HDY_ACTION_ROW (row), ephy_password_record_get_username (record));
    hdy_expander_row_set_show_enable_switch (HDY_EXPANDER_ROW (row), FALSE);

    separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_top (separator, 8);
    gtk_widget_set_margin_bottom (separator, 8);
    hdy_action_row_add_action (HDY_ACTION_ROW (row), separator);

    button = gtk_button_new_from_icon_name ("edit-copy-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    hdy_action_row_add_action (HDY_ACTION_ROW (row), button);
    g_signal_connect (button, "clicked", G_CALLBACK (copy_password_clicked), (void *)(ephy_password_record_get_password (record)));

    separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_top (separator, 8);
    gtk_widget_set_margin_bottom (separator, 8);
    gtk_container_add (GTK_CONTAINER (row), separator);

    /* Username */
    sub_row = GTK_WIDGET (hdy_action_row_new ());
    hdy_action_row_set_title (HDY_ACTION_ROW (sub_row), _("Username"));
    gtk_container_add (GTK_CONTAINER (row), sub_row);

    button = gtk_button_new_from_icon_name ("edit-copy-symbolic", GTK_ICON_SIZE_BUTTON);
    g_signal_connect (button, "clicked", G_CALLBACK (copy_username_clicked), (void *)(ephy_password_record_get_username (record)));
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    hdy_action_row_add_action (HDY_ACTION_ROW (sub_row), button);

    separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_top (separator, 8);
    gtk_widget_set_margin_bottom (separator, 8);
    hdy_action_row_add_action (HDY_ACTION_ROW (sub_row), separator);

    entry = gtk_entry_new ();
    gtk_widget_set_hexpand (entry, TRUE);
    gtk_widget_set_valign (entry, GTK_ALIGN_CENTER);
    gtk_editable_set_editable (GTK_EDITABLE (entry), FALSE);
    gtk_entry_set_alignment (GTK_ENTRY (entry), 1.0f);
    gtk_entry_set_has_frame (GTK_ENTRY (entry), FALSE);

    text = ephy_password_record_get_username (record);
    if (text)
      gtk_entry_set_text (GTK_ENTRY (entry), text);

    hdy_action_row_add_action (HDY_ACTION_ROW (sub_row), entry);

    /* Password */
    sub_row = GTK_WIDGET (hdy_action_row_new ());
    hdy_action_row_set_title (HDY_ACTION_ROW (sub_row), _("Password"));
    gtk_container_add (GTK_CONTAINER (row), sub_row);

    button = gtk_toggle_button_new ();
    image = gtk_image_new_from_icon_name ("dialog-password-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (button), image);
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    hdy_action_row_add_action (HDY_ACTION_ROW (sub_row), button);

    separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_top (separator, 8);
    gtk_widget_set_margin_bottom (separator, 8);
    hdy_action_row_add_action (HDY_ACTION_ROW (sub_row), separator);

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

    g_object_bind_property (G_OBJECT (button), "active", G_OBJECT (entry), "visibility", G_BINDING_DEFAULT);
    hdy_action_row_add_action (HDY_ACTION_ROW (sub_row), entry);

    /* Remove button */
    button = gtk_button_new_with_label (_("Remove Password"));
    gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top (button, 18);
    gtk_widget_set_margin_bottom (button, 18);
    dzl_gtk_widget_add_style_class (button, GTK_STYLE_CLASS_DESTRUCTIVE_ACTION);
    g_signal_connect (button, "clicked", G_CALLBACK (forget_clicked), record);
    gtk_container_add (GTK_CONTAINER (row), button);

    g_object_set_data (G_OBJECT (record), "dialog", dialog);

    gtk_list_box_insert (GTK_LIST_BOX (dialog->listbox), row, -1);
  }

  if (g_list_length (records)) {
    ephy_data_dialog_set_has_data (EPHY_DATA_DIALOG (dialog), TRUE);
    gtk_widget_show_all (dialog->listbox);
  }

  g_assert (!dialog->records);
  dialog->records = g_list_copy_deep (records, (GCopyFunc)g_object_ref, NULL);
}

static void
populate_model (EphyPasswordsDialog *dialog)
{
  g_assert (EPHY_IS_PASSWORDS_DIALOG (dialog));
  g_assert (!ephy_data_dialog_get_has_data (EPHY_DATA_DIALOG (dialog)));

  ephy_data_dialog_set_is_loading (EPHY_DATA_DIALOG (dialog), TRUE);
  /* Ask for all password records. */
  ephy_password_manager_query (dialog->manager,
                               NULL, NULL, NULL, NULL, NULL, NULL,
                               populate_model_cb, dialog);
}

static GActionGroup *
create_action_group (EphyPasswordsDialog *dialog)
{
  const GActionEntry entries[] = {
    { "forget-all", forget_all },
  };

  GSimpleActionGroup *group;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), dialog);

  return G_ACTION_GROUP (group);
}

static void
show_dialog_cb (GtkWidget *widget,
                gpointer   user_data)
{
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (widget);

  populate_model (dialog);
}

static gboolean
password_filter (GtkListBoxRow *row,
                 gpointer       user_data)
{
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (user_data);
  HdyActionRow *action_row = HDY_ACTION_ROW (row);
  EphyPasswordRecord *record = g_object_get_data (G_OBJECT (action_row), "record");
  const char *username;
  const char *origin;
  gboolean visible = FALSE;
  const char *search_text = ephy_data_dialog_get_search_text (EPHY_DATA_DIALOG (dialog));

  if (search_text == NULL)
    return TRUE;

  origin = ephy_password_record_get_origin (record);
  username = ephy_password_record_get_username (record);

  if (origin != NULL && g_strrstr (origin, search_text) != NULL)
    visible = TRUE;
  else if (username != NULL && g_strrstr (username, search_text) != NULL)
    visible = TRUE;

  return visible;
}

static void
ephy_passwords_dialog_init (EphyPasswordsDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));

  dialog->action_group = create_action_group (dialog);
  gtk_widget_insert_action_group (GTK_WIDGET (dialog), "passwords", dialog->action_group);

  g_signal_connect (GTK_WIDGET (dialog), "show", G_CALLBACK (show_dialog_cb), NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->listbox), hdy_list_box_separator_header, NULL, NULL);
  gtk_list_box_set_filter_func (GTK_LIST_BOX (dialog->listbox), password_filter, dialog, NULL);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (dialog->listbox), GTK_SELECTION_NONE);
}

EphyPasswordsDialog *
ephy_passwords_dialog_new (EphyPasswordManager *manager)
{
  return EPHY_PASSWORDS_DIALOG (g_object_new (EPHY_TYPE_PASSWORDS_DIALOG,
                                              "password-manager", manager,
                                              NULL));
}
