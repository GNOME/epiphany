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
#include "prefs-privacy-page.h"

#include "ephy-settings.h"
#include "ephy-shell.h"

enum {
  CLEAR_DATA_ROW_ACTIVATED,
  AUTOFILL_ROW_ACTIVATED,

  LAST_SIGNAL
};

struct _PrefsPrivacyPage {
  AdwPreferencesPage parent_instance;

  /* Web Tracking */
  GtkWidget *enable_itp_row;
  GtkWidget *enable_website_data_storage_row;

  /* Search Suggestions */
  GtkWidget *search_suggestions_box;
  GtkWidget *enable_search_suggestions_row;

  /* Autofill Data */
  GtkWidget *autofill_data_row;
};

static guint signals[LAST_SIGNAL];

G_DEFINE_FINAL_TYPE (PrefsPrivacyPage, prefs_privacy_page, ADW_TYPE_PREFERENCES_PAGE)

static void
on_autofill_row_activated (GtkWidget        *row,
                           PrefsPrivacyPage *privacy_page)
{
  g_signal_emit (privacy_page, signals[AUTOFILL_ROW_ACTIVATED], 0);
}

static void
on_clear_data_row_activated (GtkWidget        *row,
                             PrefsPrivacyPage *privacy_page)
{
  g_signal_emit (privacy_page, signals[CLEAR_DATA_ROW_ACTIVATED], 0);
}

static void
setup_privacy_page (PrefsPrivacyPage *privacy_page)
{
  GSettings *web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);

  /* ======================================================================== */
  /* ========================== Web Tracking ================================ */
  /* ======================================================================== */

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_ITP,
                   privacy_page->enable_itp_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_WEBSITE_DATA_STORAGE,
                   privacy_page->enable_website_data_storage_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* ======================================================================== */
  /* ====================== Forms and Autofill ============================== */
  /* ======================================================================== */
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_AUTOFILL_DATA,
                   privacy_page->autofill_data_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* ======================================================================== */
  /* ========================== Search Suggestions ========================== */
  /* ======================================================================== */
  g_settings_bind (EPHY_SETTINGS_MAIN,
                   EPHY_PREFS_USE_SEARCH_SUGGESTIONS,
                   privacy_page->enable_search_suggestions_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
prefs_privacy_page_class_init (PrefsPrivacyPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-privacy-page.ui");

  signals[CLEAR_DATA_ROW_ACTIVATED] =
    g_signal_new ("clear-data-row-activated",
                  EPHY_TYPE_PREFS_PRIVACY_PAGE,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[AUTOFILL_ROW_ACTIVATED] =
    g_signal_new ("autofill-row-activated",
                  EPHY_TYPE_PREFS_PRIVACY_PAGE,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /* Web Tracking */
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, enable_itp_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, enable_website_data_storage_row);

  /* Search Suggestions */
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, search_suggestions_box);
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, enable_search_suggestions_row);

  /* Forms and Autofill */
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, autofill_data_row);

  /* Template file callbacks */
  gtk_widget_class_bind_template_callback (widget_class, on_autofill_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_clear_data_row_activated);
}

static void
prefs_privacy_page_init (PrefsPrivacyPage *privacy_page)
{
  EphyEmbedShellMode mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());

  gtk_widget_init_template (GTK_WIDGET (privacy_page));

  setup_privacy_page (privacy_page);

  gtk_widget_set_visible (privacy_page->search_suggestions_box,
                          mode != EPHY_EMBED_SHELL_MODE_APPLICATION);
}
