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
  PASSWORDS_ROW_ACTIVATED,
  CLEAR_DATA_ROW_ACTIVATED,

  LAST_SIGNAL
};

struct _PrefsPrivacyPage {
  HdyPreferencesPage parent_instance;

  /* Web Safety */
  GtkWidget *safe_browsing_group;
  GtkWidget *enable_safe_browsing_switch;

  /* Web Tracking */
  GtkWidget *enable_itp_switch;
  GtkWidget *enable_website_data_storage_switch;

  /* Search Suggestions */
  GtkWidget *enable_google_search_suggestions_switch;

  /* Passwords */
  GtkWidget *remember_passwords_switch;
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (PrefsPrivacyPage, prefs_privacy_page, HDY_TYPE_PREFERENCES_PAGE)

static void
on_passwords_row_activated (GtkWidget        *row,
                            PrefsPrivacyPage *privacy_page)
{
  g_signal_emit (privacy_page, signals[PASSWORDS_ROW_ACTIVATED], 0);
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
  /* ========================== Web Safety ================================== */
  /* ======================================================================== */

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_SAFE_BROWSING,
                   privacy_page->enable_safe_browsing_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  gtk_widget_set_visible (privacy_page->safe_browsing_group, ENABLE_GSB);

  /* ======================================================================== */
  /* ========================== Web Tracking ================================ */
  /* ======================================================================== */

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_ITP,
                   privacy_page->enable_itp_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_WEBSITE_DATA_STORAGE,
                   privacy_page->enable_website_data_storage_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* ======================================================================== */
  /* ========================== Passwords =================================== */
  /* ======================================================================== */
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_REMEMBER_PASSWORDS,
                   privacy_page->remember_passwords_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* ======================================================================== */
  /* ========================== Search Suggestions ========================== */
  /* ======================================================================== */
  g_settings_bind (EPHY_SETTINGS_MAIN,
                   EPHY_PREFS_USE_GOOGLE_SEARCH_SUGGESTIONS,
                   privacy_page->enable_google_search_suggestions_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
prefs_privacy_page_class_init (PrefsPrivacyPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-privacy-page.ui");

  signals[PASSWORDS_ROW_ACTIVATED] =
    g_signal_new ("passwords-row-activated",
                  EPHY_TYPE_PREFS_PRIVACY_PAGE,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[CLEAR_DATA_ROW_ACTIVATED] =
    g_signal_new ("clear-data-row-activated",
                  EPHY_TYPE_PREFS_PRIVACY_PAGE,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  /* Web Safety */
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, safe_browsing_group);
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, enable_safe_browsing_switch);

  /* Web Tracking */
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, enable_itp_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, enable_website_data_storage_switch);

  /* Search Suggestions */
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, enable_google_search_suggestions_switch);

  /* Passwords */
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, remember_passwords_switch);

  /* Template file callbacks */
  gtk_widget_class_bind_template_callback (widget_class, on_passwords_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_clear_data_row_activated);
}

static void
prefs_privacy_page_init (PrefsPrivacyPage *privacy_page)
{
  gtk_widget_init_template (GTK_WIDGET (privacy_page));

  setup_privacy_page (privacy_page);
}
