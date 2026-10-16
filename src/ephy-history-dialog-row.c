/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright 2026 Red Hat
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

#include "ephy-history-dialog-row.h"

#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-favicon-helpers.h"
#include "ephy-time-helpers.h"
#include "ephy-uri-helpers.h"

struct _EphyHistoryDialogRow {
  AdwActionRow parent_instance;

  GtkWidget *icon;
  GtkWidget *check_button;
  GtkWidget *date_label;
  GtkWidget *copy_url_button;
  GCancellable *cancellable;

  char *url;
  char *date;
};

G_DEFINE_FINAL_TYPE (EphyHistoryDialogRow, ephy_history_dialog_row, ADW_TYPE_ACTION_ROW)

typedef enum {
  PROP_URL = 1,
  PROP_DATE,
} EphyHistoryDialogRowProps;

static GParamSpec *props[PROP_DATE + 1];

static void
ephy_history_dialog_row_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  EphyHistoryDialogRow *self = EPHY_HISTORY_DIALOG_ROW (object);

  switch ((EphyHistoryDialogRowProps)prop_id) {
    case PROP_URL:
      g_value_set_string (value, self->url);
      break;
    case PROP_DATE:
      g_value_set_string (value, self->date);
      break;
  }
}

static void
ephy_history_dialog_row_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  EphyHistoryDialogRow *self = EPHY_HISTORY_DIALOG_ROW (object);

  switch ((EphyHistoryDialogRowProps)prop_id) {
    case PROP_URL:
      g_set_str (&self->url, g_value_dup_string (value));
      break;
    case PROP_DATE:
      g_set_str (&self->date, g_value_dup_string (value));
      break;
  }
}

static void
ephy_history_dialog_row_favicon_loaded_cb (GObject      *source,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  g_autoptr (GtkWidget) icon = user_data;
  WebKitFaviconDatabase *database = WEBKIT_FAVICON_DATABASE (source);
  g_autoptr (GdkTexture) icon_texture = NULL;
  g_autoptr (GIcon) favicon = NULL;
  int scale;

  icon_texture = webkit_favicon_database_get_favicon_finish (database, result, NULL);
  if (!icon_texture)
    return;

  scale = gtk_widget_get_scale_factor (icon);
  favicon = ephy_favicon_get_from_texture_scaled (icon_texture, FAVICON_SIZE * scale, FAVICON_SIZE * scale);
  if (favicon && icon)
    gtk_image_set_from_gicon (GTK_IMAGE (icon), favicon);
}

static void
ephy_history_dialog_row_constructed (GObject *object)
{
  EphyHistoryDialogRow *self = EPHY_HISTORY_DIALOG_ROW (object);
  g_autofree char *decoded_url = ephy_uri_decode (self->url);
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  WebKitFaviconDatabase *database;

  G_OBJECT_CLASS (ephy_history_dialog_row_parent_class)->constructed (object);

  if (!decoded_url)
    decoded_url = g_strdup (self->url);

  adw_action_row_set_subtitle (ADW_ACTION_ROW (self), decoded_url);
  gtk_widget_set_tooltip_text (GTK_WIDGET (self), decoded_url);

  database = ephy_embed_shell_get_favicon_database (shell);
  webkit_favicon_database_get_favicon (database,
                                       self->url,
                                       self->cancellable,
                                       (GAsyncReadyCallback)ephy_history_dialog_row_favicon_loaded_cb,
                                       g_object_ref (self->icon));

  gtk_widget_set_sensitive (self->check_button,
                            ephy_embed_shell_get_mode (shell) != EPHY_EMBED_SHELL_MODE_INCOGNITO);
}

static void
ephy_history_dialog_row_dispose (GObject *object)
{
  EphyHistoryDialogRow *self = EPHY_HISTORY_DIALOG_ROW (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (ephy_history_dialog_row_parent_class)->dispose (object);
}

static void
ephy_history_dialog_row_finalize (GObject *object)
{
  EphyHistoryDialogRow *self = EPHY_HISTORY_DIALOG_ROW (object);

  g_free (self->url);
  g_free (self->date);

  G_OBJECT_CLASS (ephy_history_dialog_row_parent_class)->finalize (object);
}

static void
ephy_history_dialog_row_class_init (EphyHistoryDialogRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ephy_history_dialog_row_get_property;
  object_class->set_property = ephy_history_dialog_row_set_property;
  object_class->constructed = ephy_history_dialog_row_constructed;
  object_class->dispose = ephy_history_dialog_row_dispose;
  object_class->finalize = ephy_history_dialog_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/history-dialog-row.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialogRow, icon);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialogRow, check_button);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialogRow, date_label);
  gtk_widget_class_bind_template_child (widget_class, EphyHistoryDialogRow, copy_url_button);

  props[PROP_URL] =
    g_param_spec_string ("url", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_DATE] =
    g_param_spec_string ("date", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (props), props);
}

static void
ephy_history_dialog_row_init (EphyHistoryDialogRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();
}

GtkWidget *
ephy_history_dialog_row_new (EphyHistoryURL *url)
{
  return g_object_new (EPHY_TYPE_HISTORY_DIALOG_ROW,
                       "url", url->url,
                       "title", url->title,
                       "date", ephy_time_helpers_utf_friendly_time (url->last_visit_time / 1000000),
                       NULL);
}

GtkWidget *
ephy_history_dialog_row_get_check_button (EphyHistoryDialogRow *self)
{
  g_assert (EPHY_IS_HISTORY_DIALOG_ROW (self));

  return self->check_button;
}

GtkWidget *
ephy_history_dialog_row_get_copy_url_button (EphyHistoryDialogRow *self)
{
  g_assert (EPHY_IS_HISTORY_DIALOG_ROW (self));

  return self->copy_url_button;
}

/* Warning: this does not contain the same data as an EphyHistoryURL created by
 * the history service. It should be used for querying.
 */
EphyHistoryURL *
ephy_history_dialog_row_create_history_url (EphyHistoryDialogRow *self)
{
  g_assert (EPHY_IS_HISTORY_DIALOG_ROW (self));

  return ephy_history_url_new (self->url,
                               adw_preferences_row_get_title (ADW_PREFERENCES_ROW (self)),
                               0,
                               0,
                               0);
}
