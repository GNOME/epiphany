/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2009 Collabora Ltd.
 *  Copyright © 2016 Iulian-Gabriel Radu
 *  Copyright © 2011, 2017 Igalia S.L.
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

#include "window-commands.h"

#include "ephy-bookmarks-export.h"
#include "ephy-bookmarks-import.h"
#include "ephy-bookmarks-manager.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-embed.h"
#include "ephy-encoding-dialog.h"
#include "ephy-favicon-helpers.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-find-toolbar.h"
#include "ephy-flatpak-utils.h"
#include "ephy-gui.h"
#include "ephy-header-bar.h"
#include "ephy-history-dialog.h"
#include "ephy-link.h"
#include "ephy-location-entry.h"
#include "ephy-password-import.h"
#include "ephy-prefs.h"
#include "ephy-session.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-tab-view.h"
#include "ephy-view-source-handler.h"
#include "ephy-web-app-utils.h"
#include "ephy-web-extension-dialog.h"
#include "ephy-zoom.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit2/webkit2.h>
#include <libportal-gtk3/portal-gtk3.h>

#define DEFAULT_ICON_SIZE 192
#define FAVICON_SIZE 16

void
window_cmd_new_window (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  EphyWindow *new_window;
  EphyShell *shell = ephy_shell_get_default ();

  if (ephy_embed_shell_get_mode (EPHY_EMBED_SHELL (shell)) == EPHY_EMBED_SHELL_MODE_INCOGNITO) {
    ephy_open_default_instance_window ();
    return;
  }

  new_window = ephy_window_new ();
  ephy_link_open (EPHY_LINK (new_window), NULL, NULL, EPHY_LINK_HOME_PAGE);
}

void
window_cmd_new_incognito_window (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
  ephy_open_incognito_window (NULL);
}

typedef enum {
  IMPORT_TYPE_CHOOSE,
  IMPORT_TYPE_IMPORT
} ImportTypes;


struct import_option {
  const char *name;
  ImportTypes type;
  gboolean (*exists)(void);
};

static gboolean firefox_exists (void);
static gboolean chrome_exists (void);
static gboolean chromium_exists (void);

static struct import_option import_options[] = {
  { N_("GVDB File"), IMPORT_TYPE_CHOOSE, NULL },
  { N_("HTML File"), IMPORT_TYPE_CHOOSE, NULL },
  { N_("Firefox"), IMPORT_TYPE_IMPORT, firefox_exists },
  { N_("Chrome"), IMPORT_TYPE_IMPORT, chrome_exists },
  { N_("Chromium"), IMPORT_TYPE_IMPORT, chromium_exists }
};

static void
combo_box_changed_cb (GtkComboBox *combo_box,
                      GtkButton   *button)
{
  int active;

  g_assert (GTK_IS_COMBO_BOX (combo_box));
  g_assert (GTK_IS_BUTTON (button));

  active = gtk_combo_box_get_active (combo_box);
  if (import_options[active].type == IMPORT_TYPE_CHOOSE)
    gtk_button_set_label (button, _("Ch_oose File"));
  else if (import_options[active].type == IMPORT_TYPE_IMPORT)
    gtk_button_set_label (button, _("I_mport"));
}

static GSList *
get_firefox_profiles (void)
{
  GKeyFile *keyfile;
  GSList *profiles = NULL;
  g_autofree gchar *filename = NULL;
  g_autoptr (GError) error = NULL;
  g_auto (GStrv) groups = NULL;
  int i = 0;

  filename = g_build_filename (g_get_home_dir (),
                               FIREFOX_PROFILES_DIR,
                               FIREFOX_PROFILES_FILE,
                               NULL);
  keyfile = g_key_file_new ();
  g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, &error);
  if (error) {
    g_warning ("Failed to load %s: %s", filename, error->message);

    return NULL;
  }

  groups = g_key_file_get_groups (keyfile, NULL);
  while (groups[i]) {
    const char *group = groups[i++];
    char *path;

    if (!g_str_has_prefix (group, "Profile"))
      continue;

    path = g_key_file_get_string (keyfile, group, "Path", &error);
    if (error) {
      g_warning ("Failed to parse profile %s in %s: %s",
                 groups[i], filename, error->message);

      continue;
    }

    profiles = g_slist_append (profiles, path);
  }

  return profiles;
}

static gboolean
firefox_exists (void)
{
  GSList *firefox_profiles;
  gboolean has_firefox_profile;

  firefox_profiles = get_firefox_profiles ();
  has_firefox_profile = g_slist_length (firefox_profiles) > 0;
  g_slist_free_full (firefox_profiles, g_free);

  return has_firefox_profile;
}

static gboolean
chrome_exists (void)
{
  g_autofree char *filename = NULL;

  filename = g_build_filename (g_get_user_config_dir (), "google-chrome", "Default", "Bookmarks", NULL);

  return g_file_test (filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR);
}

static gboolean
chromium_exists (void)
{
  g_autofree char *filename = NULL;

  filename = g_build_filename (g_get_user_config_dir (), "chromium", "Default", "Bookmarks", NULL);

  return g_file_test (filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR);
}

static GtkTreeModel *
create_tree_model (void)
{
  enum {
    TEXT_COL
  };
  GtkListStore *list_store;
  GtkTreeIter iter;
  int i;

  list_store = gtk_list_store_new (1, G_TYPE_STRING);
  for (i = G_N_ELEMENTS (import_options) - 1; i >= 0; i--) {
    if (import_options[i].exists && !import_options[i].exists ())
      continue;

    gtk_list_store_prepend (list_store, &iter);
    gtk_list_store_set (list_store, &iter,
                        TEXT_COL, _(import_options[i].name),
                        -1);
  }

  return GTK_TREE_MODEL (list_store);
}

static void
show_import_export_result (GtkWindow  *parent,
                           gboolean    destroy_parent,
                           gboolean    success,
                           GError     *error,
                           const char *message)
{
  GtkWidget *info_dialog;

  info_dialog = gtk_message_dialog_new (parent,
                                        GTK_DIALOG_MODAL,
                                        success ? GTK_MESSAGE_INFO : GTK_MESSAGE_WARNING,
                                        GTK_BUTTONS_OK,
                                        "%s",
                                        success ? message : error->message);

  if (destroy_parent)
    g_signal_connect_swapped (info_dialog, "response",
                              G_CALLBACK (gtk_widget_destroy), parent);

  g_signal_connect (info_dialog, "response",
                    G_CALLBACK (gtk_widget_destroy), NULL);

  gtk_window_present (GTK_WINDOW (info_dialog));
}

static void
show_firefox_profile_selector_cb (GtkDialog       *selector,
                                  GtkResponseType  response,
                                  GtkWindow       *parent)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  const char *selected_profile = NULL;

  if (response == GTK_RESPONSE_OK) {
    GtkListBox *list_box;
    GtkListBoxRow *row;
    GtkWidget *row_widget;

    list_box = g_object_get_data (G_OBJECT (selector), "list_box");
    row = gtk_list_box_get_selected_row (GTK_LIST_BOX (list_box));
    row_widget = gtk_bin_get_child (GTK_BIN (row));
    selected_profile = g_object_steal_data (G_OBJECT (row_widget), "profile_path");
  }

  gtk_widget_destroy (GTK_WIDGET (selector));

  /* If there are multiple profiles, but the user didn't select one in
   * the profile (he pressed Cancel), don't display the import info dialog
   * as no import took place
   */
  if (selected_profile) {
    g_autoptr (GError) error = NULL;
    gboolean imported = ephy_bookmarks_import_from_firefox (manager, selected_profile, &error);

    show_import_export_result (parent, imported, imported, error,
                               _("Bookmarks successfully imported!"));
  }
}

static void
show_firefox_profile_selector (GtkWindow *parent,
                               GSList    *profiles)
{
  GtkWidget *selector;
  GtkWidget *list_box;
  GtkWidget *content_area;
  GSList *l;

  selector = gtk_dialog_new_with_buttons (_("Select Profile"),
                                          parent,
                                          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
                                          _("_Cancel"),
                                          GTK_RESPONSE_CANCEL,
                                          _("_Select"),
                                          GTK_RESPONSE_OK,
                                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (selector), GTK_RESPONSE_OK);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (selector));
  gtk_widget_set_valign (content_area, GTK_ALIGN_CENTER);

  list_box = gtk_list_box_new ();
  gtk_widget_set_margin_top (list_box, 5);
  gtk_widget_set_margin_bottom (list_box, 5);
  gtk_widget_set_margin_start (list_box, 5);
  gtk_widget_set_margin_end (list_box, 5);

  for (l = profiles; l != NULL; l = l->next) {
    const gchar *profile = l->data;
    GtkWidget *label;

    label = gtk_label_new (strchr (profile, '.') + 1);
    g_object_set_data_full (G_OBJECT (label), "profile_path", g_strdup (profile), g_free);
    gtk_widget_set_margin_top (label, 6);
    gtk_widget_set_margin_bottom (label, 6);
    gtk_list_box_insert (GTK_LIST_BOX (list_box), label, -1);
  }
  gtk_container_add (GTK_CONTAINER (content_area), list_box);
  g_object_set_data (G_OBJECT (selector), "list_box", list_box);

  gtk_widget_show_all (content_area);

  g_signal_connect (selector, "response",
                    G_CALLBACK (show_firefox_profile_selector_cb),
                    parent);

  gtk_window_present (GTK_WINDOW (selector));
}

static void
dialog_bookmarks_import_file_chooser_cb (GtkNativeDialog *file_chooser_dialog,
                                         GtkResponseType  response,
                                         GtkWindow       *parent)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;
  g_autofree char *filename = NULL;
  gboolean imported;

  gtk_native_dialog_destroy (file_chooser_dialog);

  if (response != GTK_RESPONSE_ACCEPT)
    return;

  file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (file_chooser_dialog));
  filename = g_file_get_path (file);
  imported = ephy_bookmarks_import (manager, filename, &error);

  show_import_export_result (parent, imported, imported, error,
                             _("Bookmarks successfully imported!"));
}

static void
dialog_bookmarks_import (GtkWindow *parent)
{
  GtkFileChooserNative *file_chooser_dialog;
  GtkFileFilter *filter;

  file_chooser_dialog = gtk_file_chooser_native_new (_("Choose File"),
                                                     parent,
                                                     GTK_FILE_CHOOSER_ACTION_OPEN,
                                                     _("I_mport"),
                                                     _("_Cancel"));

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*.gvdb");
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (file_chooser_dialog), filter);

  g_signal_connect (file_chooser_dialog, "response",
                    G_CALLBACK (dialog_bookmarks_import_file_chooser_cb),
                    parent);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (file_chooser_dialog));
}

static void
dialog_bookmarks_import_from_html_file_chooser_cb (GtkNativeDialog *file_chooser_dialog,
                                                   GtkResponseType  response,
                                                   GtkWindow       *parent)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;
  g_autofree char *filename = NULL;
  gboolean imported;

  gtk_native_dialog_destroy (file_chooser_dialog);

  if (response != GTK_RESPONSE_ACCEPT)
    return;

  file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (file_chooser_dialog));
  filename = g_file_get_path (file);
  imported = ephy_bookmarks_import_from_html (manager, filename, &error);

  show_import_export_result (parent, imported, imported, error,
                             _("Bookmarks successfully imported!"));
}

static void
dialog_bookmarks_import_from_html (GtkWindow *parent)
{
  GtkFileChooserNative *file_chooser_dialog;
  GtkFileFilter *filter;

  file_chooser_dialog = gtk_file_chooser_native_new (_("Choose File"),
                                                     parent,
                                                     GTK_FILE_CHOOSER_ACTION_OPEN,
                                                     _("I_mport"),
                                                     _("_Cancel"));

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*.html");
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (file_chooser_dialog), filter);

  g_signal_connect (file_chooser_dialog, "response",
                    G_CALLBACK (dialog_bookmarks_import_from_html_file_chooser_cb),
                    parent);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (file_chooser_dialog));
}

static void
dialog_bookmarks_import_from_firefox (GtkWindow *parent)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  g_autoptr (GError) error = NULL;
  GSList *profiles;
  int num_profiles;
  gboolean imported = FALSE;

  profiles = get_firefox_profiles ();

  /* Import default profile */
  num_profiles = g_slist_length (profiles);
  if (num_profiles == 1) {
    imported = ephy_bookmarks_import_from_firefox (manager, profiles->data, &error);

    show_import_export_result (parent, imported, imported, error,
                               _("Bookmarks successfully imported!"));
  } else if (num_profiles > 1) {
    show_firefox_profile_selector (parent, profiles);
  } else {
    g_assert_not_reached ();
  }

  g_slist_free_full (profiles, g_free);
}

static void
dialog_bookmarks_import_from_chrome (GtkWindow *parent)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  g_autoptr (GError) error = NULL;
  g_autofree gchar *filename = NULL;
  gboolean imported;

  filename = g_build_filename (g_get_user_config_dir (), "google-chrome", "Default", "Bookmarks", NULL);

  imported = ephy_bookmarks_import_from_chrome (manager, filename, &error);

  show_import_export_result (parent, imported, imported, error,
                             _("Bookmarks successfully imported!"));
}

static void
dialog_bookmarks_import_from_chromium (GtkWindow *parent)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  g_autoptr (GError) error = NULL;
  g_autofree gchar *filename = NULL;
  gboolean imported;

  filename = g_build_filename (g_get_user_config_dir (), "chromium", "Default", "Bookmarks", NULL);

  imported = ephy_bookmarks_import_from_chrome (manager, filename, &error);

  show_import_export_result (parent, imported, imported, error,
                             _("Bookmarks successfully imported!"));
}

static void
dialog_bookmarks_import_cb (GtkWindow       *parent,
                            GtkResponseType  response,
                            GtkComboBox     *combo_box)
{
  int active;

  if (response == GTK_RESPONSE_OK) {
    active = gtk_combo_box_get_active (combo_box);
    switch (active) {
      case 0:
        dialog_bookmarks_import (parent);
        break;
      case 1:
        dialog_bookmarks_import_from_html (parent);
        break;
      case 2:
        dialog_bookmarks_import_from_firefox (parent);
        break;
      case 3:
        dialog_bookmarks_import_from_chrome (parent);
        break;
      case 4:
        dialog_bookmarks_import_from_chromium (parent);
        break;
      default:
        g_assert_not_reached ();
    }
  } else if (response == GTK_RESPONSE_CANCEL) {
    gtk_widget_destroy (GTK_WIDGET (parent));
  }
}

void
window_cmd_import_bookmarks (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  GtkWidget *dialog;
  GtkWidget *content_area;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *combo_box;
  GtkTreeModel *tree_model;
  GtkCellRenderer *cell_renderer;

  dialog = g_object_new (GTK_TYPE_DIALOG,
                         "use-header-bar", TRUE,
                         "modal", TRUE,
                         "transient-for", window,
                         "title", _("Import Bookmarks"),
                         NULL);
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("_Cancel"),
                          GTK_RESPONSE_CANCEL,
                          _("Ch_oose File"),
                          GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_widget_set_valign (content_area, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top (content_area, 5);
  gtk_widget_set_margin_bottom (content_area, 5);
  gtk_widget_set_margin_start (content_area, 30);
  gtk_widget_set_margin_end (content_area, 30);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);

  label = gtk_label_new (_("From:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

  tree_model = create_tree_model ();
  combo_box = gtk_combo_box_new_with_model (GTK_TREE_MODEL (tree_model));
  gtk_widget_set_hexpand (combo_box, TRUE);
  g_object_unref (tree_model);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);

  g_signal_connect (GTK_COMBO_BOX (combo_box), "changed",
                    G_CALLBACK (combo_box_changed_cb),
                    gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK));

  cell_renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), cell_renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), cell_renderer,
                                  "text", 0, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), combo_box, FALSE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (content_area), hbox);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (dialog_bookmarks_import_cb),
                    GTK_COMBO_BOX (combo_box));

  gtk_widget_show_all (dialog);
}

static void
bookmarks_export_cb (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  EphyBookmarksManager *manager = EPHY_BOOKMARKS_MANAGER (source_object);
  GtkWindow *window = GTK_WINDOW (user_data);
  gboolean exported;
  g_autoptr (GError) error = NULL;

  exported = ephy_bookmarks_export_finish (manager, result, &error);

  show_import_export_result (window, FALSE, exported, error,
                             _("Bookmarks successfully exported!"));

  g_object_unref (manager);
  g_object_unref (window);
}

static void
export_bookmarks_file_chooser_cb (GtkNativeDialog *dialog,
                                  GtkResponseType  response,
                                  GtkWindow       *parent)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  g_autoptr (GFile) file = NULL;
  g_autofree char *filename = NULL;

  gtk_native_dialog_destroy (dialog);

  if (response != GTK_RESPONSE_ACCEPT)
    return;

  file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
  filename = g_file_get_path (file);
  ephy_bookmarks_export (g_object_ref (manager),
                         filename,
                         NULL,
                         bookmarks_export_cb,
                         parent);
}

void
window_cmd_export_bookmarks (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  GtkFileChooser *dialog;
  GtkFileFilter *filter;
  GtkWindow *window = user_data;

  dialog = GTK_FILE_CHOOSER (gtk_file_chooser_native_new (_("Choose File"),
                                                          window,
                                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                                          _("_Save"),
                                                          _("_Cancel")));
  gtk_file_chooser_set_show_hidden (dialog, TRUE);

  /* Translators: Only translate the part before ".html" (e.g. "bookmarks") */
  gtk_file_chooser_set_current_name (dialog, _("bookmarks.html"));

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*.html");
  gtk_file_filter_add_pattern (filter, "*.gvdb");
  gtk_file_chooser_set_filter (dialog, filter);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (export_bookmarks_file_chooser_cb),
                    g_object_ref (window));

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));
}

static gboolean
chrome_passwords_exists (void)
{
  g_autofree char *filename = NULL;

  filename = g_build_filename (g_get_user_config_dir (), "google-chrome", "Default", "Login Data", NULL);

  return g_file_test (filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR);
}

static gboolean
chromium_passwords_exists (void)
{
  g_autofree char *filename = NULL;

  filename = g_build_filename (g_get_user_config_dir (), "chromium", "Default", "Login Data", NULL);

  return g_file_test (filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR);
}

static struct import_option import_passwords_options[] = {
  { N_("Chrome"), IMPORT_TYPE_IMPORT, chrome_passwords_exists },
  { N_("Chromium"), IMPORT_TYPE_IMPORT, chromium_passwords_exists }
};

static GtkTreeModel *
create_import_passwords_tree_model (void)
{
  enum {
    TEXT_COL
  };
  GtkListStore *list_store;
  GtkTreeIter iter;
  int i;

  list_store = gtk_list_store_new (1, G_TYPE_STRING);
  for (i = G_N_ELEMENTS (import_passwords_options) - 1; i >= 0; i--) {
    if (import_passwords_options[i].exists && !import_passwords_options[i].exists ())
      continue;

    gtk_list_store_prepend (list_store, &iter);
    gtk_list_store_set (list_store, &iter,
                        TEXT_COL, _(import_passwords_options[i].name),
                        -1);
  }

  return GTK_TREE_MODEL (list_store);
}

static void
dialog_password_import_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  GtkWindow *parent = user_data;
  g_autoptr (GError) error = NULL;
  gboolean imported = ephy_password_import_from_chrome_finish (source_object, res, &error);

  show_import_export_result (parent, imported, imported, error,
                             _("Passwords successfully imported!"));
}

static void
dialog_passwords_import_cb (GtkDialog   *dialog,
                            int          response,
                            GtkComboBox *combo_box)
{
  if (response == GTK_RESPONSE_OK) {
    EphyPasswordManager *manager;
    int active;

    manager = ephy_embed_shell_get_password_manager (EPHY_EMBED_SHELL (ephy_shell_get_default ()));
    active = gtk_combo_box_get_active (combo_box);

    switch (active) {
      case 0:
        ephy_password_import_from_chrome_async (manager, CHROME, dialog_password_import_cb, dialog);
        break;
      case 1:
        ephy_password_import_from_chrome_async (manager, CHROMIUM, dialog_password_import_cb, dialog);
        break;
      default:
        g_assert_not_reached ();
    }
  } else {
    gtk_widget_destroy (GTK_WIDGET (dialog));
  }
}

static void
passwords_combo_box_changed_cb (GtkComboBox *combo_box,
                                GtkButton   *button)
{
  int active;

  g_assert (GTK_IS_COMBO_BOX (combo_box));
  g_assert (GTK_IS_BUTTON (button));

  active = gtk_combo_box_get_active (combo_box);
  if (import_passwords_options[active].type == IMPORT_TYPE_CHOOSE)
    gtk_button_set_label (button, _("Ch_oose File"));
  else if (import_passwords_options[active].type == IMPORT_TYPE_IMPORT)
    gtk_button_set_label (button, _("I_mport"));
}

void
window_cmd_import_passwords (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  GtkWidget *dialog;
  GtkWidget *content_area;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *combo_box;
  GtkTreeModel *tree_model;
  GtkCellRenderer *cell_renderer;

  dialog = g_object_new (GTK_TYPE_DIALOG,
                         "use-header-bar", TRUE,
                         "modal", TRUE,
                         "transient-for", window,
                         "title", _("Import Passwords"),
                         NULL);
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("_Cancel"),
                          GTK_RESPONSE_CANCEL,
                          _("I_mport"),
                          GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_widget_set_valign (content_area, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top (content_area, 5);
  gtk_widget_set_margin_bottom (content_area, 5);
  gtk_widget_set_margin_start (content_area, 30);
  gtk_widget_set_margin_end (content_area, 30);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);

  label = gtk_label_new (_("From:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

  tree_model = create_import_passwords_tree_model ();

  if (gtk_tree_model_iter_n_children (tree_model, NULL))
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, TRUE);
  else
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);

  combo_box = gtk_combo_box_new_with_model (GTK_TREE_MODEL (tree_model));
  gtk_widget_set_hexpand (combo_box, TRUE);
  g_object_unref (tree_model);

  g_signal_connect (GTK_COMBO_BOX (combo_box), "changed",
                    G_CALLBACK (passwords_combo_box_changed_cb),
                    gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK));

  gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);

  cell_renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), cell_renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), cell_renderer,
                                  "text", 0, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), combo_box, FALSE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (content_area), hbox);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (dialog_passwords_import_cb),
                    GTK_COMBO_BOX (combo_box));

  gtk_widget_show_all (dialog);
}


void
window_cmd_show_history (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  GtkWidget *dialog;

  dialog = ephy_shell_get_history_dialog (ephy_shell_get_default ());

  if (GTK_WINDOW (user_data) != gtk_window_get_transient_for (GTK_WINDOW (dialog)))
    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  GTK_WINDOW (user_data));
  gtk_window_present (GTK_WINDOW (dialog));
}

void
window_cmd_show_firefox_sync (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  GtkWidget *dialog;

  dialog = ephy_shell_get_firefox_sync_dialog (ephy_shell_get_default ());

  if (GTK_WINDOW (user_data) != gtk_window_get_transient_for (GTK_WINDOW (dialog)))
    gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                  GTK_WINDOW (user_data));
  gtk_window_present (GTK_WINDOW (dialog));
}

void
window_cmd_show_preferences (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  GtkWindow *dialog;

  dialog = GTK_WINDOW (ephy_shell_get_prefs_dialog (ephy_shell_get_default ()));

  if (GTK_WINDOW (user_data) != gtk_window_get_transient_for (dialog))
    gtk_window_set_transient_for (dialog,
                                  GTK_WINDOW (user_data));

  gtk_window_present (dialog);
}

void
window_cmd_show_shortcuts (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  static GtkWidget *shortcuts_window;

  if (shortcuts_window == NULL) {
    GtkBuilder *builder;

    builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/shortcuts-dialog.ui");
    shortcuts_window = GTK_WIDGET (gtk_builder_get_object (builder, "shortcuts-dialog"));

    if (!ephy_can_install_web_apps ())
      gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "shortcuts-web-apps-group")));

    if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL) {
      GtkShortcutsShortcut *shortcut;

      shortcut = GTK_SHORTCUTS_SHORTCUT (gtk_builder_get_object (builder, "go-back-shortcut"));
      g_object_set (shortcut, "accelerator", "<Alt>Right", NULL);

      shortcut = GTK_SHORTCUTS_SHORTCUT (gtk_builder_get_object (builder, "go-forward-shortcut"));
      g_object_set (shortcut, "accelerator", "<Alt>Left", NULL);

      shortcut = GTK_SHORTCUTS_SHORTCUT (gtk_builder_get_object (builder, "go-back-gesture"));
      g_object_set (shortcut, "shortcut-type", GTK_SHORTCUT_GESTURE_TWO_FINGER_SWIPE_LEFT, NULL);

      shortcut = GTK_SHORTCUTS_SHORTCUT (gtk_builder_get_object (builder, "go-forward-gesture"));
      g_object_set (shortcut, "shortcut-type", GTK_SHORTCUT_GESTURE_TWO_FINGER_SWIPE_RIGHT, NULL);
    }

    g_signal_connect (shortcuts_window,
                      "destroy",
                      G_CALLBACK (gtk_widget_destroyed),
                      &shortcuts_window);

    g_object_unref (builder);
  }

  if (gtk_window_get_transient_for (GTK_WINDOW (shortcuts_window)) != GTK_WINDOW (user_data))
    gtk_window_set_transient_for (GTK_WINDOW (shortcuts_window), GTK_WINDOW (user_data));

  gtk_window_present (GTK_WINDOW (shortcuts_window));
}

void
window_cmd_show_help (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  ephy_gui_help (GTK_WIDGET (user_data), NULL);
}

#define ABOUT_GROUP "About"

void
window_cmd_show_about (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  GtkAboutDialog *dialog;
  char *comments = NULL;
  GKeyFile *key_file;
  GBytes *bytes;
  GError *error = NULL;
  char **orig_authors, **maintainers, **past_maintainers, **contributors, **artists, **documenters;
  char **authors = NULL;
  guint index;
  guint author_index = 0;
  guint length;

  key_file = g_key_file_new ();
  bytes = g_resources_lookup_data ("/org/gnome/epiphany/about.ini", 0, NULL);
  if (!g_key_file_load_from_data (key_file, g_bytes_get_data (bytes, NULL), -1, 0, &error)) {
    g_warning ("Couldn't load about data: %s\n", error->message);
    g_error_free (error);
    g_key_file_free (key_file);
    return;
  }
  g_bytes_unref (bytes);

  orig_authors = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Author", NULL, NULL);
  maintainers = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Maintainers", NULL, NULL);
  past_maintainers = g_key_file_get_string_list (key_file, ABOUT_GROUP, "PastMaintainers", NULL, NULL);
  contributors = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Contributors", NULL, NULL);
  artists = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Artists", NULL, NULL);
  documenters = g_key_file_get_string_list (key_file, ABOUT_GROUP, "Documenters", NULL, NULL);
  g_key_file_free (key_file);

  comments = g_strdup_printf (_("A simple, clean, beautiful view of the web.\n"
                                "Powered by WebKitGTK %d.%d.%d" WEBKIT_REVISION),
                              webkit_get_major_version (),
                              webkit_get_minor_version (),
                              webkit_get_micro_version ());

  dialog = GTK_ABOUT_DIALOG (gtk_about_dialog_new ());

  if (window) {
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  }

  if (g_str_equal (PROFILE, "Canary"))
    gtk_about_dialog_set_program_name (dialog, _("Epiphany Canary"));
  else {
#if !TECH_PREVIEW
    gtk_about_dialog_set_program_name (dialog, _("Web"));
#else
    gtk_about_dialog_set_program_name (dialog, _("Epiphany Technology Preview"));
#endif
  }

  gtk_about_dialog_set_version (dialog, VERSION);
  gtk_about_dialog_set_copyright (dialog,
                                  "Copyright © 2002–2004 Marco Pesenti Gritti\n"
                                  "Copyright © 2003–2021 The GNOME Web Developers");
  gtk_about_dialog_set_comments (dialog, comments);
  gtk_about_dialog_set_license_type (dialog, GTK_LICENSE_GPL_3_0);
  gtk_about_dialog_set_website (dialog, "https://wiki.gnome.org/Apps/Web");
  gtk_about_dialog_set_website_label (dialog, _("Website"));
  gtk_about_dialog_set_logo_icon_name (dialog, APPLICATION_ID);

  length = g_strv_length (orig_authors) +
           g_strv_length (maintainers) +
           g_strv_length (past_maintainers) +
           g_strv_length (contributors) +
           3;

  authors = g_malloc0 (sizeof (char *) * (length + 1));

  for (index = 0; index < g_strv_length (orig_authors); index++)
    authors[author_index++] = g_strdup (orig_authors[index]);

  authors[author_index++] = g_strdup ("");

  for (index = 0; index < g_strv_length (maintainers); index++)
    authors[author_index++] = g_strdup (maintainers[index]);

  authors[author_index++] = g_strdup ("");

  for (index = 0; index < g_strv_length (past_maintainers); index++)
    authors[author_index++] = g_strdup (past_maintainers[index]);

  authors[author_index++] = g_strdup ("");

  for (index = 0; index < g_strv_length (contributors); index++) {
    authors[author_index++] = g_strdup (contributors[index]);
  }

  gtk_about_dialog_set_authors (dialog, (const char **)authors);
  gtk_about_dialog_set_artists (dialog, (const char **)artists);
  gtk_about_dialog_set_documenters (dialog, (const char **)documenters);
  gtk_about_dialog_set_translator_credits (dialog, _("translator-credits"));

  g_signal_connect (dialog, "response",
                    G_CALLBACK (gtk_widget_destroy), NULL);
  gtk_window_present (GTK_WINDOW (dialog));

  g_free (comments);
  g_strfreev (artists);
  g_strfreev (authors);
  g_strfreev (contributors);
  g_strfreev (documenters);
  g_strfreev (maintainers);
  g_strfreev (past_maintainers);
}

void
window_cmd_quit (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  ephy_shell_try_quit (ephy_shell_get_default ());
}

void
window_cmd_reopen_closed_tab (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  EphySession *session = ephy_shell_get_session (ephy_shell_get_default ());

  g_assert (session != NULL);
  ephy_session_undo_close_tab (session);
}

void
window_cmd_navigation (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;
  WebKitWebView *web_view;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (embed != NULL);

  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  if (strstr (g_action_get_name (G_ACTION (action)), "back")) {
    webkit_web_view_go_back (web_view);
    gtk_widget_grab_focus (GTK_WIDGET (embed));
  } else {
    webkit_web_view_go_forward (web_view);
    gtk_widget_grab_focus (GTK_WIDGET (embed));
  }
}

void
window_cmd_navigation_new_tab (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;
  WebKitWebView *web_view;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (embed != NULL);

  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  if (strstr (g_action_get_name (G_ACTION (action)), "back")) {
    const char *back_uri;
    WebKitBackForwardList *history;
    WebKitBackForwardListItem *back_item;

    history = webkit_web_view_get_back_forward_list (web_view);
    back_item = webkit_back_forward_list_get_back_item (history);
    back_uri = webkit_back_forward_list_item_get_original_uri (back_item);

    embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                                NULL,
                                0);

    web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
    webkit_web_view_load_uri (web_view, back_uri);
    gtk_widget_grab_focus (GTK_WIDGET (embed));
  } else {
    const char *forward_uri;
    WebKitBackForwardList *history;
    WebKitBackForwardListItem *forward_item;

    /* Forward history is not copied when opening
     *  a new tab, so get the forward URI manually
     *  and load it */
    history = webkit_web_view_get_back_forward_list (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
    forward_item = webkit_back_forward_list_get_forward_item (history);
    forward_uri = webkit_back_forward_list_item_get_original_uri (forward_item);

    embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                                embed,
                                0);

    web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
    webkit_web_view_load_uri (web_view, forward_uri);
  }
}

void
window_cmd_stop (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;

  embed = ephy_embed_container_get_active_child
            (EPHY_EMBED_CONTAINER (window));
  g_assert (embed != NULL);

  gtk_widget_grab_focus (GTK_WIDGET (embed));

  webkit_web_view_stop_loading (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
}

static void
check_tab_has_modified_forms_confirm_cb (GtkDialog       *dialog,
                                         GtkResponseType  response,
                                         EphyEmbed       *embed)
{
  WebKitWebView *view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  gtk_widget_destroy (GTK_WIDGET (dialog));

  if (response == GTK_RESPONSE_ACCEPT) {
    gtk_widget_grab_focus (GTK_WIDGET (embed));
    webkit_web_view_reload (view);
  }

  g_object_unref (embed);
}

static void
check_tab_has_modified_forms_and_reload_cb (EphyWebView  *view,
                                            GAsyncResult *result,
                                            EphyEmbed    *embed)
{
  EphyWindow *window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view)));
  GtkWidget *dialog;
  GtkWidget *button;
  gboolean has_modified_forms;

  has_modified_forms = ephy_web_view_has_modified_forms_finish (view, result, NULL);
  if (has_modified_forms) {
    dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_WARNING,
                                     GTK_BUTTONS_CANCEL,
                                     "%s", _("Do you want to reload this website?"));

    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", _("A form you modified has not been submitted."));

    button = gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Discard form"), GTK_RESPONSE_ACCEPT);
    gtk_style_context_add_class (gtk_widget_get_style_context (button), "destructive-action");

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

    gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (window)),
                                 GTK_WINDOW (dialog));

    g_signal_connect (dialog, "response",
                      G_CALLBACK (check_tab_has_modified_forms_confirm_cb), embed);

    gtk_window_present (GTK_WINDOW (dialog));

    return;
  }

  gtk_widget_grab_focus (GTK_WIDGET (embed));
  webkit_web_view_reload (WEBKIT_WEB_VIEW (view));

  g_object_unref (embed);
}

void
window_cmd_reload (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;

  embed = EPHY_EMBED (ephy_tab_view_get_current_page (ephy_window_get_tab_view (window)));
  g_assert (embed != NULL);

  ephy_web_view_has_modified_forms (ephy_embed_get_web_view (embed),
                                    NULL,
                                    (GAsyncReadyCallback)check_tab_has_modified_forms_and_reload_cb,
                                    g_object_ref (embed));
}

void
window_cmd_reload_bypass_cache (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;
  WebKitWebView *view;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (embed != NULL);

  gtk_widget_grab_focus (GTK_WIDGET (embed));

  view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
  webkit_web_view_reload_bypass_cache (view);
}

void
window_cmd_combined_stop_reload (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
  GActionGroup *action_group;
  GAction *gaction;
  GVariant *state;

  action_group = gtk_widget_get_action_group (GTK_WIDGET (user_data), "toolbar");

  state = g_action_get_state (G_ACTION (action));
  /* If loading */
  if (g_variant_get_boolean (state))
    gaction = g_action_map_lookup_action (G_ACTION_MAP (action_group), "stop");
  else
    gaction = g_action_map_lookup_action (G_ACTION_MAP (action_group), "reload");

  g_action_activate (gaction, NULL);

  g_variant_unref (state);
}

void
window_cmd_page_menu (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)

{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyHeaderBar *header_bar;
  GtkMenuButton *button;
  GtkPopover *popover;

  header_bar = EPHY_HEADER_BAR (ephy_window_get_header_bar (window));
  button = GTK_MENU_BUTTON (ephy_header_bar_get_page_menu_button (header_bar));
  popover = gtk_menu_button_get_popover (button);
  gtk_popover_popup (popover);
}

void
window_cmd_new_tab (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  EphyWindow *window = user_data;
  char *url;

  url = g_settings_get_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_HOMEPAGE_URL);
  if (g_strcmp0 (url, "about:newtab") != 0) {
    g_free (url);
    url = NULL;
  }

  ephy_link_open (EPHY_LINK (window),
                  url, NULL,
                  EPHY_LINK_NEW_TAB | EPHY_LINK_JUMP_TO);
  ephy_window_close_pages_view (window);
  g_free (url);
}

static void
open_response_cb (GtkNativeDialog *dialog,
                  int              response,
                  EphyWindow      *window)
{
  if (response == GTK_RESPONSE_ACCEPT) {
    g_autoptr (GFile) file = NULL;
    g_autofree char *uri = NULL;
    g_autofree char *converted = NULL;

    file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
    uri = g_file_get_uri (file);
    if (uri != NULL) {
      converted = g_filename_to_utf8 (uri, -1, NULL, NULL, NULL);

      if (converted != NULL) {
        ephy_window_load_url (window, converted);
      }
    }
  }

  g_object_unref (dialog);
}

void
window_cmd_open (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkFileChooser *dialog;

  dialog = ephy_create_file_chooser (_("Open"),
                                     GTK_WIDGET (window),
                                     GTK_FILE_CHOOSER_ACTION_OPEN,
                                     EPHY_FILE_FILTER_ALL_SUPPORTED);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (open_response_cb), window);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));
}

typedef struct {
  EphyWebView *view;
  const char *display_address;
  const char *url;
  char *icon_href;
  char *title;
  char *chosen_name;
  char *app_id;
  char *token;
  GVariant *icon_v;
  GdkRGBA icon_rgba;
  GdkPixbuf *framed_pixbuf;
  GCancellable *cancellable;
  EphyWebApplicationOptions webapp_options;
  gboolean webapp_options_set;
  WebKitDownload *download;
  EphyWindow *window;
} EphyApplicationDialogData;

static void
session_bus_ready_cb (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  g_autoptr (GDBusConnection) connection = g_bus_get_finish (res, NULL);
  g_autofree gchar *desktop_file = user_data;
  g_autofree gchar *app_id = NULL;
  GVariant *app;

  if (!connection)
    return;

  app_id = g_path_get_basename (desktop_file);
  app = g_variant_new_string (app_id);

  g_dbus_connection_call (connection,
                          "org.gnome.Shell",
                          "/org/gnome/Shell",
                          "org.gnome.Shell",
                          "FocusApp",
                          g_variant_new_tuple (&app, 1),
                          NULL,
                          G_DBUS_CALL_FLAGS_NO_AUTO_START,
                          -1,
                          NULL,
                          NULL,
                          NULL);
}

static void
ephy_focus_desktop_app (const char *webapp_id)
{
  g_autofree char *desktop_path = NULL;
  /* Note this desktop_path is wrong under Flatpak, but we only use it for its
   * basename.
   */
  desktop_path = ephy_web_application_get_desktop_path (webapp_id);
  g_bus_get (G_BUS_TYPE_SESSION, NULL, session_bus_ready_cb, g_steal_pointer (&desktop_path));
}

static void download_finished_cb (WebKitDownload            *download,
                                  EphyApplicationDialogData *data);

static void download_failed_cb (WebKitDownload            *download,
                                GError                    *error,
                                EphyApplicationDialogData *data);

static void
ephy_application_dialog_data_free (EphyApplicationDialogData *data)
{
  if (data->download) {
    g_signal_handlers_disconnect_by_func (data->download, download_finished_cb, data);
    g_signal_handlers_disconnect_by_func (data->download, download_failed_cb, data);

    data->download = NULL;
  }

  g_cancellable_cancel (data->cancellable);
  g_object_unref (data->cancellable);
  g_object_unref (data->window);
  if (data->framed_pixbuf)
    g_object_unref (data->framed_pixbuf);
  if (data->icon_v)
    g_variant_unref (data->icon_v);
  g_free (data->icon_href);
  g_free (data->title);
  g_free (data->chosen_name);
  g_free (data->token);
  g_free (data->app_id);
  g_free (data);
}

static void
rounded_rectangle (cairo_t *cr,
                   gdouble  aspect,
                   gdouble  x,
                   gdouble  y,
                   gdouble  corner_radius,
                   gdouble  width,
                   gdouble  height)
{
  gdouble radius;
  gdouble degrees;

  radius = corner_radius / aspect;
  degrees = G_PI / 180.0;

  cairo_new_sub_path (cr);
  cairo_arc (cr,
             x + width - radius,
             y + radius,
             radius,
             -90 * degrees,
             0 * degrees);
  cairo_arc (cr,
             x + width - radius,
             y + height - radius,
             radius,
             0 * degrees,
             90 * degrees);
  cairo_arc (cr,
             x + radius,
             y + height - radius,
             radius,
             90 * degrees,
             180 * degrees);
  cairo_arc (cr,
             x + radius,
             y + radius,
             radius,
             180 * degrees,
             270 * degrees);
  cairo_close_path (cr);
}

static GdkPixbuf *
frame_pixbuf (GdkPixbuf *pixbuf,
              GdkRGBA   *rgba,
              int        width,
              int        height)
{
  GdkPixbuf *framed;
  cairo_surface_t *surface;
  cairo_t *cr;
  int frame_width;
  int radius;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        width, height);
  cr = cairo_create (surface);

  frame_width = 0;
  radius = 20;

  rounded_rectangle (cr,
                     1.0,
                     frame_width + 0.5,
                     frame_width + 0.5,
                     radius,
                     width - frame_width * 2 - 1,
                     height - frame_width * 2 - 1);
  if (rgba != NULL)
    cairo_set_source_rgba (cr,
                           rgba->red,
                           rgba->green,
                           rgba->blue,
                           rgba->alpha);
  else
    cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.3);
  cairo_fill_preserve (cr);

  if (pixbuf != NULL) {
    GdkPixbuf *scaled;
    int w;
    int h;

    w = gdk_pixbuf_get_width (pixbuf);
    h = gdk_pixbuf_get_height (pixbuf);

    if (w < 48 || h < 48) {
      scaled = gdk_pixbuf_scale_simple (pixbuf, w * 3, h * 3, GDK_INTERP_NEAREST);
    } else if (w > width || h > height) {
      double ws, hs, s;

      ws = (double)width / w;
      hs = (double)height / h;
      s = MIN (ws, hs);
      scaled = gdk_pixbuf_scale_simple (pixbuf, w * s, h * s, GDK_INTERP_BILINEAR);
    } else {
      scaled = g_object_ref (pixbuf);
    }

    w = gdk_pixbuf_get_width (scaled);
    h = gdk_pixbuf_get_height (scaled);

    gdk_cairo_set_source_pixbuf (cr, scaled,
                                 (width - w) / 2,
                                 (height - h) / 2);
    g_object_unref (scaled);
    cairo_fill (cr);
  }

  framed = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return framed;
}

static void create_install_dialog_when_ready (EphyApplicationDialogData *data);

static void
set_image_from_favicon (EphyApplicationDialogData *data)
{
  g_autoptr (GdkPixbuf) icon = NULL;
  cairo_surface_t *icon_surface = webkit_web_view_get_favicon (WEBKIT_WEB_VIEW (data->view));

  if (icon_surface)
    icon = ephy_pixbuf_get_from_surface_scaled (icon_surface, 0, 0);

  if (icon != NULL) {
    data->framed_pixbuf = frame_pixbuf (icon, NULL, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE);
    g_assert (data->icon_v == NULL);
    data->icon_v = g_icon_serialize (G_ICON (data->framed_pixbuf));
    create_install_dialog_when_ready (data);
  }
  if (data->icon_v == NULL) {
    g_warning ("Failed to get favicon for web app %s, giving up", data->display_address);
    ephy_application_dialog_data_free (data);
  }
}

static void
set_app_icon_from_filename (EphyApplicationDialogData *data,
                            const char                *filename)
{
  g_autoptr (GdkPixbuf) pixbuf = NULL;

  pixbuf = gdk_pixbuf_new_from_file_at_size (filename, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE, NULL);

  if (pixbuf != NULL) {
    data->framed_pixbuf = frame_pixbuf (pixbuf, &data->icon_rgba, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE);
    g_assert (data->icon_v == NULL);
    data->icon_v = g_icon_serialize (G_ICON (data->framed_pixbuf));
    create_install_dialog_when_ready (data);
  }
  if (data->icon_v == NULL) {
    g_warning ("Failed to get icon for web app %s, giving up", data->display_address);
    ephy_application_dialog_data_free (data);
    return;
  }
}

static void
download_finished_cb (WebKitDownload            *download,
                      EphyApplicationDialogData *data)
{
  g_autofree char *filename = NULL;

  filename = g_filename_from_uri (webkit_download_get_destination (download), NULL, NULL);
  set_app_icon_from_filename (data, filename);
}

static void
download_failed_cb (WebKitDownload            *download,
                    GError                    *error,
                    EphyApplicationDialogData *data)
{
  g_signal_handlers_disconnect_by_func (download, download_finished_cb, data);
  /* Something happened, default to a page snapshot. */
  set_image_from_favicon (data);
}

static void
download_icon_and_set_image (EphyApplicationDialogData *data)
{
  g_autofree char *destination = NULL;
  g_autofree char *destination_uri = NULL;
  g_autofree char *tmp_filename = NULL;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  data->download = webkit_web_context_download_uri (ephy_embed_shell_get_web_context (shell),
                                                    data->icon_href);
  /* We do not want this download to show up in the UI, so let's
   * set 'ephy-download-set' to make Epiphany think this is
   * already there. */
  /* FIXME: it's probably better to just do this in a clean way
   * instead of using this workaround. */
  g_object_set_data (G_OBJECT (data->download), "ephy-download-set", GINT_TO_POINTER (TRUE));

  tmp_filename = ephy_file_tmp_filename (".ephy-download-XXXXXX", NULL);
  destination = g_build_filename (ephy_file_tmp_dir (), tmp_filename, NULL);
  destination_uri = g_filename_to_uri (destination, NULL, NULL);
  webkit_download_set_destination (data->download, destination_uri);

  g_signal_connect (data->download, "finished",
                    G_CALLBACK (download_finished_cb), data);
  g_signal_connect (data->download, "failed",
                    G_CALLBACK (download_failed_cb), data);
}

static void
fill_default_application_image_cb (GObject      *source,
                                   GAsyncResult *async_result,
                                   gpointer      user_data)
{
  EphyApplicationDialogData *data = user_data;
  char *uri = NULL;
  GdkRGBA color = { 0.5, 0.5, 0.5, 0.3 };
  g_autoptr (GError) error = NULL;

  ephy_web_view_get_best_web_app_icon_finish (EPHY_WEB_VIEW (source), async_result, &uri, &color, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  data->icon_href = uri;
  data->icon_rgba = color;
  if (data->icon_href != NULL)
    download_icon_and_set_image (data);
  else
    set_image_from_favicon (data);
}

typedef struct {
  const char *host;
  const char *name;
} SiteInfo;

static SiteInfo sites[] = {
  { "www.facebook.com", "Facebook" },
  { "twitter.com", "Twitter" },
  { "gmail.com", "GMail" },
  { "youtube.com", "YouTube" },
};

static char *
get_special_case_application_title_for_host (const char *host)
{
  char *title = NULL;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (sites) && title == NULL; i++) {
    SiteInfo *info = &sites[i];
    if (strcmp (host, info->host) == 0) {
      title = g_strdup (info->name);
    }
  }

  return title;
}

static void
set_default_application_title (EphyApplicationDialogData *data,
                               char                      *title)
{
  if (title == NULL || title[0] == '\0') {
    g_autoptr (GUri) uri = NULL;
    const char *host;

    uri = g_uri_parse (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (data->view)), G_URI_FLAGS_NONE, NULL);
    host = g_uri_get_host (uri);

    if (host != NULL && host[0] != '\0') {
      title = get_special_case_application_title_for_host (host);

      if (title == NULL || title[0] == '\0') {
        if (g_str_has_prefix (host, "www."))
          title = g_strdup (host + strlen ("www."));
        else
          title = g_strdup (host);
      }
    }
  }

  if (title == NULL || title[0] == '\0') {
    title = g_strdup (webkit_web_view_get_title (WEBKIT_WEB_VIEW (data->view)));
  }

  data->title = g_strdup (title);
  create_install_dialog_when_ready (data);
  g_free (title);
}

static void
fill_default_application_title_cb (GObject      *source,
                                   GAsyncResult *async_result,
                                   gpointer      user_data)
{
  EphyApplicationDialogData *data = user_data;
  g_autofree char *title = NULL;
  g_autoptr (GError) error = NULL;

  /* Confusing: this can return NULL for no title, even when there is no error. */
  title = ephy_web_view_get_web_app_title_finish (EPHY_WEB_VIEW (source), async_result, &error);
  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    set_default_application_title (data, g_steal_pointer (&title));
  else
    ephy_application_dialog_data_free (data);
}

static void
fill_mobile_capable_cb (GObject      *source,
                        GAsyncResult *async_result,
                        gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  EphyApplicationDialogData *data = user_data;
  gboolean capable;

  capable = ephy_web_view_get_web_app_mobile_capable_finish (EPHY_WEB_VIEW (source), async_result, &error);
  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    data->webapp_options = capable ? EPHY_WEB_APPLICATION_MOBILE_CAPABLE : EPHY_WEB_APPLICATION_NONE;
    data->webapp_options_set = TRUE;
    create_install_dialog_when_ready (data);
  } else {
    ephy_application_dialog_data_free (data);
  }
}

static void
save_as_application_proceed (EphyApplicationDialogData *data)
{
  g_autofree gchar *desktop_file_id = NULL;
  g_autofree char *message = NULL;
  GNotification *notification;
  gboolean success;
  g_autoptr (GError) error = NULL;

  /* Create Web Application, including a new profile and .desktop file. */
  success = ephy_web_application_create (data->app_id,
                                         data->url,
                                         data->token,
                                         data->webapp_options,
                                         &error);

  if (success)
    message = g_strdup_printf (_("The application “%s” is ready to be used"),
                               data->chosen_name);
  else
    message = g_strdup_printf (_("The application “%s” could not be created: %s"),
                               data->chosen_name, error->message);

  notification = g_notification_new (message);

  g_notification_set_icon (notification, G_ICON (data->framed_pixbuf));

  if (success) {
    /* Translators: Desktop notification when a new web app is created. */
    g_notification_add_button_with_target (notification, _("Launch"), "app.launch-app", "s", data->app_id);
    g_notification_set_default_action_and_target (notification, "app.launch-app", "s", data->app_id);

    ephy_focus_desktop_app (data->app_id);
  }

  g_notification_set_priority (notification, G_NOTIFICATION_PRIORITY_LOW);

  g_application_send_notification (G_APPLICATION (g_application_get_default ()), data->chosen_name, notification);

  ephy_application_dialog_data_free (data);
}

static void
dialog_save_as_application_confirmation_cb (GtkDialog                 *dialog,
                                            GtkResponseType            response,
                                            EphyApplicationDialogData *data)
{
  gtk_widget_destroy (GTK_WIDGET (dialog));

  if (response == GTK_RESPONSE_OK) {
    ephy_web_application_delete (data->app_id, NULL);
    save_as_application_proceed (data);
  } else {
    ephy_application_dialog_data_free (data);
  }
}

static void
prepare_install_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  XdpPortal *portal = XDP_PORTAL (object);
  EphyApplicationDialogData *data = user_data;
  g_autoptr (GVariant) ret = NULL;
  g_autoptr (GVariant) chosen_name_v = NULL;
  g_autoptr (GVariant) token_v = NULL;
  GtkWidget *confirmation_dialog;
  g_autoptr (GError) error = NULL;

  ret = xdp_portal_dynamic_launcher_prepare_install_finish (portal, result, &error);
  if (ret == NULL) {
    /* This might just mean the user canceled the operation */
    g_warning ("Failed to install web app, PrepareInstall() failed: %s", error->message);
    ephy_application_dialog_data_free (data);
    return;
  }

  chosen_name_v = g_variant_lookup_value (ret, "name", G_VARIANT_TYPE_STRING);
  token_v = g_variant_lookup_value (ret, "token", G_VARIANT_TYPE_STRING);
  if (chosen_name_v == NULL || token_v == NULL) {
    g_warning ("Failed to install web app, PrepareInstall() returned invalid data");
    ephy_application_dialog_data_free (data);
    return;
  }
  data->chosen_name = g_strdup (g_variant_get_string (chosen_name_v, NULL));
  data->token = g_strdup (g_variant_get_string (token_v, NULL));

  data->app_id = ephy_web_application_get_app_id_from_name (data->chosen_name);

  if (!ephy_web_application_exists (data->app_id)) {
    save_as_application_proceed (data);
    return;
  }

  confirmation_dialog =
    gtk_message_dialog_new (GTK_WINDOW (data->window),
                            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                            GTK_MESSAGE_QUESTION,
                            GTK_BUTTONS_NONE,
                            _("A web application named “%s” already exists. Do you want to replace it?"),
                            data->chosen_name);
  gtk_dialog_add_buttons (GTK_DIALOG (confirmation_dialog),
                          _("Cancel"),
                          GTK_RESPONSE_CANCEL,
                          _("Replace"),
                          GTK_RESPONSE_OK,
                          NULL);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (confirmation_dialog),
                                            _("An application with the same name already exists. Replacing it will "
                                              "overwrite it."));
  gtk_dialog_set_default_response (GTK_DIALOG (confirmation_dialog), GTK_RESPONSE_CANCEL);

  g_signal_connect (confirmation_dialog, "response",
                    G_CALLBACK (dialog_save_as_application_confirmation_cb), data);
  gtk_window_present (GTK_WINDOW (confirmation_dialog));
}

static void
create_install_dialog_when_ready (EphyApplicationDialogData *data)
{
  XdpPortal *portal;
  XdpParent *parent;

  if (!data->webapp_options_set || !data->title || !data->icon_v)
    return;

  /* Create a dialog for the user to confirm */
  portal = ephy_get_portal ();
  parent = xdp_parent_new_gtk (GTK_WINDOW (data->window));
  xdp_portal_dynamic_launcher_prepare_install (portal, parent,
                                               data->title, data->icon_v,
                                               XDP_LAUNCHER_WEBAPP, data->url,
                                               TRUE, TRUE, /* editable name, icon */
                                               data->cancellable,
                                               prepare_install_cb,
                                               data);
}

void
window_cmd_save_as_application (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyEmbed *embed;
  EphyApplicationDialogData *data;

  if (!ephy_can_install_web_apps ())
    return;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (embed != NULL);

  data = g_new0 (EphyApplicationDialogData, 1);
  data->window = g_object_ref (window);
  data->view = EPHY_WEB_VIEW (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
  data->display_address = ephy_web_view_get_display_address (data->view);
  data->url = webkit_web_view_get_uri (WEBKIT_WEB_VIEW (data->view));
  data->cancellable = g_cancellable_new ();

  ephy_web_view_get_best_web_app_icon (data->view, data->cancellable, fill_default_application_image_cb, data);
  ephy_web_view_get_web_app_title (data->view, data->cancellable, fill_default_application_title_cb, data);
  ephy_web_view_get_web_app_mobile_capable (data->view, data->cancellable, fill_mobile_capable_cb, data);
}

static char *
get_suggested_filename (EphyEmbed *embed)
{
  EphyWebView *view;
  const char *suggested_filename;
  const char *mimetype;
  WebKitURIResponse *response;
  WebKitWebResource *web_resource;
  g_autoptr (GUri) uri = NULL;

  view = ephy_embed_get_web_view (embed);
  web_resource = webkit_web_view_get_main_resource (WEBKIT_WEB_VIEW (view));
  response = webkit_web_resource_get_response (web_resource);
  mimetype = webkit_uri_response_get_mime_type (response);
  uri = g_uri_parse (webkit_web_resource_get_uri (web_resource), G_URI_FLAGS_SCHEME_NORMALIZE, NULL);

  if (g_ascii_strncasecmp (mimetype, "text/html", 9) == 0 && g_strcmp0 (g_uri_get_scheme (uri), EPHY_VIEW_SOURCE_SCHEME) != 0) {
    /* Web Title will be used as suggested filename */
    return g_strconcat (ephy_embed_get_title (embed), ".mhtml", NULL);
  }

  suggested_filename = webkit_uri_response_get_suggested_filename (response);
  if (!suggested_filename) {
    const char *path = g_uri_get_path (uri);
    char *last_slash = strrchr (path, '/');
    if (last_slash)
      path = last_slash + 1;

    if (path[0] != '\0')
      return g_strdup (path);
  }

  return suggested_filename ? g_strdup (suggested_filename) : g_strdup ("index.html");
}


static void
save_snapshot (cairo_surface_t *surface,
               const char      *file)
{
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autofree char *snapshot_path = NULL;
  g_autoptr (GError) error = NULL;
  int width;
  int height;
  gboolean ret;

  /* Create a pixbuf */
  width = cairo_image_surface_get_width (surface);
  height = cairo_image_surface_get_height (surface);

  pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);
  if (!pixbuf)
    return;

  ret = gdk_pixbuf_save (pixbuf, file, "png", &error, NULL);
  if (!ret) {
    g_warning ("Failed to save image to %s: %s", snapshot_path, error->message);
    return;
  }
}

static void
take_snapshot_full_cb (GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  WebKitWebView *view = WEBKIT_WEB_VIEW (source);
  GError *error = NULL;
  cairo_surface_t *surface;
  gchar *file = user_data;

  if (!file)
    return;

  surface = webkit_web_view_get_snapshot_finish (view, res, &error);
  if (error) {
    g_warning ("Failed to take snapshot: %s", error->message);
    return;
  }

  save_snapshot (surface, file);
  cairo_surface_destroy (surface);

  g_free (file);
  g_object_unref (view);
}

void
take_snapshot (EphyEmbed *embed,
               char      *file)
{
  WebKitWebView *view;

  view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);
  webkit_web_view_get_snapshot (g_object_ref (view),
                                WEBKIT_SNAPSHOT_REGION_FULL_DOCUMENT,
                                WEBKIT_SNAPSHOT_OPTIONS_NONE,
                                NULL,
                                take_snapshot_full_cb,
                                g_filename_from_uri (file, NULL, NULL));
}


static void
save_response_cb (GtkNativeDialog *dialog,
                  int              response,
                  EphyEmbed       *embed)
{
  if (response == GTK_RESPONSE_ACCEPT) {
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) current_file = NULL;
    g_autofree char *uri = NULL;
    g_autofree char *converted = NULL;
    g_autofree char *current_path = NULL;

    file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
    uri = g_file_get_uri (file);
    if (uri != NULL) {
      converted = g_filename_to_utf8 (uri, -1, NULL, NULL, NULL);

      if (converted != NULL) {
        if (g_str_has_suffix (converted, ".png")) {
          take_snapshot (embed, converted);
        } else {
          EphyWebView *web_view = ephy_embed_get_web_view (embed);
          ephy_web_view_save (web_view, converted);
        }
      }
    }

    current_file = gtk_file_chooser_get_current_folder_file (GTK_FILE_CHOOSER (dialog));
    current_path = g_file_get_path (current_file);
    g_settings_set_string (EPHY_SETTINGS_WEB,
                           EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY,
                           current_path);
  }

  g_object_unref (dialog);
}

void
window_cmd_save_as (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyEmbed *embed;
  GtkFileChooser *dialog;
  GtkFileFilter *filter;
  char *suggested_filename;
  const char *last_directory_path;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (embed != NULL);

  dialog = ephy_create_file_chooser (_("Save"),
                                     GTK_WIDGET (window),
                                     GTK_FILE_CHOOSER_ACTION_SAVE,
                                     EPHY_FILE_FILTER_NONE);

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

  last_directory_path = g_settings_get_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY);

  if (last_directory_path && last_directory_path[0]) {
    g_autoptr (GFile) last_directory = NULL;
    g_autoptr (GError) error = NULL;

    last_directory = g_file_new_for_path (last_directory_path);
    gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (dialog), last_directory, &error);

    if (error)
      g_warning ("Failed to set current folder %s: %s", last_directory_path, error->message);
  }

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (GTK_FILE_FILTER (filter), _("HTML"));
  gtk_file_filter_add_pattern (GTK_FILE_FILTER (filter), "*.html");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (GTK_FILE_FILTER (filter), _("MHTML"));
  gtk_file_filter_add_pattern (GTK_FILE_FILTER (filter), "*.mhtml");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (GTK_FILE_FILTER (filter), _("PNG"));
  gtk_file_filter_add_pattern (GTK_FILE_FILTER (filter), "*.png");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  suggested_filename = ephy_sanitize_filename (get_suggested_filename (embed));

  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), suggested_filename);
  g_free (suggested_filename);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (save_response_cb), embed);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));
}

void
window_cmd_undo (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkWidget *widget;
  GtkWidget *embed;
  GtkWidget *location_entry;

  widget = gtk_window_get_focus (GTK_WINDOW (window));
  location_entry = gtk_widget_get_ancestor (widget, EPHY_TYPE_LOCATION_ENTRY);

  if (location_entry) {
    ephy_location_entry_reset (EPHY_LOCATION_ENTRY (location_entry));
  } else {
    embed = gtk_widget_get_ancestor (widget, EPHY_TYPE_EMBED);

    if (embed) {
      webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (EPHY_EMBED (embed)), "Undo");
    }
  }
}

void
window_cmd_redo (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkWidget *widget;
  GtkWidget *embed;
  GtkWidget *location_entry;

  widget = gtk_window_get_focus (GTK_WINDOW (window));
  location_entry = gtk_widget_get_ancestor (widget, EPHY_TYPE_LOCATION_ENTRY);

  if (location_entry) {
    ephy_location_entry_undo_reset (EPHY_LOCATION_ENTRY (location_entry));
  } else {
    embed = gtk_widget_get_ancestor (widget, EPHY_TYPE_EMBED);
    if (embed) {
      webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (EPHY_EMBED (embed)), "Redo");
    }
  }
}
void
window_cmd_cut (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

  if (GTK_IS_EDITABLE (widget)) {
    gtk_editable_cut_clipboard (GTK_EDITABLE (widget));
  } else {
    EphyEmbed *embed;
    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
    g_assert (embed != NULL);

    webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), WEBKIT_EDITING_COMMAND_CUT);
  }
}

void
window_cmd_copy (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

  if (GTK_IS_EDITABLE (widget)) {
    gtk_editable_copy_clipboard (GTK_EDITABLE (widget));
  } else {
    EphyEmbed *embed;

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
    g_assert (embed != NULL);

    webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), WEBKIT_EDITING_COMMAND_COPY);
  }
}

void
window_cmd_paste (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

  if (GTK_IS_EDITABLE (widget)) {
    gtk_editable_paste_clipboard (GTK_EDITABLE (widget));
  } else {
    EphyEmbed *embed;

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
    g_assert (embed != NULL);

    webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), WEBKIT_EDITING_COMMAND_PASTE);
  }
}

void
window_cmd_paste_as_plain_text (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

  if (GTK_IS_EDITABLE (widget)) {
    gtk_editable_paste_clipboard (GTK_EDITABLE (widget));
  } else {
    EphyEmbed *embed;

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
    g_assert (embed != NULL);

    webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), WEBKIT_EDITING_COMMAND_PASTE_AS_PLAIN_TEXT);
  }
}

void
window_cmd_delete (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

  if (GTK_IS_EDITABLE (widget)) {
    gtk_editable_delete_text (GTK_EDITABLE (widget), 0, -1);
  } else {
    EphyEmbed *embed;

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
    g_assert (embed != NULL);

    /* FIXME: TODO */
#if 0
    ephy_command_manager_do_command (EPHY_COMMAND_MANAGER (embed),
                                     "cmd_delete");
#endif
  }
}

void
window_cmd_print (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyEmbed *embed;
  EphyWebView *view;

  embed = ephy_embed_container_get_active_child
            (EPHY_EMBED_CONTAINER (window));
  g_assert (EPHY_IS_EMBED (embed));
  view = ephy_embed_get_web_view (embed);

  ephy_web_view_print (view);
}

void
window_cmd_find (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyFindToolbar *toolbar;

  toolbar = EPHY_FIND_TOOLBAR (ephy_window_get_current_find_toolbar (window));
  ephy_find_toolbar_open (toolbar);
}

void
window_cmd_find_prev (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyFindToolbar *toolbar;

  toolbar = EPHY_FIND_TOOLBAR (ephy_window_get_current_find_toolbar (window));
  ephy_find_toolbar_find_previous (toolbar);
}

void
window_cmd_find_next (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyFindToolbar *toolbar;

  toolbar = EPHY_FIND_TOOLBAR (ephy_window_get_current_find_toolbar (window));
  ephy_find_toolbar_find_next (toolbar);
}

void
window_cmd_bookmark_page (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyHeaderBar *header_bar;
  EphyTitleWidget *title_widget;

  header_bar = EPHY_HEADER_BAR (ephy_window_get_header_bar (window));
  title_widget = ephy_header_bar_get_title_widget (header_bar);
  g_assert (EPHY_IS_LOCATION_ENTRY (title_widget));

  ephy_location_entry_show_add_bookmark_popover (EPHY_LOCATION_ENTRY (title_widget));
}

void
window_cmd_bookmarks (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyHeaderBar *header_bar;
  EphyActionBarEnd *action_bar_end;

  header_bar = EPHY_HEADER_BAR (ephy_window_get_header_bar (window));
  action_bar_end = EPHY_ACTION_BAR_END (ephy_header_bar_get_action_bar_end (header_bar));
  ephy_action_bar_end_show_bookmarks (action_bar_end);
}

void
window_cmd_show_downloads (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyHeaderBar *header_bar;
  EphyActionBarEnd *action_bar_end;

  header_bar = EPHY_HEADER_BAR (ephy_window_get_header_bar (window));
  action_bar_end = EPHY_ACTION_BAR_END (ephy_header_bar_get_action_bar_end (header_bar));
  ephy_action_bar_end_show_downloads (action_bar_end);
}

void
window_cmd_zoom_in (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  EphyWindow *window = user_data;

  ephy_window_set_zoom (window, ZOOM_IN);
}

void
window_cmd_zoom_out (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  EphyWindow *window = user_data;

  ephy_window_set_zoom (window, ZOOM_OUT);
}

void
window_cmd_zoom_normal (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  EphyWindow *window = user_data;
  ephy_window_set_zoom (window, 0.0);
}

void
window_cmd_encoding (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyEncodingDialog *dialog;

  dialog = ephy_encoding_dialog_new (window);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
  gtk_window_present (GTK_WINDOW (dialog));
}

void
window_cmd_page_source (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyEmbed *embed;
  EphyEmbed *new_embed;
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GUri) converted_uri = NULL;
  char *source_uri;
  const char *address;

  embed = ephy_embed_container_get_active_child
            (EPHY_EMBED_CONTAINER (window));
  g_assert (embed != NULL);

  address = ephy_web_view_get_address (ephy_embed_get_web_view (embed));

  /* Abort if we're already in view source mode */
  if (strstr (address, EPHY_VIEW_SOURCE_SCHEME) == address)
    return;

  uri = g_uri_parse (address, G_URI_FLAGS_ENCODED | G_URI_FLAGS_SCHEME_NORMALIZE, NULL);
  if (!uri) {
    g_critical ("Failed to construct GUri for %s", address);
    return;
  }

  /* Convert e.g. https://gnome.org to ephy-source://gnome.org#https */
  converted_uri = g_uri_build (g_uri_get_flags (uri),
                               EPHY_VIEW_SOURCE_SCHEME,
                               g_uri_get_userinfo (uri),
                               g_uri_get_host (uri),
                               g_uri_get_port (uri),
                               g_uri_get_path (uri),
                               g_uri_get_query (uri),
                               g_uri_get_scheme (uri));
  source_uri = g_uri_to_string (converted_uri);

  new_embed = ephy_shell_new_tab
                (ephy_shell_get_default (),
                EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                embed,
                EPHY_NEW_TAB_JUMP | EPHY_NEW_TAB_APPEND_AFTER);

  webkit_web_view_load_uri (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (new_embed), source_uri);
  gtk_widget_grab_focus (GTK_WIDGET (new_embed));

  g_free (source_uri);
}

void
window_cmd_toggle_inspector (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyEmbed *embed;
  WebKitWebView *view;
  WebKitWebInspector *inspector_window;

  embed = ephy_embed_container_get_active_child
            (EPHY_EMBED_CONTAINER (window));
  g_assert (embed != NULL);

  gtk_widget_grab_focus (GTK_WIDGET (embed));

  view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  inspector_window = webkit_web_view_get_inspector (view);

  if (!ephy_embed_inspector_is_loaded (embed))
    webkit_web_inspector_show (inspector_window);
  else
    webkit_web_inspector_close (inspector_window);
}

void
window_cmd_select_all (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  EphyWindow *window = user_data;

  GtkWidget *widget = gtk_window_get_focus (GTK_WINDOW (window));

  if (GTK_IS_EDITABLE (widget)) {
    gtk_editable_select_region (GTK_EDITABLE (widget), 0, -1);
  } else {
    EphyEmbed *embed;

    embed = ephy_embed_container_get_active_child
              (EPHY_EMBED_CONTAINER (window));
    g_assert (embed != NULL);

    webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), "SelectAll");
  }
}

void
window_cmd_send_to (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;
  char *command, *subject, *body;
  const char *location, *title;
  GError *error = NULL;

  embed = ephy_embed_container_get_active_child
            (EPHY_EMBED_CONTAINER (window));
  g_assert (embed != NULL);

  location = ephy_web_view_get_address (ephy_embed_get_web_view (embed));
  title = ephy_embed_get_title (embed);

  subject = g_uri_escape_string (title, NULL, TRUE);
  body = g_uri_escape_string (location, NULL, TRUE);

  command = g_strconcat ("mailto:",
                         "?Subject=", subject,
                         "&Body=", body, NULL);

  g_free (subject);
  g_free (body);

  if (!gtk_show_uri_on_window (GTK_WINDOW (window), command, GDK_CURRENT_TIME, &error)) {
    g_warning ("Unable to send link by email: %s\n", error->message);
    g_error_free (error);
  }

  g_free (command);
}

void
window_cmd_go_location (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  ephy_window_activate_location (user_data);
}

void
window_cmd_location_search (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  ephy_window_location_search (user_data);
}

void
window_cmd_go_home (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  ephy_link_open (EPHY_LINK (user_data),
                  NULL, NULL,
                  EPHY_LINK_HOME_PAGE);
}

void
window_cmd_go_content (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  ephy_window_close_pages_view (EPHY_WINDOW (user_data));
}

void
window_cmd_go_tabs_view (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  ephy_window_open_pages_view (EPHY_WINDOW (user_data));
}

static void
enable_browse_with_caret_state_cb (GtkMessageDialog *dialog,
                                   GtkResponseType   response,
                                   EphyWindow       *window)
{
  GActionGroup *action_group = gtk_widget_get_action_group (GTK_WIDGET (window), "win");
  GAction *action;

  gtk_widget_destroy (GTK_WIDGET (dialog));

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "browse-with-caret");

  if (response == GTK_RESPONSE_CANCEL) {
    g_simple_action_set_state (G_SIMPLE_ACTION (action),
                               g_variant_new_boolean (FALSE));

    return;
  }

  g_simple_action_set_state (G_SIMPLE_ACTION (action),
                             g_variant_new_boolean (TRUE));
  g_settings_set_boolean (EPHY_SETTINGS_MAIN,
                          EPHY_PREFS_ENABLE_CARET_BROWSING, TRUE);
}

void
window_cmd_change_browse_with_caret_state (GSimpleAction *action,
                                           GVariant      *state,
                                           gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  gboolean active;

  active = g_variant_get_boolean (state);

  if (active) {
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
                                     _("Enable caret browsing mode?"));

    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              _("Pressing F7 turns caret browsing on or off. This feature "
                                                "places a moveable cursor in web pages, allowing you to move "
                                                "around with your keyboard. Do you want to enable caret browsing?"));
    gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Enable"), GTK_RESPONSE_ACCEPT);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

    g_signal_connect (dialog, "response",
                      G_CALLBACK (enable_browse_with_caret_state_cb), window);

    gtk_window_present (GTK_WINDOW (dialog));

    return;
  }

  g_simple_action_set_state (action, g_variant_new_boolean (active));
  g_settings_set_boolean (EPHY_SETTINGS_MAIN,
                          EPHY_PREFS_ENABLE_CARET_BROWSING, active);
}

void
window_cmd_change_fullscreen_state (GSimpleAction *action,
                                    GVariant      *state,
                                    gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  gboolean active;

  active = g_variant_get_boolean (state);

  /* This is performed only here because we don't want it occuring when a window
   * enters fullscreen mode for some other reason other than action activation.
   * E.g. we don't want it appearing for fullscreen video.
   */
  ephy_window_show_fullscreen_header_bar (window);

  if (active)
    gtk_window_fullscreen (GTK_WINDOW (window));
  else
    gtk_window_unfullscreen (GTK_WINDOW (window));

  g_simple_action_set_state (action, g_variant_new_boolean (active));
}

void
window_cmd_tabs_duplicate (GSimpleAction *action,
                           GVariant      *variant,
                           gpointer       user_data)
{
  EphyTabView *tab_view;
  EphyEmbed *embed, *new_embed;
  WebKitWebView *view, *new_view;
  WebKitWebViewSessionState *session_state;
  WebKitBackForwardList *bf_list;
  WebKitBackForwardListItem *item;

  tab_view = ephy_window_get_tab_view (EPHY_WINDOW (user_data));
  embed = EPHY_EMBED (ephy_tab_view_get_current_page (tab_view));

  view = WEBKIT_WEB_VIEW (ephy_embed_get_web_view (embed));
  session_state = webkit_web_view_get_session_state (view);

  new_embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                  EPHY_WINDOW (user_data),
                                  embed,
                                  EPHY_NEW_TAB_APPEND_AFTER | EPHY_NEW_TAB_JUMP);

  new_view = WEBKIT_WEB_VIEW (ephy_embed_get_web_view (new_embed));

  webkit_web_view_restore_session_state (new_view, session_state);
  webkit_web_view_session_state_unref (session_state);

  bf_list = webkit_web_view_get_back_forward_list (new_view);
  item = webkit_back_forward_list_get_current_item (bf_list);
  if (item)
    webkit_web_view_go_to_back_forward_list_item (new_view, item);
  else
    ephy_web_view_load_url (EPHY_WEB_VIEW (new_view), webkit_web_view_get_uri (view));
}

void
window_cmd_tabs_close (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyTabView *tab_view;

  tab_view = ephy_window_get_tab_view (window);

  if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                              EPHY_PREFS_LOCKDOWN_QUIT) &&
      ephy_tab_view_get_n_pages (tab_view) <= 1) {
    return;
  }

  ephy_tab_view_close_selected (tab_view);
}

void
window_cmd_tabs_close_left (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       user_data)
{
  EphyWindow *window = user_data;

  ephy_tab_view_close_left (ephy_window_get_tab_view (window));
}

void
window_cmd_tabs_close_right (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  EphyWindow *window = user_data;

  ephy_tab_view_close_right (ephy_window_get_tab_view (window));
}

void
window_cmd_tabs_close_others (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  EphyWindow *window = user_data;

  ephy_tab_view_close_other (ephy_window_get_tab_view (window));
}

static void
reload_cb (GtkWidget *widget,
           gpointer   user_data)
{
  EphyEmbed *embed = EPHY_EMBED (widget);
  WebKitWebView *view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  webkit_web_view_reload (view);
}

void
window_cmd_tabs_reload_all_tabs (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
  EphyWindow *window = user_data;

  ephy_tab_view_foreach (ephy_window_get_tab_view (window),
                         reload_cb,
                         NULL);
}

void
window_cmd_toggle_reader_mode (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;
  EphyWebView *web_view;
  gboolean active;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (embed != NULL);

  web_view = ephy_embed_get_web_view (embed);

  if (!ephy_web_view_is_reader_mode_available (web_view))
    return;

  active = ephy_web_view_get_reader_mode_state (web_view);

  ephy_web_view_toggle_reader_mode (web_view, !active);
}

void
window_cmd_open_application_manager (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;
  EphyWebView *web_view;

  embed = ephy_shell_new_tab (ephy_shell_get_default (),
                              window,
                              NULL,
                              EPHY_NEW_TAB_JUMP);

  web_view = ephy_embed_get_web_view (embed);

  ephy_web_view_load_url (web_view, "about:applications");
}

void
window_cmd_homepage_new_tab (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;
  EphyWebView *web_view;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (embed != NULL);

  embed = ephy_shell_new_tab (ephy_shell_get_default (),
                              EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                              NULL,
                              0);

  web_view = ephy_embed_get_web_view (embed);
  ephy_web_view_load_homepage (web_view);

  ephy_embed_container_set_active_child (EPHY_EMBED_CONTAINER (window), embed);

  gtk_widget_grab_focus (GTK_WIDGET (embed));
}

static void
clipboard_text_received_cb (GtkClipboard *clipboard,
                            const gchar  *text,
                            EphyWindow   *window)
{
  EphyEmbed *embed;
  EphyWebView *web_view;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (embed != NULL);

  embed = ephy_shell_new_tab (ephy_shell_get_default (),
                              EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (embed))),
                              NULL,
                              0);

  web_view = ephy_embed_get_web_view (embed);
  ephy_web_view_load_url (web_view, text);

  ephy_embed_container_set_active_child (EPHY_EMBED_CONTAINER (window), embed);
  gtk_widget_grab_focus (GTK_WIDGET (embed));

  g_object_unref (window);
}

void
window_cmd_new_tab_from_clipboard (GSimpleAction *action,
                                   GVariant      *parameter,
                                   gpointer       user_data)
{
  EphyWindow *ephy_window = EPHY_WINDOW (user_data);
  GtkClipboard *clipboard;

  clipboard = gtk_clipboard_get_default (gdk_display_get_default ());
  gtk_clipboard_request_text (clipboard,
                              (GtkClipboardTextReceivedFunc)clipboard_text_received_cb,
                              g_object_ref (ephy_window));
}

void
window_cmd_tabs_pin (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);

  ephy_tab_view_pin (ephy_window_get_tab_view (window));
}

void
window_cmd_tabs_unpin (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);

  ephy_tab_view_unpin (ephy_window_get_tab_view (window));
}

void
window_cmd_change_tabs_mute_state (GSimpleAction *action,
                                   GVariant      *state,
                                   gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;
  EphyWebView *view;
  gboolean mute;

  embed = EPHY_EMBED (ephy_tab_view_get_current_page (ephy_window_get_tab_view (window)));
  g_assert (embed != NULL);

  view = ephy_embed_get_web_view (embed);

  if (!webkit_web_view_is_playing_audio (WEBKIT_WEB_VIEW (view)))
    return;

  mute = !webkit_web_view_get_is_muted (WEBKIT_WEB_VIEW (view));

  webkit_web_view_set_is_muted (WEBKIT_WEB_VIEW (view), mute);

  g_simple_action_set_state (action, g_variant_new_boolean (mute));
}

void
window_cmd_extensions (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  GtkWidget *dialog;

  dialog = ephy_web_extension_dialog_new ();
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
  gtk_widget_show_all (dialog);
}
