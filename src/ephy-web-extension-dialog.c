/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019-2020 Jan-Michael Brummer <jan.brummer@tabos.org>
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

#include "ephy-file-helpers.h"
#include "ephy-shell.h"
#include "ephy-web-extension.h"
#include "ephy-web-extension-dialog.h"
#include "ephy-web-extension-manager.h"

#include <gtk/gtk.h>

struct _EphyWebExtensionDialog {
  HdyWindow parent_instance;

  EphyWebExtensionManager *web_extension_manager;

  GtkWidget *listbox;
  GtkStack *stack;
};

G_DEFINE_TYPE (EphyWebExtensionDialog, ephy_web_extension_dialog, HDY_TYPE_WINDOW)

static void
clear_listbox (GtkWidget *listbox)
{
  GtkListBoxRow *row;

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (listbox), 0)))
    gtk_container_remove (GTK_CONTAINER (listbox), GTK_WIDGET (row));
}

static void
on_remove_confirmed (GtkDialog       *dialog,
                     GtkResponseType  response,
                     gpointer         user_data)
{
  GtkListBoxRow *row = user_data;
  EphyWebExtensionDialog *self =
    EPHY_WEB_EXTENSION_DIALOG (gtk_widget_get_toplevel (GTK_WIDGET (row)));

  gtk_widget_destroy (GTK_WIDGET (dialog));

  if (response == GTK_RESPONSE_OK) {
    EphyWebExtension *web_extension = g_object_get_data (G_OBJECT (row), "web_extension");

    g_assert (web_extension);
    ephy_web_extension_manager_uninstall (self->web_extension_manager, web_extension);
  }
}

static void
on_remove_button_clicked (GtkButton *button,
                          gpointer   user_data)
{
  EphyWebExtensionDialog *self = EPHY_WEB_EXTENSION_DIALOG (user_data);
  GtkWidget *dialog = NULL;
  GtkListBoxRow *row;
  GtkWidget *widget;

  row = g_object_get_data (G_OBJECT (button), "row");
  if (!row)
    return;

  dialog = gtk_message_dialog_new (GTK_WINDOW (self),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("Do you really want to remove this extension?"));
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("_Cancel"),
                          GTK_RESPONSE_CANCEL,
                          _("_Remove"),
                          GTK_RESPONSE_OK,
                          NULL);

  widget = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_style_context_add_class (gtk_widget_get_style_context (widget), "destructive-action");

  g_signal_connect (dialog, "response", G_CALLBACK (on_remove_confirmed), row);
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
toggle_state_set_cb (GtkSwitch *widget,
                     gboolean   state,
                     gpointer   user_data)
{
  EphyWebExtensionManager *manager = ephy_shell_get_web_extension_manager (ephy_shell_get_default ());
  EphyWebExtension *web_extension = EPHY_WEB_EXTENSION (user_data);

  ephy_web_extension_manager_set_active (manager, web_extension, state);
}

static void
homepage_activated_cb (HdyActionRow *row,
                       gpointer      user_data)
{
  EphyWebExtensionDialog *self = EPHY_WEB_EXTENSION_DIALOG (user_data);
  EphyWebExtension *web_extension = g_object_get_data (G_OBJECT (row), "web_extension");
  g_autoptr (GError) error = NULL;

  gtk_show_uri_on_window (GTK_WINDOW (self),
                          ephy_web_extension_get_homepage_url (web_extension),
                          GDK_CURRENT_TIME,
                          &error);

  if (error)
    g_warning ("Couldn't to open homepage: %s", error->message);
}

static GtkWidget *
create_row (EphyWebExtensionDialog *self,
            EphyWebExtension       *web_extension)
{
  GtkWidget *row;
  GtkWidget *sub_row;
  GtkWidget *image;
  GtkWidget *toggle;
  GtkWidget *button;
  GtkWidget *homepage_icon;
  GtkWidget *author;
  GtkWidget *version;
  g_autoptr (GdkPixbuf) icon = NULL;
  EphyWebExtensionManager *manager = ephy_shell_get_web_extension_manager (ephy_shell_get_default ());

  row = hdy_expander_row_new ();
  g_object_set_data (G_OBJECT (row), "web_extension", web_extension);

  /* Tooltip */
  gtk_widget_set_tooltip_text (GTK_WIDGET (row), ephy_web_extension_get_name (web_extension));

  /* Icon */
  icon = ephy_web_extension_get_icon (web_extension, 32);
  image = icon ? gtk_image_new_from_pixbuf (icon) : gtk_image_new_from_icon_name ("application-x-addon-symbolic", GTK_ICON_SIZE_DND);
  gtk_image_set_pixel_size (GTK_IMAGE (image), 32);
  hdy_expander_row_add_prefix (HDY_EXPANDER_ROW (row), image);

  /* Titles */
  hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (row), ephy_web_extension_get_name (web_extension));
  hdy_expander_row_set_subtitle (HDY_EXPANDER_ROW (row), ephy_web_extension_get_description (web_extension));
  hdy_expander_row_set_show_enable_switch (HDY_EXPANDER_ROW (row), FALSE);

  toggle = gtk_switch_new ();
  gtk_switch_set_active (GTK_SWITCH (toggle), ephy_web_extension_manager_is_active (manager, web_extension));
  g_signal_connect (toggle, "state-set", G_CALLBACK (toggle_state_set_cb), web_extension);
  gtk_widget_set_valign (toggle, GTK_ALIGN_CENTER);
  hdy_expander_row_add_action (HDY_EXPANDER_ROW (row), toggle);

  /* Author */
  if (ephy_web_extension_get_author (web_extension)) {
    sub_row = hdy_action_row_new ();
    gtk_container_add (GTK_CONTAINER (row), sub_row);
    hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (sub_row), _("Author"));
    author = gtk_label_new (ephy_web_extension_get_author (web_extension));
    gtk_label_set_line_wrap (GTK_LABEL (author), TRUE);
    gtk_container_add (GTK_CONTAINER (sub_row), author);
  }

  /* Version */
  sub_row = hdy_action_row_new ();
  gtk_container_add (GTK_CONTAINER (row), sub_row);
  hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (sub_row), _("Version"));
  version = gtk_label_new (ephy_web_extension_get_version (web_extension));
  dzl_gtk_widget_add_style_class (version, "dim-label");
  gtk_container_add (GTK_CONTAINER (sub_row), version);

  /* Homepage url */
  if (ephy_web_extension_get_homepage_url (web_extension)) {
    sub_row = hdy_action_row_new ();
    gtk_container_add (GTK_CONTAINER (row), sub_row);
    hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (sub_row), _("Homepage"));
    gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (sub_row), TRUE);
    g_signal_connect (sub_row, "activated", G_CALLBACK (homepage_activated_cb), self);
    homepage_icon = gtk_image_new_from_icon_name ("ephy-open-link-symbolic", GTK_ICON_SIZE_BUTTON);
    dzl_gtk_widget_add_style_class (homepage_icon, "dim-label");
    gtk_container_add (GTK_CONTAINER (sub_row), homepage_icon);
    g_object_set_data (G_OBJECT (sub_row), "web_extension", web_extension);
  }

  /* Remove button */
  sub_row = hdy_action_row_new ();
  gtk_container_add (GTK_CONTAINER (row), sub_row);

  button = gtk_button_new_with_mnemonic (_("_Remove"));
  gtk_widget_set_valign (GTK_WIDGET (button), GTK_ALIGN_CENTER);
  dzl_gtk_widget_add_style_class (button, "destructive-action");
  g_signal_connect (button, "clicked", G_CALLBACK (on_remove_button_clicked), self);
  gtk_widget_set_tooltip_text (button, _("Remove selected WebExtension"));
  gtk_container_add (GTK_CONTAINER (sub_row), button);
  g_object_set_data (G_OBJECT (button), "row", row);

  gtk_widget_show_all (GTK_WIDGET (row));

  return GTK_WIDGET (row);
}

static void
ephy_web_extension_dialog_refresh_listbox (EphyWebExtensionDialog *self)
{
  GList *extensions = ephy_web_extension_manager_get_web_extensions (self->web_extension_manager);
  gboolean empty = TRUE;

  clear_listbox (self->listbox);

  for (GList *tmp = extensions; tmp && tmp->data; tmp = tmp->next) {
    EphyWebExtension *web_extension = tmp->data;
    GtkWidget *row;

    row = create_row (self, web_extension);
    gtk_list_box_insert (GTK_LIST_BOX (self->listbox), row, -1);
    empty = FALSE;
  }

  gtk_stack_set_visible_child_name (self->stack, empty ? "empty" : "list");
}

static void
on_add_file_selected (GtkNativeDialog *dialog,
                      GtkResponseType  response,
                      gpointer         user_data)
{
  EphyWebExtensionDialog *self = user_data;

  gtk_native_dialog_destroy (dialog);

  if (response == GTK_RESPONSE_ACCEPT) {
    g_autoptr (GFile) file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

    ephy_web_extension_manager_install (self->web_extension_manager, file);
  }
}

static void
on_add_button_clicked (GtkButton *button,
                       gpointer   user_data)
{
  EphyWebExtensionDialog *self = EPHY_WEB_EXTENSION_DIALOG (user_data);
  GtkFileChooserNative *dialog = NULL;
  GtkFileFilter *filter;

  /* Translators: this is the title of a file chooser dialog. */
  dialog = gtk_file_chooser_native_new (_("Open File (manifest.json/xpi)"),
                                        GTK_WINDOW (self),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("_Open"),
                                        _("_Cancel"));

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (GTK_FILE_FILTER (filter), "WebExtensions");
  gtk_file_filter_add_mime_type (GTK_FILE_FILTER (filter), "application/json");
  gtk_file_filter_add_mime_type (GTK_FILE_FILTER (filter), "application/x-xpinstall");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), g_steal_pointer (&filter));

  g_signal_connect (dialog, "response", G_CALLBACK (on_add_file_selected), self);
  gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));
}

static void
on_web_extension_manager_changed (EphyWebExtensionManager *manager,
                                  gpointer                 user_data)
{
  EphyWebExtensionDialog *self = EPHY_WEB_EXTENSION_DIALOG (user_data);

  ephy_web_extension_dialog_refresh_listbox (self);
}

static void
ephy_web_extension_dialog_dispose (GObject *object)
{
  EphyWebExtensionDialog *self = EPHY_WEB_EXTENSION_DIALOG (object);

  g_clear_weak_pointer (&self->web_extension_manager);

  G_OBJECT_CLASS (ephy_web_extension_dialog_parent_class)->dispose (object);
}

static void
ephy_web_extension_dialog_class_init (EphyWebExtensionDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ephy_web_extension_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/web-extensions-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyWebExtensionDialog, listbox);
  gtk_widget_class_bind_template_child (widget_class, EphyWebExtensionDialog, stack);

  gtk_widget_class_bind_template_callback (widget_class, on_add_button_clicked);
}

static void
ephy_web_extension_dialog_init (EphyWebExtensionDialog *self)
{
  EphyWebExtensionManager *manager;

  gtk_widget_init_template (GTK_WIDGET (self));

  manager = ephy_shell_get_web_extension_manager (ephy_shell_get_default ());
  g_assert (manager != NULL);

  g_set_weak_pointer (&self->web_extension_manager, manager);
  g_signal_connect_object (self->web_extension_manager, "changed", G_CALLBACK (on_web_extension_manager_changed), self, 0);

  ephy_web_extension_dialog_refresh_listbox (self);
}

GtkWidget *
ephy_web_extension_dialog_new (void)
{
  return g_object_new (EPHY_TYPE_WEB_EXTENSION_DIALOG, NULL);
}
