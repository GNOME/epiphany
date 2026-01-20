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
#include "ephy-header-bar.h"
#include "ephy-lang-row.h"
#include "ephy-langs.h"
#include "ephy-location-entry.h"
#include "ephy-settings.h"
#include "ephy-search-engine-listbox.h"
#include "ephy-shell.h"
#include "ephy-web-app-utils.h"

#include "gnome-languages.h"
#include <glib/gi18n.h>

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

  AdwDialog *add_lang_dialog;
  GtkListView *add_lang_list_view;

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

  if (general_page->add_lang_dialog)
    adw_dialog_close (general_page->add_lang_dialog);

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

static void create_add_language_dialog (PrefsGeneralPage *general_page);

static void
language_editor_add_activated (GtkWidget *listbox,
                               GtkWidget *activated_row,
                               GtkWidget *add_row)
{
  PrefsGeneralPage *general_page;

  if (add_row != activated_row)
    return;

  general_page = EPHY_PREFS_GENERAL_PAGE (gtk_widget_get_ancestor (listbox, EPHY_TYPE_PREFS_GENERAL_PAGE));

  if (!general_page->add_lang_dialog)
    create_add_language_dialog (general_page);
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

static void
on_setup_list_item (GtkListItemFactory *factory,
                    GtkListItem        *item)
{
  GtkWidget *label = gtk_label_new (NULL);

  gtk_label_set_xalign (GTK_LABEL (label), 0);

  gtk_list_item_set_child (item, label);
}

static void
on_bind_list_item (GtkListItemFactory *factory,
                   GtkListItem        *item)
{
  GtkWidget *label = gtk_list_item_get_child (item);
  const char *lang_string = gtk_string_object_get_string (gtk_list_item_get_item (item));
  char *text;

  text = g_strsplit_set (lang_string, ":", -1)[0];

  gtk_label_set_text (GTK_LABEL (label), text);
}

static void
on_teardown_list_item (GtkListItemFactory *factory,
                       GtkListItem        *item)
{
  gtk_list_item_set_child (item, NULL);
}

static void
on_add_lang_dialog_response (GtkWidget        *button,
                             PrefsGeneralPage *general_page)
{
  AdwDialog *dialog = general_page->add_lang_dialog;
  GtkSelectionModel *selection_model;
  GtkBitset *selection;
  GtkBitsetIter iter;
  guint i;

  g_assert (dialog);

  selection_model = gtk_list_view_get_model (general_page->add_lang_list_view);
  selection = gtk_selection_model_get_selection (selection_model);

  for (gtk_bitset_iter_init_first (&iter, selection, &i);
       gtk_bitset_iter_is_valid (&iter);
       gtk_bitset_iter_next (&iter, &i)) {
    GtkStringObject *item = g_list_model_get_item (G_LIST_MODEL (selection_model), i);
    const char *lang_string = gtk_string_object_get_string (item);
    char *desc, *code;

    desc = g_strsplit_set (lang_string, ":", -1)[0];
    code = g_strsplit_set (lang_string, ":", -1)[1];

    language_editor_add (general_page, code, desc);

    g_free (desc);
    g_free (code);
  }

  language_editor_update_pref (general_page);
  language_editor_update_state (general_page);

  adw_dialog_close (dialog);
}

static void
on_add_lang_dialog_selection_changed (GtkSelectionModel *selection_model,
                                      guint              position,
                                      guint              n_items,
                                      GtkWidget         *button)
{
  gtk_widget_set_sensitive (button, n_items > 0);
}

static void
on_add_lang_dialog_closed (PrefsGeneralPage *general_page)
{
  general_page->add_lang_dialog = NULL;
}

static void
add_language_add_system_language_entry (GtkStringList *string_list)
{
  char **sys_langs;
  char *system, *text;
  int n_sys_langs;
  char *lang_string;

  sys_langs = ephy_langs_get_languages ();
  n_sys_langs = g_strv_length (sys_langs);

  system = g_strjoinv (", ", sys_langs);

  text = g_strdup_printf
           (ngettext ("System language (%s)",
                      "System languages (%s)", n_sys_langs), system);

  lang_string = g_strconcat (text, ":", "system", NULL);
  gtk_string_list_append (string_list, lang_string);

  g_strfreev (sys_langs);
  g_free (system);
  g_free (text);
}

static int
sort_languages_func (GtkStringObject *lang1,
                     GtkStringObject *lang2)
{
  const char *str1 = gtk_string_object_get_string (lang1);
  const char *str2 = gtk_string_object_get_string (lang2);

  return g_utf8_collate (str1, str2);
}

static void
create_add_language_dialog (PrefsGeneralPage *general_page)
{
  AdwDialog *ad;
  GtkWidget *add_button;
  GtkListView *list_view;
  GtkStringList *string_list;
  GtkListItemFactory *factory;
  GtkCustomSorter *sorter;
  GtkSortListModel *sort_model;
  GtkSelectionModel *selection_model;
  guint i, n;
  g_autoptr (GtkBuilder) builder = NULL;
  g_auto (GStrv) locales = NULL;

  builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/prefs-lang-dialog.ui");
  ad = ADW_DIALOG (gtk_builder_get_object (builder, "add_language_dialog"));

  add_button = GTK_WIDGET (gtk_builder_get_object (builder, "add_button"));
  gtk_widget_set_sensitive (add_button, FALSE);

  list_view = GTK_LIST_VIEW (gtk_builder_get_object (builder, "languages_list_view"));
  general_page->add_lang_list_view = list_view;

  factory = gtk_signal_list_item_factory_new ();
  gtk_list_view_set_factory (list_view, factory);

  g_signal_connect_object (factory, "setup", G_CALLBACK (on_setup_list_item),
                           NULL, G_CONNECT_DEFAULT);
  g_signal_connect_object (factory, "bind", G_CALLBACK (on_bind_list_item),
                           NULL, G_CONNECT_DEFAULT);
  g_signal_connect_object (factory, "teardown", G_CALLBACK (on_teardown_list_item),
                           NULL, G_CONNECT_DEFAULT);

  string_list = gtk_string_list_new (NULL);

  locales = gnome_get_all_locales ();
  n = g_strv_length (locales);

  for (i = 0; i < n; i++) {
    const char *locale = locales[i];
    g_autofree char *language_code = NULL;
    g_autofree char *country_code = NULL;
    g_autofree char *language_name = NULL;
    g_autofree char *shortened_locale = NULL;
    char *lang_string;

    if (!gnome_parse_locale (locale, &language_code, &country_code, NULL, NULL))
      break;

    if (!language_code)
      break;

    language_name = gnome_get_language_from_locale (locale, locale);

    if (country_code)
      shortened_locale = g_strdup_printf ("%s-%s", language_code, country_code);
    else
      shortened_locale = g_strdup (language_code);

    lang_string = g_strconcat (language_name, ":", shortened_locale, NULL);
    gtk_string_list_append (string_list, lang_string);
  }

  add_language_add_system_language_entry (string_list);

  sorter = gtk_custom_sorter_new ((GCompareDataFunc)sort_languages_func, NULL, NULL);
  sort_model = gtk_sort_list_model_new (G_LIST_MODEL (string_list), GTK_SORTER (sorter));

  selection_model = GTK_SELECTION_MODEL (gtk_multi_selection_new (G_LIST_MODEL (sort_model)));
  gtk_list_view_set_model (list_view, selection_model);

  g_signal_connect_object (selection_model, "selection-changed", G_CALLBACK (on_add_lang_dialog_selection_changed),
                           add_button, G_CONNECT_DEFAULT);

  g_signal_connect_object (add_button, "clicked", G_CALLBACK (on_add_lang_dialog_response),
                           general_page, G_CONNECT_DEFAULT);

  g_signal_connect_object (ad, "closed", G_CALLBACK (on_add_lang_dialog_closed),
                           general_page, G_CONNECT_SWAPPED);

  general_page->add_lang_dialog = ad;
  adw_dialog_present (ad, GTK_WIDGET (general_page));
}

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
on_always_show_full_url_row_active_changed (GtkWidget *row)
{
  EphyWindow *window = EPHY_WINDOW (gtk_widget_get_ancestor (row, EPHY_TYPE_WINDOW));
  EphyHeaderBar *header_bar = EPHY_HEADER_BAR (ephy_window_get_header_bar (window));
  EphyTitleWidget *title_widget = ephy_header_bar_get_title_widget (header_bar);

  if (EPHY_IS_LOCATION_ENTRY (title_widget))
    ephy_location_entry_update_url_button_style (EPHY_LOCATION_ENTRY (title_widget));
}

static void
prefs_general_page_init (PrefsGeneralPage *general_page)
{
  EphyEmbedShellMode mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());

  g_type_ensure (EPHY_TYPE_SEARCH_ENGINE_LIST_BOX);

  gtk_widget_init_template (GTK_WIDGET (general_page));

  setup_general_page (general_page);

  general_page->cancellable = g_cancellable_new ();

  g_signal_connect (general_page->always_show_full_url_row, "notify::active",
                    G_CALLBACK (on_always_show_full_url_row_active_changed), NULL);

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
