/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019-2020 Jan-Michael Brummer <jan.brummer@tabos.org>
 *  Copyright © 2022 Jamie Murphy
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

#include "prefs-extensions-page.h"

#include "ephy-web-extension.h"
#include "ephy-web-extension-manager.h"

enum {
  EXTENSION_ROW_ACTIVATED,

  LAST_SIGNAL
};

struct _PrefsExtensionsPage {
  AdwPreferencesPage parent_instance;
  EphyWebExtensionManager *web_extension_manager;

  GtkWidget *stack;
  GtkWidget *listbox;
  GCancellable *cancellable;
};

static guint signals[LAST_SIGNAL];

G_DEFINE_FINAL_TYPE (PrefsExtensionsPage, prefs_extensions_page, ADW_TYPE_PREFERENCES_PAGE)

static void
on_extension_row_activated (GtkWidget           *row,
                            PrefsExtensionsPage *page)
{
  EphyWebExtension *web_extension = g_object_get_data (G_OBJECT (row), "web_extension");

  g_signal_emit (page, signals[EXTENSION_ROW_ACTIVATED], 0, web_extension);
}

static void
on_add_file_selected (GtkFileDialog       *dialog,
                      GAsyncResult        *result,
                      PrefsExtensionsPage *self)
{
  g_autoptr (GFile) file = gtk_file_dialog_open_finish (dialog, result, NULL);

  if (file)
    ephy_web_extension_manager_install (self->web_extension_manager, file);
}

static void
on_add_button_clicked (GtkButton *button,
                       gpointer   user_data)
{
  PrefsExtensionsPage *self = EPHY_PREFS_EXTENSIONS_PAGE (user_data);
  GtkFileDialog *dialog;
  g_autoptr (GtkFileFilter) filter = NULL;
  g_autoptr (GListStore) filters = NULL;

  dialog = gtk_file_dialog_new ();
  /* Translators: this is the title of a file chooser dialog. */
  gtk_file_dialog_set_title (dialog, _("Open File (manifest.json/xpi)"));

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "WebExtensions");
  gtk_file_filter_add_mime_type (filter, "application/json");
  gtk_file_filter_add_mime_type (filter, "application/x-xpinstall");

  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                        self->cancellable,
                        (GAsyncReadyCallback)on_add_file_selected,
                        self);
}

static void
toggle_state_set_cb (GtkSwitch *widget,
                     gboolean   state,
                     gpointer   user_data)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  EphyWebExtension *web_extension = EPHY_WEB_EXTENSION (user_data);

  ephy_web_extension_manager_set_active (manager, web_extension, state);
}

static GtkWidget *
create_row (PrefsExtensionsPage *self,
            EphyWebExtension    *web_extension)
{
  GtkWidget *row;
  GtkWidget *image;
  GtkWidget *toggle;
  GtkWidget *arrow;
  g_autoptr (GdkPixbuf) icon = NULL;
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();

  row = adw_action_row_new ();
  g_object_set_data (G_OBJECT (row), "web_extension", web_extension);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), true);
  g_signal_connect (row, "activated", G_CALLBACK (on_extension_row_activated), self);

  /* Tooltip */
  gtk_widget_set_tooltip_text (GTK_WIDGET (row), ephy_web_extension_get_name (web_extension));

  /* Images */
  icon = ephy_web_extension_get_icon (web_extension, 32);
  image = icon ? gtk_image_new_from_pixbuf (icon) : gtk_image_new_from_icon_name ("application-x-addon-symbolic");
  gtk_image_set_pixel_size (GTK_IMAGE (image), 32);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), image);

  /* Titles */
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), ephy_web_extension_get_name (web_extension));
  adw_action_row_set_subtitle (ADW_ACTION_ROW (row), ephy_web_extension_get_description (web_extension));
  adw_action_row_set_subtitle_lines (ADW_ACTION_ROW (row), 1);

  /* Toggle */
  toggle = gtk_switch_new ();
  gtk_switch_set_active (GTK_SWITCH (toggle), ephy_web_extension_manager_is_active (manager, web_extension));
  g_signal_connect (toggle, "state-set", G_CALLBACK (toggle_state_set_cb), web_extension);
  gtk_widget_set_valign (toggle, GTK_ALIGN_CENTER);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), toggle);

  /* Arrow */
  arrow = gtk_image_new_from_icon_name ("go-next-symbolic");
  gtk_widget_set_margin_start (arrow, 6);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), arrow);

  return GTK_WIDGET (row);
}

static void
clear_listbox (GtkWidget *listbox)
{
  GtkListBoxRow *row;

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (listbox), 0)))
    gtk_list_box_remove (GTK_LIST_BOX (listbox), GTK_WIDGET (row));
}

static void
refresh_listbox (PrefsExtensionsPage *self)
{
  GPtrArray *extensions = ephy_web_extension_manager_get_web_extensions (self->web_extension_manager);
  gboolean empty = TRUE;

  clear_listbox (self->listbox);

  for (guint i = 0; i < extensions->len; i++) {
    EphyWebExtension *web_extension = g_ptr_array_index (extensions, i);
    GtkWidget *row;

    row = create_row (self, web_extension);
    gtk_list_box_insert (GTK_LIST_BOX (self->listbox), row, -1);
    empty = FALSE;
  }

  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), empty ? "empty" : "list");
}

static void
on_web_extension_manager_changed (EphyWebExtensionManager *manager,
                                  gpointer                 user_data)
{
  PrefsExtensionsPage *self = EPHY_PREFS_EXTENSIONS_PAGE (user_data);

  refresh_listbox (self);
}

static void
prefs_extensions_page_dispose (GObject *object)
{
  PrefsExtensionsPage *self = EPHY_PREFS_EXTENSIONS_PAGE (object);

  if (self->cancellable) {
    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
  }

  g_clear_weak_pointer (&self->web_extension_manager);

  G_OBJECT_CLASS (prefs_extensions_page_parent_class)->dispose (object);
}

static void
prefs_extensions_page_class_init (PrefsExtensionsPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = prefs_extensions_page_dispose;

  signals[EXTENSION_ROW_ACTIVATED] =
    g_signal_new ("extension-row-activated",
                  EPHY_TYPE_PREFS_EXTENSIONS_PAGE,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  EPHY_TYPE_WEB_EXTENSION);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-extensions-page.ui");

  gtk_widget_class_bind_template_child (widget_class, PrefsExtensionsPage, stack);
  gtk_widget_class_bind_template_child (widget_class, PrefsExtensionsPage, listbox);

  gtk_widget_class_bind_template_callback (widget_class, on_add_button_clicked);
}

static void
prefs_extensions_page_init (PrefsExtensionsPage *self)
{
  EphyWebExtensionManager *manager;

  gtk_widget_init_template (GTK_WIDGET (self));

  manager = ephy_web_extension_manager_get_default ();
  g_set_weak_pointer (&self->web_extension_manager, manager);
  g_signal_connect_object (self->web_extension_manager, "changed", G_CALLBACK (on_web_extension_manager_changed), self, 0);

  self->cancellable = g_cancellable_new ();

  refresh_listbox (self);
}
