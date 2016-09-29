/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-notification-container.h"

struct _EphyNotificationContainer {
  GdNotification  parent_instance;

  GtkWidget      *grid;
};

struct _EphyNotificationContainerClass {
  GdNotificationClass parent_class;
};

G_DEFINE_TYPE (EphyNotificationContainer, ephy_notification_container, GD_TYPE_NOTIFICATION);

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
                       "show-close-button", TRUE,
                       "timeout", -1,
                       NULL);
}

void
ephy_notification_container_add_notification (EphyNotificationContainer *self,
                                              GtkWidget                 *notification)
{
  g_return_if_fail (EPHY_IS_NOTIFICATION_CONTAINER (self));
  g_return_if_fail (GTK_IS_WIDGET (notification));

  gtk_container_add (GTK_CONTAINER (self->grid), notification);
  gtk_widget_show_all (GTK_WIDGET (self));
}
