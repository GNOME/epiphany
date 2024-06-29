/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Iulian-Gabriel Radu <iulian.radu67@gmail.com>
 *  Copyright 2022 Igalia S.L.
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
#include "ephy-browser-action-row.h"

#include "ephy-indicator-bin-private.h"
#include "ephy-pixbuf-utils.h"

struct _EphyBrowserActionRow {
  GtkListBoxRow parent_instance;

  EphyBrowserAction *browser_action;

  GtkWidget *browser_action_image;
  GtkWidget *title_label;
  GtkWidget *badge;
};

G_DEFINE_FINAL_TYPE (EphyBrowserActionRow, ephy_browser_action_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_BROWSER_ACTION,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_browser_action_row_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  EphyBrowserActionRow *self = EPHY_BROWSER_ACTION_ROW (object);

  switch (prop_id) {
    case PROP_BROWSER_ACTION:
      self->browser_action = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_browser_action_row_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  EphyBrowserActionRow *self = EPHY_BROWSER_ACTION_ROW (object);

  switch (prop_id) {
    case PROP_BROWSER_ACTION:
      g_value_set_object (value, ephy_browser_action_row_get_browser_action (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
on_badge_text (EphyBrowserAction *action,
               GParamSpec        *spec,
               gpointer           user_data)
{
  EphyBrowserActionRow *self = EPHY_BROWSER_ACTION_ROW (user_data);
  const char *text = ephy_browser_action_get_badge_text (EPHY_BROWSER_ACTION (self->browser_action));

  ephy_indicator_bin_set_badge (EPHY_INDICATOR_BIN (self->badge), text);
}

static void
on_badge_color (EphyBrowserAction *action,
                GParamSpec        *spec,
                gpointer           user_data)
{
  EphyBrowserActionRow *self = EPHY_BROWSER_ACTION_ROW (user_data);
  GdkRGBA *color = ephy_browser_action_get_badge_background_color (EPHY_BROWSER_ACTION (self->browser_action));

  ephy_indicator_bin_set_badge_color (EPHY_INDICATOR_BIN (self->badge), color);
}

static void
ephy_browser_action_row_constructed (GObject *object)
{
  EphyBrowserActionRow *self = EPHY_BROWSER_ACTION_ROW (object);
  g_autoptr (GdkTexture) texture = NULL;
  GdkPixbuf *pixbuf;

  gtk_label_set_label (GTK_LABEL (self->title_label),
                       ephy_browser_action_get_title (self->browser_action));

  pixbuf = ephy_browser_action_get_pixbuf (self->browser_action, 16);
  texture = ephy_texture_new_for_pixbuf (pixbuf);
  gtk_image_set_from_paintable (GTK_IMAGE (self->browser_action_image), GDK_PAINTABLE (texture));

  ephy_indicator_bin_set_badge (EPHY_INDICATOR_BIN (self->badge), ephy_browser_action_get_badge_text (EPHY_BROWSER_ACTION (self->browser_action)));
  g_signal_connect (EPHY_BROWSER_ACTION (self->browser_action), "notify::badge-text", G_CALLBACK (on_badge_text), self);
  g_signal_connect (EPHY_BROWSER_ACTION (self->browser_action), "notify::badge-color", G_CALLBACK (on_badge_color), self);

  G_OBJECT_CLASS (ephy_browser_action_row_parent_class)->constructed (object);
}

static void
ephy_browser_action_row_dispose (GObject *object)
{
  EphyBrowserActionRow *self = EPHY_BROWSER_ACTION_ROW (object);

  g_clear_object (&self->browser_action);

  G_OBJECT_CLASS (ephy_browser_action_row_parent_class)->dispose (object);
}

static void
ephy_browser_action_row_class_init (EphyBrowserActionRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ephy_browser_action_row_set_property;
  object_class->get_property = ephy_browser_action_row_get_property;
  object_class->dispose = ephy_browser_action_row_dispose;
  object_class->constructed = ephy_browser_action_row_constructed;

  obj_properties[PROP_BROWSER_ACTION] =
    g_param_spec_object ("browser-action",
                         NULL, NULL,
                         EPHY_TYPE_BROWSER_ACTION,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/browser-action-row.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyBrowserActionRow, browser_action_image);
  gtk_widget_class_bind_template_child (widget_class, EphyBrowserActionRow, title_label);
  gtk_widget_class_bind_template_child (widget_class, EphyBrowserActionRow, badge);
}

static void
ephy_browser_action_row_init (EphyBrowserActionRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ephy_browser_action_row_new (EphyBrowserAction *browser_action)
{
  return g_object_new (EPHY_TYPE_BROWSER_ACTION_ROW,
                       "browser-action", browser_action,
                       NULL);
}

EphyBrowserAction *
ephy_browser_action_row_get_browser_action (EphyBrowserActionRow *self)
{
  g_assert (EPHY_IS_BROWSER_ACTION_ROW (self));

  return self->browser_action;
}
