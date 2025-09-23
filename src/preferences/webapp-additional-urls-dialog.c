/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2018 Igalia S.L.
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
#include "webapp-additional-urls-dialog.h"

#include "ephy-settings.h"
#include "ephy-string.h"
#include "ephy-uri-helpers.h"

#include <glib/gi18n.h>
#include <libsoup/soup.h>

struct _EphyWebappAdditionalURLsDialog {
  AdwNavigationPage parent_instance;

  GtkWidget *url_group;
  GtkWidget *add_row;
  GtkWidget *add_button;
};

G_DEFINE_FINAL_TYPE (EphyWebappAdditionalURLsDialog, ephy_webapp_additional_urls_dialog, ADW_TYPE_NAVIGATION_PAGE)

static void
on_remove_clicked (GtkWidget *button,
                   gpointer   user_data)
{
  EphyWebappAdditionalURLsDialog *self = EPHY_WEBAPP_ADDITIONAL_URLS_DIALOG (user_data);
  GtkWidget *row = gtk_widget_get_ancestor (button, ADW_TYPE_ACTION_ROW);
  g_auto (GStrv) urls = NULL;
  g_auto (GStrv) new_urls = NULL;
  const char *url = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));

  urls = g_settings_get_strv (EPHY_SETTINGS_WEB_APP, EPHY_PREFS_WEB_APP_ADDITIONAL_URLS);

  new_urls = ephy_strv_remove ((const char * const *)urls, url);
  g_settings_set_strv (EPHY_SETTINGS_WEB_APP, EPHY_PREFS_WEB_APP_ADDITIONAL_URLS, (const char * const *)new_urls);

  adw_preferences_group_remove (ADW_PREFERENCES_GROUP (self->url_group), row);
}

static void
add_row_internal (EphyWebappAdditionalURLsDialog *self,
                  const char                     *url)
{
  GtkWidget *row = adw_action_row_new ();
  GtkWidget *remove_button;

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), url);
  remove_button = gtk_button_new ();
  g_signal_connect (remove_button, "clicked", G_CALLBACK (on_remove_clicked), self);
  gtk_button_set_icon_name (GTK_BUTTON (remove_button), "edit-delete-symbolic");
  gtk_widget_add_css_class (remove_button, "flat");
  gtk_widget_set_tooltip_text (remove_button, _("Remove URL"));
  gtk_widget_set_valign (remove_button, GTK_ALIGN_CENTER);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), remove_button);

  adw_preferences_group_add (ADW_PREFERENCES_GROUP (self->url_group), row);
}

static void
on_add_clicked (GtkWidget *button,
                gpointer   user_data)
{
  EphyWebappAdditionalURLsDialog *self = EPHY_WEBAPP_ADDITIONAL_URLS_DIALOG (user_data);
  g_auto (GStrv) urls = NULL;
  g_auto (GStrv) lower_urls = NULL;
  g_auto (GStrv) new_urls = NULL;
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GError) error = NULL;
  const char *url = gtk_editable_get_text (GTK_EDITABLE (self->add_row));
  g_autofree char *base_domain = NULL;
  g_autoptr (GStrvBuilder) builder = NULL;

  urls = g_settings_get_strv (EPHY_SETTINGS_WEB_APP, EPHY_PREFS_WEB_APP_ADDITIONAL_URLS);

  if (!strstr (url, "://")) {
    g_autofree char *tmp_url = NULL;

    tmp_url = g_strdup_printf ("http://%s", url);
    uri = g_uri_parse (tmp_url, G_URI_FLAGS_PARSE_RELAXED, &error);
  } else {
    uri = g_uri_parse (url, G_URI_FLAGS_PARSE_RELAXED, &error);
  }

  if (error) {
    g_warning ("Could not parse url %s: %s", url, error->message);
    return;
  }

  base_domain = ephy_uri_get_base_domain (g_uri_get_host (uri));
  if (!base_domain) {
    g_warning ("Could not get base domain from host %s", g_uri_get_host (uri));
    return;
  }

  builder = g_strv_builder_new ();
  for (guint idx = 0; idx < g_strv_length (urls); idx++) {
    g_autofree char *lower_url = g_utf8_strdown (urls[idx], -1);

    g_strv_builder_add (builder, lower_url);
  }

  lower_urls = g_strv_builder_end (builder);

  if (g_strv_contains ((const char * const *)lower_urls, base_domain))
    return;

  add_row_internal (self, base_domain);

  new_urls = ephy_strv_append ((const char * const *)lower_urls, base_domain);
  g_settings_set_strv (EPHY_SETTINGS_WEB_APP, EPHY_PREFS_WEB_APP_ADDITIONAL_URLS, (const char * const *)new_urls);

  gtk_editable_set_text (GTK_EDITABLE (self->add_row), "");
}

static void
on_add_row_changed (GtkEditable *editable,
                    gpointer     user_data)
{
  EphyWebappAdditionalURLsDialog *self = EPHY_WEBAPP_ADDITIONAL_URLS_DIALOG (user_data);
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GError) error = NULL;
  const char *url = gtk_editable_get_text (editable);

  if (!strstr (url, "://")) {
    g_autofree char *tmp_url = NULL;

    tmp_url = g_strdup_printf ("http://%s", url);
    uri = g_uri_parse (tmp_url, G_URI_FLAGS_PARSE_RELAXED, &error);
  } else {
    uri = g_uri_parse (url, G_URI_FLAGS_PARSE_RELAXED, &error);
  }

  gtk_widget_set_sensitive (self->add_button, !error);
}

static void
on_add_row_entry_activated (AdwEntryRow *row,
                            gpointer     user_data)
{
  EphyWebappAdditionalURLsDialog *self = EPHY_WEBAPP_ADDITIONAL_URLS_DIALOG (user_data);

  gtk_widget_activate (self->add_button);
}

static void
ephy_webapp_additional_urls_dialog_class_init (EphyWebappAdditionalURLsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/webapp-additional-urls-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyWebappAdditionalURLsDialog, url_group);
  gtk_widget_class_bind_template_child (widget_class, EphyWebappAdditionalURLsDialog, add_row);
  gtk_widget_class_bind_template_child (widget_class, EphyWebappAdditionalURLsDialog, add_button);

  gtk_widget_class_bind_template_callback (widget_class, on_add_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_add_row_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_add_row_entry_activated);
}

static void
realize_dialog_cb (GtkWidget *widget,
                   gpointer   user_data)
{
  EphyWebappAdditionalURLsDialog *self = EPHY_WEBAPP_ADDITIONAL_URLS_DIALOG (widget);
  g_auto (GStrv) urls = NULL;
  guint i;

  urls = g_settings_get_strv (EPHY_SETTINGS_WEB_APP, EPHY_PREFS_WEB_APP_ADDITIONAL_URLS);
  for (i = 0; urls[i]; i++)
    add_row_internal (self, urls[i]);
}

static void
ephy_webapp_additional_urls_dialog_init (EphyWebappAdditionalURLsDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));

  g_signal_connect (GTK_WIDGET (dialog), "realize", G_CALLBACK (realize_dialog_cb), NULL);
}

EphyWebappAdditionalURLsDialog *
ephy_webapp_additional_urls_dialog_new (void)
{
  return g_object_new (EPHY_TYPE_WEBAPP_ADDITIONAL_URLS_DIALOG, NULL);
}
