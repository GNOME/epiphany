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

#ifndef EPHY_NOTIFICATION_MANAGER_H
#define EPHY_NOTIFICATION_MANAGER_H

#include <libgd/gd.h>

G_BEGIN_DECLS

#define EPHY_TYPE_NOTIFICATION_MANAGER (ephy_notification_manager_get_type ())

#define EPHY_NOTIFICATION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   EPHY_TYPE_NOTIFICATION_MANAGER, EphyNotificationManager))

#define EPHY_IS_NOTIFICATION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   EPHY_TYPE_NOTIFICATION_MANAGER))

typedef struct _EphyNotificationManager      EphyNotificationManager;
typedef struct _EphyNotificationManagerClass EphyNotificationManagerClass;

GType                    ephy_notification_manager_get_type         (void) G_GNUC_CONST;

EphyNotificationManager *ephy_notification_manager_dup_singleton    (void);

void                     ephy_notification_manager_add_notification (EphyNotificationManager *self,
                                                                     GtkWidget               *notification);

guint                    ephy_notification_manager_get_children_num (EphyNotificationManager *self);

G_END_DECLS

#endif
