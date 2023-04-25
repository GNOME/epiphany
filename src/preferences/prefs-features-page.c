/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2023 Igalia S.L.
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

#include "prefs-features-page.h"

#include "ephy-embed-prefs.h"
#include "ephy-settings.h"
#include <webkit/webkit.h>

struct _PrefsFeaturesPage {
  AdwPreferencesPage parent_instance;
};

G_DEFINE_FINAL_TYPE (PrefsFeaturesPage, prefs_features_page, ADW_TYPE_PREFERENCES_PAGE)

static void
prefs_features_page_class_init (PrefsFeaturesPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-features-page.ui");
}

static AdwPreferencesGroup *
get_or_create_group (GPtrArray  *groups,
                     const char *title)
{
  AdwPreferencesGroup *group;

  for (unsigned i = 0; i < groups->len; i++) {
    group = g_ptr_array_index (groups, i);
    if (!strcmp (title, adw_preferences_group_get_title (group)))
      return group;
  }

  group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
  adw_preferences_group_set_title (group, title);
  g_ptr_array_add (groups, group);
  return group;
}

static void
feature_switch_notify_active_cb (GtkSwitch     *swtch,
                                 GParamSpec    *pspec,
                                 WebKitFeature *feature)
{
  gboolean enabled = gtk_switch_get_active (swtch);
  WebKitSettings *settings = ephy_embed_prefs_get_settings ();
  if (enabled != webkit_settings_get_feature_enabled (settings, feature))
    webkit_settings_set_feature_enabled (settings, feature, enabled);
}

static void
feature_switch_reset_cb (GtkWidget     *button,
                         WebKitFeature *feature)
{
  WebKitSettings *settings = ephy_embed_prefs_get_settings ();
  gboolean enabled = webkit_feature_get_default_value (feature);
  if (enabled != webkit_settings_get_feature_enabled (settings, feature)) {
    GtkWidget *parent = gtk_widget_get_ancestor (button, ADW_TYPE_ACTION_ROW);
    GtkWidget *swtch = adw_action_row_get_activatable_widget (ADW_ACTION_ROW (parent));
    webkit_settings_set_feature_enabled (settings, feature, enabled);
    gtk_switch_set_active (GTK_SWITCH (swtch), enabled);
  }
}

static int
compare_groups_cb (AdwPreferencesGroup **a,
                   AdwPreferencesGroup **b)
{
  return strcmp (adw_preferences_group_get_title (*a),
                 adw_preferences_group_get_title (*b));
}

static void
prefs_features_page_init (PrefsFeaturesPage *self)
{
  g_autoptr (GPtrArray) groups = g_ptr_array_new_full (10, NULL);
  g_autoptr (GEnumClass) status_enum = g_type_class_ref (WEBKIT_TYPE_FEATURE_STATUS);
  g_autoptr (WebKitFeatureList) feature_list = webkit_settings_get_all_features ();
  WebKitSettings *settings = ephy_embed_prefs_get_settings ();
  gboolean show_internal = g_settings_get_boolean (EPHY_SETTINGS_UI,
                                                   EPHY_PREFS_UI_WEBKIT_INTERNAL_FEATURES);

  gtk_widget_init_template (GTK_WIDGET (self));

  for (size_t i = 0; i < webkit_feature_list_get_length (feature_list); i++) {
    WebKitFeature *feature = webkit_feature_list_get (feature_list, i);
    WebKitFeatureStatus status = webkit_feature_get_status (feature);
    if (status != WEBKIT_FEATURE_STATUS_EMBEDDER && (show_internal || status != WEBKIT_FEATURE_STATUS_INTERNAL)) {
      AdwPreferencesGroup *group = get_or_create_group (groups, webkit_feature_get_category (feature));
      GtkWidget *row = adw_action_row_new ();
      GtkWidget *swtch = gtk_switch_new ();
      GtkWidget *reset = gtk_button_new_from_icon_name ("edit-undo-symbolic");
      GtkWidget *label = gtk_label_new (g_enum_get_value (status_enum, webkit_feature_get_status (feature))->value_nick);

      adw_preferences_row_set_use_markup (ADW_PREFERENCES_ROW (row), FALSE);
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                     webkit_feature_get_name (feature));
      adw_action_row_set_subtitle (ADW_ACTION_ROW (row),
                                   webkit_feature_get_details (feature));

      gtk_widget_set_valign (swtch, GTK_ALIGN_CENTER);
      gtk_switch_set_active (GTK_SWITCH (swtch),
                             webkit_settings_get_feature_enabled (settings, feature));

      gtk_widget_set_tooltip_text (reset, _("Reset to default"));
      gtk_widget_set_valign (reset, GTK_ALIGN_CENTER);
      gtk_widget_add_css_class (reset, "flat");

      gtk_label_set_use_markup (GTK_LABEL (label), FALSE);
      gtk_widget_add_css_class (label, "dim-label");
      gtk_widget_add_css_class (label, "caption");

      g_signal_connect_data (reset,
                             "clicked",
                             G_CALLBACK (feature_switch_reset_cb),
                             webkit_feature_ref (feature),
                             (GClosureNotify)webkit_feature_unref,
                             G_CONNECT_DEFAULT);

      g_signal_connect_data (swtch,
                             "notify::active",
                             G_CALLBACK (feature_switch_notify_active_cb),
                             webkit_feature_ref (feature),
                             (GClosureNotify)webkit_feature_unref,
                             G_CONNECT_DEFAULT);

      adw_action_row_add_suffix (ADW_ACTION_ROW (row), label);
      adw_action_row_add_suffix (ADW_ACTION_ROW (row), swtch);
      adw_action_row_add_suffix (ADW_ACTION_ROW (row), reset);
      adw_action_row_set_activatable_widget (ADW_ACTION_ROW (row), swtch);
      adw_preferences_group_add (group, row);
    }
  }

  g_ptr_array_sort (groups, (GCompareFunc)compare_groups_cb);

  for (unsigned i = 0; i < groups->len; i++) {
    AdwPreferencesGroup *group = g_ptr_array_index (groups, i);
    adw_preferences_page_add (ADW_PREFERENCES_PAGE (self), group);
  }
}
