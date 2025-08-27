/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2010, 2017 Igalia S.L.
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
#include "prefs-general-page.h"

#include "ephy-embed-shell.h"
#include "ephy-file-helpers.h"
#include "ephy-flatpak-utils.h"
#include "ephy-lang-row.h"
#include "ephy-langs.h"
#include "ephy-settings.h"
#include "ephy-search-engine-listbox.h"
#include "ephy-shell.h"
#include "ephy-web-app-utils.h"

#include "gnome-languages.h"
#include <glib/gi18n.h>

enum {
  COL_LANG_NAME,
  COL_LANG_CODE
};

enum {
  WEBAPP_ADDITIONAL_URLS_ROW_ACTIVATED,
  LAST_SIGNAL
};

struct _PrefsGeneralPage {
  AdwPreferencesPage parent_instance;

  /* Web Application */
  guint webapp_save_id;
  GtkWidget *webapp_box;
  GtkWidget *webapp_icon;
  GtkWidget *webapp_icon_row;
  GtkWidget *webapp_url_row;
  GtkWidget *webapp_title_row;

  /* Web Content */
  GtkWidget *adblock_allow_row;
  GtkWidget *popups_allow_row;
  GtkWidget *cookie_banner_allow_row;

  /* Homepage */
  GtkWidget *homepage_box;
  GtkWidget *new_tab_homepage_radiobutton;
  GtkWidget *blank_homepage_radiobutton;
  GtkWidget *custom_homepage_radiobutton;
  GtkWidget *custom_homepage_entry;

  /* Downloads */
  GtkWidget *download_box;
  GtkWidget *ask_on_download_row;
  GtkWidget *download_folder_row;
  GtkWidget *download_folder_label;

  /* Search Engines */
  GtkWidget *default_search_engines;
  GtkWidget *standard_search_engine;
  GtkWidget *incognito_search_engine;
  GtkWidget *search_engine_group;
  EphySearchEngineListBox *search_engine_list_box;

  /* Session */
  GtkWidget *session_box;
  GtkWidget *start_in_incognito_mode_row;
  GtkWidget *restore_session_row;

  /* Browsing */
  GtkWidget *browsing_box;
  GtkWidget *enable_mouse_gesture_row;
  GtkWidget *enable_switch_to_new_tab;
  GtkWidget *enable_navigation_gestures_row;

  /* Languages */
  AdwPreferencesGroup *lang_group;
  GtkWidget *lang_listbox;
  GtkWidget *enable_spell_checking_row;

  /* Developer */
  AdwPreferencesGroup *dev_group;
  GtkWidget *show_developer_actions_row;
  GtkWidget *always_show_full_url_row;

  GtkWindow *add_lang_dialog;
  GtkTreeView *add_lang_treeview;

  GCancellable *cancellable;
};

static guint signals[LAST_SIGNAL];

G_DEFINE_FINAL_TYPE (PrefsGeneralPage, prefs_general_page, ADW_TYPE_PREFERENCES_PAGE)

static void
prefs_general_page_dispose (GObject *object)
{
  PrefsGeneralPage *general_page = EPHY_PREFS_GENERAL_PAGE (object);

  if (general_page->cancellable) {
    g_cancellable_cancel (general_page->cancellable);
    g_clear_object (&general_page->cancellable);
  }

  if (general_page->add_lang_dialog) {
    GtkWindow **add_lang_dialog = &general_page->add_lang_dialog;

    g_object_remove_weak_pointer (G_OBJECT (general_page->add_lang_dialog),
                                  (gpointer *)add_lang_dialog);
    g_object_unref (general_page->add_lang_dialog);
  }

  G_OBJECT_CLASS (prefs_general_page_parent_class)->dispose (object);
}

static int
get_list_box_length (GtkListBox *list_box)
{
  int i = -1;

  while (gtk_list_box_get_row_at_index (list_box, ++i));

  return i;
}

static void
language_editor_update_rows_movable (PrefsGeneralPage *general_page,
                                     GtkListBox       *list_box)
{
  GtkListBoxRow *row;
  int n_rows = get_list_box_length (list_box) - 1;
  int i = 0;

  while ((row = gtk_list_box_get_row_at_index (list_box, i++))) {
    if (!EPHY_IS_LANG_ROW (row))
      continue;

    gtk_widget_action_set_enabled (GTK_WIDGET (row), "row.move-up", i > 0);
    gtk_widget_action_set_enabled (GTK_WIDGET (row), "row.move-down", i < (n_rows - 1));
    ephy_lang_row_set_movable (EPHY_LANG_ROW (row), n_rows > 1);
  }
}

static void
language_editor_update_pref (PrefsGeneralPage *general_page)
{
  GVariantBuilder builder;
  GtkListBox *lang_listbox = GTK_LIST_BOX (general_page->lang_listbox);
  int len = get_list_box_length (lang_listbox);
  int index;

  if (get_list_box_length (lang_listbox) <= 1) {
    g_settings_set (EPHY_SETTINGS_WEB,
                    EPHY_PREFS_WEB_LANGUAGE,
                    "as", NULL);
    return;
  }

  g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);

  for (index = 0; index < len - 1; index++) {
    GtkListBoxRow *row = gtk_list_box_get_row_at_index (lang_listbox, index);
    const char *code = ephy_lang_row_get_code (EPHY_LANG_ROW (row));

    if (code)
      g_variant_builder_add (&builder, "s", code);
  }

  g_settings_set (EPHY_SETTINGS_WEB,
                  EPHY_PREFS_WEB_LANGUAGE,
                  "as", &builder);
}

static GtkWindow *setup_add_language_dialog (PrefsGeneralPage *general_page);

static void
language_editor_add_activated (GtkWidget *listbox,
                               GtkWidget *activated_row,
                               GtkWidget *add_row)
{
  PrefsGeneralPage *general_page;

  if (add_row != activated_row)
    return;

  general_page = EPHY_PREFS_GENERAL_PAGE (gtk_widget_get_ancestor (listbox, EPHY_TYPE_PREFS_GENERAL_PAGE));

  if (!general_page->add_lang_dialog) {
    GtkWindow *window;
    GtkWindow **add_lang_dialog;

    window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (general_page)));
    general_page->add_lang_dialog = setup_add_language_dialog (general_page);
    gtk_window_set_transient_for (GTK_WINDOW (general_page->add_lang_dialog), window);

    add_lang_dialog = &general_page->add_lang_dialog;

    g_object_add_weak_pointer
      (G_OBJECT (general_page->add_lang_dialog),
      (gpointer *)add_lang_dialog);
  }

  gtk_window_present (GTK_WINDOW (general_page->add_lang_dialog));
}

static void
language_editor_add_function_buttons (PrefsGeneralPage *general_page)
{
  GtkWidget *row = adw_button_row_new ();

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), _("Add Language"));
  adw_button_row_set_start_icon_name (ADW_BUTTON_ROW (row), "list-add-symbolic");

  gtk_list_box_append (GTK_LIST_BOX (general_page->lang_listbox), row);

  g_signal_connect_object (general_page->lang_listbox, "row-activated",
                           G_CALLBACK (language_editor_add_activated), row, 0);
}

static void
language_editor_update_state (PrefsGeneralPage *general_page)
{
  GtkListBox *lang_listbox = GTK_LIST_BOX (general_page->lang_listbox);
  int length = get_list_box_length (lang_listbox);
  int index;

  /* If there's only one language row in the list we want to make its
   * remove button insensitive */
  if (length == 2) {
    GtkListBoxRow *row = gtk_list_box_get_row_at_index (lang_listbox, 0);

    ephy_lang_row_set_delete_sensitive (EPHY_LANG_ROW (row), FALSE);
    return;
  }

  for (index = 0; index < length - 1; index++) {
    GtkListBoxRow *row = gtk_list_box_get_row_at_index (lang_listbox, index);

    ephy_lang_row_set_delete_sensitive (EPHY_LANG_ROW (row), TRUE);
  }
}

static void
language_editor_delete_button_clicked_cb (EphyLangRow      *row,
                                          PrefsGeneralPage *general_page)
{
  gtk_list_box_remove (GTK_LIST_BOX (general_page->lang_listbox), GTK_WIDGET (row));
  language_editor_update_pref (general_page);
  language_editor_update_state (general_page);
  language_editor_update_rows_movable (general_page, GTK_LIST_BOX (general_page->lang_listbox));
}

static void
language_editor_move_row_cb (EphyLangRow      *row,
                             EphyLangRow      *dest_row,
                             PrefsGeneralPage *general_page)
{
  int index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (dest_row));

  g_object_ref (row);
  gtk_list_box_remove (GTK_LIST_BOX (general_page->lang_listbox),
                       GTK_WIDGET (row));
  gtk_list_box_insert (GTK_LIST_BOX (general_page->lang_listbox),
                       GTK_WIDGET (row), index);
  g_object_unref (row);

  language_editor_update_pref (general_page);
}

static void
language_editor_add (PrefsGeneralPage *general_page,
                     const char       *code,
                     const char       *desc)
{
  GtkWidget *row;
  int len;
  int index;

  g_assert (code && desc);

  len = get_list_box_length (GTK_LIST_BOX (general_page->lang_listbox));

  for (index = 0; index < len - 1; index++) {
    GtkListBoxRow *widget;
    const char *row_code;

    widget = gtk_list_box_get_row_at_index (GTK_LIST_BOX (general_page->lang_listbox), index);

    row_code = ephy_lang_row_get_code (EPHY_LANG_ROW (widget));
    if (row_code && strcmp (row_code, code) == 0)
      return;
  }

  row = ephy_lang_row_new ();

  ephy_lang_row_set_code (EPHY_LANG_ROW (row), code);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), desc);

  g_signal_connect (row, "delete-button-clicked", G_CALLBACK (language_editor_delete_button_clicked_cb), general_page);
  g_signal_connect (row, "move-row", G_CALLBACK (language_editor_move_row_cb), general_page);

  gtk_list_box_insert (GTK_LIST_BOX (general_page->lang_listbox), row, len - 1);
  language_editor_update_rows_movable (general_page, GTK_LIST_BOX (general_page->lang_listbox));
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static void
add_lang_dialog_response_cb (GtkWidget        *button,
                             PrefsGeneralPage *general_page)
{
  GtkWindow *dialog = general_page->add_lang_dialog;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GList *rows, *r;

  g_assert (dialog);

  selection = gtk_tree_view_get_selection (general_page->add_lang_treeview);

  rows = gtk_tree_selection_get_selected_rows (selection, &model);

  for (r = rows; r; r = r->next) {
    GtkTreePath *path = (GtkTreePath *)r->data;

    if (gtk_tree_model_get_iter (model, &iter, path)) {
      char *code, *desc;

      gtk_tree_model_get (model, &iter,
                          COL_LANG_NAME, &desc,
                          COL_LANG_CODE, &code,
                          -1);

      language_editor_add (general_page, code, desc);

      g_free (desc);
      g_free (code);
    }
  }

  g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
  g_list_free (rows);

  language_editor_update_pref (general_page);
  language_editor_update_state (general_page);

  gtk_window_close (GTK_WINDOW (dialog));
}

static void
add_lang_dialog_selection_changed (GtkTreeSelection *selection,
                                   GtkWidget        *button)
{
  int n_selected;

  n_selected = gtk_tree_selection_count_selected_rows (selection);
  gtk_widget_set_sensitive (button, n_selected > 0);
}

static void
add_language_add_system_language_entry (GtkListStore *store)
{
  GtkTreeIter iter;
  char **sys_langs;
  char *system, *text;
  int n_sys_langs;

  sys_langs = ephy_langs_get_languages ();
  n_sys_langs = g_strv_length (sys_langs);

  system = g_strjoinv (", ", sys_langs);

  text = g_strdup_printf
           (ngettext ("System language (%s)",
                      "System languages (%s)", n_sys_langs), system);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
                      COL_LANG_NAME, text,
                      COL_LANG_CODE, "system",
                      -1);

  g_strfreev (sys_langs);
  g_free (system);
  g_free (text);
}

static GtkWindow *
setup_add_language_dialog (PrefsGeneralPage *general_page)
{
  GtkWidget *ad;
  GtkWidget *add_button;
  GtkListStore *store;
  GtkTreeModel *sortmodel;
  GtkTreeView *treeview;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  guint i, n;
  g_autoptr (GtkBuilder) builder = NULL;
  g_auto (GStrv) locales = NULL;

  builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/prefs-lang-dialog.ui");
  ad = GTK_WIDGET (gtk_builder_get_object (builder, "add_language_dialog"));
  add_button = GTK_WIDGET (gtk_builder_get_object (builder, "add_button"));
  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder, "languages_treeview"));
  general_page->add_lang_treeview = treeview;

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  locales = gnome_get_all_locales ();
  n = g_strv_length (locales);

  for (i = 0; i < n; i++) {
    const char *locale = locales[i];
    g_autofree char *language_code = NULL;
    g_autofree char *country_code = NULL;
    g_autofree char *language_name = NULL;
    g_autofree char *shortened_locale = NULL;

    if (!gnome_parse_locale (locale, &language_code, &country_code, NULL, NULL))
      break;

    if (!language_code)
      break;

    language_name = gnome_get_language_from_locale (locale, locale);

    if (country_code)
      shortened_locale = g_strdup_printf ("%s-%s", language_code, country_code);
    else
      shortened_locale = g_strdup (language_code);

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
                        COL_LANG_NAME, language_name,
                        COL_LANG_CODE, shortened_locale,
                        -1);
  }

  add_language_add_system_language_entry (store);

  sortmodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_sortable_set_sort_column_id
    (GTK_TREE_SORTABLE (sortmodel), COL_LANG_NAME, GTK_SORT_ASCENDING);

  gtk_window_set_modal (GTK_WINDOW (ad), TRUE);

  gtk_tree_view_set_reorderable (GTK_TREE_VIEW (treeview), FALSE);

  gtk_tree_view_set_model (treeview, sortmodel);

  gtk_tree_view_set_headers_visible (treeview, FALSE);

  renderer = gtk_cell_renderer_text_new ();

  gtk_tree_view_insert_column_with_attributes (treeview,
                                               0, "Language",
                                               renderer,
                                               "text", 0,
                                               NULL);
  column = gtk_tree_view_get_column (treeview, 0);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_column_set_sort_column_id (column, COL_LANG_NAME);

  selection = gtk_tree_view_get_selection (treeview);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

  add_lang_dialog_selection_changed (GTK_TREE_SELECTION (selection), add_button);
  g_signal_connect (selection, "changed",
                    G_CALLBACK (add_lang_dialog_selection_changed), add_button);

  g_signal_connect (add_button, "clicked",
                    G_CALLBACK (add_lang_dialog_response_cb), general_page);

  g_object_unref (store);
  g_object_unref (sortmodel);

  return GTK_WINDOW (ad);
}
G_GNUC_END_IGNORE_DEPRECATIONS

static char *
language_for_locale (const char *locale)
{
  g_autoptr (GString) string = g_string_new (locale);

  /* Before calling gnome_get_language_from_locale() we have to convert
   * from web locales (e.g. es-ES) to UNIX (e.g. es_ES.UTF-8).
   */
  g_strdelimit (string->str, "-", '_');
  g_string_append (string, ".UTF-8");

  return gnome_get_language_from_locale (string->str, string->str);
}

static char *
normalize_locale (const char *locale)
{
  char *result = g_strdup (locale);

  /* The result we store in prefs looks like es-ES or en-US. We don't
   * store codeset (not used in Accept-Langs) and we store with hyphen
   * instead of underscore (ditto). So here we just uppercase the
   * country code, converting e.g. es-es to es-ES. We have to do this
   * because older versions of Epiphany stored locales as entirely
   * lowercase.
   */
  for (char *p = strchr (result, '-'); p && *p != '\0'; p++)
    *p = g_ascii_toupper (*p);

  return result;
}

static void
add_system_language_entry (PrefsGeneralPage *general_page)
{
  g_auto (GStrv) sys_langs = NULL;
  g_autofree char *system = NULL;
  g_autofree char *text = NULL;
  int n_sys_langs;

  sys_langs = ephy_langs_get_languages ();
  n_sys_langs = g_strv_length (sys_langs);

  system = g_strjoinv (", ", sys_langs);

  text = g_strdup_printf
           (ngettext ("System language (%s)",
                      "System languages (%s)", n_sys_langs), system);

  language_editor_add (general_page, "system", text);
}

static void
download_folder_file_dialog_cb (GtkFileDialog    *dialog,
                                GAsyncResult     *result,
                                PrefsGeneralPage *general_page)
{
  g_autoptr (GFile) file = NULL;
  g_autofree char *path = NULL;

  file = gtk_file_dialog_select_folder_finish (dialog, result, NULL);

  if (!file)
    return;

  path = g_file_get_path (file);

  if (path)
    g_settings_set_string (EPHY_SETTINGS_STATE,
                           EPHY_PREFS_STATE_DOWNLOAD_DIR, path);
}

static void
download_folder_row_activated_cb (PrefsGeneralPage *general_page)
{
  g_autofree char *downloads_path = NULL;
  g_autoptr (GFile) downloads_dir = NULL;
  GtkFileDialog *dialog;
  GtkRoot *root;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, _("Select a Directory"));

  downloads_path = ephy_file_get_downloads_dir ();

  if (downloads_path && downloads_path[0])
    downloads_dir = g_file_new_for_path (downloads_path);

  gtk_file_dialog_set_initial_folder (dialog, downloads_dir);

  root = gtk_widget_get_root (GTK_WIDGET (general_page));

  gtk_file_dialog_select_folder (dialog,
                                 GTK_WINDOW (root),
                                 general_page->cancellable,
                                 (GAsyncReadyCallback)download_folder_file_dialog_cb,
                                 general_page);
}

static gboolean
download_folder_get_mapping (GValue   *value,
                             GVariant *variant,
                             gpointer  user_data)
{
  g_autofree char *path = ephy_file_get_downloads_dir ();
  g_autoptr (GFile) dir = g_file_new_for_path (path);

  g_value_take_string (value, ephy_file_get_display_name (dir));

  return TRUE;
}

static gboolean
restore_session_get_mapping (GValue   *value,
                             GVariant *variant,
                             gpointer  user_data)
{
  const char *policy = g_variant_get_string (variant, NULL);
  /* FIXME: Is it possible to somehow use EPHY_PREFS_RESTORE_SESSION_POLICY_ALWAYS here? */
  g_value_set_boolean (value, !strcmp (policy, "always"));
  return TRUE;
}

static GVariant *
restore_session_set_mapping (const GValue       *value,
                             const GVariantType *expected_type,
                             gpointer            user_data)
{
  /* FIXME: Is it possible to somehow use EphyPrefsRestoreSessionPolicy here? */
  if (g_value_get_boolean (value))
    return g_variant_new_string ("always");
  return g_variant_new_string ("crashed");
}

static gboolean
save_web_application (PrefsGeneralPage *general_page)
{
  gboolean changed = FALSE;
  const char *text;
  EphyWebApplication *webapp = ephy_shell_get_webapp (ephy_shell_get_default ());

  general_page->webapp_save_id = 0;

  if (!webapp)
    return G_SOURCE_REMOVE;

  text = gtk_editable_get_text (GTK_EDITABLE (general_page->webapp_url_row));
  if (g_strcmp0 (webapp->url, text) != 0) {
    g_free (webapp->url);
    webapp->url = g_strdup (text);
    changed = TRUE;
  }

  text = gtk_editable_get_text (GTK_EDITABLE (general_page->webapp_title_row));
  if (g_strcmp0 (webapp->name, text) != 0) {
    g_free (webapp->name);
    webapp->name = g_strdup (text);
    changed = TRUE;
  }

  text = (const char *)g_object_get_data (G_OBJECT (general_page->webapp_icon), "ephy-webapp-icon-path");
  if (g_strcmp0 (webapp->icon_path, text) != 0) {
    g_free (webapp->icon_path);
    webapp->icon_path = g_strdup (text);
    changed = TRUE;
  }

  if (changed) {
    ephy_web_application_save (webapp);
    ephy_shell_resync_title_boxes (ephy_shell_get_default (),
                                   webapp->name,
                                   webapp->url);
  }

  return G_SOURCE_REMOVE;
}

static void
prefs_general_page_save_web_application (PrefsGeneralPage *general_page)
{
  EphyWebApplication *webapp = ephy_shell_get_webapp (ephy_shell_get_default ());
  if (!webapp)
    return;

  g_clear_handle_id (&general_page->webapp_save_id, g_source_remove);
  general_page->webapp_save_id = g_timeout_add_seconds (1, (GSourceFunc)save_web_application, general_page);
}

static void
prefs_general_page_update_webapp_icon (PrefsGeneralPage *general_page,
                                       const char       *icon_path)
{
  g_autoptr (GdkPixbuf) icon = gdk_pixbuf_new_from_file (icon_path, NULL);

  if (!icon)
    return;

  gtk_image_set_from_gicon (GTK_IMAGE (general_page->webapp_icon), G_ICON (icon));
  gtk_image_set_pixel_size (GTK_IMAGE (general_page->webapp_icon), 32);

  g_object_set_data_full (G_OBJECT (general_page->webapp_icon), "ephy-webapp-icon-path",
                          g_strdup (icon_path), g_free);
}

static void
webapp_icon_dialog_cb (GtkFileDialog    *dialog,
                       GAsyncResult     *result,
                       PrefsGeneralPage *general_page)
{
  g_autoptr (GFile) file = NULL;
  g_autofree char *icon_path = NULL;

  file = gtk_file_dialog_open_finish (dialog, result, NULL);

  if (!file)
    return;

  icon_path = g_file_get_path (file);
  prefs_general_page_update_webapp_icon (general_page, icon_path);
  prefs_general_page_save_web_application (general_page);
}

static void
on_webapp_entry_changed (GtkEditable      *editable,
                         PrefsGeneralPage *dialog)
{
  prefs_general_page_save_web_application (dialog);
}

void
prefs_general_page_on_pd_close_request (PrefsGeneralPage *general_page)
{
  if (general_page->webapp_save_id) {
    g_source_remove (general_page->webapp_save_id);
    general_page->webapp_save_id = 0;
    save_web_application (general_page);
  }
}

/* Used by EphyAddOpenSearchButton to scroll to the just added engine. */
EphySearchEngineListBox *
prefs_general_page_get_search_engine_list_box (PrefsGeneralPage *general_page)
{
  return general_page->search_engine_list_box;
}

static void
on_webapp_icon_row_activated (GtkWidget        *button,
                              PrefsGeneralPage *general_page)
{
  GtkFileDialog *dialog;
  GSList *pixbuf_formats, *l;
  g_autoptr (GtkFileFilter) images_filter = NULL;
  g_autoptr (GListStore) filters = NULL;
  GtkRoot *root;

  dialog = gtk_file_dialog_new ();

  root = gtk_widget_get_root (GTK_WIDGET (general_page));
  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);

  images_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (images_filter, _("Supported Image Files"));
  g_list_store_append (filters, images_filter);

  pixbuf_formats = gdk_pixbuf_get_formats ();
  for (l = pixbuf_formats; l; l = g_slist_next (l)) {
    GdkPixbufFormat *format = (GdkPixbufFormat *)l->data;
    g_autoptr (GtkFileFilter) filter = NULL;
    g_autofree char *name = NULL;
    gchar **mime_types;
    guint i;

    if (gdk_pixbuf_format_is_disabled (format) || !gdk_pixbuf_format_is_writable (format))
      continue;

    filter = gtk_file_filter_new ();
    name = gdk_pixbuf_format_get_description (format);
    gtk_file_filter_set_name (filter, name);

    mime_types = gdk_pixbuf_format_get_mime_types (format);
    for (i = 0; mime_types[i] != 0; i++) {
      gtk_file_filter_add_mime_type (images_filter, mime_types[i]);
      gtk_file_filter_add_mime_type (filter, mime_types[i]);
    }
    g_strfreev (mime_types);

    g_list_store_append (filters, filter);
  }
  g_slist_free (pixbuf_formats);

  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));

  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (root),
                        general_page->cancellable,
                        (GAsyncReadyCallback)webapp_icon_dialog_cb,
                        general_page);
}

static void
custom_homepage_entry_changed (GtkEditable      *editable,
                               PrefsGeneralPage *general_page)
{
  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (general_page->custom_homepage_radiobutton))) {
    g_settings_set_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_HOMEPAGE_URL,
                           gtk_editable_get_text (editable));
  } else if ((gtk_editable_get_text (editable)) &&
             gtk_check_button_get_active (GTK_CHECK_BUTTON (general_page->new_tab_homepage_radiobutton))) {
    g_settings_set_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_HOMEPAGE_URL, gtk_editable_get_text (editable));
    gtk_widget_set_sensitive (general_page->custom_homepage_entry, TRUE);
    gtk_widget_grab_focus (general_page->custom_homepage_entry);
  }
}

static void
custom_homepage_entry_icon_released (GtkEntry             *entry,
                                     GtkEntryIconPosition  icon_pos)
{
  if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
    gtk_editable_set_text (GTK_EDITABLE (entry), "");
}

static gboolean
new_tab_homepage_get_mapping (GValue   *value,
                              GVariant *variant,
                              gpointer  user_data)
{
  const char *setting;

  setting = g_variant_get_string (variant, NULL);
  if (!setting || setting[0] == '\0')
    g_value_set_boolean (value, TRUE);

  return TRUE;
}

static GVariant *
new_tab_homepage_set_mapping (const GValue       *value,
                              const GVariantType *expected_type,
                              gpointer            user_data)
{
  PrefsGeneralPage *general_page = EPHY_PREFS_GENERAL_PAGE (user_data);

  if (!g_value_get_boolean (value))
    return NULL;

  /* In case the new tab button is pressed while there's text in the custom homepage entry */
  gtk_editable_set_text (GTK_EDITABLE (general_page->custom_homepage_entry), "");
  gtk_widget_set_sensitive (general_page->custom_homepage_entry, FALSE);

  return g_variant_new_string ("");
}

static gboolean
blank_homepage_get_mapping (GValue   *value,
                            GVariant *variant,
                            gpointer  user_data)
{
  const char *setting;

  setting = g_variant_get_string (variant, NULL);
  if (g_strcmp0 (setting, "about:newtab") == 0)
    g_value_set_boolean (value, TRUE);

  return TRUE;
}

static GVariant *
blank_homepage_set_mapping (const GValue       *value,
                            const GVariantType *expected_type,
                            gpointer            user_data)
{
  PrefsGeneralPage *general_page = EPHY_PREFS_GENERAL_PAGE (user_data);

  if (!g_value_get_boolean (value))
    return NULL;

  gtk_editable_set_text (GTK_EDITABLE (general_page->custom_homepage_entry), "");

  return g_variant_new_string ("about:newtab");
}

static gboolean
custom_homepage_get_mapping (GValue   *value,
                             GVariant *variant,
                             gpointer  user_data)
{
  const char *setting;

  setting = g_variant_get_string (variant, NULL);
  if (setting && setting[0] != '\0' && g_strcmp0 (setting, "about:newtab") != 0)
    g_value_set_boolean (value, TRUE);
  return TRUE;
}

static GVariant *
custom_homepage_set_mapping (const GValue       *value,
                             const GVariantType *expected_type,
                             gpointer            user_data)
{
  PrefsGeneralPage *general_page = EPHY_PREFS_GENERAL_PAGE (user_data);
  const char *setting;

  if (!g_value_get_boolean (value)) {
    gtk_widget_set_sensitive (general_page->custom_homepage_entry, FALSE);
    gtk_editable_set_text (GTK_EDITABLE (general_page->custom_homepage_entry), "");
    return NULL;
  }

  gtk_widget_set_sensitive (general_page->custom_homepage_entry, TRUE);
  gtk_widget_grab_focus (general_page->custom_homepage_entry);
  setting = gtk_editable_get_text (GTK_EDITABLE (general_page->custom_homepage_entry));
  if (!setting || setting[0] == '\0')
    return NULL;

  gtk_editable_set_text (GTK_EDITABLE (general_page->custom_homepage_entry), setting);

  return g_variant_new_string (setting);
}

static void
on_manage_webapp_additional_urls_row_activated (GtkWidget        *row,
                                                PrefsGeneralPage *general_page)
{
  g_signal_emit (general_page, signals[WEBAPP_ADDITIONAL_URLS_ROW_ACTIVATED], 0);
}

static void
search_engine_row_expanded_cb (GObject  *object,
                               gpointer  user_data)
{
  EphySearchEngineRow *row = EPHY_SEARCH_ENGINE_ROW (object);

  ephy_search_engine_row_focus_name_entry (row);
  g_signal_handlers_disconnect_by_func (object, search_engine_row_expanded_cb, user_data);
}

#define EMPTY_NEW_SEARCH_ENGINE_NAME (_("New search engine"))
static void
on_add_search_engine_button_clicked_cb (GtkButton *button,
                                        gpointer   user_data)
{
  PrefsGeneralPage *page = EPHY_PREFS_GENERAL_PAGE (user_data);
  EphySearchEngineManager *manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());
  EphySearchEngine *old_empty_engine = ephy_search_engine_manager_find_engine_by_name (manager, EMPTY_NEW_SEARCH_ENGINE_NAME);

  if (old_empty_engine) {
    EphySearchEngineRow *row =
      ephy_search_engine_list_box_find_row_for_engine (page->search_engine_list_box, old_empty_engine);

    /* Change the focus to an arbitrary widget we have available here first,
     * because the scroll-to-focus feature won't work if the focus is grabbed on
     * an already focused widget.
     */
    ephy_search_engine_row_focus_bang_entry (row);
    adw_expander_row_set_expanded (ADW_EXPANDER_ROW (row), FALSE);
    adw_expander_row_set_expanded (ADW_EXPANDER_ROW (row), TRUE);
    g_signal_connect (row, "fully-expanded", G_CALLBACK (search_engine_row_expanded_cb), NULL);
  } else {
    g_autoptr (EphySearchEngine) empty_engine =
      g_object_new (EPHY_TYPE_SEARCH_ENGINE,
                    "name", EMPTY_NEW_SEARCH_ENGINE_NAME,
                    "url", "https://www.example.com/search?q=%s",
                    NULL);
    ephy_search_engine_manager_add_engine (manager, empty_engine);
  }
}

static void
prefs_general_page_class_init (PrefsGeneralPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = prefs_general_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-general-page.ui");

  signals[WEBAPP_ADDITIONAL_URLS_ROW_ACTIVATED] =
    g_signal_new ("webapp-additional-row-activated",
                  EPHY_TYPE_PREFS_GENERAL_PAGE,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /* Web Application */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, webapp_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, webapp_icon);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, webapp_icon_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, webapp_url_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, webapp_title_row);

  /* Web Content */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, adblock_allow_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, popups_allow_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, cookie_banner_allow_row);

  /* Homepage */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, homepage_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, new_tab_homepage_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, blank_homepage_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, custom_homepage_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, custom_homepage_entry);

  /* Downloads */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, download_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, ask_on_download_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, download_folder_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, download_folder_label);

  /* Search Engines */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, default_search_engines);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, standard_search_engine);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, incognito_search_engine);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, search_engine_group);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, search_engine_list_box);

  /* Session */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, session_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, start_in_incognito_mode_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, restore_session_row);

  /* Browsing */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, browsing_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, enable_mouse_gesture_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, enable_switch_to_new_tab);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, enable_navigation_gestures_row);

  /* Languages */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, lang_group);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, lang_listbox);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, enable_spell_checking_row);

  /* Developer */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, dev_group);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, show_developer_actions_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, always_show_full_url_row);

  /* Signals */
  gtk_widget_class_bind_template_callback (widget_class, on_webapp_icon_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_webapp_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, download_folder_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_manage_webapp_additional_urls_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_add_search_engine_button_clicked_cb);
}

static void
init_lang_listbox (PrefsGeneralPage *general_page)
{
  char **list = NULL;
  int i;

  list = g_settings_get_strv (EPHY_SETTINGS_WEB,
                              EPHY_PREFS_WEB_LANGUAGE);

  language_editor_add_function_buttons (general_page);

  /* Fill languages editor */
  for (i = 0; list[i]; i++) {
    const char *code = list[i];
    if (strcmp (code, "system") == 0) {
      add_system_language_entry (general_page);
    } else if (code[0] != '\0') {
      g_autofree char *normalized_locale = normalize_locale (code);
      if (normalized_locale) {
        g_autofree char *language_name = language_for_locale (normalized_locale);
        if (!language_name)
          language_name = g_strdup (normalized_locale);
        language_editor_add (general_page, normalized_locale, language_name);
      }
    }
  }

  language_editor_update_state (general_page);
}

static gboolean
default_search_engine_get_mapping (GValue   *value,
                                   GVariant *variant,
                                   gpointer  user_data)
{
  EphySearchEngineManager *manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());
  const char *text = g_variant_get_string (variant, NULL);

  g_assert (text);

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (manager)); i++) {
    EphySearchEngine *engine = g_list_model_get_item (G_LIST_MODEL (manager), i);

    if (g_strcmp0 (ephy_search_engine_get_name (engine), text) == 0) {
      g_value_set_uint (value, i);
      return TRUE;
    }
  }

  return FALSE;
}

static GVariant *
default_search_engine_set_mapping (const GValue       *value,
                                   const GVariantType *expected_type,
                                   gpointer            user_data)
{
  EphySearchEngineManager *manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());
  EphySearchEngine *engine;
  guint i = g_value_get_uint (value);

  if (i >= g_list_model_get_n_items (G_LIST_MODEL (manager)))
    return NULL;

  engine = g_list_model_get_item (G_LIST_MODEL (manager), i);
  g_assert (engine);

  ephy_search_engine_manager_set_default_engine (manager, engine);
  return g_variant_new_string (ephy_search_engine_get_name (engine));
}

static gboolean
incognito_search_engine_get_mapping (GValue   *value,
                                     GVariant *variant,
                                     gpointer  user_data)
{
  EphySearchEngineManager *manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());
  const char *text = g_variant_get_string (variant, NULL);

  g_assert (text);

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (manager)); i++) {
    EphySearchEngine *engine = g_list_model_get_item (G_LIST_MODEL (manager), i);

    if (g_strcmp0 (ephy_search_engine_get_name (engine), text) == 0) {
      g_value_set_uint (value, i);
      return TRUE;
    }
  }

  return FALSE;
}

static GVariant *
incognito_search_engine_set_mapping (const GValue       *value,
                                     const GVariantType *expected_type,
                                     gpointer            user_data)
{
  EphySearchEngineManager *manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());
  EphySearchEngine *engine;
  guint i = g_value_get_uint (value);

  if (i >= g_list_model_get_n_items (G_LIST_MODEL (manager)))
    return NULL;

  engine = g_list_model_get_item (G_LIST_MODEL (manager), i);
  g_assert (engine);

  ephy_search_engine_manager_set_incognito_engine (manager, engine);
  return g_variant_new_string (ephy_search_engine_get_name (engine));
}

static void
init_search_engines (PrefsGeneralPage *general_page)
{
  EphySearchEngineManager *manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());
  GtkExpression *expression;

  /* Default */
  expression = gtk_property_expression_new (EPHY_TYPE_SEARCH_ENGINE, NULL, "name");
  adw_combo_row_set_expression (ADW_COMBO_ROW (general_page->standard_search_engine), expression);
  adw_combo_row_set_model (ADW_COMBO_ROW (general_page->standard_search_engine), G_LIST_MODEL (manager));

  g_settings_bind_with_mapping (EPHY_SETTINGS_MAIN,
                                EPHY_PREFS_DEFAULT_SEARCH_ENGINE,
                                general_page->standard_search_engine,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                default_search_engine_get_mapping,
                                default_search_engine_set_mapping,
                                NULL, NULL);

  /* Incognito */
  expression = gtk_property_expression_new (EPHY_TYPE_SEARCH_ENGINE, NULL, "name");
  adw_combo_row_set_expression (ADW_COMBO_ROW (general_page->incognito_search_engine), expression);
  adw_combo_row_set_model (ADW_COMBO_ROW (general_page->incognito_search_engine), G_LIST_MODEL (manager));

  g_settings_bind_with_mapping (EPHY_SETTINGS_MAIN,
                                EPHY_PREFS_INCOGNITO_SEARCH_ENGINE,
                                general_page->incognito_search_engine,
                                "selected",
                                G_SETTINGS_BIND_DEFAULT,
                                incognito_search_engine_get_mapping,
                                incognito_search_engine_set_mapping,
                                NULL, NULL);
}

static void
setup_general_page (PrefsGeneralPage *general_page)
{
  GSettings *settings = ephy_settings_get (EPHY_PREFS_SCHEMA);
  GSettings *web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);

  /* ======================================================================== */
  /* ========================== Web Application ============================= */
  /* ======================================================================== */
  EphyWebApplication *webapp = ephy_shell_get_webapp (ephy_shell_get_default ());
  if (webapp && !ephy_is_running_inside_sandbox () && !g_settings_get_boolean (EPHY_SETTINGS_WEB_APP, EPHY_PREFS_WEB_APP_SYSTEM)) {
    prefs_general_page_update_webapp_icon (general_page, webapp->icon_path);
    gtk_editable_set_text (GTK_EDITABLE (general_page->webapp_url_row), webapp->url);
    gtk_editable_set_text (GTK_EDITABLE (general_page->webapp_title_row), webapp->name);
  }

  /* ======================================================================== */
  /* ========================== Web Content ================================= */
  /* ======================================================================== */
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_ADBLOCK,
                   general_page->adblock_allow_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_POPUPS,
                   general_page->popups_allow_row,
                   "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_COOKIE_BANNER,
                   general_page->cookie_banner_allow_row,
                   "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  /* ======================================================================== */
  /* ========================== Homepage ==================================== */
  /* ======================================================================== */
  g_settings_bind_with_mapping (settings,
                                EPHY_PREFS_HOMEPAGE_URL,
                                general_page->new_tab_homepage_radiobutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                new_tab_homepage_get_mapping,
                                new_tab_homepage_set_mapping,
                                general_page,
                                NULL);

  g_settings_bind_with_mapping (settings,
                                EPHY_PREFS_HOMEPAGE_URL,
                                general_page->blank_homepage_radiobutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                blank_homepage_get_mapping,
                                blank_homepage_set_mapping,
                                general_page,
                                NULL);

  g_settings_bind_with_mapping (settings,
                                EPHY_PREFS_HOMEPAGE_URL,
                                general_page->custom_homepage_radiobutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                custom_homepage_get_mapping,
                                custom_homepage_set_mapping,
                                general_page,
                                NULL);

  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (general_page->custom_homepage_radiobutton))) {
    gtk_widget_set_sensitive (general_page->custom_homepage_entry, TRUE);
    gtk_editable_set_text (GTK_EDITABLE (general_page->custom_homepage_entry),
                           g_settings_get_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_HOMEPAGE_URL));
  } else {
    gtk_widget_set_sensitive (general_page->custom_homepage_entry, FALSE);
    gtk_editable_set_text (GTK_EDITABLE (general_page->custom_homepage_entry), "");
  }

  g_signal_connect (general_page->custom_homepage_entry, "changed",
                    G_CALLBACK (custom_homepage_entry_changed),
                    general_page);
  g_signal_connect (general_page->custom_homepage_entry, "icon-release",
                    G_CALLBACK (custom_homepage_entry_icon_released),
                    NULL);

  /* ======================================================================== */
  /* ========================== Downloads =================================== */
  /* ======================================================================== */
  if (ephy_is_running_inside_sandbox ())
    gtk_widget_set_visible (general_page->download_box, FALSE);
  else
    g_settings_bind_with_mapping (EPHY_SETTINGS_STATE,
                                  EPHY_PREFS_STATE_DOWNLOAD_DIR,
                                  general_page->download_folder_label,
                                  "label",
                                  G_SETTINGS_BIND_GET,
                                  download_folder_get_mapping,
                                  NULL,
                                  general_page,
                                  NULL);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ASK_ON_DOWNLOAD,
                   general_page->ask_on_download_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* ======================================================================== */
  /* ========================== Session ===================================== */
  /* ======================================================================== */
  g_settings_bind (settings,
                   EPHY_PREFS_START_IN_INCOGNITO_MODE,
                   general_page->start_in_incognito_mode_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (settings,
                   EPHY_PREFS_START_IN_INCOGNITO_MODE,
                   general_page->restore_session_row,
                   "sensitive",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_settings_bind_with_mapping (settings,
                                EPHY_PREFS_RESTORE_SESSION_POLICY,
                                general_page->restore_session_row,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                restore_session_get_mapping,
                                restore_session_set_mapping,
                                NULL, NULL);

  /* ======================================================================== */
  /* ========================== Browsing ==================================== */
  /* ======================================================================== */
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_MOUSE_GESTURES,
                   general_page->enable_mouse_gesture_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_SWITCH_TO_NEW_TAB,
                   general_page->enable_switch_to_new_tab,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_NAVIGATION_GESTURES,
                   general_page->enable_navigation_gestures_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* ======================================================================== */
  /* ========================== Languages =================================== */
  /* ======================================================================== */
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_SPELL_CHECKING,
                   general_page->enable_spell_checking_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* ======================================================================== */
  /* ========================== Developer==================================== */
  /* ======================================================================== */
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_SHOW_DEVELOPER_ACTIONS,
                   general_page->show_developer_actions_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ALWAYS_SHOW_FULL_URL,
                   general_page->always_show_full_url_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  init_lang_listbox (general_page);
  init_search_engines (general_page);
}

static void
prefs_general_page_init (PrefsGeneralPage *general_page)
{
  EphyEmbedShellMode mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());

  g_type_ensure (EPHY_TYPE_SEARCH_ENGINE_LIST_BOX);

  gtk_widget_init_template (GTK_WIDGET (general_page));

  setup_general_page (general_page);

  general_page->cancellable = g_cancellable_new ();

  gtk_widget_set_visible (general_page->webapp_box,
                          mode == EPHY_EMBED_SHELL_MODE_APPLICATION &&
                          !g_settings_get_boolean (EPHY_SETTINGS_WEB_APP,
                                                   EPHY_PREFS_WEB_APP_SYSTEM));
  gtk_widget_set_visible (general_page->webapp_icon_row, !ephy_is_running_inside_sandbox ());
  gtk_widget_set_visible (general_page->webapp_url_row, !ephy_is_running_inside_sandbox ());
  gtk_widget_set_visible (general_page->webapp_title_row, !ephy_is_running_inside_sandbox ());
  gtk_widget_set_visible (general_page->default_search_engines,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (general_page->homepage_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (general_page->search_engine_group,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (general_page->session_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (general_page->browsing_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
}
