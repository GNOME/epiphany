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

#pragma once

#include <glib-object.h>
#include <adwaita.h>

G_BEGIN_DECLS

#define EPHY_TYPE_NOTIFICATION (ephy_notification_get_type ())

G_DECLARE_FINAL_TYPE (EphyNotification, ephy_notification, EPHY, NOTIFICATION, AdwBin)

EphyNotification *ephy_notification_new  (const char *head,
                                          const char *body);
void              ephy_notification_show (EphyNotification *self);

gboolean          ephy_notification_is_duplicate (EphyNotification *notification_a,
                                                  EphyNotification *notification_b);

G_END_DECLS
