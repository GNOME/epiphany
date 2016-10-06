/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2015 Gustavo Noronha Silva <gns@gnome.org>
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

#ifndef EPHY_HOSTS_MANAGER_H
#define EPHY_HOSTS_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_HOSTS_MANAGER (ephy_hosts_manager_get_type ())

G_DECLARE_FINAL_TYPE (EphyHostsManager, ephy_hosts_manager, EPHY, HOSTS_MANAGER, GObject)

typedef enum {
  EPHY_HOST_PERMISSION_UNDECIDED = -1,
  EPHY_HOST_PERMISSION_DENY = 0,
  EPHY_HOST_PERMISSION_ALLOW = 1,
} EphyHostPermission;

EphyHostsManager*       ephy_hosts_manager_new                                      (void);
EphyHostPermission      ephy_hosts_manager_get_notifications_permission_for_address (EphyHostsManager    *manager,
                                                                                     const char          *address);
void                    ephy_hosts_manager_set_notifications_permission_for_address (EphyHostsManager    *manager,
                                                                                     const char          *address,
                                                                                     EphyHostPermission   permission);
EphyHostPermission      ephy_hosts_manager_get_save_password_permission_for_address (EphyHostsManager    *manager,
                                                                                     const char          *address);
void                    ephy_hosts_manager_set_save_password_permission_for_address (EphyHostsManager    *manager,
                                                                                     const char          *address,
                                                                                     EphyHostPermission   permission);


G_END_DECLS

#endif /* EPHY_HOSTS_MANAGER_H */
