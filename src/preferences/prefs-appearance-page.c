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

#include "prefs-appearance-page.h"

#include "ephy-embed-prefs.h"
#include "ephy-file-helpers.h"
#include "ephy-flatpak-utils.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-settings.h"
#include "ephy-shell.h"

#include <math.h>

struct _PrefsAppearancePage {
  HdyPreferencesPage parent_instance;

  /* Fonts */
  GtkWidget *use_gnome_fonts_row;
  GtkWidget *sans_fontbutton;
  GtkWidget *serif_fontbutton;
  GtkWidget *mono_fontbutton;

  /* Reader Mode */
  GtkWidget *reader_mode_box;
  GtkWidget *reader_mode_font_style;
  GtkWidget *reader_mode_color_scheme;

  /* Style */
  GtkWidget *css_switch;
  GtkWidget *css_edit_button;
  GtkWidget *js_switch;
  GtkWidget *js_edit_button;
  GtkWidget *default_zoom_spin_button;
};

G_DEFINE_TYPE (PrefsAppearancePage, prefs_appearance_page, HDY_TYPE_PREFERENCES_PAGE)

static gchar *
reader_font_style_get_name (HdyEnumValueObject *value,
                            gpointer            user_data)
{
  g_assert (HDY_IS_ENUM_VALUE_OBJECT (value));

  switch (hdy_enum_value_object_get_value (value)) {
    case EPHY_PREFS_READER_FONT_STYLE_SANS:
      return g_strdup_printf ("<span font-family=\"%s\">%s</span>", "sans", _("Sans"));
    case EPHY_PREFS_READER_FONT_STYLE_SERIF:
      return g_strdup_printf ("<span font-family=\"%s\">%s</span>", "serif", _("Serif"));
    default:
      return NULL;
  }
}

static GtkWidget *
reader_font_style_create_list_widget (gpointer item,
                                      gpointer user_data)
{
  g_autofree gchar *name = reader_font_style_get_name (item, NULL);

  return g_object_new (GTK_TYPE_LABEL,
                       "ellipsize", PANGO_ELLIPSIZE_END,
                       "label", name,
                       "use-markup", TRUE,
                       "max-width-chars", 20,
                       "valign", GTK_ALIGN_CENTER,
                       "visible", TRUE,
                       "xalign", 0.0,
                       NULL);
}

static GtkWidget *
reader_font_style_create_current_widget (gpointer item,
                                         gpointer user_data)
{
  g_autofree gchar *name = reader_font_style_get_name (item, NULL);

  return g_object_new (GTK_TYPE_LABEL,
                       "ellipsize", PANGO_ELLIPSIZE_END,
                       "halign", GTK_ALIGN_END,
                       "label", name,
                       "use-markup", TRUE,
                       "valign", GTK_ALIGN_CENTER,
                       "visible", TRUE,
                       "xalign", 0.0,
                       NULL);
}

static gboolean
reader_font_style_get_mapping (GValue   *value,
                               GVariant *variant,
                               gpointer  user_data)
{
  const char *reader_colors = g_variant_get_string (variant, NULL);

  if (g_strcmp0 (reader_colors, "sans") == 0)
    g_value_set_int (value, EPHY_PREFS_READER_FONT_STYLE_SANS);
  else if (g_strcmp0 (reader_colors, "serif") == 0)
    g_value_set_int (value, EPHY_PREFS_READER_FONT_STYLE_SERIF);

  return TRUE;
}

static GVariant *
reader_font_style_set_mapping (const GValue       *value,
                               const GVariantType *expected_type,
                               gpointer            user_data)
{
  switch (g_value_get_int (value)) {
    case EPHY_PREFS_READER_FONT_STYLE_SANS:
      return g_variant_new_string ("sans");
    case EPHY_PREFS_READER_FONT_STYLE_SERIF:
      return g_variant_new_string ("serif");
    default:
      return g_variant_new_string ("crashed");
  }
}

static gchar *
reader_color_scheme_get_name (HdyEnumValueObject *value,
                              gpointer            user_data)
{
  g_assert (HDY_IS_ENUM_VALUE_OBJECT (value));

  switch (hdy_enum_value_object_get_value (value)) {
    case EPHY_PREFS_READER_COLORS_LIGHT:
      return g_strdup (_("Light"));
    case EPHY_PREFS_READER_COLORS_DARK:
      return g_strdup (_("Dark"));
    default:
      return NULL;
  }
}

static gboolean
reader_color_scheme_get_mapping (GValue   *value,
                                 GVariant *variant,
                                 gpointer  user_data)
{
  const char *reader_colors = g_variant_get_string (variant, NULL);

  if (g_strcmp0 (reader_colors, "light") == 0)
    g_value_set_int (value, EPHY_PREFS_READER_COLORS_LIGHT);
  else if (g_strcmp0 (reader_colors, "dark") == 0)
    g_value_set_int (value, EPHY_PREFS_READER_COLORS_DARK);

  return TRUE;
}

static GVariant *
reader_color_scheme_set_mapping (const GValue       *value,
                                 const GVariantType *expected_type,
                                 gpointer            user_data)
{
  switch (g_value_get_int (value)) {
    case EPHY_PREFS_READER_COLORS_LIGHT:
      return g_variant_new_string ("light");
    case EPHY_PREFS_READER_COLORS_DARK:
      return g_variant_new_string ("dark");
    default:
      return g_variant_new_string ("crashed");
  }
}

static void
css_file_created_cb (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  g_autoptr (GFile) file = G_FILE (source);
  g_autoptr (GFileOutputStream) stream = NULL;
  g_autoptr (GError) error = NULL;

  stream = g_file_create_finish (file, result, &error);
  if (stream == NULL && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    g_warning ("Failed to create %s: %s", g_file_get_path (file), error->message);
  else {
    if (ephy_is_running_inside_sandbox ()) {
      g_autofree char *uri = g_file_get_uri (file);
      ephy_open_uri_via_flatpak_portal (uri);
    } else {
      ephy_file_launch_handler (file);
    }
  }
}

static void
css_edit_button_clicked_cb (GtkWidget           *button,
                            PrefsAppearancePage *appearance_page)
{
  GFile *css_file;

  css_file = g_file_new_for_path (g_build_filename (ephy_profile_dir (),
                                                    USER_STYLESHEET_FILENAME,
                                                    NULL));

  g_file_create_async (css_file, G_FILE_CREATE_NONE, G_PRIORITY_DEFAULT, NULL, css_file_created_cb, NULL);
}

static void
js_file_created_cb (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr (GFile) file = G_FILE (source);
  g_autoptr (GFileOutputStream) stream = NULL;
  g_autoptr (GError) error = NULL;

  stream = g_file_create_finish (file, result, &error);
  if (stream == NULL && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    g_warning ("Failed to create %s: %s", g_file_get_path (file), error->message);
  else {
    if (ephy_is_running_inside_sandbox ()) {
      g_autofree char *uri = g_file_get_uri (file);
      ephy_open_uri_via_flatpak_portal (uri);
    } else {
      ephy_file_launch_handler (file);
    }
  }
}

static void
js_edit_button_clicked_cb (GtkWidget           *button,
                           PrefsAppearancePage *appearance_page)
{
  GFile *js_file;

  js_file = g_file_new_for_path (g_build_filename (ephy_profile_dir (),
                                                   USER_JAVASCRIPT_FILENAME,
                                                   NULL));

  g_file_create_async (g_steal_pointer (&js_file), G_FILE_CREATE_NONE, G_PRIORITY_DEFAULT, NULL, js_file_created_cb, NULL);
}

static gboolean
on_default_zoom_spin_button_output (GtkSpinButton *spin,
                                    gpointer       user_data)
{
  GtkAdjustment *adjustment;
  g_autofree gchar *text = NULL;
  gdouble value;

  adjustment = gtk_spin_button_get_adjustment (spin);
  value = (int)gtk_adjustment_get_value (adjustment);
  text = g_strdup_printf ("%.f%%", value);
  gtk_entry_set_text (GTK_ENTRY (spin), text);

  return TRUE;
}

static void
on_default_zoom_spin_button_value_changed (GtkSpinButton *spin,
                                           gpointer       user_data)
{
  GtkAdjustment *adjustment;
  gdouble value;

  adjustment = gtk_spin_button_get_adjustment (spin);
  value = gtk_adjustment_get_value (adjustment);
  value = round (value) / 100;
  g_settings_set_double (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_DEFAULT_ZOOM_LEVEL, value);
}

static void
setup_font_row (PrefsAppearancePage *appearance_page)
{
  g_autoptr (GListStore) store = g_list_store_new (HDY_TYPE_ENUM_VALUE_OBJECT);
  g_autoptr (GEnumClass) enum_class = g_type_class_ref (EPHY_TYPE_PREFS_READER_FONT_STYLE);

  for (guint i = 0; i < enum_class->n_values; i++) {
    g_autoptr (HdyEnumValueObject) obj = hdy_enum_value_object_new (&enum_class->values[i]);

    g_list_store_append (store, obj);
  }

  hdy_combo_row_bind_model (HDY_COMBO_ROW (appearance_page->reader_mode_font_style),
                            G_LIST_MODEL (store),
                            (GtkListBoxCreateWidgetFunc)reader_font_style_create_list_widget,
                            (GtkListBoxCreateWidgetFunc)reader_font_style_create_current_widget,
                            NULL,
                            NULL);
}

static void
setup_appearance_page (PrefsAppearancePage *appearance_page)
{
  GSettings *web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);
  GSettings *reader_settings = ephy_settings_get (EPHY_PREFS_READER_SCHEMA);

  /* ======================================================================== */
  /* ========================== Fonts ======================================= */
  /* ======================================================================== */
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_USE_GNOME_FONTS,
                   appearance_page->use_gnome_fonts_row,
                   "enable-expansion",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_SANS_SERIF_FONT,
                   appearance_page->sans_fontbutton,
                   "font-name",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_SERIF_FONT,
                   appearance_page->serif_fontbutton,
                   "font-name",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_MONOSPACE_FONT,
                   appearance_page->mono_fontbutton,
                   "font-name",
                   G_SETTINGS_BIND_DEFAULT);

  /* ======================================================================== */
  /* ========================== Reader Mode ================================= */
  /* ======================================================================== */

  setup_font_row (appearance_page);

  g_settings_bind_with_mapping (reader_settings,
                                EPHY_PREFS_READER_FONT_STYLE,
                                appearance_page->reader_mode_font_style,
                                "selected-index",
                                G_SETTINGS_BIND_DEFAULT,
                                reader_font_style_get_mapping,
                                reader_font_style_set_mapping,
                                NULL, NULL);

  g_object_bind_property (hdy_style_manager_get_default (), "system-supports-color-schemes",
                          appearance_page->reader_mode_color_scheme, "visible",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  hdy_combo_row_set_for_enum (HDY_COMBO_ROW (appearance_page->reader_mode_color_scheme),
                              EPHY_TYPE_PREFS_READER_COLOR_SCHEME,
                              reader_color_scheme_get_name, NULL, NULL);

  g_settings_bind_with_mapping (reader_settings,
                                EPHY_PREFS_READER_COLOR_SCHEME,
                                appearance_page->reader_mode_color_scheme,
                                "selected-index",
                                G_SETTINGS_BIND_DEFAULT,
                                reader_color_scheme_get_mapping,
                                reader_color_scheme_set_mapping,
                                NULL, NULL);

  /* ======================================================================== */
  /* ========================== Style ======================================= */
  /* ======================================================================== */
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_USER_CSS,
                   appearance_page->css_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_USER_CSS,
                   appearance_page->css_edit_button,
                   "sensitive",
                   G_SETTINGS_BIND_GET);

  g_signal_connect (appearance_page->css_edit_button,
                    "clicked",
                    G_CALLBACK (css_edit_button_clicked_cb),
                    appearance_page);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_USER_JS,
                   appearance_page->js_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_USER_JS,
                   appearance_page->js_edit_button,
                   "sensitive",
                   G_SETTINGS_BIND_GET);

  g_signal_connect (appearance_page->js_edit_button,
                    "clicked",
                    G_CALLBACK (js_edit_button_clicked_cb),
                    appearance_page);

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (appearance_page->default_zoom_spin_button),
                             g_settings_get_double (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_DEFAULT_ZOOM_LEVEL) * 100);
}

static void
prefs_appearance_page_class_init (PrefsAppearancePageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-appearance-page.ui");

  /* Fonts */
  gtk_widget_class_bind_template_child (widget_class, PrefsAppearancePage, use_gnome_fonts_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsAppearancePage, sans_fontbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsAppearancePage, serif_fontbutton);
  gtk_widget_class_bind_template_child (widget_class, PrefsAppearancePage, mono_fontbutton);

  /* Reader Mode */
  gtk_widget_class_bind_template_child (widget_class, PrefsAppearancePage, reader_mode_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsAppearancePage, reader_mode_font_style);
  gtk_widget_class_bind_template_child (widget_class, PrefsAppearancePage, reader_mode_color_scheme);

  /* Style */
  gtk_widget_class_bind_template_child (widget_class, PrefsAppearancePage, css_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsAppearancePage, css_edit_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsAppearancePage, js_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsAppearancePage, js_edit_button);
  gtk_widget_class_bind_template_child (widget_class, PrefsAppearancePage, default_zoom_spin_button);

  /* Signals */
  gtk_widget_class_bind_template_callback (widget_class, on_default_zoom_spin_button_output);
  gtk_widget_class_bind_template_callback (widget_class, on_default_zoom_spin_button_value_changed);
}

static void
prefs_appearance_page_init (PrefsAppearancePage *appearance_page)
{
  EphyEmbedShellMode mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());

  gtk_widget_init_template (GTK_WIDGET (appearance_page));

  gtk_widget_set_visible (appearance_page->reader_mode_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);

  setup_appearance_page (appearance_page);
}
