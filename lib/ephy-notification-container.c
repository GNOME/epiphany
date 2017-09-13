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
#include "ephy-notification-container.h"

struct _EphyNotificationContainer {
  GtkRevealer  parent_instance;

  GtkWidget   *grid;
};

struct _EphyNotificationContainerClass {
  GtkRevealerClass parent_class;
};

G_DEFINE_TYPE (EphyNotificationContainer, ephy_notification_container, GTK_TYPE_REVEALER);

static EphyNotificationContainer *notification_container = NULL;

static void
ephy_notification_container_init (EphyNotificationContainer *self)
{
  /* Globally accessible singleton */
  g_assert (notification_container == NULL);
  notification_container = self;
  g_object_add_weak_pointer (G_OBJECT (notification_container),
                             (gpointer *)&notification_container);

  gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_START);

  self->grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (self->grid), 6);
  gtk_container_add (GTK_CONTAINER (self), self->grid);
}

static void
ephy_notification_container_class_init (EphyNotificationContainerClass *klass)
{
}

EphyNotificationContainer *
ephy_notification_container_get_default (void)
{
  if (notification_container != NULL)
    return notification_container;

  return g_object_new (EPHY_TYPE_NOTIFICATION_CONTAINER,
                       NULL);
}

void
ephy_notification_container_add_notification (EphyNotificationContainer *self,
                                              GtkWidget                 *notification)
{
  g_assert (EPHY_IS_NOTIFICATION_CONTAINER (self));
  g_assert (GTK_IS_WIDGET (notification));

  gtk_container_add (GTK_CONTAINER (self->grid), notification);
  gtk_widget_show_all (GTK_WIDGET (self));
  gtk_revealer_set_reveal_child (GTK_REVEALER (self), TRUE);
}
