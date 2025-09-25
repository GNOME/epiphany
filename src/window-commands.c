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

#include "clear-data-view.h"
#include "ephy-bookmark-properties.h"
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
#include "ephy-file-dialog-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-find-toolbar.h"
#include "ephy-flatpak-utils.h"
#include "ephy-header-bar.h"
#include "ephy-history-dialog.h"
#include "ephy-link.h"
#include "ephy-location-entry.h"
#include "ephy-page-menu-button.h"
#include "ephy-password-export.h"
#include "ephy-password-import.h"
#include "ephy-passwords-view.h"
#include "ephy-pixbuf-utils.h"
#include "ephy-prefs-dialog.h"
#include "ephy-prefs.h"
#include "ephy-privacy-report.h"
#include "ephy-security-dialog.h"
#include "ephy-session.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-string.h"
#include "ephy-tab-view.h"
#include "ephy-view-source-handler.h"
#include "ephy-web-app-utils.h"
#include "ephy-zoom.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gst/gst.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit/webkit.h>
#include <libportal-gtk4/portal-gtk4.h>
#include <adwaita.h>

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

#define IMPORT_FROM_HTML_ID "html"
#define IMPORT_FROM_FIREFOX_ID "firefox"
#define IMPORT_FROM_CHROME_ID "chrome"
#define IMPORT_FROM_CHROMIUM_ID "chromium"
#define IMPORT_FROM_CSV_ID "csv"

typedef enum {
  IMPORT_TYPE_CHOOSE,
  IMPORT_TYPE_IMPORT
} ImportTypes;

struct import_option {
  const char *name;
  ImportTypes type;
  const char *id;
  gboolean (*exists)(void);
};

static gboolean firefox_exists (void);
static gboolean chrome_exists (void);
static gboolean chromium_exists (void);

static struct import_option import_options[] = {
  { N_("HTML File"), IMPORT_TYPE_CHOOSE, IMPORT_FROM_HTML_ID, NULL },
  { N_("Firefox"), IMPORT_TYPE_IMPORT, IMPORT_FROM_FIREFOX_ID, firefox_exists },
  { N_("Chrome"), IMPORT_TYPE_IMPORT, IMPORT_FROM_CHROME_ID, chrome_exists },
  { N_("Chromium"), IMPORT_TYPE_IMPORT, IMPORT_FROM_CHROMIUM_ID, chromium_exists }
};

static char **
get_available_bookmark_import_options ()
{
  GPtrArray *options = NULL;

  options = g_ptr_array_new ();

  for (int i = G_N_ELEMENTS (import_options) - 1; i >= 0; i--) {
    if (!import_options[i].exists || import_options[i].exists ())
      g_ptr_array_add (options, g_strdup (import_options[i].id));
  }

  g_ptr_array_add (options, NULL);

  return (char **)g_ptr_array_free (options, FALSE);
}

static void
update_bookmarks_select_button_label (AdwComboRow *combo_row,
                                      GtkButton   *button)
{
  GtkStringObject *str_obj;
  const char *option_name;
  int i = 0;
  gboolean option_found = FALSE;

  g_assert (ADW_IS_COMBO_ROW (combo_row));
  g_assert (GTK_IS_BUTTON (button));

  str_obj = adw_combo_row_get_selected_item (combo_row);
  option_name = gtk_string_object_get_string (str_obj);
  for (i = G_N_ELEMENTS (import_options) - 1; i >= 0; i--)
    if (g_strcmp0 (import_options[i].name, option_name) == 0) {
      option_found = TRUE;
      break;
    }

  g_assert (option_found);
  if (import_options[i].type == IMPORT_TYPE_CHOOSE)
    gtk_button_set_label (button, _("_Select File"));
  else if (import_options[i].type == IMPORT_TYPE_IMPORT)
    gtk_button_set_label (button, _("I_mport"));
}

static void
bookmarks_row_selected_cb (AdwComboRow *combo_row,
                           GParamSpec  *pspec,
                           GtkButton   *button)
{
  update_bookmarks_select_button_label (combo_row, button);
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
  if (error && !g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
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

static GListModel *
create_bookmarks_model ()
{
  GtkStringList *string_list = gtk_string_list_new (NULL);
  int i;

  for (i = G_N_ELEMENTS (import_options) - 1; i >= 0; i--) {
    if (import_options[i].exists && !import_options[i].exists ())
      continue;

    gtk_string_list_append (string_list, import_options[i].name);
  }

  return G_LIST_MODEL (string_list);
}

static void
show_import_export_result (GtkWindow  *parent,
                           gboolean    destroy_parent,
                           gboolean    success,
                           GError     *error,
                           const char *message)
{
  AdwDialog *info_dialog;

  info_dialog = adw_alert_dialog_new (NULL, success ? message : error->message);

  adw_alert_dialog_add_response (ADW_ALERT_DIALOG (info_dialog),
                                 "close", _("_Close"));

  if (destroy_parent)
    g_signal_connect_swapped (info_dialog, "response",
                              G_CALLBACK (gtk_window_destroy), parent);

  adw_dialog_present (info_dialog, GTK_WIDGET (parent));
}

static void
show_firefox_profile_selector_cb (GtkWidget *button,
                                  GtkWindow *parent)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  GtkWindow *selector;
  GtkListBox *list_box;
  GtkListBoxRow *row;
  GtkWidget *row_widget;
  const char *selected_profile = NULL;

  selector = GTK_WINDOW (gtk_widget_get_root (button));
  list_box = GTK_LIST_BOX (gtk_window_get_child (selector));
  row = gtk_list_box_get_selected_row (GTK_LIST_BOX (list_box));
  row_widget = gtk_list_box_row_get_child (GTK_LIST_BOX_ROW (row));
  selected_profile = g_object_steal_data (G_OBJECT (row_widget), "profile_path");

  gtk_window_close (GTK_WINDOW (selector));

  /* If there are multiple profiles, but the user didn't select one in
   * the profile (he pressed Cancel), don't display the import info dialog
   * as no import took place
   */
  if (selected_profile) {
    g_autoptr (GError) error = NULL;
    gboolean imported = ephy_bookmarks_import_from_firefox (manager, selected_profile, &error);

    show_import_export_result (parent, FALSE, imported, error,
                               _("Bookmarks successfully imported!"));
  }
}

static void
show_firefox_profile_selector (GtkWindow *parent,
                               GSList    *profiles)
{
  GtkWidget *selector;
  GtkWidget *header_bar;
  GtkWidget *button;
  GtkWidget *list_box;
  GtkEventController *controller;
  GtkShortcut *shortcut;
  GSList *l;

  selector = gtk_window_new ();
  gtk_window_set_modal (GTK_WINDOW (selector), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (selector), parent);
  gtk_window_set_title (GTK_WINDOW (selector), _("Select Profile"));

  shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_Escape, 0),
                               gtk_named_action_new ("window.close"));
  controller = gtk_shortcut_controller_new ();
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);
  gtk_widget_add_controller (selector, controller);

  header_bar = adw_header_bar_new ();
  adw_header_bar_set_show_start_title_buttons (ADW_HEADER_BAR (header_bar), FALSE);
  adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (header_bar), FALSE);
  gtk_window_set_titlebar (GTK_WINDOW (selector), header_bar);

  button = gtk_button_new_with_mnemonic (_("_Cancel"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "window.close");
  adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar), button);

  button = gtk_button_new_with_mnemonic (_("_Select"));
  gtk_widget_add_css_class (button, "suggested-action");
  gtk_window_set_default_widget (GTK_WINDOW (selector), button);
  adw_header_bar_pack_end (ADW_HEADER_BAR (header_bar), button);

  list_box = gtk_list_box_new ();
  gtk_widget_set_margin_top (list_box, 5);
  gtk_widget_set_margin_bottom (list_box, 5);
  gtk_widget_set_margin_start (list_box, 5);
  gtk_widget_set_margin_end (list_box, 5);
  gtk_window_set_child (GTK_WINDOW (selector), list_box);

  for (l = profiles; l; l = l->next) {
    const gchar *profile = l->data;
    GtkWidget *label;

    label = gtk_label_new (strchr (profile, '.') + 1);
    g_object_set_data_full (G_OBJECT (label), "profile_path", g_strdup (profile), g_free);
    gtk_widget_set_margin_top (label, 6);
    gtk_widget_set_margin_bottom (label, 6);
    gtk_list_box_insert (GTK_LIST_BOX (list_box), label, -1);
  }

  g_signal_connect (button, "clicked",
                    G_CALLBACK (show_firefox_profile_selector_cb),
                    parent);

  gtk_window_present (GTK_WINDOW (selector));
}

static void
dialog_bookmarks_import_from_html_file_dialog_cb (GtkFileDialog *dialog,
                                                  GAsyncResult  *result,
                                                  GtkWindow     *parent)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;
  g_autofree char *filename = NULL;
  gboolean imported;

  file = gtk_file_dialog_open_finish (dialog, result, &error);

  if (error) {
    if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
      g_warning ("Failed to open file: %s", error->message);
    return;
  }

  filename = g_file_get_path (file);
  imported = ephy_bookmarks_import_from_html (manager, filename, &error);

  show_import_export_result (parent, FALSE, imported, error,
                             _("Bookmarks successfully imported!"));
}

static void
dialog_bookmarks_import_from_html (GtkWindow *parent)
{
  GtkFileDialog *dialog;
  g_autoptr (GtkFileFilter) filter = NULL;
  g_autoptr (GListStore) filters = NULL;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, _("Choose File"));

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*.html");

  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  gtk_file_dialog_open (dialog,
                        parent,
                        NULL,
                        (GAsyncReadyCallback)dialog_bookmarks_import_from_html_file_dialog_cb,
                        parent);
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

    show_import_export_result (parent, FALSE, imported, error,
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

  show_import_export_result (parent, FALSE, imported, error,
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

  show_import_export_result (parent, FALSE, imported, error,
                             _("Bookmarks successfully imported!"));
}

static void
import_bookmarks_using_option_id (const char *option_id,
                                  GtkWindow  *window)
{
  if (strcmp (option_id, IMPORT_FROM_HTML_ID) == 0)
    dialog_bookmarks_import_from_html (window);
  else if (strcmp (option_id, IMPORT_FROM_FIREFOX_ID) == 0)
    dialog_bookmarks_import_from_firefox (window);
  else if (strcmp (option_id, IMPORT_FROM_CHROME_ID) == 0)
    dialog_bookmarks_import_from_chrome (window);
  else if (strcmp (option_id, IMPORT_FROM_CHROMIUM_ID) == 0)
    dialog_bookmarks_import_from_chromium (window);
  else
    g_assert_not_reached ();
}

static void
dialog_bookmarks_import_cb (GtkWidget   *button,
                            AdwComboRow *combo_row)
{
  AdwDialog *dialog = ADW_DIALOG (gtk_widget_get_ancestor (button, ADW_TYPE_DIALOG));
  GtkWindow *window = GTK_WINDOW (gtk_widget_get_ancestor (button, GTK_TYPE_WINDOW));
  GtkStringObject *str_obj = adw_combo_row_get_selected_item (combo_row);
  const char *option_name = gtk_string_object_get_string (str_obj);
  int i = 0;
  const char *option_id = NULL;

  for (i = G_N_ELEMENTS (import_options) - 1; i >= 0; i--)
    if (g_strcmp0 (import_options[i].name, option_name) == 0) {
      option_id = import_options[i].id;
      break;
    }

  g_assert (option_id);
  import_bookmarks_using_option_id (option_id, window);

  adw_dialog_force_close (dialog);
}

static void
import_bookmarks_using_option_dialog (EphyWindow *window)
{
  GtkWidget *dialog;
  GtkWidget *header_bar;
  GtkWidget *toolbar_view;
  GtkWidget *cancel_button;
  GtkWidget *select_button;
  GtkWidget *prefs_group;
  GtkWidget *combo_row;
  GListModel *model;

  dialog = GTK_WIDGET (adw_dialog_new ());
  adw_dialog_set_title (ADW_DIALOG (dialog), _("Import Bookmarks"));

  header_bar = adw_header_bar_new ();
  adw_header_bar_set_show_start_title_buttons (ADW_HEADER_BAR (header_bar), FALSE);
  adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (header_bar), FALSE);

  toolbar_view = GTK_WIDGET (adw_toolbar_view_new ());
  adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);
  adw_dialog_set_child (ADW_DIALOG (dialog), toolbar_view);

  cancel_button = gtk_button_new_with_mnemonic (_("_Cancel"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (cancel_button), "window.close");
  adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar), cancel_button);

  select_button = gtk_button_new_with_mnemonic (_("_Select File"));
  gtk_widget_add_css_class (select_button, "suggested-action");
  adw_dialog_set_default_widget (ADW_DIALOG (dialog), select_button);
  adw_header_bar_pack_end (ADW_HEADER_BAR (header_bar), select_button);

  prefs_group = adw_preferences_group_new ();
  gtk_widget_set_margin_top (prefs_group, 12);
  gtk_widget_set_margin_bottom (prefs_group, 12);
  gtk_widget_set_margin_start (prefs_group, 12);
  gtk_widget_set_margin_end (prefs_group, 12);
  adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), prefs_group);

  model = create_bookmarks_model ();
  combo_row = adw_combo_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (combo_row), _("File Type"));
  adw_combo_row_set_model (ADW_COMBO_ROW (combo_row), model);
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (prefs_group), combo_row);

  g_signal_connect_object (ADW_COMBO_ROW (combo_row), "notify::selected",
                           G_CALLBACK (bookmarks_row_selected_cb),
                           GTK_BUTTON (select_button), 0);
  g_signal_connect_object (select_button, "clicked",
                           G_CALLBACK (dialog_bookmarks_import_cb),
                           ADW_COMBO_ROW (combo_row), 0);

  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (window));
  update_bookmarks_select_button_label (ADW_COMBO_ROW (combo_row),
                                        GTK_BUTTON (select_button));
}

void
window_cmd_import_bookmarks (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  g_auto (GStrv) available_import_options = NULL;

  available_import_options = get_available_bookmark_import_options ();

  if (g_strv_length (available_import_options) == 1)
    import_bookmarks_using_option_id (available_import_options[0], GTK_WINDOW (window));
  else {
    GListModel *dialogs = adw_application_window_get_dialogs (ADW_APPLICATION_WINDOW (window));
    int i = 0;
    int n = g_list_model_get_n_items (dialogs);
    gboolean presented = FALSE;
    AdwDialog *dialog;

    for (i = 0; i < n; i++) {
      dialog = g_list_model_get_item (dialogs, i);

      if (g_strcmp0 (adw_dialog_get_title (dialog), "Import Bookmarks") == 0) {
        presented = TRUE;
        break;
      }
    }

    if (!presented)
      import_bookmarks_using_option_dialog (window);
    else
      adw_dialog_present (dialog, GTK_WIDGET (window));
  }
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
export_bookmarks_file_dialog_cb (GtkFileDialog *dialog,
                                 GAsyncResult  *result,
                                 GtkWindow     *parent)
{
  EphyBookmarksManager *manager = ephy_shell_get_bookmarks_manager (ephy_shell_get_default ());
  g_autoptr (GFile) file = NULL;
  g_autofree char *filename = NULL;

  file = gtk_file_dialog_save_finish (dialog, result, NULL);

  if (!file) {
    g_object_unref (parent);
    return;
  }

  filename = g_file_get_path (file);
  ephy_bookmarks_export (g_object_ref (manager),
                         filename,
                         FALSE,
                         FALSE,
                         NULL,
                         bookmarks_export_cb,
                         parent);
}

void
window_cmd_export_bookmarks (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  GtkFileDialog *dialog;
  g_autoptr (GtkFileFilter) filter = NULL;
  g_autoptr (GListStore) filters = NULL;
  GtkWindow *window = user_data;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, _("Choose File"));

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*.html");
  gtk_file_filter_add_pattern (filter, "*.gvdb");

  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  /* Translators: Only translate the part before ".html" (e.g. "bookmarks") */
  gtk_file_dialog_set_initial_name (dialog, _("bookmarks.html"));

  gtk_file_dialog_save (dialog,
                        window,
                        NULL,
                        (GAsyncReadyCallback)export_bookmarks_file_dialog_cb,
                        g_object_ref (window));
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
  { N_("CSV File"), IMPORT_TYPE_CHOOSE, IMPORT_FROM_CSV_ID, NULL },
  { N_("Chrome"), IMPORT_TYPE_IMPORT, IMPORT_FROM_CHROME_ID, chrome_passwords_exists },
  { N_("Chromium"), IMPORT_TYPE_IMPORT, IMPORT_FROM_CHROMIUM_ID, chromium_passwords_exists }
};

static GListModel *
create_passwords_model ()
{
  GtkStringList *string_list = gtk_string_list_new (NULL);
  int i;

  for (i = G_N_ELEMENTS (import_passwords_options) - 1; i >= 0; i--) {
    if (import_options[i].exists && !import_passwords_options[i].exists ())
      continue;

    gtk_string_list_append (string_list, import_passwords_options[i].name);
  }

  return G_LIST_MODEL (string_list);
}

static char **
get_available_password_import_options ()
{
  GPtrArray *options = NULL;

  options = g_ptr_array_new ();

  for (int i = G_N_ELEMENTS (import_passwords_options) - 1; i >= 0; i--) {
    if (!import_passwords_options[i].exists || import_passwords_options[i].exists ())
      g_ptr_array_add (options, g_strdup (import_passwords_options[i].id));
  }

  g_ptr_array_add (options, NULL);

  return (char **)g_ptr_array_free (options, FALSE);
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
dialog_password_import_file_dialog_cb (GtkFileDialog *dialog,
                                       GAsyncResult  *result,
                                       GtkWindow     *parent)
{
  EphyPasswordManager *manager;
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;
  g_autofree char *filename = NULL;
  gboolean imported;

  manager = ephy_embed_shell_get_password_manager (ephy_embed_shell_get_default ());
  file = gtk_file_dialog_open_finish (dialog, result, &error);

  if (error) {
    if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
      g_warning ("Failed to open file: %s", error->message);
    return;
  }

  filename = g_file_get_path (file);
  imported = ephy_password_import_from_csv (manager, filename, &error);
  show_import_export_result (parent, FALSE, imported, error,
                             _("Passwords successfully imported!"));
}

static void
dialog_password_import_from_csv (GtkWindow *parent)
{
  GtkFileDialog *dialog;
  g_autoptr (GtkFileFilter) filter = NULL;
  g_autoptr (GListStore) filters = NULL;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, _("Choose File"));

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*.csv");

  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  gtk_file_dialog_open (dialog,
                        parent,
                        NULL,
                        (GAsyncReadyCallback)dialog_password_import_file_dialog_cb,
                        parent);
}

static void
import_passwords_using_option_id (const char *option_id,
                                  GtkWindow  *window)
{
  EphyPasswordManager *manager;

  manager = ephy_embed_shell_get_password_manager (ephy_embed_shell_get_default ());

  if (strcmp (option_id, IMPORT_FROM_CHROME_ID) == 0)
    ephy_password_import_from_chrome_async (manager, CHROME, dialog_password_import_cb, window);
  else if (strcmp (option_id, IMPORT_FROM_CHROMIUM_ID) == 0)
    ephy_password_import_from_chrome_async (manager, CHROMIUM, dialog_password_import_cb, window);
  else if (strcmp (option_id, IMPORT_FROM_CSV_ID) == 0)
    dialog_password_import_from_csv (window);
  else
    g_assert_not_reached ();
}

static void
dialog_passwords_import_cb (GtkWidget   *button,
                            AdwComboRow *combo_row)
{
  AdwDialog *dialog = ADW_DIALOG (gtk_widget_get_ancestor (button, ADW_TYPE_DIALOG));
  GtkWindow *window = GTK_WINDOW (gtk_widget_get_ancestor (button, GTK_TYPE_WINDOW));
  GtkStringObject *str_obj = adw_combo_row_get_selected_item (combo_row);
  const char *option_name = gtk_string_object_get_string (str_obj);
  int i = 0;
  const char *option_id = NULL;

  for (i = G_N_ELEMENTS (import_passwords_options) - 1; i >= 0; i--)
    if (g_strcmp0 (import_passwords_options[i].name, option_name) == 0) {
      option_id = import_passwords_options[i].id;
      break;
    }

  g_assert (option_id);
  import_passwords_using_option_id (option_id, window);

  adw_dialog_force_close (dialog);
}

static void
update_passwords_select_button_label (AdwComboRow *combo_row,
                                      GtkButton   *button)
{
  GtkStringObject *str_obj;
  const char *option_name;
  int i = 0;
  gboolean option_found = FALSE;

  g_assert (ADW_IS_COMBO_ROW (combo_row));
  g_assert (GTK_IS_BUTTON (button));

  str_obj = adw_combo_row_get_selected_item (combo_row);
  option_name = gtk_string_object_get_string (str_obj);
  for (i = G_N_ELEMENTS (import_passwords_options) - 1; i >= 0; i--)
    if (g_strcmp0 (import_passwords_options[i].name, option_name) == 0) {
      option_found = TRUE;
      break;
    }

  g_assert (option_found);
  if (import_passwords_options[i].type == IMPORT_TYPE_CHOOSE)
    gtk_button_set_label (button, _("_Select File"));
  else if (import_passwords_options[i].type == IMPORT_TYPE_IMPORT)
    gtk_button_set_label (button, _("I_mport"));
}

static void
passwords_row_selected_cb (AdwComboRow *combo_row,
                           GParamSpec  *pspec,
                           GtkButton   *button)
{
  update_passwords_select_button_label (combo_row, button);
}

static void
import_passwords_using_option_dialog (EphyWindow *window)
{
  GtkWidget *dialog;
  GtkWidget *header_bar;
  GtkWidget *toolbar_view;
  GtkWidget *cancel_button;
  GtkWidget *select_button;
  GtkWidget *prefs_group;
  GtkWidget *combo_row;
  GListModel *model;

  dialog = GTK_WIDGET (adw_dialog_new ());
  adw_dialog_set_title (ADW_DIALOG (dialog), _("Import Passwords"));

  header_bar = adw_header_bar_new ();
  adw_header_bar_set_show_start_title_buttons (ADW_HEADER_BAR (header_bar), FALSE);
  adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (header_bar), FALSE);

  toolbar_view = GTK_WIDGET (adw_toolbar_view_new ());
  adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar_view), header_bar);
  adw_dialog_set_child (ADW_DIALOG (dialog), toolbar_view);

  cancel_button = gtk_button_new_with_mnemonic (_("_Cancel"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (cancel_button), "window.close");
  adw_header_bar_pack_start (ADW_HEADER_BAR (header_bar), cancel_button);

  select_button = gtk_button_new_with_mnemonic (_("_Select File"));
  gtk_widget_add_css_class (select_button, "suggested-action");
  adw_dialog_set_default_widget (ADW_DIALOG (dialog), select_button);
  adw_header_bar_pack_end (ADW_HEADER_BAR (header_bar), select_button);

  prefs_group = adw_preferences_group_new ();
  gtk_widget_set_margin_top (prefs_group, 12);
  gtk_widget_set_margin_bottom (prefs_group, 12);
  gtk_widget_set_margin_start (prefs_group, 12);
  gtk_widget_set_margin_end (prefs_group, 12);
  adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar_view), prefs_group);

  model = create_passwords_model ();
  combo_row = adw_combo_row_new ();
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (combo_row), _("File Type"));
  adw_combo_row_set_model (ADW_COMBO_ROW (combo_row), model);
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (prefs_group), combo_row);

  g_signal_connect_object (ADW_COMBO_ROW (combo_row), "notify::selected",
                           G_CALLBACK (passwords_row_selected_cb),
                           GTK_BUTTON (select_button), 0);
  g_signal_connect (select_button, "clicked",
                    G_CALLBACK (dialog_passwords_import_cb),
                    ADW_COMBO_ROW (combo_row));

  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (window));
  update_passwords_select_button_label (ADW_COMBO_ROW (combo_row),
                                        GTK_BUTTON (select_button));
}

void
window_cmd_import_passwords (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  g_auto (GStrv) available_import_options = NULL;

  available_import_options = get_available_password_import_options ();

  if (g_strv_length (available_import_options) == 1)
    import_passwords_using_option_id (available_import_options[0], GTK_WINDOW (window));
  else
    import_passwords_using_option_dialog (window);
}

static void
passwords_export_cb (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  EphyPasswordManager *manager;
  g_autoptr (GtkWindow) window = NULL;
  g_autoptr (GError) error = NULL;
  gboolean exported;

  manager = EPHY_PASSWORD_MANAGER (source_object);
  window = GTK_WINDOW (user_data);

  exported = ephy_passwords_export_finish (manager, result, &error);

  show_import_export_result (window, FALSE, exported, error,
                             _("Passwords successfully exported!"));
}

static void
export_passwords_file_dialog_cb (GtkFileDialog *dialog,
                                 GAsyncResult  *result,
                                 GtkWindow     *parent)
{
  EphyPasswordManager *manager;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GError) error = NULL;

  file = gtk_file_dialog_save_finish (dialog, result, &error);
  manager = ephy_embed_shell_get_password_manager (ephy_embed_shell_get_default ());

  if (error) {
    if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
      g_warning ("Failed to open file: %s", error->message);

    g_object_unref (parent);
    return;
  }

  ephy_password_export (manager,
                        g_file_peek_path (file),
                        NULL,
                        passwords_export_cb,
                        g_steal_pointer (&parent));
}

void
window_cmd_export_passwords (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  GtkFileDialog *dialog;
  g_autoptr (GtkFileFilter) filter = NULL;
  g_autoptr (GListStore) filters = NULL;
  GtkWindow *window = user_data;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, _("Choose File"));

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*.csv");

  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  /* Translators: Only translate the part before ".csv" (e.g. "passwords") */
  gtk_file_dialog_set_initial_name (dialog, _("passwords.csv"));

  gtk_file_dialog_save (dialog,
                        window,
                        NULL,
                        (GAsyncReadyCallback)export_passwords_file_dialog_cb,
                        g_object_ref (window));
}

G_GNUC_END_IGNORE_DEPRECATIONS

void
window_cmd_show_history (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  EphyHistoryDialog *dialog;

  user_data = EPHY_WINDOW (user_data);

  dialog = EPHY_HISTORY_DIALOG (ephy_shell_get_history_dialog (ephy_shell_get_default ()));
  if (ephy_history_dialog_get_parent_window (dialog) != user_data &&
      ephy_history_dialog_get_parent_window (dialog)) {
    adw_dialog_force_close (ADW_DIALOG (dialog));
    dialog = EPHY_HISTORY_DIALOG (ephy_shell_get_history_dialog (ephy_shell_get_default ()));
  }

  adw_dialog_present (ADW_DIALOG (dialog), user_data);
  ephy_history_dialog_set_parent_window (dialog, user_data);
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
window_cmd_show_clear_data_view (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       user_data)
{
  EphyPrefsDialog *dialog;

  user_data = EPHY_WINDOW (user_data);

  dialog = EPHY_PREFS_DIALOG (ephy_shell_get_prefs_dialog (ephy_shell_get_default ()));
  if (ephy_prefs_dialog_get_parent_window (dialog) != user_data &&
      ephy_prefs_dialog_get_parent_window (dialog)) {
    adw_dialog_force_close (ADW_DIALOG (dialog));
    dialog = EPHY_PREFS_DIALOG (ephy_shell_get_prefs_dialog (ephy_shell_get_default ()));
  }

  adw_dialog_present (ADW_DIALOG (dialog), user_data);
  ephy_prefs_dialog_set_parent_window (dialog, user_data);

  ephy_prefs_dialog_show_clear_data_view (dialog);
}

void
window_cmd_show_preferences (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  EphyPrefsDialog *dialog;

  user_data = EPHY_WINDOW (user_data);

  dialog = EPHY_PREFS_DIALOG (ephy_shell_get_prefs_dialog (ephy_shell_get_default ()));
  if (ephy_prefs_dialog_get_parent_window (dialog) != user_data &&
      ephy_prefs_dialog_get_parent_window (dialog)) {
    adw_dialog_force_close (ADW_DIALOG (dialog));
    dialog = EPHY_PREFS_DIALOG (ephy_shell_get_prefs_dialog (ephy_shell_get_default ()));
  }

  adw_dialog_present (ADW_DIALOG (dialog), user_data);
  ephy_prefs_dialog_set_parent_window (dialog, user_data);
}

void
window_cmd_show_help (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /* https://gitlab.gnome.org/GNOME/gtk/-/issues/6135 */
  gtk_show_uri (GTK_WINDOW (user_data),
                "help:epiphany",
                GDK_CURRENT_TIME);
  G_GNUC_END_IGNORE_DEPRECATIONS
}

#define ABOUT_GROUP "About"

void
window_cmd_show_about (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  AdwAboutDialog *dialog;
  char *debug_info;
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

  debug_info = g_strdup_printf ("WebKitGTK %d.%d.%d" WEBKIT_REVISION "\n"
                                "%s\n"
                                "GTK %d.%d.%d\n"
                                "Libadwaita %d.%d.%d\n"
                                "Distributor: %s",
                                webkit_get_major_version (),
                                webkit_get_minor_version (),
                                webkit_get_micro_version (),
                                gst_version_string (),
                                gtk_get_major_version (),
                                gtk_get_minor_version (),
                                gtk_get_micro_version (),
                                adw_get_major_version (),
                                adw_get_minor_version (),
                                adw_get_micro_version (),
                                DISTRIBUTOR_NAME);

  length = g_strv_length (orig_authors) +
           g_strv_length (maintainers) +
           g_strv_length (past_maintainers) +
           g_strv_length (contributors);

  authors = g_malloc0 (sizeof (char *) * (length + 1));

  for (index = 0; index < g_strv_length (orig_authors); index++)
    authors[author_index++] = g_strdup (orig_authors[index]);

  for (index = 0; index < g_strv_length (maintainers); index++)
    authors[author_index++] = g_strdup (maintainers[index]);

  for (index = 0; index < g_strv_length (past_maintainers); index++)
    authors[author_index++] = g_strdup (past_maintainers[index]);

  for (index = 0; index < g_strv_length (contributors); index++) {
    authors[author_index++] = g_strdup (contributors[index]);
  }

  dialog = ADW_ABOUT_DIALOG (adw_about_dialog_new ());

  if (g_str_equal (PROFILE, "Canary"))
    adw_about_dialog_set_application_name (dialog, _("Epiphany Canary"));
  else {
#if !TECH_PREVIEW
    adw_about_dialog_set_application_name (dialog, _("Web"));
#else
    adw_about_dialog_set_application_name (dialog, _("Epiphany Technology Preview"));
#endif
  }

  adw_about_dialog_set_version (dialog, VERSION);
  adw_about_dialog_set_copyright (dialog,
                                  "Copyright © 2002–2004 Marco Pesenti Gritti\n"
                                  "Copyright © 2003–2023 The GNOME Web Developers");
  adw_about_dialog_set_developer_name (dialog, _("The GNOME Project"));

  adw_about_dialog_set_debug_info (dialog, debug_info);
  adw_about_dialog_set_license_type (dialog, GTK_LICENSE_GPL_3_0);
  adw_about_dialog_set_website (dialog, "https://apps.gnome.org/Epiphany");
  adw_about_dialog_set_application_icon (dialog, APPLICATION_ID);

  adw_about_dialog_set_developers (dialog, (const char **)authors);
  adw_about_dialog_set_designers (dialog, (const char **)artists);
  adw_about_dialog_set_documenters (dialog, (const char **)documenters);
  adw_about_dialog_set_translator_credits (dialog, _("translator-credits"));
  adw_about_dialog_set_issue_url (dialog, "https://gitlab.gnome.org/GNOME/epiphany/-/issues/");

  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (window));

  g_free (debug_info);
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
  EphyShell *shell = ephy_shell_get_default ();
  EphySession *session = ephy_shell_get_session (shell);
  EphyWindow *window = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (shell)));

  g_assert (session);

  if (!ephy_window_get_sidebar_shown (window))
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
  g_assert (embed);

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
  g_assert (embed);

  web_view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  if (strstr (g_action_get_name (G_ACTION (action)), "back")) {
    const char *back_uri;
    WebKitBackForwardList *history;
    WebKitBackForwardListItem *back_item;

    history = webkit_web_view_get_back_forward_list (web_view);
    back_item = webkit_back_forward_list_get_back_item (history);
    back_uri = webkit_back_forward_list_item_get_original_uri (back_item);

    embed = ephy_shell_new_tab (ephy_shell_get_default (),
                                EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (embed))),
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
                                EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (embed))),
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
  g_assert (embed);

  gtk_widget_grab_focus (GTK_WIDGET (embed));

  webkit_web_view_stop_loading (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
}

static void
check_tab_has_modified_forms_confirm_cb (AdwAlertDialog *dialog,
                                         const char     *response,
                                         EphyEmbed      *embed)
{
  WebKitWebView *view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed);

  if (!strcmp (response, "discard")) {
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
  GtkRoot *window = gtk_widget_get_root (GTK_WIDGET (view));
  AdwDialog *dialog;
  gboolean has_modified_forms;

  has_modified_forms = ephy_web_view_has_modified_forms_finish (view, result, NULL);
  if (has_modified_forms) {
    dialog = adw_alert_dialog_new (_("Reload Website?"),
                                   _("A form was modified and has not been submitted"));

    adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                    "cancel", _("_Cancel"),
                                    "discard", _("_Discard Form"),
                                    NULL);

    adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
                                              "discard",
                                              ADW_RESPONSE_DESTRUCTIVE);

    g_signal_connect (dialog, "response",
                      G_CALLBACK (check_tab_has_modified_forms_confirm_cb), embed);

    adw_dialog_present (dialog, GTK_WIDGET (window));

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
  g_assert (embed);

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
  g_assert (embed);

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

  action_group = ephy_window_get_action_group (EPHY_WINDOW (user_data), "toolbar");

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
  EphyPageMenuButton *button;

  header_bar = EPHY_HEADER_BAR (ephy_window_get_header_bar (window));
  button = EPHY_PAGE_MENU_BUTTON (ephy_header_bar_get_page_menu_button (header_bar));
  ephy_page_menu_button_popup (button);
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
  g_free (url);
}

static void
open_dialog_cb (GtkFileDialog *dialog,
                GAsyncResult  *result,
                EphyWindow    *window)
{
  g_autoptr (GFile) file = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *uri = NULL;
  g_autofree char *converted = NULL;

  file = gtk_file_dialog_open_finish (dialog, result, &error);

  if (error) {
    if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
      g_warning ("Failed to open file: %s", error->message);
    return;
  }

  uri = g_file_get_uri (file);
  if (uri) {
    converted = g_filename_to_utf8 (uri, -1, NULL, NULL, NULL);

    if (converted)
      ephy_window_load_url (window, converted);
  }
}

void
window_cmd_open (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  EphyWindow *window = user_data;
  GtkFileDialog *dialog;

  dialog = gtk_file_dialog_new ();

  ephy_file_dialog_add_filters (dialog);

  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (window),
                        NULL,
                        (GAsyncReadyCallback)open_dialog_cb,
                        window);
}

typedef struct {
  EphyWebView *view;
  const char *display_address;
  const char *url;
  char *manifest_url;
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
  EphyDownload *download_manifest;
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
    g_signal_handlers_disconnect_by_data (data->download, data);

    g_clear_object (&data->download);
  }

  g_cancellable_cancel (data->cancellable);
  g_object_unref (data->cancellable);
  g_object_unref (data->window);
  if (data->framed_pixbuf)
    g_object_unref (data->framed_pixbuf);
  if (data->icon_v)
    g_variant_unref (data->icon_v);
  g_free (data->manifest_url);
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
scaled_pixbuf_from_icon (GIcon *icon,
                         int    width,
                         int    height)
{
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  int w, h;
  GdkPixbuf *scaled;

  if (!icon)
    return NULL;

  if (GDK_IS_PIXBUF (icon))
    pixbuf = GDK_PIXBUF (g_object_ref (icon));
  else if (GDK_IS_TEXTURE (icon))
    pixbuf = ephy_texture_to_pixbuf (GDK_TEXTURE (icon), TRUE);
  else
    g_assert_not_reached ();

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

  return scaled;
}

static GdkPixbuf *
frame_pixbuf (GIcon   *icon,
              GdkRGBA *rgba,
              int      width,
              int      height)
{
  GdkPixbuf *framed;
  g_autoptr (GdkPixbuf) scaled = NULL;
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
  if (rgba)
    cairo_set_source_rgba (cr,
                           rgba->red,
                           rgba->green,
                           rgba->blue,
                           rgba->alpha);
  else
    cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.3);
  cairo_fill_preserve (cr);

  scaled = scaled_pixbuf_from_icon (icon, width, height);
  if (scaled) {
    int w = gdk_pixbuf_get_width (scaled);
    int h = gdk_pixbuf_get_height (scaled);

    gdk_cairo_set_source_pixbuf (cr, scaled,
                                 (width - w) / 2,
                                 (height - h) / 2);
    cairo_fill (cr);
  }

  framed = ephy_get_pixbuf_from_surface (surface, 0, 0, width, height);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return framed;
}

static void create_install_dialog_when_ready (EphyApplicationDialogData *data);

static void
set_image_from_favicon (EphyApplicationDialogData *data)
{
  g_autoptr (GIcon) icon = NULL;
  GdkTexture *icon_texture = webkit_web_view_get_favicon (WEBKIT_WEB_VIEW (data->view));

  icon = ephy_favicon_get_from_texture_scaled (icon_texture, 0, 0);
  if (icon) {
    data->framed_pixbuf = frame_pixbuf (icon, NULL, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE);
    g_assert (!data->icon_v);
    data->icon_v = g_icon_serialize (G_ICON (data->framed_pixbuf));
  } else {
    g_autoptr (GBytes) bytes = NULL;

    bytes = g_resources_lookup_data ("/org/gnome/epiphany/page-icons/web-app-icon-missing.svg",
                                     G_RESOURCE_LOOKUP_FLAGS_NONE,
                                     NULL);
    g_assert (bytes);

    icon = g_bytes_icon_new (bytes);
    data->icon_v = g_icon_serialize (icon);
  }

  g_assert (data->icon_v);
  create_install_dialog_when_ready (data);
}

static void
set_app_icon_from_filename (EphyApplicationDialogData *data,
                            const char                *filename)
{
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autoptr (GError) error = NULL;

  pixbuf = gdk_pixbuf_new_from_file_at_size (filename, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE, &error);
  if (!pixbuf)
    g_warning ("Failed to create pixbuf for %s: %s", filename, error->message);

  if (pixbuf) {
    data->framed_pixbuf = frame_pixbuf (G_ICON (pixbuf), &data->icon_rgba, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE);
    g_assert (!data->icon_v);
    data->icon_v = g_icon_serialize (G_ICON (data->framed_pixbuf));
    create_install_dialog_when_ready (data);
  }
  if (!data->icon_v) {
    g_warning ("Failed to get icon for web app %s, falling back to favicon", data->display_address);
    set_image_from_favicon (data);
  }
}

static void
download_finished_cb (WebKitDownload            *download,
                      EphyApplicationDialogData *data)
{
  set_app_icon_from_filename (data, webkit_download_get_destination (download));
}

static void
download_failed_cb (WebKitDownload            *download,
                    GError                    *error,
                    EphyApplicationDialogData *data)
{
  WebKitURIRequest *request = webkit_download_get_request (download);

  /* favicon.ico is our last resort fallback. It's not expected to necessarily exist. */
  if (!g_str_has_suffix (webkit_uri_request_get_uri (request), "favicon.ico"))
    g_warning ("Failed to download web app icon %s: %s", webkit_uri_request_get_uri (request), error->message);

  g_signal_handlers_disconnect_by_func (download, download_finished_cb, data);

  /* Something happened, default to a page snapshot. */
  set_image_from_favicon (data);
}

static void
download_icon_and_set_image (EphyApplicationDialogData *data)
{
  g_autofree char *destination = NULL;
  g_autofree char *filename = NULL;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  data->download = webkit_network_session_download_uri (ephy_embed_shell_get_network_session (shell),
                                                        data->icon_href);
  webkit_download_set_allow_overwrite (data->download, TRUE);

  /* We do not want this download to show up in the UI, so let's
   * set 'ephy-download-set' to make Epiphany think this is
   * already there. */
  /* FIXME: it's probably better to just do this in a clean way
   * instead of using this workaround. */
  g_object_set_data (G_OBJECT (data->download), "ephy-download-set", GINT_TO_POINTER (TRUE));

  filename = ephy_file_tmp_filename (".ephy-web-app-icon-XXXXXX", NULL);
  destination = g_build_filename (ephy_file_tmp_dir (), filename, NULL);
  webkit_download_set_destination (data->download, destination);

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
  if (data->icon_href)
    download_icon_and_set_image (data);
  else
    set_image_from_favicon (data);
}

static void
set_default_application_title (EphyApplicationDialogData *data,
                               char                      *title)
{
  if (!title || title[0] == '\0') {
    g_autoptr (GUri) uri = NULL;
    const char *host;

    uri = g_uri_parse (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (data->view)), G_URI_FLAGS_PARSE_RELAXED, NULL);
    host = g_uri_get_host (uri);

    if (host && host[0] != '\0') {
      if (g_str_has_prefix (host, "www."))
        title = g_strdup (host + strlen ("www."));
      else
        title = g_strdup (host);
    }
  }

  if (!title || title[0] == '\0') {
    g_clear_pointer (&title, g_free);
    title = g_strdup (webkit_web_view_get_title (WEBKIT_WEB_VIEW (data->view)));
  }

  if (!title || title[0] == '\0') {
    g_clear_pointer (&title, g_free);
    title = g_strdup (_("New Web App"));
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
send_web_app_install_notification (EphyApplicationDialogData *data,
                                   GError                    *error)
{
  g_autofree char *message = NULL;
  GNotification *notification;

  if (!error) {
    message = g_strdup_printf (_("The application “%s” is ready to be used"),
                               data->chosen_name);
  } else {
    message = g_strdup_printf (_("The application “%s” could not be created: %s"),
                               data->chosen_name, error->message);
    g_warning ("The application “%s” could not be created: %s", data->chosen_name, error->message);
  }

  notification = g_notification_new (message);

  if (data->framed_pixbuf)
    g_notification_set_icon (notification, G_ICON (data->framed_pixbuf));

  if (!error) {
    /* Translators: Desktop notification when a new web app is created. */
    g_notification_add_button_with_target (notification, _("Launch"), "app.launch-app", "s", data->app_id);
    g_notification_set_default_action_and_target (notification, "app.launch-app", "s", data->app_id);
  }

  g_notification_set_priority (notification, G_NOTIFICATION_PRIORITY_LOW);

  g_application_send_notification (G_APPLICATION (g_application_get_default ()), data->chosen_name, notification);
}

static void
save_as_application_proceed (EphyApplicationDialogData *data)
{
  gboolean success;
  g_autoptr (GError) error = NULL;

  /* Create Web Application, including a new profile and .desktop file. */
  success = ephy_web_application_create (data->app_id,
                                         data->url,
                                         data->token,
                                         data->webapp_options,
                                         &error);

  send_web_app_install_notification (data, error);

  if (success)
    ephy_focus_desktop_app (data->app_id);

  ephy_application_dialog_data_free (data);
}

static void
dialog_save_as_application_confirmation_cb (AdwAlertDialog            *dialog,
                                            const char                *response,
                                            EphyApplicationDialogData *data)
{
  if (!strcmp (response, "replace")) {
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
  AdwDialog *confirmation_dialog;
  g_autoptr (GError) error = NULL;

  ret = xdp_portal_dynamic_launcher_prepare_install_finish (portal, result, &error);
  if (!ret) {
    /* This might just mean the user canceled the operation */
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      send_web_app_install_notification (data, error);
      g_warning ("Failed to install web app, PrepareInstall() failed: %s", error->message);
    }
    ephy_application_dialog_data_free (data);
    return;
  }

  chosen_name_v = g_variant_lookup_value (ret, "name", G_VARIANT_TYPE_STRING);
  token_v = g_variant_lookup_value (ret, "token", G_VARIANT_TYPE_STRING);
  if (!chosen_name_v || !token_v) {
    send_web_app_install_notification (data, error);
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
    adw_alert_dialog_new (_("Replace Existing Web App?"), NULL);

  adw_alert_dialog_format_body (ADW_ALERT_DIALOG (confirmation_dialog),
                                _("An application named “%s” already exists, replacing it will overwrite it"),
                                data->chosen_name);

  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (confirmation_dialog),
                                  "cancel", _("_Cancel"),
                                  "replace", _("_Replace"),
                                  NULL);

  g_signal_connect (confirmation_dialog, "response",
                    G_CALLBACK (dialog_save_as_application_confirmation_cb), data);
  adw_dialog_present (confirmation_dialog, GTK_WIDGET (data->window));
}

static void
create_install_dialog_when_ready (EphyApplicationDialogData *data)
{
  XdpPortal *portal;
  g_autoptr (XdpParent) parent = NULL;

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

static void
start_fallback (EphyApplicationDialogData *data)
{
  LOG ("No webmanifest, using old scraping");
  ephy_web_view_get_best_web_app_icon (data->view, data->cancellable, fill_default_application_image_cb, data);
  ephy_web_view_get_web_app_title (data->view, data->cancellable, fill_default_application_title_cb, data);
  ephy_web_view_get_web_app_mobile_capable (data->view, data->cancellable, fill_mobile_capable_cb, data);
}

static char *
icon_url_from_relative_url (const char *relative_url,
                            const char *manifest_url)
{
  g_autofree char *url = NULL;
  g_autoptr (GError) error = NULL;

  url = g_uri_resolve_relative (manifest_url, relative_url, G_URI_FLAGS_NONE, &error);
  if (!url) {
    g_warning ("Failed to resolve URL %s relative to %s: %s", relative_url, manifest_url, error->message);
    return NULL;
  }

  return g_steal_pointer (&url);
}

static void
download_manifest_finished_cb (EphyDownload              *download,
                               EphyApplicationDialogData *data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (JsonParser) parser = json_parser_new ();
  JsonNode *root;
  JsonObject *manifest_object;
  JsonArray *icons;
  JsonObject *icon;
  g_autofree char *filename = NULL;
  const char *title = NULL;
  const char *str;
  const char *display;
  gint pos = 0;
  gint max_width = 0;
  g_autofree char *uri = NULL;

  filename = g_filename_from_uri (ephy_download_get_destination (download), NULL, NULL);
  json_parser_load_from_file (parser, ephy_download_get_destination (download), &error);
  if (error) {
    g_warning ("Unable to parse manifest %s: %s", filename, error->message);
    start_fallback (data);
    return;
  }

  root = json_parser_get_root (parser);
  manifest_object = json_node_get_object (root);

  icons = ephy_json_object_get_array (manifest_object, "icons");
  if (!icons) {
    start_fallback (data);
    return;
  }

  for (guint i = 0; i < json_array_get_length (icons); i++) {
    g_auto (GStrv) size = NULL;
    const char *sizes;
    gint width;

    icon = ephy_json_array_get_object (icons, i);

    if (ephy_json_object_get_string (icon, "purpose")) {
      LOG ("Skipping icon as purpose is set..");
      continue;
    }

    sizes = ephy_json_object_get_string (icon, "sizes");
    if (!sizes)
      continue;

    size = g_strsplit (sizes, "x", 2);
    if (!size)
      continue;

    width = strtol (size[0], NULL, 10);
    if (max_width < width) {
      pos = i;
      max_width = width;
    }
  }

  icon = ephy_json_array_get_object (icons, pos);
  if (!icon) {
    start_fallback (data);
    return;
  }

  str = ephy_json_object_get_string (icon, "src");
  if (!str) {
    start_fallback (data);
    return;
  }

  if (ephy_embed_utils_address_has_web_scheme (str))
    uri = g_strdup (str);
  else
    uri = icon_url_from_relative_url (str, data->manifest_url);
  if (!uri) {
    start_fallback (data);
    return;
  }

  display = ephy_json_object_get_string (manifest_object, "display");
  if (g_strcmp0 (display, "standalone") == 0 || g_strcmp0 (display, "fullscreen") == 0)
    data->webapp_options = EPHY_WEB_APPLICATION_MOBILE_CAPABLE;
  else
    data->webapp_options = EPHY_WEB_APPLICATION_NONE;

  data->webapp_options_set = TRUE;

  data->icon_href = g_steal_pointer (&uri);

  download_icon_and_set_image (data);

  if (json_object_has_member (manifest_object, "short_name"))
    title = json_object_get_string_member (manifest_object, "short_name");
  else if (json_object_has_member (manifest_object, "name"))
    title = json_object_get_string_member (manifest_object, "name");

  if (title)
    set_default_application_title (data, g_strdup (title));
  else
    ephy_web_view_get_web_app_title (data->view, data->cancellable, fill_default_application_title_cb, data);
}

static void
download_manifest_failed_cb (EphyDownload              *download,
                             GError                    *error,
                             EphyApplicationDialogData *data)
{
  WebKitDownload *webkit_download = ephy_download_get_webkit_download (download);
  WebKitURIRequest *request = webkit_download_get_request (webkit_download);

  g_warning ("Could not download manifest from %s", webkit_uri_request_get_uri (request));
  start_fallback (data);
}

static void
download_and_use_manifest (EphyApplicationDialogData *data,
                           const char                *manifest_url)
{
  g_autofree char *destination = NULL;
  g_autofree char *tmp_filename = NULL;

  LOG ("%s: manifest url %s", __FUNCTION__, manifest_url);
  data->manifest_url = g_strdup (manifest_url);
  data->download_manifest = ephy_download_new_for_uri_internal (manifest_url);
  webkit_download_set_allow_overwrite (ephy_download_get_webkit_download (data->download_manifest), TRUE);
  tmp_filename = ephy_file_tmp_filename (".ephy-download-XXXXXX", NULL);
  destination = g_build_filename (ephy_file_tmp_dir (), tmp_filename, NULL);
  ephy_download_set_destination (data->download_manifest, destination);

  g_signal_connect (data->download_manifest, "completed", G_CALLBACK (download_manifest_finished_cb), data);
  g_signal_connect (data->download_manifest, "error", G_CALLBACK (download_manifest_failed_cb), data);
}

static void
got_manifest_url_cb (GObject      *source,
                     GAsyncResult *async_result,
                     gpointer      user_data)
{
  EphyApplicationDialogData *data = user_data;
  g_autofree char *manifest_url = NULL;
  g_autoptr (GError) error = NULL;

  manifest_url = ephy_web_view_get_web_app_manifest_url_finish (EPHY_WEB_VIEW (source), async_result, &error);
  if (error || !manifest_url) {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;

    /* No web manifest, fallback to old scraping */
    start_fallback (data);
    return;
  }

  download_and_use_manifest (data, manifest_url);
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
  g_assert (embed);

  data = g_new0 (EphyApplicationDialogData, 1);
  data->window = g_object_ref (window);
  data->view = EPHY_WEB_VIEW (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed));
  data->display_address = ephy_web_view_get_display_address (data->view);
  data->url = webkit_web_view_get_uri (WEBKIT_WEB_VIEW (data->view));
  data->cancellable = g_cancellable_new ();

  ephy_web_view_get_web_app_manifest_url (data->view, data->cancellable, got_manifest_url_cb, data);
}

static char *
get_suggested_filename (EphyEmbed  *embed,
                        const char *file_extension)
{
  EphyWebView *view;
  const char *suggested_filename;
  const char *mimetype;
  const char *page_title;
  WebKitURIResponse *response;
  WebKitWebResource *web_resource;
  g_autoptr (GUri) uri = NULL;
  g_autofree char *filename = NULL;

  view = ephy_embed_get_web_view (embed);
  web_resource = webkit_web_view_get_main_resource (WEBKIT_WEB_VIEW (view));
  response = webkit_web_resource_get_response (web_resource);
  if (!response)
    return g_strdup (file_extension);

  mimetype = webkit_uri_response_get_mime_type (response);
  uri = g_uri_parse (webkit_web_resource_get_uri (web_resource), G_URI_FLAGS_PARSE_RELAXED | G_URI_FLAGS_SCHEME_NORMALIZE, NULL);
  page_title = ephy_embed_get_title (embed);
  filename = g_strconcat (page_title, file_extension, NULL);

  if (g_ascii_strncasecmp (mimetype, "text/html", 9) == 0 && g_strcmp0 (g_uri_get_scheme (uri), EPHY_VIEW_SOURCE_SCHEME) != 0) {
    /* Web Title will be used as suggested filename */
    return g_steal_pointer (&filename);
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

  return suggested_filename ? g_strdup (suggested_filename) : g_steal_pointer (&filename);
}


static void
take_snapshot_full_cb (GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  g_autoptr (WebKitWebView) view = WEBKIT_WEB_VIEW (source);
  g_autoptr (GError) error = NULL;
  g_autoptr (GdkTexture) texture = NULL;
  g_autofree char *file = user_data;
  EphyShell *shell = ephy_shell_get_default ();
  EphyWindow *window = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (shell)));

  if (!file)
    return;

  texture = webkit_web_view_get_snapshot_finish (view, res, &error);
  if (error) {
    g_warning ("Failed to take snapshot: %s", error->message);
    return;
  }

  gdk_texture_save_to_png (texture, file);

  ephy_window_show_toast (window, _("Screenshot finished"));
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

typedef enum {
  SAVE_DIALOG_MODE_SAVE_AS,
  SAVE_DIALOG_MODE_SCREENSHOT
} SaveDialogMode;

typedef struct {
  EphyEmbed *embed;
  char *suggested_filename;
  SaveDialogMode mode;
} SaveDialogAsyncData;

static SaveDialogAsyncData *
save_dialog_async_data_new (EphyEmbed      *embed,
                            const char     *suggested_filename,
                            SaveDialogMode  mode)
{
  SaveDialogAsyncData *data = g_new (SaveDialogAsyncData, 1);
  data->embed = g_object_ref (embed);
  data->suggested_filename = g_strdup (suggested_filename);
  data->mode = mode;
  return data;
}

static void
save_dialog_async_data_free (SaveDialogAsyncData *data)
{
  g_object_unref (data->embed);
  g_free (data->suggested_filename);
  g_free (data);
}

static void
save_dialog_cb (GtkFileDialog       *dialog,
                GAsyncResult        *result,
                SaveDialogAsyncData *data)
{
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFile) current_file = NULL;
  g_autofree char *uri = NULL;
  g_autofree char *converted = NULL;
  g_autofree char *current_path = NULL;

  file = gtk_file_dialog_save_finish (dialog, result, NULL);

  if (!file)
    goto out;

  uri = g_file_get_uri (file);
  if (uri) {
    converted = g_filename_to_utf8 (uri, -1, NULL, NULL, NULL);

    if (converted) {
      /* Easter egg: allow power users to take a screenshot of anything that's
       * not a PNG by changing the suffix to .png.
       */
      if (data->mode == SAVE_DIALOG_MODE_SCREENSHOT || (g_str_has_suffix (converted, ".png") && !g_str_has_suffix (data->suggested_filename, ".png"))) {
        take_snapshot (data->embed, converted);
      } else {
        EphyWebView *web_view = ephy_embed_get_web_view (data->embed);
        ephy_web_view_save (web_view, converted);
      }
    }
  }

  current_file = g_file_get_parent (file);
  current_path = g_file_get_path (current_file);
  g_settings_set_string (EPHY_SETTINGS_WEB,
                         EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY,
                         current_path);

out:
  save_dialog_async_data_free (data);
}

void
window_cmd_save_as (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyEmbed *embed;
  GtkFileDialog *dialog;
  g_autoptr (GtkFileFilter) html_filter = NULL;
  g_autoptr (GtkFileFilter) mthtml_filter = NULL;
  g_autoptr (GListStore) filters = NULL;
  g_autofree char *suggested_filename = NULL;
  const char *last_directory_path;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (embed);

  dialog = gtk_file_dialog_new ();

  last_directory_path = g_settings_get_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY);

  if (last_directory_path && last_directory_path[0]) {
    g_autoptr (GFile) last_directory = NULL;

    last_directory = g_file_new_for_path (last_directory_path);
    gtk_file_dialog_set_initial_folder (dialog, last_directory);
  }

  html_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (html_filter, _("HTML"));
  gtk_file_filter_add_pattern (html_filter, "*.html");

  mthtml_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (mthtml_filter, _("MHTML"));
  gtk_file_filter_add_pattern (mthtml_filter, "*.mhtml");

  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, html_filter);
  g_list_store_append (filters, mthtml_filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  suggested_filename = ephy_sanitize_filename (get_suggested_filename (embed, ".mhtml"));
  gtk_file_dialog_set_initial_name (dialog, suggested_filename);

  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (window),
                        NULL,
                        (GAsyncReadyCallback)save_dialog_cb,
                        save_dialog_async_data_new (embed, suggested_filename, SAVE_DIALOG_MODE_SAVE_AS));
}

void
window_cmd_screenshot (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyEmbed *embed;
  GtkFileDialog *dialog;
  g_autoptr (GtkFileFilter) filter = NULL;
  g_autoptr (GListStore) filters = NULL;
  g_autofree char *suggested_filename = NULL;
  const char *last_directory_path;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (embed);

  dialog = gtk_file_dialog_new ();

  last_directory_path = g_settings_get_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY);

  if (last_directory_path && last_directory_path[0]) {
    g_autoptr (GFile) last_directory = NULL;

    last_directory = g_file_new_for_path (last_directory_path);
    gtk_file_dialog_set_initial_folder (dialog, last_directory);
  }

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("PNG"));
  gtk_file_filter_add_pattern (filter, "*.png");

  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  suggested_filename = ephy_sanitize_filename (get_suggested_filename (embed, ".png"));
  gtk_file_dialog_set_initial_name (dialog, suggested_filename);

  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (window),
                        NULL,
                        (GAsyncReadyCallback)save_dialog_cb,
                        save_dialog_async_data_new (embed, suggested_filename, SAVE_DIALOG_MODE_SCREENSHOT));
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
    gtk_widget_activate_action (widget, "clipboard.cut", NULL);
  } else {
    EphyEmbed *embed;
    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));

    if (!embed)
      return;

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
    gtk_widget_activate_action (widget, "clipboard.copy", NULL);
  } else {
    EphyEmbed *embed;

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));

    if (!embed)
      return;

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
    gtk_widget_activate_action (widget, "clipboard.paste", NULL);
  } else {
    EphyEmbed *embed;

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));

    if (!embed)
      return;

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
    gtk_widget_activate_action (widget, "clipboard.paste", NULL);
  } else {
    EphyEmbed *embed;

    embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));

    if (!embed)
      return;

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

  if (GTK_IS_EDITABLE (widget))
    gtk_editable_delete_text (GTK_EDITABLE (widget), 0, -1);
}

static void
dismiss_page_popover (EphyWindow *window)
{
  EphyHeaderBar *header_bar;
  EphyPageMenuButton *button;

  header_bar = EPHY_HEADER_BAR (ephy_window_get_header_bar (window));
  button = EPHY_PAGE_MENU_BUTTON (ephy_header_bar_get_page_menu_button (header_bar));
  ephy_page_menu_button_popdown (button);
}

void
window_cmd_print (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyEmbed *embed;
  EphyWebView *view;

  dismiss_page_popover (window);

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
  EphyWindow *window = EPHY_WINDOW (user_data);
  EphyFindToolbar *toolbar;

  dismiss_page_popover (window);

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
  GtkWidget *dialog;

  dialog = ephy_bookmark_properties_new_for_window (window);
  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (window));
}

void
window_cmd_bookmarks (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  EphyWindow *window = EPHY_WINDOW (user_data);

  ephy_window_toggle_bookmarks (window);
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
  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (window));
}

void
window_cmd_page_source (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyEmbed *embed;
  EphyEmbed *new_embed;
  g_autofree char *source_uri = NULL;
  const char *address;

  embed = ephy_embed_container_get_active_child
            (EPHY_EMBED_CONTAINER (window));
  g_assert (embed);

  address = ephy_web_view_get_address (ephy_embed_get_web_view (embed));

  /* Abort if we're already in view source mode */
  if (strstr (address, EPHY_VIEW_SOURCE_SCHEME) == address)
    return;

  source_uri = g_strdup_printf ("%s:%s", EPHY_VIEW_SOURCE_SCHEME, address);

  new_embed = ephy_shell_new_tab
                (ephy_shell_get_default (),
                EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (embed))),
                embed,
                EPHY_NEW_TAB_JUMP | EPHY_NEW_TAB_APPEND_AFTER);

  webkit_web_view_load_uri (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (new_embed), source_uri);
  gtk_widget_grab_focus (GTK_WIDGET (new_embed));
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
  g_assert (embed);

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

    webkit_web_view_execute_editing_command (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed), "SelectAll");
  }
}

void
window_cmd_go_location (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  ephy_window_focus_location_entry (user_data);
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
window_cmd_go_tabs_view (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       user_data)
{
  ephy_window_toggle_tab_overview (EPHY_WINDOW (user_data));
}

static void
enable_browse_with_caret_state_cb (AdwAlertDialog *dialog,
                                   const char     *response,
                                   EphyWindow     *window)
{
  GActionGroup *action_group = ephy_window_get_action_group (window, "win");
  GAction *action;

  action = g_action_map_lookup_action (G_ACTION_MAP (action_group),
                                       "browse-with-caret");

  if (strcmp (response, "enable")) {
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
  GtkWidget *window = GTK_WIDGET (user_data);
  gboolean active;

  active = g_variant_get_boolean (state);

  if (active) {
    AdwDialog *dialog;

    dialog = adw_alert_dialog_new (_("Enable Caret Browsing Mode?"),
                                   _("Pressing F7 turns caret browsing on or off. This feature "
                                     "places a moveable cursor in web pages, allowing you to move "
                                     "around with your keyboard. Do you want to enable caret browsing?"));
    adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                    "cancel", _("_Cancel"),
                                    "enable", _("_Enable"),
                                    NULL);

    g_signal_connect (dialog, "response",
                      G_CALLBACK (enable_browse_with_caret_state_cb), window);

    adw_dialog_present (dialog, window);

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
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  gboolean active;
  gboolean fullscreen_lockdown;

  fullscreen_lockdown = g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN, EPHY_PREFS_LOCKDOWN_FULLSCREEN) ||
                        ephy_embed_shell_get_mode (EPHY_EMBED_SHELL (shell)) == EPHY_EMBED_SHELL_MODE_KIOSK;
  if (fullscreen_lockdown)
    return;

  dismiss_page_popover (window);

  active = g_variant_get_boolean (state);

  /* This is performed only here because we don't want it occurring when a window
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
  gboolean fullscreen_lockdown;

  fullscreen_lockdown = g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN, EPHY_PREFS_LOCKDOWN_FULLSCREEN) ||
                        ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_KIOSK;
  if (fullscreen_lockdown)
    return;

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
  g_assert (embed);

  web_view = ephy_embed_get_web_view (embed);
  active = ephy_web_view_get_reader_mode_state (web_view);

  if (!ephy_web_view_is_reader_mode_available (web_view) && !active)
    return;

  ephy_web_view_toggle_reader_mode (web_view, !active);

  g_simple_action_set_state (G_SIMPLE_ACTION (action),
                             g_variant_new_boolean (!active));
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
  g_assert (embed);

  embed = ephy_shell_new_tab (ephy_shell_get_default (),
                              EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (embed))),
                              NULL,
                              0);

  web_view = ephy_embed_get_web_view (embed);
  ephy_web_view_load_homepage (web_view);

  ephy_embed_container_set_active_child (EPHY_EMBED_CONTAINER (window), embed);

  gtk_widget_grab_focus (GTK_WIDGET (embed));
}

static void
clipboard_text_received_cb (GdkClipboard *clipboard,
                            GAsyncResult *res,
                            EphyWindow   *window)
{
  EphyEmbed *embed;
  EphyWebView *web_view;
  g_autoptr (GError) error = NULL;
  g_autofree char *text = NULL;
  g_autofree char *normalized = NULL;

  text = gdk_clipboard_read_text_finish (clipboard, res, &error);
  if (error) {
    g_warning ("Failed to the URL from clipboard: %s", error->message);
    g_object_unref (window);
    return;
  }

  normalized = ephy_embed_utils_normalize_or_autosearch_address (text);

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
  g_assert (embed);

  embed = ephy_shell_new_tab (ephy_shell_get_default (),
                              EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (embed))),
                              NULL,
                              0);

  web_view = ephy_embed_get_web_view (embed);
  ephy_web_view_load_url (web_view, normalized);

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
  GdkClipboard *clipboard;

  clipboard = gdk_display_get_primary_clipboard (gtk_widget_get_display (GTK_WIDGET (ephy_window)));
  gdk_clipboard_read_text_async (clipboard,
                                 NULL,
                                 (GAsyncReadyCallback)clipboard_text_received_cb,
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
  g_assert (embed);

  view = ephy_embed_get_web_view (embed);

  mute = !webkit_web_view_get_is_muted (WEBKIT_WEB_VIEW (view));

  webkit_web_view_set_is_muted (WEBKIT_WEB_VIEW (view), mute);

  g_simple_action_set_state (action, g_variant_new_boolean (mute));
}

void
window_cmd_switch_new_tab (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  EphyWindow *window = user_data;

  ephy_window_switch_to_new_tab (window);
}

void
window_cmd_privacy_report (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  EphyWindow *window = user_data;

  ephy_privacy_report_show (window);
}

void
window_cmd_passwords (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
  EphyWindow *window = user_data;

  ephy_passwords_show (window);
}

void
window_cmd_close_all_tabs (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  EphyWindow *window = user_data;

  ephy_tab_view_close_all (ephy_window_get_tab_view (window));
}

void
window_cmd_security_and_permissions (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  EphyWindow *window = user_data;
  EphyWebView *view;
  const char *address;
  GTlsCertificate *certificate;
  GTlsCertificateFlags tls_errors;
  EphySecurityLevel security_level;
  GtkWidget *security_dialog;

  view = ephy_embed_get_web_view (ephy_window_get_active_embed (window));
  ephy_web_view_get_security_level (view, &security_level, &address, &certificate, &tls_errors);

  security_dialog = ephy_security_dialog_new (address, certificate, tls_errors, security_level);

  adw_dialog_present (ADW_DIALOG (security_dialog), GTK_WIDGET (window));
}
