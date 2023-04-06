/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Gabriel Ivascu <gabrielivascu@gnome.org>
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
#include "ephy-notification.h"

#include "ephy-notification-container.h"

struct _EphyNotification {
  AdwBin parent_instance;

  GtkWidget *grid;

  GtkWidget *head;
  GtkWidget *body;
  GtkWidget *close_button;

  char *head_msg;
  char *body_msg;
};

enum {
  PROP_0,
  PROP_HEAD,
  PROP_BODY
};

enum {
  CLOSE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_FINAL_TYPE (EphyNotification, ephy_notification, ADW_TYPE_BIN);

static void
ephy_notification_constructed (GObject *object)
{
  EphyNotification *self = EPHY_NOTIFICATION (object);

  G_OBJECT_CLASS (ephy_notification_parent_class)->constructed (object);

  gtk_label_set_text (GTK_LABEL (self->head), self->head_msg);
  gtk_label_set_text (GTK_LABEL (self->body), self->body_msg);
}

static void
ephy_notification_finalize (GObject *object)
{
  EphyNotification *self = EPHY_NOTIFICATION (object);

  g_free (self->head_msg);
  g_free (self->body_msg);

  G_OBJECT_CLASS (ephy_notification_parent_class)->finalize (object);
}

static void
ephy_notification_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  EphyNotification *self = EPHY_NOTIFICATION (object);

  switch (prop_id) {
    case PROP_HEAD:
      self->head_msg = g_value_dup_string (value);
      break;
    case PROP_BODY:
      self->body_msg = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_notification_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  EphyNotification *self = EPHY_NOTIFICATION (object);

  switch (prop_id) {
    case PROP_HEAD:
      g_value_set_string (value, self->head_msg);
      break;
    case PROP_BODY:
      g_value_set_string (value, self->body_msg);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
close_button_clicked_cb (GtkButton        *button,
                         EphyNotification *self)
{
  g_signal_emit (self, signals[CLOSE], 0);
}

static void
ephy_notification_init (EphyNotification *self)
{
  gtk_widget_add_css_class (GTK_WIDGET (self), "app-notification");

  self->grid = gtk_grid_new ();
  adw_bin_set_child (ADW_BIN (self), self->grid);

  self->head = gtk_label_new (NULL);
  gtk_label_set_wrap (GTK_LABEL (self->head), TRUE);
  gtk_label_set_xalign (GTK_LABEL (self->head), 0);
  gtk_widget_set_hexpand (self->head, TRUE);
  gtk_grid_attach (GTK_GRID (self->grid), self->head, 0, 0, 1, 1);

  self->body = gtk_label_new (NULL);
  gtk_label_set_wrap (GTK_LABEL (self->body), TRUE);
  gtk_label_set_xalign (GTK_LABEL (self->body), 0);
  gtk_widget_set_hexpand (self->body, TRUE);
  gtk_grid_attach (GTK_GRID (self->grid), self->body, 0, 1, 1, 1);

  self->close_button = gtk_button_new_from_icon_name ("window-close-symbolic");
  gtk_widget_set_focus_on_click (self->close_button, FALSE);
  gtk_widget_set_margin_top (self->close_button, 6);
  gtk_widget_set_margin_bottom (self->close_button, 6);
  gtk_widget_set_margin_start (self->close_button, 6);
  gtk_widget_set_margin_end (self->close_button, 6);
  gtk_widget_add_css_class (self->close_button, "flat");
  gtk_grid_attach (GTK_GRID (self->grid), self->close_button, 1, 0, 1, 2);

  g_signal_connect (self->close_button,
                    "clicked",
                    G_CALLBACK (close_button_clicked_cb),
                    self);
}

static void
ephy_notification_class_init (EphyNotificationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ephy_notification_constructed;
  object_class->finalize = ephy_notification_finalize;
  object_class->set_property = ephy_notification_set_property;
  object_class->get_property = ephy_notification_get_property;

  g_object_class_install_property (object_class,
                                   PROP_HEAD,
                                   g_param_spec_string ("head",
                                                        NULL, NULL,
                                                        "",
                                                        G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_BODY,
                                   g_param_spec_string ("body",
                                                        NULL, NULL,
                                                        "",
                                                        G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  signals[CLOSE] =
    g_signal_new ("close",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

EphyNotification *
ephy_notification_new (const char *head,
                       const char *body)
{
  return g_object_new (EPHY_TYPE_NOTIFICATION,
                       "head", head,
                       "body", body,
                       NULL);
}

void
ephy_notification_show (EphyNotification *self)
{
  g_assert (EPHY_IS_NOTIFICATION (self));

  ephy_notification_container_add_notification (ephy_notification_container_get_default (),
                                                GTK_WIDGET (self));
}

gboolean
ephy_notification_is_duplicate (EphyNotification *notification_a,
                                EphyNotification *notification_b)
{
  return g_strcmp0 (notification_a->head_msg, notification_b->head_msg) == 0 && g_strcmp0 (notification_a->body_msg, notification_b->body_msg) == 0;
}
