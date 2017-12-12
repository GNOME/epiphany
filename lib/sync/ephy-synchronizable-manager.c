/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <gabrielivascu@gnome.org>
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
#include "ephy-synchronizable-manager.h"

G_DEFINE_INTERFACE (EphySynchronizableManager, ephy_synchronizable_manager, G_TYPE_OBJECT);

enum {
  SYNCHRONIZABLE_DELETED,
  SYNCHRONIZABLE_MODIFIED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
ephy_synchronizable_manager_default_init (EphySynchronizableManagerInterface *iface)
{
  iface->get_collection_name = ephy_synchronizable_manager_get_collection_name;
  iface->get_synchronizable_type = ephy_synchronizable_manager_get_synchronizable_type;
  iface->is_initial_sync = ephy_synchronizable_manager_is_initial_sync;
  iface->set_is_initial_sync = ephy_synchronizable_manager_set_is_initial_sync;
  iface->get_sync_time = ephy_synchronizable_manager_get_sync_time;
  iface->set_sync_time = ephy_synchronizable_manager_set_sync_time;
  iface->add = ephy_synchronizable_manager_add;
  iface->remove = ephy_synchronizable_manager_remove;
  iface->save = ephy_synchronizable_manager_save;
  iface->merge = ephy_synchronizable_manager_merge;

  signals[SYNCHRONIZABLE_DELETED] =
    g_signal_new ("synchronizable-deleted",
                  EPHY_TYPE_SYNCHRONIZABLE_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  EPHY_TYPE_SYNCHRONIZABLE);

  signals[SYNCHRONIZABLE_MODIFIED] =
    g_signal_new ("synchronizable-modified",
                  EPHY_TYPE_SYNCHRONIZABLE_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  EPHY_TYPE_SYNCHRONIZABLE,
                  G_TYPE_BOOLEAN);
}

/**
 * ephy_synchronizable_manager_get_collection_name:
 * @manager: an #EphySynchronizableManager
 *
 * Returns the name of the collection managed by @manager.
 *
 * Return value: (transfer none): @manager's collection name
 **/
const char *
ephy_synchronizable_manager_get_collection_name (EphySynchronizableManager *manager)
{
  EphySynchronizableManagerInterface *iface;

  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));

  iface = EPHY_SYNCHRONIZABLE_MANAGER_GET_IFACE (manager);
  return iface->get_collection_name (manager);
}

/**
 * ephy_synchronizable_manager_get_synchronizable_type:
 * @manager: an #EphySynchronizableManager
 *
 * Returns the #GType of the #EphySynchronizable objects managed by @manager.
 *
 * Return value: the #GType of @manager's objects
 **/
GType
ephy_synchronizable_manager_get_synchronizable_type (EphySynchronizableManager *manager)
{
  EphySynchronizableManagerInterface *iface;

  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));

  iface = EPHY_SYNCHRONIZABLE_MANAGER_GET_IFACE (manager);
  return iface->get_synchronizable_type (manager);
}

/**
 * ephy_synchronizable_manager_is_initial_sync:
 * @manager: an #EphySynchronizableManager
 *
 * Returns a boolean saying whether the collection managed by @manager requires
 * an initial sync (i.e. a first time sync).
 *
 * Return value: %TRUE is @manager's collections requires an initial sync, %FALSE otherwise
 **/
gboolean
ephy_synchronizable_manager_is_initial_sync (EphySynchronizableManager *manager)
{
  EphySynchronizableManagerInterface *iface;

  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));

  iface = EPHY_SYNCHRONIZABLE_MANAGER_GET_IFACE (manager);
  return iface->is_initial_sync (manager);
}

/**
 * ephy_synchronizable_manager_set_is_initial_sync:
 * @manager: an #EphySynchronizableManager
 * @is_initial: a boolean saying whether the collection managed by @manager
 *              requires an initial sync (i.e. a first time sync)
 *
 * Sets @manager's 'requires initial sync' flag.
 **/
void
ephy_synchronizable_manager_set_is_initial_sync (EphySynchronizableManager *manager,
                                                 gboolean                   is_initial)
{
  EphySynchronizableManagerInterface *iface;

  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));

  iface = EPHY_SYNCHRONIZABLE_MANAGER_GET_IFACE (manager);
  iface->set_is_initial_sync (manager, is_initial);
}

/**
 * ephy_synchronizable_manager_get_sync_time:
 * @manager: an #EphySynchronizableManager
 *
 * Returns the timestamp at which @manager's collection was last synced,
 * in seconds since UNIX epoch.
 *
 * Return value: the timestamp of @manager's collection last sync.
 **/
gint64
ephy_synchronizable_manager_get_sync_time (EphySynchronizableManager *manager)
{
  EphySynchronizableManagerInterface *iface;

  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));

  iface = EPHY_SYNCHRONIZABLE_MANAGER_GET_IFACE (manager);
  return iface->get_sync_time (manager);
}

/**
 * ephy_synchronizable_manager_set_sync_time:
 * @manager: an #EphySynchronizableManager
 * @sync_time: the timestamp of the last sync, in seconds since UNIX epoch
 *
 * Sets the timestamp at which @manager's collection was last synced.
 **/
void
ephy_synchronizable_manager_set_sync_time (EphySynchronizableManager *manager,
                                           gint64                     sync_time)
{
  EphySynchronizableManagerInterface *iface;

  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));

  iface = EPHY_SYNCHRONIZABLE_MANAGER_GET_IFACE (manager);
  iface->set_sync_time (manager, sync_time);
}

/**
 * ephy_synchronizable_manager_add:
 * @manager: an #EphySynchronizableManager
 * @synchronizable: (transfer none): an #EphySynchronizable
 *
 * Adds @synchronizable to the local instance of the collection managed by @manager.
 **/
void
ephy_synchronizable_manager_add (EphySynchronizableManager *manager,
                                 EphySynchronizable        *synchronizable)
{
  EphySynchronizableManagerInterface *iface;

  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));
  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));

  iface = EPHY_SYNCHRONIZABLE_MANAGER_GET_IFACE (manager);
  iface->add (manager, synchronizable);
}

/**
 * ephy_synchronizable_manager_remove:
 * @manager: an #EphySynchronizableManager
 * @synchronizable: (transfer none): an #EphySynchronizable
 *
 * Removes @synchronizable from the local instance of the collection managed by @manager.
 **/
void
ephy_synchronizable_manager_remove (EphySynchronizableManager *manager,
                                    EphySynchronizable        *synchronizable)
{
  EphySynchronizableManagerInterface *iface;

  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));
  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));

  iface = EPHY_SYNCHRONIZABLE_MANAGER_GET_IFACE (manager);
  iface->remove (manager, synchronizable);
}

/**
 * ephy_synchronizable_manager_save:
 * @manager: an #EphySynchronizableManager
 * @synchronizable: (transfer none): an #EphySynchronizable
 *
 * Saves @synchronizable in the local storage (e.g. file, database, secret schema).
 * This should only be called on a synchronizable object that already exists in
 * in @manager's collection (i.e. previously added with ephy_synchronizable_manager_add)
 * as the @manager expects it to be present. Normally ephy_synchronizable_manager_add
 * saves the object in the local storage, so the only use case of this function
 * is to save an existing object which was modified outside the manager (i.e. modified
 * by the sync service).
 **/
void
ephy_synchronizable_manager_save (EphySynchronizableManager *manager,
                                  EphySynchronizable        *synchronizable)
{
  EphySynchronizableManagerInterface *iface;

  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));
  g_assert (EPHY_IS_SYNCHRONIZABLE (synchronizable));

  iface = EPHY_SYNCHRONIZABLE_MANAGER_GET_IFACE (manager);
  iface->save (manager, synchronizable);
}

/**
 * ephy_synchronizable_manager_merge:
 * @manager: an #EphySynchronizableManager
 * @is_initial: a boolean saying whether the collection managed by @manager
 *              requires an initial sync (i.e. a first time sync)
 * @remotes_deleted: (transfer none): a #GList holding the #EphySynchronizable
 *                   objects that were removed remotely from the server.
 * @remotes_updated: (transfer none): a #GList holding the #EphySynchronizable
 *                   objects that were updated remotely on the server.
 * @callback: an #EphySynchronizableManagerMergeCallback that will be called
 *            when the merge is complete.
 * @user_data: user data to pass to the @callback.
 *
 * Merges a list of remote-deleted objects and a list of remote-updated objects
 * with the local objects in @manager's collection. When the merge is completed,
 * @callback will be invoked with a list of objects that need to be (re)uploaded
 * to server. Transfer full for the list of objects to be (re)uploaded.
 **/
void
ephy_synchronizable_manager_merge (EphySynchronizableManager              *manager,
                                   gboolean                                is_initial,
                                   GList                                  *remotes_deleted,
                                   GList                                  *remotes_updated,
                                   EphySynchronizableManagerMergeCallback  callback,
                                   gpointer                                user_data)
{
  EphySynchronizableManagerInterface *iface;

  g_assert (EPHY_IS_SYNCHRONIZABLE_MANAGER (manager));
  g_assert (callback);

  iface = EPHY_SYNCHRONIZABLE_MANAGER_GET_IFACE (manager);
  iface->merge (manager, is_initial, remotes_deleted, remotes_updated, callback, user_data);
}
