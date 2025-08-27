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

struct _EphyNotificationContainer {
  AdwBin parent_instance;

  GtkWidget *revealer;
  GtkWidget *box;
};

G_DEFINE_FINAL_TYPE (EphyNotificationContainer, ephy_notification_container, ADW_TYPE_BIN);

static EphyNotificationContainer *notification_container = NULL;

static void
ephy_notification_container_init (EphyNotificationContainer *self)
{
  /* Globally accessible singleton */
  g_assert (!notification_container);
  notification_container = self;
  g_object_add_weak_pointer (G_OBJECT (notification_container),
                             (gpointer *)&notification_container);

  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_START);

  self->revealer = gtk_revealer_new ();
  adw_bin_set_child (ADW_BIN (self), self->revealer);

  self->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_revealer_set_child (GTK_REVEALER (self->revealer), self->box);

  gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
}

static void
ephy_notification_container_class_init (EphyNotificationContainerClass *klass)
{
}

EphyNotificationContainer *
ephy_notification_container_get_default (void)
{
  if (notification_container)
    return notification_container;

  return g_object_new (EPHY_TYPE_NOTIFICATION_CONTAINER,
                       NULL);
}

static void
notification_close_cb (EphyNotification          *notification,
                       EphyNotificationContainer *self)
{
  gtk_box_remove (GTK_BOX (self->box), GTK_WIDGET (notification));

  if (!gtk_widget_get_first_child (self->box)) {
    gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
    gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), FALSE);
  }
}

void
ephy_notification_container_add_notification (EphyNotificationContainer *self,
                                              GtkWidget                 *notification)
{
  GtkWidget *child;

  g_assert (EPHY_IS_NOTIFICATION_CONTAINER (self));
  g_assert (GTK_IS_WIDGET (notification));

  for (child = gtk_widget_get_first_child (self->box);
       child;
       child = gtk_widget_get_next_sibling (child)) {
    EphyNotification *child_notification = EPHY_NOTIFICATION (child);

    if (ephy_notification_is_duplicate (child_notification, EPHY_NOTIFICATION (notification))) {
      /* Don't need the newly-created EphyNotification. Goodbye!
       *
       * Since we will not add it to the widget hierarchy, we have to sink its floating ref.
       */
      g_object_ref_sink (G_OBJECT (notification));
      g_object_unref (G_OBJECT (notification));
      return;
    }
  }

  gtk_box_append (GTK_BOX (self->box), notification);
  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
  gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), TRUE);

  g_signal_connect (notification, "close", G_CALLBACK (notification_close_cb), self);
}
