/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
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

#include "ephy-synchronizable.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SYNCHRONIZABLE_MANAGER (ephy_synchronizable_manager_get_type ())

G_DECLARE_INTERFACE (EphySynchronizableManager, ephy_synchronizable_manager, EPHY, SYNCHRONIZABLE_MANAGER, GObject)

struct _EphySynchronizableManagerInterface {
  GTypeInterface parent_iface;

  const char         * (*get_collection_name)     (EphySynchronizableManager *manager);
  GType                (*get_synchronizable_type) (EphySynchronizableManager *manager);
  gboolean             (*is_initial_sync)         (EphySynchronizableManager *manager);
  void                 (*set_is_initial_sync)     (EphySynchronizableManager *manager,
                                                   gboolean                   is_initial);
  double               (*get_sync_time)           (EphySynchronizableManager *manager);
  void                 (*set_sync_time)           (EphySynchronizableManager *manager,
                                                   double                     sync_time);
  void                 (*add)                     (EphySynchronizableManager *manager,
                                                   EphySynchronizable        *synchronizable);
  void                 (*remove)                  (EphySynchronizableManager *manager,
                                                   EphySynchronizable        *synchronizable);
  GSList             * (*merge)                   (EphySynchronizableManager *manager,
                                                   gboolean                   is_initial,
                                                   GSList                    *remotes_deleted,
                                                   GSList                    *remotes_updated);
};

const char         *ephy_synchronizable_manager_get_collection_name     (EphySynchronizableManager *manager);
GType               ephy_synchronizable_manager_get_synchronizable_type (EphySynchronizableManager *manager);
gboolean            ephy_synchronizable_manager_is_initial_sync         (EphySynchronizableManager *manager);
void                ephy_synchronizable_manager_set_is_initial_sync     (EphySynchronizableManager *manager,
                                                                         gboolean                   is_initial);
double              ephy_synchronizable_manager_get_sync_time           (EphySynchronizableManager *manager);
void                ephy_synchronizable_manager_set_sync_time           (EphySynchronizableManager *manager,
                                                                         double                     sync_time);
void                ephy_synchronizable_manager_add                     (EphySynchronizableManager *manager,
                                                                         EphySynchronizable        *synchronizable);
void                ephy_synchronizable_manager_remove                  (EphySynchronizableManager *manager,
                                                                         EphySynchronizable        *synchronizable);
GSList             *ephy_synchronizable_manager_merge                   (EphySynchronizableManager *manager,
                                                                         gboolean                   is_initial,
                                                                         GSList                    *remotes_deleted,
                                                                         GSList                    *remotes_updated);

G_END_DECLS
