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

#include "prefs-privacy-page.h"

#include "cookies-dialog.h"
#include "clear-data-dialog.h"
#include "passwords-dialog.h"
#include "ephy-settings.h"
#include "ephy-shell.h"

struct _PrefsPrivacyPage {
  HdyPreferencesPage parent_instance;

  /* Web Content */
  GtkWidget *popups_allow_switch;
  GtkWidget *adblock_allow_switch;
  GtkWidget *enable_safe_browsing_switch;

  /* Cookies */
  GtkWidget *always;
  GtkWidget *no_third_party;
  GtkWidget *never;

  /* Passwords */
  GtkWidget *remember_passwords_switch;

  /* Personal Data */
  GtkWidget *clear_personal_data_button;
};

G_DEFINE_TYPE (PrefsPrivacyPage, prefs_privacy_page, HDY_TYPE_PREFERENCES_PAGE)

static void
on_manage_cookies_button_clicked (GtkWidget        *button,
                                  PrefsPrivacyPage *privacy_page)
{
  EphyCookiesDialog *cookies_dialog;
  GtkWindow *prefs_dialog;

  cookies_dialog = ephy_cookies_dialog_new ();
  prefs_dialog = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (privacy_page)));

  gtk_window_set_transient_for (GTK_WINDOW (cookies_dialog), prefs_dialog);
  gtk_window_set_modal (GTK_WINDOW (cookies_dialog), TRUE);
  gtk_window_present_with_time (GTK_WINDOW (cookies_dialog), gtk_get_current_event_time ());
}

static void
on_manage_passwords_button_clicked (GtkWidget        *button,
                                    PrefsPrivacyPage *privacy_page)
{
  EphyPasswordsDialog *passwords_dialog;
  EphyPasswordManager *password_manager;
  GtkWindow *prefs_dialog;

  password_manager = ephy_embed_shell_get_password_manager (EPHY_EMBED_SHELL (ephy_shell_get_default ()));
  passwords_dialog = ephy_passwords_dialog_new (password_manager);
  prefs_dialog = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (privacy_page)));

  gtk_window_set_transient_for (GTK_WINDOW (passwords_dialog), prefs_dialog);
  gtk_window_set_modal (GTK_WINDOW (passwords_dialog), TRUE);
  gtk_window_present_with_time (GTK_WINDOW (passwords_dialog), gtk_get_current_event_time ());
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
clear_personal_data_button_clicked_cb (GtkWidget        *button,
                                       PrefsPrivacyPage *privacy_page)
{
  ClearDataDialog *clear_dialog;
  GtkWindow *prefs_dialog;

  clear_dialog = g_object_new (EPHY_TYPE_CLEAR_DATA_DIALOG, NULL);
  prefs_dialog = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (privacy_page)));

  gtk_window_set_transient_for (GTK_WINDOW (clear_dialog), prefs_dialog);
  gtk_window_set_modal (GTK_WINDOW (clear_dialog), TRUE);
  gtk_window_present_with_time (GTK_WINDOW (clear_dialog), gtk_get_current_event_time ());
}

static void
setup_privacy_page (PrefsPrivacyPage *privacy_page)
{
  GSettings *web_settings = ephy_settings_get (EPHY_PREFS_WEB_SCHEMA);

  /* ======================================================================== */
  /* ========================== Web Content ================================= */
  /* ======================================================================== */
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_ADBLOCK,
                   privacy_page->adblock_allow_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_POPUPS,
                   privacy_page->popups_allow_switch,
                   "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_ENABLE_SAFE_BROWSING,
                   privacy_page->enable_safe_browsing_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* ======================================================================== */
  /* ========================== Cookies ===================================== */
  /* ======================================================================== */
  g_settings_bind_with_mapping (web_settings,
                                EPHY_PREFS_WEB_COOKIES_POLICY,
                                privacy_page->always,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                cookies_get_mapping,
                                cookies_set_mapping,
                                privacy_page->always,
                                NULL);

  g_settings_bind_with_mapping (web_settings,
                                EPHY_PREFS_WEB_COOKIES_POLICY,
                                privacy_page->no_third_party,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                cookies_get_mapping,
                                cookies_set_mapping,
                                privacy_page->no_third_party,
                                NULL);

  g_settings_bind_with_mapping (web_settings,
                                EPHY_PREFS_WEB_COOKIES_POLICY,
                                privacy_page->never,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                cookies_get_mapping,
                                cookies_set_mapping,
                                privacy_page->never,
                                NULL);

  /* ======================================================================== */
  /* ========================== Passwords =================================== */
  /* ======================================================================== */
  g_settings_bind (web_settings,
                   EPHY_PREFS_WEB_REMEMBER_PASSWORDS,
                   privacy_page->remember_passwords_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* ======================================================================== */
  /* ========================== Personal Data =============================== */
  /* ======================================================================== */
  g_signal_connect (privacy_page->clear_personal_data_button,
                    "clicked",
                    G_CALLBACK (clear_personal_data_button_clicked_cb),
                    privacy_page);
}

static void
prefs_privacy_page_class_init (PrefsPrivacyPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-privacy-page.ui");

  /* Web Content */
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, popups_allow_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, adblock_allow_switch);
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, enable_safe_browsing_switch);

  /* Cookies */
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, always);
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, no_third_party);
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, never);

  /* Passwords */
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, remember_passwords_switch);

  /* Personal Data */
  gtk_widget_class_bind_template_child (widget_class, PrefsPrivacyPage, clear_personal_data_button);

  /* Signals */
  gtk_widget_class_bind_template_callback (widget_class, on_manage_cookies_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_manage_passwords_button_clicked);
}

static void
prefs_privacy_page_init (PrefsPrivacyPage *privacy_page)
{
  gtk_widget_init_template (GTK_WIDGET (privacy_page));

  setup_privacy_page (privacy_page);
}
