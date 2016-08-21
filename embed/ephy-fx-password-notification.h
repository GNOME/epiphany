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

#ifndef EPHY_FX_PASSWORD_NOTIFICATION_H
#define EPHY_FX_PASSWORD_NOTIFICATION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_FX_PASSWORD_NOTIFICATION (ephy_fx_password_notification_get_type ())

#define EPHY_FX_PASSWORD_NOTIFICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   EPHY_TYPE_FX_PASSWORD_NOTIFICATION, EphyFxPasswordNotification))

#define EPHY_IS_FX_PASSWORD_NOTIFICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   EPHY_TYPE_FX_PASSWORD_NOTIFICATION))

typedef struct _EphyFxPasswordNotification      EphyFxPasswordNotification;
typedef struct _EphyFxPasswordNotificationClass EphyFxPasswordNotificationClass;

GType                       ephy_fx_password_notification_get_type (void) G_GNUC_CONST;

EphyFxPasswordNotification *ephy_fx_password_notification_new      (const char *user);

void                        ephy_fx_password_notification_show     (EphyFxPasswordNotification *self);

G_END_DECLS

#endif
