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

#include "autofill-view.h"
#include "clear-data-view.h"
#include "ephy-data-view.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-prefs.h"
#include "ephy-prefs-dialog.h"
#include "ephy-search-engine-manager.h"
#include "ephy-settings.h"
#include "ephy-web-extension.h"
#include "extension-view.h"
#include "prefs-features-page.h"
#include "prefs-general-page.h"
#include "prefs-extensions-page.h"
#include "webapp-additional-urls-dialog.h"

struct _EphyPrefsDialog {
  AdwPreferencesDialog parent_instance;

  PrefsGeneralPage *general_page;
  GtkWidget *extensions_page;

  EphyWindow *parent_window;

#if !TECH_PREVIEW && !CANARY
  GtkWidget *features_page;
#endif
};

G_DEFINE_FINAL_TYPE (EphyPrefsDialog, ephy_prefs_dialog, ADW_TYPE_PREFERENCES_DIALOG)

static gboolean
on_closed (EphyPrefsDialog *prefs_dialog)
{
  prefs_general_page_on_pd_close_request (prefs_dialog->general_page);
  return GDK_EVENT_PROPAGATE;
}

EphyWindow *
ephy_prefs_dialog_get_parent_window (EphyPrefsDialog *prefs_dialog)
{
  return prefs_dialog->parent_window;
}

void
ephy_prefs_dialog_set_parent_window (EphyPrefsDialog *prefs_dialog,
                                     EphyWindow      *window)
{
  prefs_dialog->parent_window = window;
}

void
ephy_prefs_dialog_show_clear_data_view (EphyPrefsDialog *prefs_dialog)
{
  AdwNavigationPage *clear_data_view;

  adw_preferences_dialog_set_visible_page_name (ADW_PREFERENCES_DIALOG (prefs_dialog),
                                                "privacy-page");

  clear_data_view = g_object_new (EPHY_TYPE_CLEAR_DATA_VIEW, NULL);
  adw_preferences_dialog_push_subpage (ADW_PREFERENCES_DIALOG (prefs_dialog), clear_data_view);
}

static void
on_autofill_row_activated (GtkWidget       *privacy_page,
                           EphyPrefsDialog *prefs_dialog)
{
  AdwNavigationPage *page = g_object_new (EPHY_TYPE_AUTOFILL_VIEW, NULL);

  adw_preferences_dialog_push_subpage (ADW_PREFERENCES_DIALOG (prefs_dialog), page);
}

static void
on_manage_webapp_additional_urls_row_activated (GtkWidget       *privacy_page,
                                                EphyPrefsDialog *prefs_dialog)
{
  AdwNavigationPage *page = ADW_NAVIGATION_PAGE (ephy_webapp_additional_urls_dialog_new ());

  adw_preferences_dialog_push_subpage (ADW_PREFERENCES_DIALOG (prefs_dialog), page);
}

static void
on_clear_data_row_activated (GtkWidget       *privacy_page,
                             EphyPrefsDialog *prefs_dialog)
{
  ephy_prefs_dialog_show_clear_data_view (prefs_dialog);
}

static void
on_extension_row_activated (GtkWidget        *extensions_page,
                            EphyWebExtension *extension,
                            EphyPrefsDialog  *prefs_dialog)
{
  AdwNavigationPage *page = ADW_NAVIGATION_PAGE (ephy_extension_view_new (extension));

  adw_preferences_dialog_push_subpage (ADW_PREFERENCES_DIALOG (prefs_dialog), page);
}

static void
sync_extensions (EphyPrefsDialog *self)
{
  gboolean enable_extensions;

  enable_extensions = g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_WEBEXTENSIONS);

  if (enable_extensions && !self->extensions_page) {
    self->extensions_page = g_object_new (EPHY_TYPE_PREFS_EXTENSIONS_PAGE, NULL);
    g_signal_connect (self->extensions_page, "extension-row-activated", G_CALLBACK (on_extension_row_activated), self);
    adw_preferences_dialog_add (ADW_PREFERENCES_DIALOG (self),
                                ADW_PREFERENCES_PAGE (self->extensions_page));
  } else if (self->extensions_page) {
    adw_preferences_dialog_remove (ADW_PREFERENCES_DIALOG (self),
                                   ADW_PREFERENCES_PAGE (self->extensions_page));
    g_clear_weak_pointer (&self->extensions_page);
  }
}

#if !TECH_PREVIEW && !CANARY
static void
sync_features (EphyPrefsDialog *self)
{
  gboolean enable_features = g_settings_get_boolean (EPHY_SETTINGS_UI, EPHY_PREFS_UI_WEBKIT_FEATURES_PANEL);

  if (enable_features && !self->features_page) {
    self->features_page = g_object_new (EPHY_TYPE_PREFS_FEATURES_PAGE, "dialog", self, NULL);
    adw_preferences_dialog_add (ADW_PREFERENCES_DIALOG (self),
                                ADW_PREFERENCES_PAGE (self->features_page));
  } else if (self->features_page) {
    adw_preferences_dialog_remove (ADW_PREFERENCES_DIALOG (self),
                                   ADW_PREFERENCES_PAGE (self->features_page));
    self->features_page = NULL;
  }
}
#endif

static void
ephy_prefs_dialog_class_init (EphyPrefsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyPrefsDialog, general_page);

  /* Template file callbacks */
  gtk_widget_class_bind_template_callback (widget_class, on_closed);
  gtk_widget_class_bind_template_callback (widget_class, on_autofill_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_clear_data_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_manage_webapp_additional_urls_row_activated);
}

static void
ephy_prefs_dialog_init (EphyPrefsDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));

  gtk_widget_set_size_request (GTK_WIDGET (dialog), 360, 200);
  sync_extensions (dialog);
  g_signal_connect_object (EPHY_SETTINGS_WEB,
                           "changed::" EPHY_PREFS_WEB_ENABLE_WEBEXTENSIONS,
                           G_CALLBACK (sync_extensions),
                           dialog,
                           G_CONNECT_SWAPPED);

#if TECH_PREVIEW || CANARY
  adw_preferences_dialog_add (ADW_PREFERENCES_DIALOG (dialog),
                              g_object_new (EPHY_TYPE_PREFS_FEATURES_PAGE, "dialog", dialog, NULL));
#else
  sync_features (dialog);
  g_signal_connect_object (EPHY_SETTINGS_UI,
                           "changed::" EPHY_PREFS_UI_WEBKIT_FEATURES_PANEL,
                           G_CALLBACK (sync_features),
                           dialog,
                           G_CONNECT_SWAPPED);
#endif
}
