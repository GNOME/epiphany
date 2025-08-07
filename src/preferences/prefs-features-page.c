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

#include "ephy-adaptive-mode.h"
#include "ephy-embed-prefs.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-prefs-dialog.h"
#include "ephy-settings.h"
#include <webkit/webkit.h>

struct _PrefsFeaturesPage {
  AdwPreferencesPage parent_instance;

  EphyPrefsDialog *dialog;
  GtkWidget *reset_all_row;
  int non_default_values;

  gboolean adaptive_mode;
};

G_DEFINE_FINAL_TYPE (PrefsFeaturesPage, prefs_features_page, ADW_TYPE_PREFERENCES_PAGE)

enum {
  PROP_0,
  PROP_DIALOG,
  PROP_ADAPTIVE_MODE,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

static void
prefs_feature_page_set_adaptive_mode (PrefsFeaturesPage *self,
                                      EphyAdaptiveMode   mode)
{
  self->adaptive_mode = mode;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ADAPTIVE_MODE]);
}

static void
prefs_feature_page_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  PrefsFeaturesPage *self = EPHY_PREFS_FEATURES_PAGE (object);

  switch (prop_id) {
    case PROP_DIALOG:
      self->dialog = g_value_get_object (value);
      break;
    case PROP_ADAPTIVE_MODE:
      prefs_feature_page_set_adaptive_mode (self, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
prefs_feature_page_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  PrefsFeaturesPage *self = EPHY_PREFS_FEATURES_PAGE (object);

  switch (prop_id) {
    case PROP_DIALOG:
      g_value_set_object (value, self->dialog);
      break;
    case PROP_ADAPTIVE_MODE:
      g_value_set_enum (value, self->adaptive_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
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
feature_switch_notify_active_cb (GtkSwitch  *switch_widget,
                                 GParamSpec *pspec,
                                 GtkWidget  *reset_button)
{
  gboolean enabled = gtk_switch_get_active (switch_widget);
  GtkWidget *row = gtk_widget_get_ancestor (reset_button, ADW_TYPE_ACTION_ROW);
  WebKitFeature *feature = g_object_get_data (G_OBJECT (row), "feature");
  gboolean is_default = enabled == webkit_feature_get_default_value (feature);
  WebKitSettings *settings = ephy_embed_prefs_get_settings ();

  if (enabled != webkit_settings_get_feature_enabled (settings, feature)) {
    PrefsFeaturesPage *self = EPHY_PREFS_FEATURES_PAGE (gtk_widget_get_ancestor (
                                                          reset_button,
                                                          EPHY_TYPE_PREFS_FEATURES_PAGE));

    webkit_settings_set_feature_enabled (settings, feature, enabled);
    gtk_widget_set_sensitive (reset_button, !is_default);

    if (is_default && self->non_default_values)
      self->non_default_values--;
    else if (!is_default)
      self->non_default_values++;
    gtk_widget_set_sensitive (self->reset_all_row, self->non_default_values);
  }
}

static void
feature_switch_reset_cb (GtkWidget     *button,
                         WebKitFeature *feature)
{
  WebKitSettings *settings = ephy_embed_prefs_get_settings ();
  gboolean enabled = webkit_feature_get_default_value (feature);

  if (enabled != webkit_settings_get_feature_enabled (settings, feature)) {
    GtkWidget *parent = gtk_widget_get_ancestor (button, ADW_TYPE_ACTION_ROW);
    PrefsFeaturesPage *self = EPHY_PREFS_FEATURES_PAGE (gtk_widget_get_ancestor (
                                                          button,
                                                          EPHY_TYPE_PREFS_FEATURES_PAGE));

    GtkWidget *switch_widget = adw_action_row_get_activatable_widget (ADW_ACTION_ROW (parent));
    webkit_settings_set_feature_enabled (settings, feature, enabled);
    gtk_switch_set_active (GTK_SWITCH (switch_widget), enabled);
    gtk_widget_set_sensitive (button, !enabled);
    self->non_default_values--;
    gtk_widget_set_sensitive (self->reset_all_row, self->non_default_values);
  }
}

static int
compare_groups_cb (AdwPreferencesGroup **a,
                   AdwPreferencesGroup **b)
{
  return strcmp (adw_preferences_group_get_title (*a),
                 adw_preferences_group_get_title (*b));
}

gboolean
feature_status_transform_cb (GBinding     *binding,
                             const GValue *from_value,
                             GValue       *to_value,
                             gpointer      user_data)
{
  EphyAdaptiveMode mode = g_value_get_enum (from_value);
  g_value_set_boolean (to_value, mode != EPHY_ADAPTIVE_MODE_NARROW);
  return TRUE;
}

gboolean
subtitle_transform_cb (GBinding     *binding,
                       const GValue *from_value,
                       GValue       *to_value,
                       gpointer      user_data)
{
  EphyAdaptiveMode mode = g_value_get_enum (from_value);
  WebKitFeature *feature = user_data;
  g_autoptr (GEnumClass) status_enum = g_type_class_ref (WEBKIT_TYPE_FEATURE_STATUS);

  if (mode == EPHY_ADAPTIVE_MODE_NARROW)
    g_value_set_string (to_value, g_enum_get_value (status_enum, webkit_feature_get_status (feature))->value_nick);
  else
    g_value_set_string (to_value, webkit_feature_get_details (feature));

  return TRUE;
}

static void
reset_all_toast_button_clicked_cb (PrefsFeaturesPage *self)
{
  WebKitSettings *settings = ephy_embed_prefs_get_settings ();
  AdwPreferencesGroup *group;
  int i = 1;

  while ((group = adw_preferences_page_get_group (ADW_PREFERENCES_PAGE (self), i++))) {
    GtkWidget *row;
    int j = 0;

    while ((row = adw_preferences_group_get_row (group, j++))) {
      GtkWidget *switch_widget = adw_action_row_get_activatable_widget (ADW_ACTION_ROW (row));
      GtkWidget *button = gtk_widget_get_last_child (gtk_widget_get_last_child (gtk_widget_get_first_child (row)));
      WebKitFeature *feature = g_object_get_data (G_OBJECT (row), "feature");
      const char *prev_value = g_object_steal_data (G_OBJECT (row), "prev-value");
      gboolean enabled;

      if (!prev_value)
        continue;

      enabled = !g_strcmp0 (prev_value, "active");
      webkit_settings_set_feature_enabled (settings, feature, enabled);
      gtk_switch_set_active (GTK_SWITCH (switch_widget), enabled);
      gtk_widget_set_sensitive (button, TRUE);
      self->non_default_values++;
    }
  }

  gtk_widget_set_sensitive (self->reset_all_row, TRUE);
}


static void
reset_all_row_activated_cb (AdwButtonRow      *row,
                            PrefsFeaturesPage *self)
{
  WebKitSettings *settings = ephy_embed_prefs_get_settings ();
  AdwPreferencesGroup *group;
  int i = 1;
  AdwToast *toast;

  while ((group = adw_preferences_page_get_group (ADW_PREFERENCES_PAGE (self), i++))) {
    GtkWidget *row;
    int j = 0;

    while ((row = adw_preferences_group_get_row (group, j++))) {
      GtkWidget *switch_widget = adw_action_row_get_activatable_widget (ADW_ACTION_ROW (row));
      GtkWidget *button = gtk_widget_get_last_child (gtk_widget_get_last_child (gtk_widget_get_first_child (row)));
      WebKitFeature *feature = g_object_get_data (G_OBJECT (row), "feature");
      gboolean active = gtk_switch_get_active (GTK_SWITCH (switch_widget));
      gboolean enabled = webkit_feature_get_default_value (feature);

      if (active == enabled)
        continue;

      if (active)
        g_object_set_data (G_OBJECT (row), "prev-value", "active");
      else
        g_object_set_data (G_OBJECT (row), "prev-value", "inactive");

      webkit_settings_set_feature_enabled (settings, feature, enabled);
      gtk_switch_set_active (GTK_SWITCH (switch_widget), enabled);
      gtk_widget_set_sensitive (button, FALSE);
    }
  }

  self->non_default_values = 0;
  gtk_widget_set_sensitive (self->reset_all_row, FALSE);

  toast = adw_toast_new (_("All values reset"));
  adw_toast_set_button_label (toast, _("_Undo"));
  g_signal_connect_object (toast, "button-clicked",
                           G_CALLBACK (reset_all_toast_button_clicked_cb), self,
                           G_CONNECT_SWAPPED);
  adw_preferences_dialog_add_toast (ADW_PREFERENCES_DIALOG (self->dialog), toast);
}

static void
prefs_feature_page_constructed (GObject *object)
{
  PrefsFeaturesPage *self = EPHY_PREFS_FEATURES_PAGE (object);
  g_autoptr (GPtrArray) groups = g_ptr_array_new_full (10, NULL);
  g_autoptr (GEnumClass) status_enum = g_type_class_ref (WEBKIT_TYPE_FEATURE_STATUS);
  g_autoptr (WebKitFeatureList) feature_list = webkit_settings_get_all_features ();
  AdwBreakpoint *breakpoint;
  WebKitSettings *settings = ephy_embed_prefs_get_settings ();
  gboolean show_internal = g_settings_get_boolean (EPHY_SETTINGS_UI,
                                                   EPHY_PREFS_UI_WEBKIT_INTERNAL_FEATURES);

  G_OBJECT_CLASS (prefs_features_page_parent_class)->constructed (object);

  gtk_widget_init_template (GTK_WIDGET (self));

  prefs_feature_page_set_adaptive_mode (self, EPHY_ADAPTIVE_MODE_NORMAL);

  for (size_t i = 0; i < webkit_feature_list_get_length (feature_list); i++) {
    WebKitFeature *feature = webkit_feature_list_get (feature_list, i);
    WebKitFeatureStatus status = webkit_feature_get_status (feature);
    if (status != WEBKIT_FEATURE_STATUS_EMBEDDER && (show_internal || status != WEBKIT_FEATURE_STATUS_INTERNAL)) {
      AdwPreferencesGroup *group = get_or_create_group (groups, webkit_feature_get_category (feature));
      GtkWidget *row = adw_action_row_new ();
      GtkWidget *switch_widget = gtk_switch_new ();
      GtkWidget *reset = gtk_button_new_from_icon_name ("edit-undo-symbolic");
      GtkWidget *label = gtk_label_new (g_enum_get_value (status_enum, webkit_feature_get_status (feature))->value_nick);
      gboolean enabled = webkit_settings_get_feature_enabled (settings, feature);

      g_object_bind_property_full (self, "adaptive-mode", label, "visible", G_BINDING_SYNC_CREATE, feature_status_transform_cb, NULL, self, NULL);
      g_object_bind_property_full (self, "adaptive-mode", row, "subtitle", G_BINDING_SYNC_CREATE, subtitle_transform_cb, NULL, feature, NULL);

      adw_preferences_row_set_use_markup (ADW_PREFERENCES_ROW (row), FALSE);
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                     webkit_feature_get_name (feature));
      adw_action_row_set_subtitle (ADW_ACTION_ROW (row),
                                   webkit_feature_get_details (feature));
      g_object_set_data (G_OBJECT (row), "feature", feature);

      gtk_widget_set_valign (switch_widget, GTK_ALIGN_CENTER);
      gtk_switch_set_active (GTK_SWITCH (switch_widget), enabled);
      gtk_widget_set_sensitive (reset, enabled != webkit_feature_get_default_value (feature));

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

      g_signal_connect_data (switch_widget,
                             "notify::active",
                             G_CALLBACK (feature_switch_notify_active_cb),
                             reset,
                             (GClosureNotify)webkit_feature_unref,
                             G_CONNECT_DEFAULT);

      adw_action_row_add_suffix (ADW_ACTION_ROW (row), label);
      adw_action_row_add_suffix (ADW_ACTION_ROW (row), switch_widget);
      adw_action_row_add_suffix (ADW_ACTION_ROW (row), reset);
      adw_action_row_set_activatable_widget (ADW_ACTION_ROW (row), switch_widget);
      adw_preferences_group_add (group, row);
    }
  }

  g_ptr_array_sort (groups, (GCompareFunc)compare_groups_cb);

  for (unsigned i = 0; i < groups->len; i++) {
    AdwPreferencesGroup *group = g_ptr_array_index (groups, i);
    adw_preferences_page_add (ADW_PREFERENCES_PAGE (self), group);
  }

  breakpoint = adw_breakpoint_new (adw_breakpoint_condition_parse ("max-width: 600px"));
  adw_breakpoint_add_setters (breakpoint,
                              G_OBJECT (self), "adaptive-mode", EPHY_ADAPTIVE_MODE_NARROW,
                              NULL);

  adw_dialog_add_breakpoint (ADW_DIALOG (self->dialog), breakpoint);
}

static void
prefs_features_page_class_init (PrefsFeaturesPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-features-page.ui");
  gtk_widget_class_bind_template_child (widget_class, PrefsFeaturesPage, reset_all_row);

  gtk_widget_class_bind_template_callback (widget_class, reset_all_row_activated_cb);

  object_class->get_property = prefs_feature_page_get_property;
  object_class->set_property = prefs_feature_page_set_property;
  object_class->constructed = prefs_feature_page_constructed;

  properties[PROP_DIALOG] = g_param_spec_object ("dialog",
                                                 NULL,
                                                 NULL,
                                                 EPHY_TYPE_PREFS_DIALOG,
                                                 G_PARAM_CONSTRUCT | G_PARAM_READWRITE |
                                                 G_PARAM_STATIC_STRINGS);

  properties[PROP_ADAPTIVE_MODE] = g_param_spec_enum ("adaptive-mode",
                                                      NULL,
                                                      NULL,
                                                      EPHY_TYPE_ADAPTIVE_MODE,
                                                      EPHY_ADAPTIVE_MODE_NORMAL,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     PROP_LAST,
                                     properties);
}

static void
prefs_features_page_init (PrefsFeaturesPage *self)
{
}
