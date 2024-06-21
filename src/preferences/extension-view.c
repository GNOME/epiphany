/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#include "extension-view.h"

struct _EphyExtensionView {
  AdwNavigationPage parent_instance;

  GtkWidget *box;

  GtkWidget *window_title;

  GtkWidget *author_row;
  GtkWidget *author_label;
  GtkWidget *version_row;
  GtkWidget *version_label;
  GtkWidget *homepage_row;

  AdwSwitchRow *enabled_row;

  GMenuModel *menu_model;

  EphyWebExtension *web_extension;
};

G_DEFINE_FINAL_TYPE (EphyExtensionView, ephy_extension_view, ADW_TYPE_NAVIGATION_PAGE)

enum {
  PROP_0,
  PROP_WEB_EXTENSION,
  LAST_PROP,
};

static GParamSpec *properties[LAST_PROP];

static void
open_inspector (GSimpleAction *action,
                GVariant      *loading,
                gpointer       user_data)
{
  EphyExtensionView *self = EPHY_EXTENSION_VIEW (user_data);
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();

  ephy_web_extension_manager_open_inspector (manager, self->web_extension);

  gtk_window_destroy (GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))));
}

static const GActionEntry extension_entries [] = {
  { "inspector", open_inspector },
};

static void
on_homepage_activated (AdwActionRow *row,
                       gpointer      user_data)
{
  EphyExtensionView *self = EPHY_EXTENSION_VIEW (user_data);
  g_autoptr (GtkUriLauncher) launcher = NULL;

  launcher = gtk_uri_launcher_new (ephy_web_extension_get_homepage_url (self->web_extension));
  gtk_uri_launcher_launch (launcher,
                           GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                           NULL, NULL, NULL);
}

static void
on_toggle_extension_enabled (EphyExtensionView *view)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();

  ephy_web_extension_manager_set_active (manager, view->web_extension,
                                         adw_switch_row_get_active (view->enabled_row));
}

static void
on_remove_confirmed (EphyExtensionView *self)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();

  g_assert (self->web_extension);
  ephy_web_extension_manager_uninstall (manager, self->web_extension);

  gtk_widget_activate_action (GTK_WIDGET (self), "navigation.pop", NULL);
}

static void
on_remove_row_activated (GtkWidget *box,
                         GtkWidget *row,
                         gpointer   user_data)
{
  EphyExtensionView *self = EPHY_EXTENSION_VIEW (user_data);
  AdwDialog *dialog;

  dialog = adw_alert_dialog_new (_("Remove Extension"),
                                 _("Do you really want to remove this extension?"));

  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "cancel", _("_Cancel"),
                                  "remove", _("_Remove"),
                                  NULL);

  adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog), "remove",
                                            ADW_RESPONSE_DESTRUCTIVE);

  adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "cancel");
  adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");

  g_signal_connect_swapped (dialog, "response::remove", G_CALLBACK (on_remove_confirmed), self);

  adw_dialog_present (dialog, GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self))));
}

static void
update (EphyExtensionView *self)
{
  GSimpleActionGroup *simple_action_group;
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();

  /* Window Title */
  adw_navigation_page_set_title (ADW_NAVIGATION_PAGE (self),
                                 ephy_web_extension_get_name (self->web_extension));

  adw_window_title_set_title (ADW_WINDOW_TITLE (self->window_title),
                              ephy_web_extension_get_name (self->web_extension));
  adw_window_title_set_subtitle (ADW_WINDOW_TITLE (self->window_title),
                                 ephy_web_extension_get_description (self->web_extension));

  /* Information Rows */
  gtk_label_set_label (GTK_LABEL (self->version_label), ephy_web_extension_get_version (self->web_extension));
  if (*ephy_web_extension_get_author (self->web_extension)) {
    gtk_widget_set_visible (self->author_row, TRUE);
    gtk_label_set_label (GTK_LABEL (self->author_label), ephy_web_extension_get_author (self->web_extension));
  }
  if (*ephy_web_extension_get_homepage_url (self->web_extension))
    gtk_widget_set_visible (self->homepage_row, TRUE);

  adw_switch_row_set_active (self->enabled_row,
                             ephy_web_extension_manager_is_active (manager, self->web_extension));

  /* Add actions */
  simple_action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (simple_action_group),
                                   extension_entries,
                                   G_N_ELEMENTS (extension_entries),
                                   self);

  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "prefs", G_ACTION_GROUP (simple_action_group));
}

static void
ephy_extension_view_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  EphyExtensionView *self = EPHY_EXTENSION_VIEW (object);

  switch (prop_id) {
    case PROP_WEB_EXTENSION:
      g_set_object (&self->web_extension, g_value_get_object (value));
      update (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_extension_view_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  EphyExtensionView *self = EPHY_EXTENSION_VIEW (object);

  switch (prop_id) {
    case PROP_WEB_EXTENSION:
      g_value_set_object (value, self->web_extension);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_extension_view_dispose (GObject *object)
{
  EphyExtensionView *self = EPHY_EXTENSION_VIEW (object);

  g_clear_object (&self->web_extension);

  G_OBJECT_CLASS (ephy_extension_view_parent_class)->dispose (object);
}

static void
ephy_extension_view_class_init (EphyExtensionViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ephy_extension_view_get_property;
  object_class->set_property = ephy_extension_view_set_property;
  object_class->dispose = ephy_extension_view_dispose;

  properties[PROP_WEB_EXTENSION] =
    g_param_spec_object ("web-extension",
                         NULL, NULL,
                         EPHY_TYPE_WEB_EXTENSION,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/extension-view.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyExtensionView, window_title);
  gtk_widget_class_bind_template_child (widget_class, EphyExtensionView, author_row);
  gtk_widget_class_bind_template_child (widget_class, EphyExtensionView, author_label);
  gtk_widget_class_bind_template_child (widget_class, EphyExtensionView, version_row);
  gtk_widget_class_bind_template_child (widget_class, EphyExtensionView, version_label);
  gtk_widget_class_bind_template_child (widget_class, EphyExtensionView, homepage_row);
  gtk_widget_class_bind_template_child (widget_class, EphyExtensionView, enabled_row);

  gtk_widget_class_bind_template_callback (widget_class, on_remove_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_homepage_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_toggle_extension_enabled);
}

static void
ephy_extension_view_init (EphyExtensionView *extension_view)
{
  gtk_widget_init_template (GTK_WIDGET (extension_view));
}

EphyExtensionView *
ephy_extension_view_new (EphyWebExtension *extension)
{
  return g_object_new (EPHY_TYPE_EXTENSION_VIEW,
                       "web-extension", extension,
                       NULL);
}
