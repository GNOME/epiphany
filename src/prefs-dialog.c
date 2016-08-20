/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 200-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2010 Igalia S.L.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "prefs-dialog.h"

#include "ephy-bookmarks.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-gui.h"
#include "ephy-langs.h"
#include "ephy-prefs.h"
#include "ephy-session.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-sync-service.h"
#include "clear-data-dialog.h"
#include "cookies-dialog.h"
#include "languages.h"
#include "passwords-dialog.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#define DOWNLOAD_BUTTON_WIDTH   8

enum {
  COL_LANG_NAME,
  COL_LANG_CODE
};

struct _PrefsDialog {
  GtkDialog parent_instance;

  /* general */
  GtkWidget *download_button_hbox;
  GtkWidget *download_button_label;
  GtkWidget *automatic_downloads_checkbutton;
  GtkWidget *search_engine_combo;
  GtkWidget *restore_session_checkbutton;
  GtkWidget *popups_allow_checkbutton;
  GtkWidget *adblock_allow_checkbutton;
  GtkWidget *enable_plugins_checkbutton;

  /* fonts */
  GtkWidget *use_gnome_fonts_checkbutton;
  GtkWidget *custom_fonts_table;
  GtkWidget *sans_fontbutton;
  GtkWidget *serif_fontbutton;
  GtkWidget *mono_fontbutton;
  GtkWidget *css_checkbox;
  GtkWidget *css_edit_button;

  /* privacy */
  GtkWidget *always;
  GtkWidget *no_third_party;
  GtkWidget *never;
  GtkWidget *remember_passwords_checkbutton;
  GtkWidget *do_not_track_checkbutton;
  GtkWidget *clear_personal_data_button;

  /* language */
  GtkTreeView *lang_treeview;
  GtkWidget *lang_add_button;
  GtkWidget *lang_remove_button;
  GtkWidget *lang_up_button;
  GtkWidget *lang_down_button;
  GtkWidget *enable_spell_checking_checkbutton;

  GtkDialog *add_lang_dialog;
  GtkTreeView *add_lang_treeview;
  GtkTreeModel *lang_model;

  GHashTable *iso_639_table;
  GHashTable *iso_3166_table;

  /* sync */
  GtkWidget *sync_top_box;
  GtkWidget *sync_authenticate_box;
  GtkWidget *sync_login_grid;
  GtkWidget *sync_logout_box;
  GtkWidget *sync_email_entry;
  GtkWidget *sync_password_entry;
  GtkWidget *sync_login_button;
  GtkWidget *sync_logout_button;
  GtkWidget *sync_logout_details_label;
  GtkWidget *sync_email_details_label;
  GtkWidget *sync_password_details_label;
  GtkWidget *sync_extra_details_label;
};

enum {
  SEARCH_ENGINE_COL_NAME,
  SEARCH_ENGINE_COL_STOCK_URL,
  SEARCH_ENGINE_COL_URL,
  SEARCH_ENGINE_NUM_COLS
};

G_DEFINE_TYPE (PrefsDialog, prefs_dialog, GTK_TYPE_DIALOG)

static void
prefs_dialog_finalize (GObject *object)
{
  PrefsDialog *dialog = EPHY_PREFS_DIALOG (object);

  if (dialog->add_lang_dialog != NULL) {
    GtkDialog **add_lang_dialog = &dialog->add_lang_dialog;

    g_object_remove_weak_pointer (G_OBJECT (dialog->add_lang_dialog),
                                  (gpointer *)add_lang_dialog);
    g_object_unref (dialog->add_lang_dialog);
  }

  g_hash_table_destroy (dialog->iso_639_table);
  g_hash_table_destroy (dialog->iso_3166_table);

  G_OBJECT_CLASS (prefs_dialog_parent_class)->finalize (object);
}

static void
on_manage_cookies_button_clicked (GtkWidget   *button,
                                  PrefsDialog *dialog)
{
  EphyCookiesDialog *cookies_dialog;

  cookies_dialog = ephy_cookies_dialog_new ();

  gtk_window_set_transient_for (GTK_WINDOW (cookies_dialog), GTK_WINDOW (dialog));
  gtk_window_set_modal (GTK_WINDOW (cookies_dialog), TRUE);
  gtk_window_present (GTK_WINDOW (cookies_dialog));
}

static void
on_manage_passwords_button_clicked (GtkWidget   *button,
                                    PrefsDialog *dialog)
{
  EphyPasswordsDialog *passwords_dialog;

  passwords_dialog = ephy_passwords_dialog_new ();

  gtk_window_set_transient_for (GTK_WINDOW (passwords_dialog), GTK_WINDOW (dialog));
  gtk_window_set_modal (GTK_WINDOW (passwords_dialog), TRUE);
  gtk_window_present (GTK_WINDOW (passwords_dialog));
}

static void
on_sync_login_button_clicked (GtkWidget   *button,
                              PrefsDialog *dialog)
{
  EphySyncService *sync_service;
  const gchar *emailUTF8;
  const gchar *passwordUTF8;
  gboolean login_ok;
  guint error_code = 0;
  gchar *error_message = NULL;

  gtk_label_set_markup (GTK_LABEL (dialog->sync_email_details_label), "");
  gtk_label_set_markup (GTK_LABEL (dialog->sync_password_details_label), "");
  gtk_label_set_markup (GTK_LABEL (dialog->sync_extra_details_label), "");

  emailUTF8 = gtk_entry_get_text (GTK_ENTRY (dialog->sync_email_entry));
  passwordUTF8 = gtk_entry_get_text (GTK_ENTRY (dialog->sync_password_entry));

  if (emailUTF8 && !emailUTF8[0]) {
    gtk_label_set_markup (GTK_LABEL (dialog->sync_email_details_label),
                          _("<span fgcolor='#e6780b'>Please insert your email</span>"));
    return;
  }

  if (passwordUTF8 && !passwordUTF8[0]) {
    gtk_label_set_markup (GTK_LABEL (dialog->sync_password_details_label),
                          _("<span fgcolor='#e6780b'>Please insert your password</span>"));
    return;
  }

LOG ("[%d] email: %s", __LINE__, emailUTF8);
LOG ("[%d] password: %s", __LINE__, passwordUTF8);

  sync_service = ephy_shell_get_global_sync_service ();
  ephy_sync_service_stretch (sync_service, emailUTF8, passwordUTF8);
  login_ok = ephy_sync_service_login (sync_service, &error_code, &error_message);

  if (login_ok == FALSE) {
    /* Translators: the %s refers to the error message. */
    gtk_label_set_markup (GTK_LABEL (dialog->sync_extra_details_label),
                          g_strdup_printf (_("<span fgcolor='#e6780b'>Error: %s</span>"),
                                           error_message));
    g_free (error_message);
    return;
  }

  g_settings_set_string (EPHY_SETTINGS_MAIN,
                         EPHY_PREFS_SYNC_USER,
                         emailUTF8);
  /* Translators: the %s refers to the email of the currently logged in user. */
  gtk_label_set_markup (GTK_LABEL (dialog->sync_logout_details_label),
                        g_strdup_printf (_("Currently logged in as <b>%s</b>"),
                                         emailUTF8));
  gtk_container_remove (GTK_CONTAINER (dialog->sync_authenticate_box),
                        dialog->sync_login_grid);
  gtk_box_pack_start (GTK_BOX (dialog->sync_authenticate_box),
                      dialog->sync_logout_box,
                      TRUE, TRUE, 0);
}

static void
on_sync_logout_button_clicked (GtkWidget   *button,
                               PrefsDialog *dialog)
{
  gtk_entry_set_text (GTK_ENTRY (dialog->sync_email_entry), "");
  gtk_entry_set_text (GTK_ENTRY (dialog->sync_password_entry), "");

  /* TODO: Call /session/destroy endpoint and forget tokens */

  g_settings_set_string (EPHY_SETTINGS_MAIN,
                         EPHY_PREFS_SYNC_USER,
                         "");
  gtk_container_remove (GTK_CONTAINER (dialog->sync_authenticate_box),
                        dialog->sync_logout_box);
  gtk_box_pack_start (GTK_BOX (dialog->sync_authenticate_box),
                      dialog->sync_login_grid,
                      TRUE, TRUE, 0);
LOG ("[%d] logged out", __LINE__);
}

static void
prefs_dialog_class_init (PrefsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = prefs_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/prefs-dialog.ui");
  /* general */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, automatic_downloads_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, search_engine_combo);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, restore_session_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, popups_allow_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, adblock_allow_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, enable_plugins_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, download_button_hbox);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, download_button_label);

  /* fonts */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, use_gnome_fonts_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, custom_fonts_table);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sans_fontbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, serif_fontbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, mono_fontbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, css_checkbox);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, css_edit_button);

  /* privacy */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, always);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, no_third_party);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, never);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, remember_passwords_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, do_not_track_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, clear_personal_data_button);

  /* language */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, lang_treeview);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, lang_add_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, lang_remove_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, lang_up_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, lang_down_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, enable_spell_checking_checkbutton);

  /* sync */
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_top_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_authenticate_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_login_grid);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_logout_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_email_entry);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_password_entry);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_login_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_logout_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_logout_details_label);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_email_details_label);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_password_details_label);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_extra_details_label);

  gtk_widget_class_bind_template_callback (widget_class, on_manage_cookies_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_manage_passwords_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_sync_login_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_sync_logout_button_clicked);
}

static void
css_edit_button_clicked_cb (GtkWidget   *button,
                            PrefsDialog *pd)
{
  GFile *css_file;

  css_file = g_file_new_for_path (g_build_filename (ephy_dot_dir (),
                                                    USER_STYLESHEET_FILENAME,
                                                    NULL));

  ephy_file_launch_handler ("text/plain", css_file,
                            gtk_get_current_event_time ());
  g_object_unref (css_file);
}

static gboolean
combo_get_mapping (GValue   *value,
                   GVariant *variant,
                   gpointer  user_data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean valid = FALSE;
  const char *settings_name;
  int i;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (user_data));
  valid = gtk_tree_model_get_iter_first (model, &iter);
  settings_name = g_variant_get_string (variant, NULL);
  i = 0;

  while (valid) {
    char *item_name;
    gtk_tree_model_get (model, &iter, 1, &item_name, -1);

    if (g_strcmp0 (item_name, settings_name) == 0) {
      g_value_set_int (value, i);
      g_free (item_name);
      break;
    }

    i++;
    valid = gtk_tree_model_iter_next (model, &iter);
    g_free (item_name);
  }

  return TRUE;
}

static GVariant *
combo_set_mapping (const GValue       *value,
                   const GVariantType *expected_type,
                   gpointer            user_data)
{
  GVariant *variant = NULL;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean valid = FALSE;
  int n;

  n = g_value_get_int (value);
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (user_data));
  valid = gtk_tree_model_iter_nth_child (model, &iter, NULL, n);

  if (valid) {
    char *item_name;
    gtk_tree_model_get (model, &iter, 1, &item_name, -1);

    variant = g_variant_new_string (item_name);

    g_free (item_name);
  }

  return variant;
}

static void
language_editor_add (PrefsDialog *pd,
                     const char  *code,
                     const char  *desc)
{
  GtkTreeIter iter;

  g_return_if_fail (code != NULL && desc != NULL);

  if (gtk_tree_model_get_iter_first (pd->lang_model, &iter)) {
    do {
      char *c;

      gtk_tree_model_get (pd->lang_model, &iter,
                          COL_LANG_CODE, &c,
                          -1);

      if (strcmp (code, c) == 0) {
        g_free (c);

        /* already in list, don't allow a duplicate */
        return;
      }
      g_free (c);
    } while (gtk_tree_model_iter_next (pd->lang_model, &iter));
  }

  gtk_list_store_append (GTK_LIST_STORE (pd->lang_model), &iter);

  gtk_list_store_set (GTK_LIST_STORE (pd->lang_model), &iter,
                      COL_LANG_NAME, desc,
                      COL_LANG_CODE, code,
                      -1);
}

static void
language_editor_update_pref (PrefsDialog *pd)
{
  GtkTreeIter iter;
  GVariantBuilder builder;

  if (gtk_tree_model_get_iter_first (pd->lang_model, &iter)) {
    g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);

    do {
      char *code;

      gtk_tree_model_get (pd->lang_model, &iter,
                          COL_LANG_CODE, &code,
                          -1);
      g_variant_builder_add (&builder, "s", code);
      g_free (code);
    } while (gtk_tree_model_iter_next (pd->lang_model, &iter));

    g_settings_set (EPHY_SETTINGS_WEB,
                    EPHY_PREFS_WEB_LANGUAGE,
                    "as", &builder);
  } else {
    g_settings_set (EPHY_SETTINGS_WEB,
                    EPHY_PREFS_WEB_LANGUAGE,
                    "as", NULL);
  }
}

static void
language_editor_update_buttons (PrefsDialog *dialog)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  gboolean can_remove = FALSE, can_move_up = FALSE, can_move_down = FALSE;
  int selected;

  selection = gtk_tree_view_get_selection (dialog->lang_treeview);

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    path = gtk_tree_model_get_path (model, &iter);

    selected = gtk_tree_path_get_indices (path)[0];

    can_remove = TRUE;
    can_move_up = selected > 0;
    can_move_down =
      selected < gtk_tree_model_iter_n_children (model, NULL) - 1;

    gtk_tree_path_free (path);
  }

  gtk_widget_set_sensitive (dialog->lang_remove_button, can_remove);
  gtk_widget_set_sensitive (dialog->lang_up_button, can_move_up);
  gtk_widget_set_sensitive (dialog->lang_down_button, can_move_down);
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
add_lang_dialog_response_cb (GtkWidget   *widget,
                             int          response,
                             PrefsDialog *pd)
{
  GtkDialog *dialog = pd->add_lang_dialog;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GList *rows, *r;

  g_return_if_fail (dialog != NULL);

  if (response == GTK_RESPONSE_ACCEPT) {
    selection = gtk_tree_view_get_selection (pd->add_lang_treeview);

    rows = gtk_tree_selection_get_selected_rows (selection, &model);

    for (r = rows; r != NULL; r = r->next) {
      GtkTreePath *path = (GtkTreePath *)r->data;

      if (gtk_tree_model_get_iter (model, &iter, path)) {
        char *code, *desc;

        gtk_tree_model_get (model, &iter,
                            COL_LANG_NAME, &desc,
                            COL_LANG_CODE, &code,
                            -1);

        language_editor_add (pd, code, desc);

        g_free (desc);
        g_free (code);
      }
    }

    g_list_foreach (rows, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (rows);

    language_editor_update_pref (pd);
    language_editor_update_buttons (pd);
  }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static char *
get_name_for_lang_code (PrefsDialog *pd,
                        const char  *code)
{
  char **str;
  char *name;
  const char *langname, *localename;
  int len;

  str = g_strsplit (code, "-", -1);
  len = g_strv_length (str);
  g_return_val_if_fail (len != 0, NULL);

  langname = (const char *)g_hash_table_lookup (pd->iso_639_table, str[0]);

  if (len == 1 && langname != NULL) {
    name = g_strdup (dgettext (ISO_639_DOMAIN, langname));
  } else if (len == 2 && langname != NULL) {
    localename = (const char *)g_hash_table_lookup (pd->iso_3166_table, str[1]);

    if (localename != NULL) {
      /* Translators: the first %s is the language name, and the
       * second %s is the locale name. Example:
       * "French (France)"
       */
      name = g_strdup_printf (C_("language", "%s (%s)"),
                              dgettext (ISO_639_DOMAIN, langname),
                              dgettext (ISO_3166_DOMAIN, localename));
    } else {
      name = g_strdup_printf (C_("language", "%s (%s)"),
                              dgettext (ISO_639_DOMAIN, langname), str[1]);
    }
  } else {
    /* Translators: this refers to a user-define language code
     * (one which isn't in our built-in list).
     */
    name = g_strdup_printf (C_("language", "User defined (%s)"), code);
  }

  g_strfreev (str);

  return name;
}

static void
add_system_language_entry (GtkListStore *store)
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

static GtkDialog *
setup_add_language_dialog (PrefsDialog *dialog)
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
  guint i;
  GtkBuilder *builder;

  builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/prefs-lang-dialog.ui");
  ad = GTK_WIDGET (gtk_builder_get_object (builder, "add_language_dialog"));
  add_button = GTK_WIDGET (gtk_builder_get_object (builder, "add_button"));
  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder, "languages_treeview"));
  dialog->add_lang_treeview = treeview;

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  for (i = 0; i < G_N_ELEMENTS (languages); i++) {
    const char *code = languages[i];
    char *name;

    name = get_name_for_lang_code (dialog, code);

    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
                        COL_LANG_NAME, name,
                        COL_LANG_CODE, code,
                        -1);
    g_free (name);
  }

  add_system_language_entry (store);

  sortmodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_sortable_set_sort_column_id
    (GTK_TREE_SORTABLE (sortmodel), COL_LANG_NAME, GTK_SORT_ASCENDING);

  gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (dialog)),
                               GTK_WINDOW (ad));
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

  g_signal_connect (ad, "response",
                    G_CALLBACK (add_lang_dialog_response_cb), dialog);

  g_object_unref (store);
  g_object_unref (sortmodel);

  return GTK_DIALOG (ad);
}

static void
language_editor_add_button_clicked_cb (GtkWidget   *button,
                                       PrefsDialog *pd)
{
  if (pd->add_lang_dialog == NULL) {
    GtkDialog **add_lang_dialog;

    pd->add_lang_dialog = setup_add_language_dialog (pd);
    gtk_window_set_transient_for (GTK_WINDOW (pd->add_lang_dialog), GTK_WINDOW (pd));

    add_lang_dialog = &pd->add_lang_dialog;

    g_object_add_weak_pointer
      (G_OBJECT (pd->add_lang_dialog),
      (gpointer *)add_lang_dialog);
  }

  gtk_window_present (GTK_WINDOW (pd->add_lang_dialog));
}

static void
language_editor_remove_button_clicked_cb (GtkWidget   *button,
                                          PrefsDialog *pd)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;

  selection = gtk_tree_view_get_selection (pd->lang_treeview);

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
  }

  language_editor_update_pref (pd);
  language_editor_update_buttons (pd);
}

static void
language_editor_up_button_clicked_cb (GtkWidget   *button,
                                      PrefsDialog *pd)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter, iter_prev;
  GtkTreePath *path;

  selection = gtk_tree_view_get_selection (pd->lang_treeview);

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    path = gtk_tree_model_get_path (model, &iter);

    if (!gtk_tree_path_prev (path)) {
      gtk_tree_path_free (path);
      return;
    }

    gtk_tree_model_get_iter (model, &iter_prev, path);

    gtk_list_store_swap (GTK_LIST_STORE (model), &iter_prev, &iter);

    gtk_tree_path_free (path);
  }

  language_editor_update_pref (pd);
  language_editor_update_buttons (pd);
}

static void
language_editor_down_button_clicked_cb (GtkWidget   *button,
                                        PrefsDialog *pd)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter, iter_next;
  GtkTreePath *path;


  selection = gtk_tree_view_get_selection (pd->lang_treeview);

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    path = gtk_tree_model_get_path (model, &iter);

    gtk_tree_path_next (path);

    gtk_tree_model_get_iter (model, &iter_next, path);

    gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &iter_next);

    gtk_tree_path_free (path);
  }

  language_editor_update_pref (pd);
  language_editor_update_buttons (pd);
}

static void
language_editor_treeview_drag_end_cb (GtkWidget      *widget,
                                      GdkDragContext *context,
                                      PrefsDialog    *dialog)
{
  language_editor_update_pref (dialog);
  language_editor_update_buttons (dialog);
}

static void
language_editor_selection_changed_cb (GtkTreeSelection *selection,
                                      PrefsDialog      *dialog)
{
  language_editor_update_buttons (dialog);
}

static void
create_language_section (PrefsDialog *dialog)
{
  GtkListStore *store;
  GtkTreeView *treeview;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeSelection *selection;
  char **list = NULL;
  int i;

  dialog->iso_639_table = ephy_langs_iso_639_table ();
  dialog->iso_3166_table = ephy_langs_iso_3166_table ();

  g_signal_connect (dialog->lang_add_button, "clicked",
                    G_CALLBACK (language_editor_add_button_clicked_cb), dialog);
  g_signal_connect (dialog->lang_remove_button, "clicked",
                    G_CALLBACK (language_editor_remove_button_clicked_cb), dialog);
  g_signal_connect (dialog->lang_up_button, "clicked",
                    G_CALLBACK (language_editor_up_button_clicked_cb), dialog);
  g_signal_connect (dialog->lang_down_button, "clicked",
                    G_CALLBACK (language_editor_down_button_clicked_cb), dialog);

  /* setup the languages treeview */
  treeview = dialog->lang_treeview;

  gtk_tree_view_set_reorderable (treeview, TRUE);
  gtk_tree_view_set_headers_visible (treeview, FALSE);

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  dialog->lang_model = GTK_TREE_MODEL (store);
  gtk_tree_view_set_model (treeview, dialog->lang_model);

  renderer = gtk_cell_renderer_text_new ();

  gtk_tree_view_insert_column_with_attributes (treeview,
                                               0, _("Language"),
                                               renderer,
                                               "text", 0,
                                               NULL);
  column = gtk_tree_view_get_column (treeview, 0);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_column_set_sort_column_id (column, COL_LANG_NAME);

  selection = gtk_tree_view_get_selection (treeview);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  /* Connect treeview signals */
  g_signal_connect (G_OBJECT (treeview), "drag_end",
                    G_CALLBACK (language_editor_treeview_drag_end_cb), dialog);
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (language_editor_selection_changed_cb), dialog);

  list = g_settings_get_strv (EPHY_SETTINGS_WEB,
                              EPHY_PREFS_WEB_LANGUAGE);

  /* Fill languages editor */
  for (i = 0; list[i]; i++) {
    const char *code = (const char *)list[i];

    if (strcmp (code, "system") == 0) {
      add_system_language_entry (store);
    } else if (code[0] != '\0') {
      char *text;

      text = get_name_for_lang_code (dialog, code);
      language_editor_add (dialog, code, text);
      g_free (text);
    }
  }
  g_object_unref (store);

  language_editor_update_buttons (dialog);
  g_strfreev (list);

  /* Lockdown if key is not writable */
  g_settings_bind_writable (EPHY_SETTINGS_WEB,
                            EPHY_PREFS_WEB_LANGUAGE,
                            dialog->lang_add_button, "sensitive", FALSE);
  g_settings_bind_writable (EPHY_SETTINGS_WEB,
                            EPHY_PREFS_WEB_LANGUAGE,
                            dialog->lang_remove_button, "sensitive", FALSE);
  g_settings_bind_writable (EPHY_SETTINGS_WEB,
                            EPHY_PREFS_WEB_LANGUAGE,
                            dialog->lang_up_button, "sensitive", FALSE);
  g_settings_bind_writable (EPHY_SETTINGS_WEB,
                            EPHY_PREFS_WEB_LANGUAGE,
                            dialog->lang_down_button, "sensitive", FALSE);
  g_settings_bind_writable (EPHY_SETTINGS_WEB,
                            EPHY_PREFS_WEB_LANGUAGE,
                            dialog->lang_treeview, "sensitive", FALSE);
}

static void
download_path_changed_cb (GtkFileChooser *button)
{
  char *dir;

  dir = gtk_file_chooser_get_filename (button);
  if (dir == NULL) return;

  g_settings_set_string (EPHY_SETTINGS_STATE,
                         EPHY_PREFS_STATE_DOWNLOAD_DIR, dir);
  g_free (dir);
}

static void
create_download_path_button (PrefsDialog *dialog)
{
  GtkWidget *button;
  EphyFileChooser *fc;
  char *dir;

  dir = ephy_file_get_downloads_dir ();

  fc = ephy_file_chooser_new (_("Select a Directory"),
                              GTK_WIDGET (dialog),
                              GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                              EPHY_FILE_FILTER_NONE);

  /* Unset the destroy-with-parent, since gtkfilechooserbutton doesn't
   * expect this */
  gtk_window_set_destroy_with_parent (GTK_WINDOW (fc), FALSE);

  button = gtk_file_chooser_button_new_with_dialog (GTK_WIDGET (fc));
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (button), dir);
  gtk_file_chooser_button_set_width_chars (GTK_FILE_CHOOSER_BUTTON (button),
                                           DOWNLOAD_BUTTON_WIDTH);
  g_signal_connect (button, "selection-changed",
                    G_CALLBACK (download_path_changed_cb), dialog);
  gtk_label_set_mnemonic_widget (GTK_LABEL (dialog->download_button_label), button);
  gtk_box_pack_start (GTK_BOX (dialog->download_button_hbox), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  g_settings_bind_writable (EPHY_SETTINGS_STATE,
                            EPHY_PREFS_STATE_DOWNLOAD_DIR,
                            button, "sensitive", FALSE);
  g_free (dir);
}

static void
prefs_dialog_response_cb (GtkDialog *widget,
                          int        response,
                          GtkDialog *dialog)
{
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
clear_personal_data_button_clicked_cb (GtkWidget   *button,
                                       PrefsDialog *dialog)
{
  ClearDataDialog *clear_dialog;

  clear_dialog = g_object_new (EPHY_TYPE_CLEAR_DATA_DIALOG,
                               "use-header-bar", TRUE,
                               NULL);
  gtk_window_set_transient_for (GTK_WINDOW (clear_dialog), GTK_WINDOW (dialog));
  gtk_window_set_modal (GTK_WINDOW (clear_dialog), TRUE);
  gtk_window_present (GTK_WINDOW (clear_dialog));
}

static gboolean
cookies_get_mapping (GValue   *value,
                     GVariant *variant,
                     gpointer  user_data)
{
  const char *setting;
  const char *name;

  setting = g_variant_get_string (variant, NULL);
  name = gtk_buildable_get_name (GTK_BUILDABLE (user_data));

  if (g_strcmp0 (name, "no_third_party") == 0)
    name = "no-third-party";

  /* If the button name matches the setting, it should be active. */
  if (g_strcmp0 (name, setting) == 0)
    g_value_set_boolean (value, TRUE);

  return TRUE;
}

static GVariant *
cookies_set_mapping (const GValue       *value,
                     const GVariantType *expected_type,
                     gpointer            user_data)
{
  GVariant *variant = NULL;
  const char *name;

  /* Don't act unless the button has been activated (turned ON). */
  if (!g_value_get_boolean (value))
    return NULL;

  name = gtk_buildable_get_name (GTK_BUILDABLE (user_data));
  if (g_strcmp0 (name, "no_third_party") == 0)
    variant = g_variant_new_string ("no-third-party");
  else
    variant = g_variant_new_string (name);

  return variant;
}

static void
search_engine_combo_add_default_engines (GtkListStore *store)
{
  guint i;
  const char *default_engines[][3] = {       /* Search engine option in the preferences dialog */
    { N_("DuckDuckGo"),
      "https://duckduckgo.com/?q=%s&t=epiphany",
      /* For the preferences dialog. Must exactly match the URL
      * you chose in the gschema, but with & instead of &amp;
      * If the match is not exact, there will be a spurious, ugly
      * entry in the preferences combo, so please test this. */
      N_("https://duckduckgo.com/?q=%s&t=epiphany") },
    /* Search engine option in the preferences dialog */
    { N_("Google"),
      "https://google.com/search?q=%s",
      /* For the preferences dialog. Consider a regional variant, like google.co.uk */
      N_("https://google.com/search?q=%s") },
    /* Search engine option in the preferences dialog */
    { N_("Bing"),
      "https://www.bing.com/search?q=%s",
      /* For the preferences dialog. Consider a regional variant, like uk.bing.com */
      N_("https://www.bing.com/search?q=%s") }
  };

  for (i = 0; i < G_N_ELEMENTS (default_engines); ++i) {
    gtk_list_store_insert_with_values (store, NULL, -1,
                                       SEARCH_ENGINE_COL_NAME,
                                       _(default_engines[i][0]),
                                       SEARCH_ENGINE_COL_STOCK_URL,
                                       default_engines[i][1],
                                       SEARCH_ENGINE_COL_URL,
                                       _(default_engines[i][2]),
                                       -1);
  }
}

static void
search_engine_combo_add_smart_bookmarks (GtkListStore *store)
{
  guint i;
  EphyBookmarks *bookmarks;
  EphyNode *smart_bookmarks_parent_node;
  GPtrArray *smart_bookmarks;

  bookmarks = ephy_shell_get_bookmarks (ephy_shell_get_default ());
  smart_bookmarks_parent_node = ephy_bookmarks_get_smart_bookmarks (bookmarks);
  smart_bookmarks = ephy_node_get_children (smart_bookmarks_parent_node);

  if (!smart_bookmarks)
    return;

  for (i = 0; i < smart_bookmarks->len; ++i) {
    EphyNode *bookmark;
    const char *bookmark_name;
    const char *bookmark_url;

    bookmark = g_ptr_array_index (smart_bookmarks, i);
    bookmark_name = ephy_node_get_property_string (bookmark, EPHY_NODE_BMK_PROP_TITLE);
    bookmark_url = ephy_node_get_property_string (bookmark, EPHY_NODE_BMK_PROP_LOCATION);

    if (!bookmark_name || !bookmark_url)
      continue;

    if (strcmp (bookmark_name, DEFAULT_SMART_BOOKMARK_TEXT) == 0)
      continue;

    gtk_list_store_insert_with_values (store, NULL, -1,
                                       SEARCH_ENGINE_COL_NAME, bookmark_name,
                                       SEARCH_ENGINE_COL_STOCK_URL, bookmark_url,
                                       SEARCH_ENGINE_COL_URL, bookmark_url,
                                       -1);
  }
}

/* Has the user manually set the engine to something not in the combo?
 * If so, add that URL as an extra item in the combo. */
static void
search_engine_combo_add_current_engine (GtkListStore *store)
{
  GtkTreeIter iter;
  char *original_url;
  gboolean in_combo = FALSE;
  gboolean has_next = FALSE;

  original_url = g_settings_get_string (EPHY_SETTINGS_MAIN,
                                        EPHY_PREFS_KEYWORD_SEARCH_URL);
  if (!original_url)
    return;

  has_next = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);

  while (!in_combo && has_next) {
    char *stock_url, *url;

    gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                        SEARCH_ENGINE_COL_STOCK_URL, &stock_url,
                        SEARCH_ENGINE_COL_URL, &url, -1);

    if (strcmp (original_url, stock_url) == 0 ||
        strcmp (original_url, url) == 0)
      in_combo = TRUE;

    g_free (stock_url);
    g_free (url);
    has_next = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter);
  }

  if (!in_combo)
    gtk_list_store_insert_with_values (store, NULL, -1,
                                       SEARCH_ENGINE_COL_NAME, original_url,
                                       SEARCH_ENGINE_COL_STOCK_URL, original_url,
                                       SEARCH_ENGINE_COL_URL, original_url,
                                       -1);

  g_free (original_url);
}

static void
create_search_engine_combo (GtkComboBox *combo)
{
  GtkCellRenderer *renderer;
  GtkListStore *store;

  store = GTK_LIST_STORE (gtk_combo_box_get_model (combo));

  search_engine_combo_add_default_engines (store);
  search_engine_combo_add_smart_bookmarks (store);
  search_engine_combo_add_current_engine (store);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), SEARCH_ENGINE_COL_NAME,
                                        GTK_SORT_ASCENDING);
  gtk_combo_box_set_model (combo, GTK_TREE_MODEL (store));

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                  "text", SEARCH_ENGINE_COL_NAME,
                                  NULL);

  g_settings_bind_with_mapping (EPHY_SETTINGS_MAIN,
                                EPHY_PREFS_KEYWORD_SEARCH_URL,
                                combo, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                combo_get_mapping,
                                combo_set_mapping,
                                combo,
                                NULL);
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

static void
setup_general_page (PrefsDialog *dialog)
{
  GSettings *settings;
  GSettings *web_settings;

  settings = ephy_settings_get (EPHY_PREFS_SCHEMA);
  web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);

  g_settings_bind (settings,
                   EPHY_PREFS_AUTO_DOWNLOADS,
                   dialog->automatic_downloads_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind_with_mapping (settings,
                                EPHY_PREFS_RESTORE_SESSION_POLICY,
                                dialog->restore_session_checkbutton,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                restore_session_get_mapping,
                                restore_session_set_mapping,
                                NULL, NULL);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_POPUPS,
                   dialog->popups_allow_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_ADBLOCK,
                   dialog->adblock_allow_checkbutton,
                   "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_PLUGINS,
                   dialog->enable_plugins_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  create_download_path_button (dialog);
  create_search_engine_combo (GTK_COMBO_BOX (dialog->search_engine_combo));
}

static void
setup_fonts_page (PrefsDialog *dialog)
{
  GSettings *web_settings;

  web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_USE_GNOME_FONTS,
                   dialog->use_gnome_fonts_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_USE_GNOME_FONTS,
                   dialog->custom_fonts_table,
                   "sensitive",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_SANS_SERIF_FONT,
                   dialog->sans_fontbutton,
                   "font-name",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_SERIF_FONT,
                   dialog->serif_fontbutton,
                   "font-name",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_MONOSPACE_FONT,
                   dialog->mono_fontbutton,
                   "font-name",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_USER_CSS,
                   dialog->css_checkbox,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_USER_CSS,
                   dialog->css_edit_button,
                   "sensitive",
                   G_SETTINGS_BIND_GET);
  g_signal_connect (dialog->css_edit_button,
                    "clicked",
                    G_CALLBACK (css_edit_button_clicked_cb),
                    dialog);
}

static void
setup_privacy_page (PrefsDialog *dialog)
{
  GSettings *settings;
  GSettings *web_settings;

  settings = ephy_settings_get (EPHY_PREFS_SCHEMA);
  web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);

  g_settings_bind_with_mapping (web_settings,
                                EPHY_PREFS_WEB_COOKIES_POLICY,
                                dialog->always,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                cookies_get_mapping,
                                cookies_set_mapping,
                                dialog->always,
                                NULL);
  g_settings_bind_with_mapping (web_settings,
                                EPHY_PREFS_WEB_COOKIES_POLICY,
                                dialog->no_third_party,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                cookies_get_mapping,
                                cookies_set_mapping,
                                dialog->no_third_party,
                                NULL);
  g_settings_bind_with_mapping (web_settings,
                                EPHY_PREFS_WEB_COOKIES_POLICY,
                                dialog->never,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                cookies_get_mapping,
                                cookies_set_mapping,
                                dialog->never,
                                NULL);
  g_settings_bind (settings,
                   EPHY_PREFS_REMEMBER_PASSWORDS,
                   dialog->remember_passwords_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_DO_NOT_TRACK,
                   dialog->do_not_track_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect (dialog->clear_personal_data_button,
                    "clicked",
                    G_CALLBACK (clear_personal_data_button_clicked_cb),
                    dialog);
}

static void
setup_language_page (PrefsDialog *dialog)
{
  GSettings *web_settings;

  web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_SPELL_CHECKING,
                   dialog->enable_spell_checking_checkbutton,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  create_language_section (dialog);
}

static void
setup_sync_page (PrefsDialog *dialog)
{
  GtkWidget *sync_main_label;
  GtkWidget *sync_secondary_label;
  gchar *sync_user = NULL;
  gboolean logged_in;

  sync_main_label = gtk_label_new (NULL);
  gtk_widget_set_visible (sync_main_label, TRUE);
  gtk_widget_set_halign (sync_main_label, GTK_ALIGN_START);
  gtk_label_set_markup (GTK_LABEL (sync_main_label),
                        _("Log in with your "
                          "<a href=\"https://www.mozilla.org/en-US/firefox/accounts/\" "
                          "title=\"Get a Firefox Account\">Firefox Account</a> "
                          "and have your data synced across all your devices.\n"));

  sync_secondary_label = gtk_label_new (NULL);
  gtk_widget_set_visible (sync_secondary_label, TRUE);
  gtk_widget_set_halign (sync_secondary_label, GTK_ALIGN_START);
  gtk_label_set_markup (GTK_LABEL (sync_secondary_label),
                        _("Note that you must own an <b>already verified account</b> "
                          "to be able to login."));

  gtk_box_pack_start (GTK_BOX (dialog->sync_top_box),
                      sync_main_label,
                      TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (dialog->sync_top_box),
                      sync_secondary_label,
                      TRUE, TRUE, 0);

  sync_user = g_settings_get_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_SYNC_USER);
  logged_in = sync_user && sync_user[0];

  if (logged_in) {
LOG ("[%d] Setup sync page, already logged in as %s", __LINE__, sync_user);
    gtk_container_remove (GTK_CONTAINER (dialog->sync_authenticate_box),
                          dialog->sync_login_grid);
    /* Translators: the %s refers to the email of the currently logged in user. */
    gtk_label_set_markup (GTK_LABEL (dialog->sync_logout_details_label),
                          g_strdup_printf (_("Currently logged in as <b>%s</b>"), sync_user));
  } else {
LOG ("[%d] Setup sync page, not logged in", __LINE__);
    gtk_container_remove (GTK_CONTAINER (dialog->sync_authenticate_box),
                          dialog->sync_logout_box);
  }
}

static void
prefs_dialog_init (PrefsDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));

  setup_general_page (dialog);
  setup_fonts_page (dialog);
  setup_privacy_page (dialog);
  setup_language_page (dialog);
  setup_sync_page (dialog);

  ephy_gui_ensure_window_group (GTK_WINDOW (dialog));
  g_signal_connect (dialog, "response",
                    G_CALLBACK (prefs_dialog_response_cb), dialog);
}
