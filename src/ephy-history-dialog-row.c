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

#include "ephy-uri-helpers.h"

struct _EphyHistoryDialogRow {
  AdwActionRow parent_instance;

  char *url;
  char *title;
};

G_DEFINE_FINAL_TYPE (EphyHistoryDialogRow, ephy_history_dialog_row, ADW_TYPE_ACTION_ROW)

enum {
  PROP_0,
  PROP_URL,
  PROP_TITLE,
  LAST_PROP,
};

static GParamSpec *props[LAST_PROP];

static void
ephy_history_dialog_row_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  EphyHistoryDialogRow *self = EPHY_HISTORY_DIALOG_ROW (object);

  switch (prop_id) {
    case PROP_URL:
      g_value_set_string (value, self->url);
      break;
    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_history_dialog_row_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  EphyHistoryDialogRow *self = EPHY_HISTORY_DIALOG_ROW (object);

  switch (prop_id) {
    case PROP_URL:
      g_set_str (&self->url, g_value_dup_string (value));
      break;
    case PROP_TITLE:
      g_set_str (&self->title, g_value_dup_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_history_dialog_row_constructed (GObject *object)
{
  EphyHistoryDialogRow *self = EPHY_HISTORY_DIALOG_ROW (object);

  g_autofree char *title_escaped = g_markup_escape_text (self->title, -1);
  g_autofree char *subtitle_escaped = NULL;
  g_autofree char *decoded_url = ephy_uri_decode (self->url);

  if (!decoded_url)
    decoded_url = g_strdup (self->url);
  subtitle_escaped = g_markup_escape_text (decoded_url, -1);

  adw_action_row_set_title_lines (ADW_ACTION_ROW (self), 1);
  adw_action_row_set_subtitle_lines (ADW_ACTION_ROW (self), 1);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self), title_escaped);
  adw_action_row_set_subtitle (ADW_ACTION_ROW (self), subtitle_escaped);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (self), TRUE);
  gtk_widget_set_tooltip_text (GTK_WIDGET (self), decoded_url);

  G_OBJECT_CLASS (ephy_history_dialog_row_parent_class)->constructed (object);
}

static void
ephy_history_dialog_row_finalize (GObject *object)
{
  EphyHistoryDialogRow *self = EPHY_HISTORY_DIALOG_ROW (object);

  g_free (self->url);
  g_free (self->title);

  G_OBJECT_CLASS (ephy_history_dialog_row_parent_class)->finalize (object);
}

static void
ephy_history_dialog_row_class_init (EphyHistoryDialogRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ephy_history_dialog_row_get_property;
  object_class->set_property = ephy_history_dialog_row_set_property;
  object_class->constructed = ephy_history_dialog_row_constructed;
  object_class->finalize = ephy_history_dialog_row_finalize;

  props[PROP_URL] =
    g_param_spec_string ("url", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  props[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL, NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
ephy_history_dialog_row_init (EphyHistoryDialogRow *self)
{
}

GtkWidget *
ephy_history_dialog_row_new (EphyHistoryURL *url)
{
  return g_object_new (EPHY_TYPE_HISTORY_DIALOG_ROW,
                       "url", url->url,
                       "title", url->title,
                       NULL);
}

/* Warning: this does not contain the same data as an EphyHistoryURL created by
 * the history service. It should be used for querying.
 */
EphyHistoryURL *
ephy_history_dialog_row_create_history_url (EphyHistoryDialogRow *self)
{
  g_assert (EPHY_IS_HISTORY_DIALOG_ROW (self));

  return ephy_history_url_new (self->url,
                               self->title,
                               0,
                               0,
                               0);
}
