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

#include "prefs-general-page.h"

#include "ephy-embed-shell.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-flatpak-utils.h"
#include "ephy-lang-row.h"
#include "ephy-langs.h"
#include "ephy-settings.h"
#include "ephy-search-engine-listbox.h"
#include "ephy-web-app-utils.h"
#include "webapp-additional-urls-dialog.h"

#include "gnome-languages.h"
#include <glib/gi18n.h>

enum {
  COL_LANG_NAME,
  COL_LANG_CODE
};

struct _PrefsGeneralPage {
  HdyPreferencesPage parent_instance;

  /* Web Application */
  EphyWebApplication *webapp;
  guint webapp_save_id;
  GtkWidget *webapp_box;
  GtkWidget *webapp_icon;
  GtkWidget *webapp_icon_row;
  GtkWidget *webapp_url;
  GtkWidget *webapp_url_row;
  GtkWidget *webapp_title;
  GtkWidget *webapp_title_row;

  /* Web Content */
  GtkWidget *adblock_allow_switch;
  GtkWidget *popups_allow_switch;

  /* Homepage */
  GtkWidget *homepage_box;
  GtkWidget *new_tab_homepage_radiobutton;
  GtkWidget *blank_homepage_radiobutton;
  GtkWidget *custom_homepage_radiobutton;
  GtkWidget *custom_homepage_entry;

  /* Downloads */
  GtkWidget *download_box;
  GtkWidget *ask_on_download_switch;
  GtkWidget *download_folder_row;
  GtkWidget *download_folder_label;

  /* Search Engines */
  GtkWidget *search_engine_group;

  /* Session */
  GtkWidget *session_box;
  GtkWidget *start_in_incognito_mode_switch;
  GtkWidget *restore_session_row;
  GtkWidget *restore_session_switch;

  /* Browsing */
  GtkWidget *browsing_box;
  GtkWidget *enable_mouse_gesture_switch;
  GtkWidget *enable_switch_to_new_tab;

  /* Languages */
  HdyPreferencesGroup *lang_group;
  GtkWidget *lang_listbox;
  GtkWidget *enable_spell_checking_switch;

  GtkDialog *add_lang_dialog;
  GtkTreeView *add_lang_treeview;
};

G_DEFINE_TYPE (PrefsGeneralPage, prefs_general_page, HDY_TYPE_PREFERENCES_PAGE)

static void
prefs_general_page_finalize (GObject *object)
{
  PrefsGeneralPage *general_page = EPHY_PREFS_GENERAL_PAGE (object);

  if (general_page->add_lang_dialog != NULL) {
    GtkDialog **add_lang_dialog = &general_page->add_lang_dialog;

    g_object_remove_weak_pointer (G_OBJECT (general_page->add_lang_dialog),
                                  (gpointer *)add_lang_dialog);
    g_object_unref (general_page->add_lang_dialog);
  }

  g_clear_pointer (&general_page->webapp, ephy_web_application_free);
  G_OBJECT_CLASS (prefs_general_page_parent_class)->finalize (object);
}

static int
get_list_box_length (GtkListBox *list_box)
{
  int i = -1;

  while (gtk_list_box_get_row_at_index (list_box, ++i));

  return i;
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

static void
drag_data_received (GtkWidget        *widget,
                    GdkDragContext   *context,
                    gint              x,
                    gint              y,
                    GtkSelectionData *selection_data,
                    guint             info,
                    guint32           time,
                    gpointer          data)
{
  PrefsGeneralPage *general_page = data;
  GtkListBox *lang_listbox = GTK_LIST_BOX (general_page->lang_listbox);
  GtkWidget *hovered_row;
  GtkWidget *dragged_row;
  int length;
  int hovered_row_pos;
  int dragged_row_pos;
  int insert_pos;

  hovered_row = GTK_WIDGET (gtk_list_box_get_row_at_y (lang_listbox, y));
  dragged_row = (gpointer) * (gpointer *)gtk_selection_data_get_data (selection_data);

  length = get_list_box_length (GTK_LIST_BOX (widget));
  hovered_row_pos = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (hovered_row));
  dragged_row_pos = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (dragged_row));

  /* If the drag ended while hovering over the dragged row then we return */
  if (hovered_row == dragged_row)
    return;

  /* If the drag ended while hovering over the "Add Language" row then we return */
  if (hovered_row_pos == length - 1)
    return;

  g_object_ref (dragged_row);
  gtk_container_remove (GTK_CONTAINER (lang_listbox), dragged_row);

  /* Calculation logic for insert_pos */
  if (y < 20) {
    insert_pos = 0;
  } else if (dragged_row_pos < hovered_row_pos) {
    insert_pos = hovered_row_pos;
  } else { /* dragged_row_pos > hovered_row_pos is true here */
    insert_pos = hovered_row_pos + 1;
  }

  gtk_list_box_insert (lang_listbox, dragged_row, insert_pos);
  g_object_unref (dragged_row);

  language_editor_update_pref (general_page);
}

static gboolean
drag_motion (GtkWidget      *widget,
             GdkDragContext *context,
             int             x,
             int             y,
             guint           time)
{
  GtkListBoxRow *first_lang_row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (widget), 0);

  GtkWidget *dragged_row;
  GtkWidget *hovered_row;
  GtkWidget *expanded_dnd_revealer;
  GtkWidget *hovered_row_dnd_revealer = NULL;

  dragged_row = g_object_get_data (G_OBJECT (widget), "dragged-row");
  hovered_row = GTK_WIDGET (gtk_list_box_get_row_at_y (GTK_LIST_BOX (widget), y));
  expanded_dnd_revealer = g_object_get_data (G_OBJECT (widget), "dnd-expanded-revealer");

  if (EPHY_IS_LANG_ROW (hovered_row))
    hovered_row_dnd_revealer = ephy_lang_row_get_dnd_bottom_revealer (EPHY_LANG_ROW (hovered_row));

  /* Edge case: If the user is hovering with the dragged row close to the
   * top of the list, then we reveal the top empty row of EphyLangRow */
  if (y < 20 && dragged_row != GTK_WIDGET (first_lang_row)) {
    GtkWidget *top_revealer = ephy_lang_row_get_dnd_top_revealer (EPHY_LANG_ROW (hovered_row));

    if (expanded_dnd_revealer)
      gtk_revealer_set_reveal_child (GTK_REVEALER (expanded_dnd_revealer), FALSE);

    gtk_revealer_set_reveal_child (GTK_REVEALER (top_revealer), TRUE);
    g_object_set_data (G_OBJECT (widget), "dnd-expanded-revealer", top_revealer);

    return TRUE;
  }

  if (dragged_row == hovered_row) {
    if (expanded_dnd_revealer)
      gtk_revealer_set_reveal_child (GTK_REVEALER (expanded_dnd_revealer), FALSE);

    g_object_set_data (G_OBJECT (widget), "dnd-expanded-revealer", NULL);

    return TRUE;
  }

  if (EPHY_IS_LANG_ROW (hovered_row)) {
    if (expanded_dnd_revealer)
      gtk_revealer_set_reveal_child (GTK_REVEALER (expanded_dnd_revealer), FALSE);

    gtk_revealer_set_reveal_child (GTK_REVEALER (hovered_row_dnd_revealer), TRUE);
    g_object_set_data (G_OBJECT (widget), "dnd-expanded-revealer", hovered_row_dnd_revealer);
    return TRUE;
  } else {
    if (expanded_dnd_revealer)
      gtk_revealer_set_reveal_child (GTK_REVEALER (expanded_dnd_revealer), FALSE);

    g_object_set_data (G_OBJECT (widget), "dnd-expanded-revealer", NULL);
    return FALSE;
  }
}

static GtkDialog *setup_add_language_dialog (PrefsGeneralPage *general_page);

static void
language_editor_add_button_release_event (GtkWidget        *button,
                                          GdkEvent         *event,
                                          PrefsGeneralPage *general_page)
{
  if (general_page->add_lang_dialog == NULL) {
    GtkWindow *prefs_dialog;
    GtkDialog **add_lang_dialog;

    prefs_dialog = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (general_page)));
    general_page->add_lang_dialog = setup_add_language_dialog (general_page);
    gtk_window_set_transient_for (GTK_WINDOW (general_page->add_lang_dialog), prefs_dialog);

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
  GtkWidget *box;
  GtkWidget *label;

  box = gtk_event_box_new ();
  label = gtk_label_new (_("Add Language"));
  g_signal_connect (box, "button-release-event", G_CALLBACK (language_editor_add_button_release_event), general_page);
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_widget_set_size_request (box, -1, 50);
  gtk_widget_show_all (GTK_WIDGET (box));

  gtk_list_box_insert (GTK_LIST_BOX (general_page->lang_listbox), box, -1);
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
  gtk_container_remove (GTK_CONTAINER (general_page->lang_listbox), GTK_WIDGET (row));
  language_editor_update_pref (general_page);
  language_editor_update_state (general_page);
}

void
drag_data_get (GtkWidget        *widget,
               GdkDragContext   *context,
               GtkSelectionData *selection_data,
               guint             info,
               guint             time,
               gpointer          data)
{
  GtkWidget *row = gtk_widget_get_ancestor (widget, EPHY_TYPE_LANG_ROW);

  gtk_selection_data_set (selection_data,
                          gdk_atom_intern_static_string ("EPHY_LANG_ROW"),
                          32,
                          (const guchar *)&row,
                          sizeof (gpointer));
}

static void
drag_begin (GtkWidget      *widget,
            GdkDragContext *context,
            gpointer        data)
{
  GtkWidget *row;
  GtkAllocation alloc;
  cairo_surface_t *surface;
  cairo_t *cr;
  int x, y;
  double sx, sy;

  row = gtk_widget_get_ancestor (widget, EPHY_TYPE_LANG_ROW);
  gtk_widget_get_allocation (row, &alloc);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, alloc.width, alloc.height);
  cr = cairo_create (surface);

  gtk_style_context_add_class (gtk_widget_get_style_context (row), "drag-icon");
  gtk_widget_draw (row, cr);
  gtk_style_context_remove_class (gtk_widget_get_style_context (row), "drag-icon");

  gtk_widget_translate_coordinates (widget, row, 0, 0, &x, &y);
  cairo_surface_get_device_scale (surface, &sx, &sy);
  cairo_surface_set_device_offset (surface, -x * sy, -y * sy);
  gtk_drag_set_icon_surface (context, surface);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  g_object_set_data (G_OBJECT (gtk_widget_get_parent (row)), "dragged-row", row);
  gtk_style_context_add_class (gtk_widget_get_style_context (row), "drag-row");
}

static void
drag_end (GtkWidget      *widget,
          GdkDragContext *context,
          gpointer        data)
{
  GtkWidget *lang_row;
  GtkWidget *lang_listbox;
  GtkWidget *dnd_expanded_revealer;

  lang_row = gtk_widget_get_ancestor (widget, EPHY_TYPE_LANG_ROW);
  lang_listbox = gtk_widget_get_parent (lang_row);
  dnd_expanded_revealer = g_object_get_data (G_OBJECT (lang_listbox), "dnd-expanded-revealer");

  g_object_set_data (G_OBJECT (lang_listbox), "dragged-row", NULL);
  gtk_style_context_remove_class (gtk_widget_get_style_context (lang_row), "drag-row");

  if (dnd_expanded_revealer) {
    gtk_revealer_set_reveal_child (GTK_REVEALER (dnd_expanded_revealer), FALSE);
    g_object_set_data (G_OBJECT (lang_listbox), "dnd-expanded-revealer", NULL);
  }
}

static GtkTargetEntry entries[] = {
  { "GTK_LIST_BOX_ROW", GTK_TARGET_SAME_APP, 0 }
};

static void
language_editor_add (PrefsGeneralPage *general_page,
                     const char       *code,
                     const char       *desc)
{
  GtkWidget *row;
  GtkWidget *event_box;
  int len;
  int index;

  g_assert (code != NULL && desc != NULL);

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
  ephy_lang_row_set_title (EPHY_LANG_ROW (row), desc);
  gtk_style_context_add_class (gtk_widget_get_style_context (row), "row");

  event_box = ephy_lang_row_get_drag_event_box (EPHY_LANG_ROW (row));
  gtk_drag_source_set (event_box, GDK_BUTTON1_MASK, entries, 1, GDK_ACTION_MOVE);
  g_signal_connect (event_box, "drag-begin", G_CALLBACK (drag_begin), general_page);
  g_signal_connect (event_box, "drag-end", G_CALLBACK (drag_end), general_page);
  g_signal_connect (event_box, "drag-data-get", G_CALLBACK (drag_data_get), general_page);

  g_signal_connect (row, "delete-button-clicked", G_CALLBACK (language_editor_delete_button_clicked_cb), general_page);

  gtk_list_box_insert (GTK_LIST_BOX (general_page->lang_listbox), row, len - 1);
}

static void
add_lang_dialog_response_cb (GtkWidget        *widget,
                             int               response,
                             PrefsGeneralPage *general_page)
{
  GtkDialog *dialog = general_page->add_lang_dialog;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GList *rows, *r;

  g_assert (dialog != NULL);

  if (response == GTK_RESPONSE_ACCEPT) {
    selection = gtk_tree_view_get_selection (general_page->add_lang_treeview);

    rows = gtk_tree_selection_get_selected_rows (selection, &model);

    for (r = rows; r != NULL; r = r->next) {
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
  }

  gtk_widget_destroy (GTK_WIDGET (dialog));
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

static GtkDialog *
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
  GtkBuilder *builder;
  GtkWidget *prefs_dialog;
  g_auto (GStrv) locales;

  builder = gtk_builder_new_from_resource ("/org/gnome/epiphany/gtk/prefs-lang-dialog.ui");
  prefs_dialog = gtk_widget_get_toplevel (GTK_WIDGET (general_page));
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

    if (language_code == NULL)
      break;

    language_name = gnome_get_language_from_locale (locale, locale);

    if (country_code != NULL)
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

  gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (prefs_dialog)),
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
                    G_CALLBACK (add_lang_dialog_response_cb), general_page);

  g_object_unref (store);
  g_object_unref (sortmodel);

  return GTK_DIALOG (ad);
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
  for (char *p = strchr (result, '-'); p != NULL && *p != '\0'; p++)
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
download_folder_file_chooser_cb (GtkNativeDialog  *chooser,
                                 GtkResponseType   response,
                                 PrefsGeneralPage *general_page)
{
  if (response == GTK_RESPONSE_ACCEPT) {
    g_autoptr (GFile) file = NULL;
    g_autofree char *path = NULL;

    file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (chooser));
    path = g_file_get_path (file);

    if (path)
      g_settings_set_string (EPHY_SETTINGS_STATE,
                             EPHY_PREFS_STATE_DOWNLOAD_DIR, path);
  }

  gtk_native_dialog_destroy (chooser);
}

static void
download_folder_row_activated_cb (PrefsGeneralPage *general_page)
{
  GtkWidget *parent;
  g_autofree char *downloads_path = NULL;
  GtkFileChooserNative *chooser;

  parent = gtk_widget_get_toplevel (GTK_WIDGET (general_page));
  chooser = gtk_file_chooser_native_new (_("Select a Directory"),
                                         GTK_WINDOW (parent),
                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                         _("_Select"),
                                         _("_Cancel"));
  gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (chooser), TRUE);

  downloads_path = ephy_file_get_downloads_dir ();

  if (downloads_path && downloads_path[0]) {
    g_autoptr (GFile) downloads_dir = NULL;
    g_autoptr (GError) error = NULL;

    downloads_dir = g_file_new_for_path (downloads_path);

    gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (chooser),
                                              downloads_dir,
                                              &error);

    if (error)
      g_warning ("Failed to set current folder %s: %s", downloads_path, error->message);
  }

  g_signal_connect (chooser, "response",
                    G_CALLBACK (download_folder_file_chooser_cb),
                    general_page);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (chooser));
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

  if (!general_page->webapp)
    return G_SOURCE_REMOVE;

  text = gtk_entry_get_text (GTK_ENTRY (general_page->webapp_url));
  if (g_strcmp0 (general_page->webapp->url, text) != 0) {
    g_free (general_page->webapp->url);
    general_page->webapp->url = g_strdup (text);
    changed = TRUE;
  }

  text = gtk_entry_get_text (GTK_ENTRY (general_page->webapp_title));
  if (g_strcmp0 (general_page->webapp->name, text) != 0) {
    g_free (general_page->webapp->name);
    general_page->webapp->name = g_strdup (text);
    changed = TRUE;
  }

  text = (const char *)g_object_get_data (G_OBJECT (general_page->webapp_icon), "ephy-webapp-icon-path");
  if (g_strcmp0 (general_page->webapp->icon_path, text) != 0) {
    g_free (general_page->webapp->icon_path);
    general_page->webapp->icon_path = g_strdup (text);
    changed = TRUE;
  }

  if (changed)
    ephy_web_application_save (general_page->webapp);

  return G_SOURCE_REMOVE;
}

static void
prefs_general_page_save_web_application (PrefsGeneralPage *general_page)
{
  if (!general_page->webapp)
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

  gtk_image_set_from_gicon (GTK_IMAGE (general_page->webapp_icon),
                            G_ICON (icon),
                            GTK_ICON_SIZE_DND);
  gtk_image_set_pixel_size (GTK_IMAGE (general_page->webapp_icon), 32);

  g_object_set_data_full (G_OBJECT (general_page->webapp_icon), "ephy-webapp-icon-path",
                          g_strdup (icon_path), g_free);
}

static void
webapp_icon_chooser_response_cb (GtkNativeDialog  *file_chooser,
                                 int               response,
                                 PrefsGeneralPage *general_page)
{
  if (response == GTK_RESPONSE_ACCEPT) {
    g_autoptr (GFile) icon_file = NULL;
    g_autofree char *icon_path = NULL;

    icon_file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (file_chooser));
    icon_path = g_file_get_path (icon_file);
    prefs_general_page_update_webapp_icon (general_page, icon_path);
    prefs_general_page_save_web_application (general_page);
  }

  g_object_unref (file_chooser);
}

static void
on_webapp_entry_changed (GtkEditable      *editable,
                         PrefsGeneralPage *dialog)
{
  prefs_general_page_save_web_application (dialog);
}

void
prefs_general_page_on_pd_delete_event (PrefsGeneralPage *general_page)
{
  if (general_page->webapp_save_id) {
    g_source_remove (general_page->webapp_save_id);
    general_page->webapp_save_id = 0;
    save_web_application (general_page);
  }
}

static void
on_webapp_icon_button_clicked (GtkWidget        *button,
                               PrefsGeneralPage *general_page)
{
  GtkFileChooser *file_chooser;
  GSList *pixbuf_formats, *l;
  GtkFileFilter *images_filter;

  file_chooser = ephy_create_file_chooser (_("Web Application Icon"),
                                           GTK_WIDGET (general_page),
                                           GTK_FILE_CHOOSER_ACTION_OPEN,
                                           EPHY_FILE_FILTER_NONE);
  images_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (images_filter, _("Supported Image Files"));
  gtk_file_chooser_add_filter (file_chooser, images_filter);

  pixbuf_formats = gdk_pixbuf_get_formats ();
  for (l = pixbuf_formats; l; l = g_slist_next (l)) {
    GdkPixbufFormat *format = (GdkPixbufFormat *)l->data;
    GtkFileFilter *filter;
    gchar *name;
    gchar **mime_types;
    guint i;

    if (gdk_pixbuf_format_is_disabled (format) || !gdk_pixbuf_format_is_writable (format))
      continue;

    filter = gtk_file_filter_new ();
    name = gdk_pixbuf_format_get_description (format);
    gtk_file_filter_set_name (filter, name);
    g_free (name);

    mime_types = gdk_pixbuf_format_get_mime_types (format);
    for (i = 0; mime_types[i] != 0; i++) {
      gtk_file_filter_add_mime_type (images_filter, mime_types[i]);
      gtk_file_filter_add_mime_type (filter, mime_types[i]);
    }
    g_strfreev (mime_types);

    gtk_file_chooser_add_filter (file_chooser, filter);
  }
  g_slist_free (pixbuf_formats);

  g_signal_connect (file_chooser, "response",
                    G_CALLBACK (webapp_icon_chooser_response_cb),
                    general_page);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (file_chooser));
}

static void
custom_homepage_entry_changed (GtkEntry         *entry,
                               PrefsGeneralPage *general_page)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (general_page->custom_homepage_radiobutton))) {
    g_settings_set_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_HOMEPAGE_URL,
                           gtk_entry_get_text (entry));
  } else if ((gtk_entry_get_text (entry) != NULL) &&
             gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (general_page->new_tab_homepage_radiobutton))) {
    g_settings_set_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_HOMEPAGE_URL, gtk_entry_get_text (entry));
    gtk_widget_set_sensitive (general_page->custom_homepage_entry, TRUE);
    gtk_widget_grab_focus (general_page->custom_homepage_entry);
  }
}

static void
custom_homepage_entry_icon_released (GtkEntry             *entry,
                                     GtkEntryIconPosition  icon_pos)
{
  if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
    gtk_entry_set_text (entry, "");
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
  gtk_entry_set_text (GTK_ENTRY (general_page->custom_homepage_entry), "");
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

  gtk_entry_set_text (GTK_ENTRY (general_page->custom_homepage_entry), "");

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
    gtk_entry_set_text (GTK_ENTRY (general_page->custom_homepage_entry), "");
    return NULL;
  }

  gtk_widget_set_sensitive (general_page->custom_homepage_entry, TRUE);
  gtk_widget_grab_focus (general_page->custom_homepage_entry);
  setting = gtk_entry_get_text (GTK_ENTRY (general_page->custom_homepage_entry));
  if (!setting || setting[0] == '\0')
    return NULL;

  gtk_entry_set_text (GTK_ENTRY (general_page->custom_homepage_entry), setting);

  return g_variant_new_string (setting);
}

static void
on_manage_webapp_additional_urls_button_clicked (GtkWidget        *button,
                                                 PrefsGeneralPage *general_page)
{
  EphyWebappAdditionalURLsDialog *urls_dialog;
  GtkWindow *prefs_dialog;

  urls_dialog = ephy_webapp_additional_urls_dialog_new ();
  prefs_dialog = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (general_page)));

  gtk_window_set_transient_for (GTK_WINDOW (urls_dialog), prefs_dialog);
  gtk_window_set_modal (GTK_WINDOW (urls_dialog), TRUE);
  gtk_window_present (GTK_WINDOW (urls_dialog));
}

static void
prefs_general_page_class_init (PrefsGeneralPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = prefs_general_page_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-general-page.ui");

  /* Web Application */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, webapp_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, webapp_icon);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, webapp_icon_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, webapp_url);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, webapp_url_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, webapp_title);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, webapp_title_row);

  /* Web Content */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, adblock_allow_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, popups_allow_switch);

  /* Homepage */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, homepage_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, new_tab_homepage_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, blank_homepage_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, custom_homepage_radiobutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, custom_homepage_entry);

  /* Downloads */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, download_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, ask_on_download_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, download_folder_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, download_folder_label);

  /* Search Engines */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, search_engine_group);

  /* Session */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, session_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, start_in_incognito_mode_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, restore_session_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, restore_session_switch);

  /* Browsing */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, browsing_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, enable_mouse_gesture_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, enable_switch_to_new_tab);

  /* Languages */
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, lang_group);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, lang_listbox);
  gtk_widget_class_bind_template_child (widget_class, PrefsGeneralPage, enable_spell_checking_switch);

  /* Signals */
  gtk_widget_class_bind_template_callback (widget_class, on_webapp_icon_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_webapp_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_manage_webapp_additional_urls_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, download_folder_row_activated_cb);
}

static void
init_lang_listbox (PrefsGeneralPage *general_page)
{
  char **list = NULL;
  int i;

  gtk_drag_dest_set (general_page->lang_listbox, GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP, entries, 1, GDK_ACTION_MOVE);
  g_signal_connect (general_page->lang_listbox, "drag-data-received", G_CALLBACK (drag_data_received), general_page);
  g_signal_connect (general_page->lang_listbox, "drag-motion", G_CALLBACK (drag_motion), NULL);

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
      if (normalized_locale != NULL) {
        g_autofree char *language_name = language_for_locale (normalized_locale);
        if (language_name == NULL)
          language_name = g_strdup (normalized_locale);
        language_editor_add (general_page, normalized_locale, language_name);
      }
    }
  }
}

static void
setup_general_page (PrefsGeneralPage *general_page)
{
  GSettings *settings = ephy_settings_get (EPHY_PREFS_SCHEMA);
  GSettings *web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);

  /* ======================================================================== */
  /* ========================== Web Application ============================= */
  /* ======================================================================== */
  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_APPLICATION &&
      !ephy_is_running_inside_sandbox ()) {
    general_page->webapp = ephy_web_application_for_profile_directory (ephy_profile_dir (),
                                                                       EPHY_WEB_APP_NO_TMP_ICON);
    g_assert (general_page->webapp);

    if (!g_settings_get_boolean (EPHY_SETTINGS_WEB_APP, EPHY_PREFS_WEB_APP_SYSTEM)) {
      prefs_general_page_update_webapp_icon (general_page, general_page->webapp->icon_path);
      gtk_entry_set_text (GTK_ENTRY (general_page->webapp_url), general_page->webapp->url);
      gtk_entry_set_text (GTK_ENTRY (general_page->webapp_title), general_page->webapp->name);
    }
  }

  /* ======================================================================== */
  /* ========================== Web Content ================================= */
  /* ======================================================================== */
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_ADBLOCK,
                   general_page->adblock_allow_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_POPUPS,
                   general_page->popups_allow_switch,
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

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (general_page->custom_homepage_radiobutton))) {
    gtk_widget_set_sensitive (general_page->custom_homepage_entry, TRUE);
    gtk_entry_set_text (GTK_ENTRY (general_page->custom_homepage_entry),
                        g_settings_get_string (EPHY_SETTINGS_MAIN, EPHY_PREFS_HOMEPAGE_URL));
  } else {
    gtk_widget_set_sensitive (general_page->custom_homepage_entry, FALSE);
    gtk_entry_set_text (GTK_ENTRY (general_page->custom_homepage_entry), "");
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
    gtk_widget_hide (general_page->download_box);
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
                   general_page->ask_on_download_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* ======================================================================== */
  /* ========================== Session ===================================== */
  /* ======================================================================== */
  g_settings_bind (settings,
                   EPHY_PREFS_START_IN_INCOGNITO_MODE,
                   general_page->start_in_incognito_mode_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (settings,
                   EPHY_PREFS_START_IN_INCOGNITO_MODE,
                   general_page->restore_session_row,
                   "sensitive",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_settings_bind_with_mapping (settings,
                                EPHY_PREFS_RESTORE_SESSION_POLICY,
                                general_page->restore_session_switch,
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
                   general_page->enable_mouse_gesture_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_SWITCH_TO_NEW_TAB,
                   general_page->enable_switch_to_new_tab,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* ======================================================================== */
  /* ========================== Languages =================================== */
  /* ======================================================================== */
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_SPELL_CHECKING,
                   general_page->enable_spell_checking_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  init_lang_listbox (general_page);
}

static void
prefs_general_page_init (PrefsGeneralPage *general_page)
{
  EphyEmbedShellMode mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());

  g_type_ensure (EPHY_TYPE_SEARCH_ENGINE_LIST_BOX);

  gtk_widget_init_template (GTK_WIDGET (general_page));

  setup_general_page (general_page);

  gtk_widget_set_visible (general_page->webapp_box,
                          mode == EPHY_EMBED_SHELL_MODE_APPLICATION &&
                          !g_settings_get_boolean (EPHY_SETTINGS_WEB_APP,
                                                   EPHY_PREFS_WEB_APP_SYSTEM));
  gtk_widget_set_visible (general_page->webapp_icon_row, !ephy_is_running_inside_sandbox ());
  gtk_widget_set_visible (general_page->webapp_url_row, !ephy_is_running_inside_sandbox ());
  gtk_widget_set_visible (general_page->webapp_title_row, !ephy_is_running_inside_sandbox ());
  gtk_widget_set_visible (general_page->homepage_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (general_page->search_engine_group,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (general_page->session_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
  gtk_widget_set_visible (general_page->browsing_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
}
