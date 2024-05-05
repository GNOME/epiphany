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
#include "passwords-view.h"
#include "prefs-general-page.h"
#include "prefs-extensions-page.h"

struct _EphyPrefsDialog {
  AdwPreferencesDialog parent_instance;

  PrefsGeneralPage *general_page;
  GtkWidget *extensions_page;
};

G_DEFINE_FINAL_TYPE (EphyPrefsDialog, ephy_prefs_dialog, ADW_TYPE_PREFERENCES_DIALOG)

static gboolean
on_closed (EphyPrefsDialog *prefs_dialog)
{
  prefs_general_page_on_pd_close_request (prefs_dialog->general_page);

  /* To avoid any unnecessary IO when typing changes in the search engine
   * list row's entries, only save when closing the prefs dialog.
   */
  ephy_search_engine_manager_save_to_settings (ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ()));

  return GDK_EVENT_PROPAGATE;
}

static void
on_passwords_row_activated (GtkWidget       *privacy_page,
                            EphyPrefsDialog *prefs_dialog)
{
  AdwNavigationPage *page = g_object_new (EPHY_TYPE_PASSWORDS_VIEW, NULL);

  adw_preferences_dialog_push_subpage (ADW_PREFERENCES_DIALOG (prefs_dialog), page);
}

static void
on_clear_data_row_activated (GtkWidget       *privacy_page,
                             EphyPrefsDialog *prefs_dialog)
{
  AdwNavigationPage *page = g_object_new (EPHY_TYPE_CLEAR_DATA_VIEW, NULL);

  adw_preferences_dialog_push_subpage (ADW_PREFERENCES_DIALOG (prefs_dialog), page);
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

static void
ephy_prefs_dialog_class_init (EphyPrefsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyPrefsDialog, general_page);

  /* Template file callbacks */
  gtk_widget_class_bind_template_callback (widget_class, on_closed);
  gtk_widget_class_bind_template_callback (widget_class, on_passwords_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_clear_data_row_activated);
}

static void
ephy_prefs_dialog_init (EphyPrefsDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));

  sync_extensions (dialog);
  g_signal_connect_object (EPHY_SETTINGS_WEB,
                           "changed::" EPHY_PREFS_WEB_ENABLE_WEBEXTENSIONS,
                           G_CALLBACK (sync_extensions),
                           dialog,
                           G_CONNECT_SWAPPED);
}
