/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-permission-manager.h"
#include "ephy-marshal.h"
#include "ephy-debug.h"

/* EphyPermissionInfo */

GType
ephy_permission_info_get_type (void)
{
	static GType type = 0;

	if (type == 0)
	{
		type = g_boxed_type_register_static ("EphyPermissionInfo",
						     (GBoxedCopyFunc) ephy_permission_info_copy,
						     (GBoxedFreeFunc) ephy_permission_info_free);
	}

	return type;
}

/**
 * ephy_permission_info_new:
 * @host: a host name
 * @type: a #EphyPermissionType
 * @allowed: whether @host should be allowed to do what @type specifies
 *
 * Return value: the new #EphyPermissionInfo
 **/
EphyPermissionInfo *
ephy_permission_info_new (const char *host,
			  EphyPermissionType type,
			  gboolean allowed)
{
	EphyPermissionInfo *info = g_new0 (EphyPermissionInfo, 1);

	info->host = g_strdup (host);
	info->type = type;
	info->allowed = allowed;

	return info;
}

/**
 * ephy_permission_info_copy:
 * @info: a #EphyPermissionInfo
 *
 * Return value: a copy of @info
 **/
EphyPermissionInfo *
ephy_permission_info_copy (EphyPermissionInfo *info)
{
	EphyPermissionInfo *copy = g_new0 (EphyPermissionInfo, 1);

	copy->host = g_strdup (info->host);
	copy->type = info->type;
	copy->allowed = info->allowed;

	return copy;
}

/**
 * ephy_permission_info_free:
 * @info: a #EphyPermissionInfo
 *
 * Frees @info.
 **/
void
ephy_permission_info_free (EphyPermissionInfo *info)
{
	if (info != NULL)
	{
		g_free (info->host);
		g_free (info);
	}
}

/* EphyPermissionManager */

static void ephy_permission_manager_base_init (gpointer g_class);

GType
ephy_permission_manager_get_type (void)
{
       static GType type = 0;

	if (type == 0)
	{
		static const GTypeInfo our_info =
		{
		sizeof (EphyPermissionManagerIFace),
		ephy_permission_manager_base_init,
		NULL,
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EphyPermissionManager",
					       &our_info,
					       (GTypeFlags) 0);
	}

	return type;
}

static void
ephy_permission_manager_base_init (gpointer g_class)
{
	static gboolean initialised = FALSE;

	if (initialised == FALSE)
	{
	/**
	 * EphyPermissionManager::permission-added
	 * @manager: the #EphyPermissionManager
	 * @info: a #EphyPermissionInfo
	 *
	 * The permission-added signal is emitted when a permission entry has
	 * been added.
	 */
	g_signal_new ("permission-added",
		      EPHY_TYPE_PERMISSION_MANAGER,
		      G_SIGNAL_RUN_FIRST,
		      G_STRUCT_OFFSET (EphyPermissionManagerIFace, added),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__BOXED,
		      G_TYPE_NONE,
		      1,
		      EPHY_TYPE_PERMISSION_INFO);

	/**
	 * EphyPermissionManager::permission-changed
	 * @manager: the #EphyPermissionManager
	 * @info: a #EphyPermissionInfo
	 *
	 * The permission-changed signal is emitted when a permission entry has
	 * been changed.
	 */
	g_signal_new ("permission-changed",
		      EPHY_TYPE_PERMISSION_MANAGER,
		      G_SIGNAL_RUN_FIRST,
		      G_STRUCT_OFFSET (EphyPermissionManagerIFace, changed),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__BOXED,
		      G_TYPE_NONE,
		      1,
		      EPHY_TYPE_PERMISSION_INFO);

	/**
	 * EphyPermissionManager::permission-deleted
	 * @manager: the #EphyPermissionManager
	 * @info: a #EphyPermissionInfo
	 *
	 * The permission-deleted signal is emitted when a permission entry has
	 * been deleted.
	 */
	g_signal_new ("permission-deleted",
		      EPHY_TYPE_PERMISSION_MANAGER,
		      G_SIGNAL_RUN_FIRST,
		      G_STRUCT_OFFSET (EphyPermissionManagerIFace, deleted),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__BOXED,
		      G_TYPE_NONE,
		      1,
		      EPHY_TYPE_PERMISSION_INFO);

	/**
	 * EphyPermissionManager::permissions-cleared
	 * @manager: the #EphyPermissionManager
	 *
	 * The permissions-cleared signal is emitted when the permissions
	 * database has been cleared.
	 */
	g_signal_new ("permissions-cleared",
		      EPHY_TYPE_PERMISSION_MANAGER,
		      G_SIGNAL_RUN_FIRST,
		      G_STRUCT_OFFSET (EphyPermissionManagerIFace, cleared),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0);

	initialised = TRUE;
	}
}

/**
 * ephy_permission_manager_add:
 * @manager: the #EphyPermissionManager
 * @host: a host name
 * @type: a #EphyPermissionType
 * @allow: the permission itself
 * 
 * Adds the permission @allow of type @type for host @host to the permissions
 * database.
 **/
void
ephy_permission_manager_add (EphyPermissionManager *manager,
			     const char *host,
			     EphyPermissionType type,
			     gboolean allow)
{
	EphyPermissionManagerIFace *iface = EPHY_PERMISSION_MANAGER_GET_IFACE (manager);
	iface->add (manager, host, type, allow);
}

/**
 * ephy_permission_manager_remove:
 * @manager: the #EphyPermissionManager
 * @host: a host name
 * @type: a #EphyPermissionType
 * 
 * Removes the permission of type @type for host @host from the permissions
 * database.
 **/
void
ephy_permission_manager_remove (EphyPermissionManager *manager,
				const char *host,
				EphyPermissionType type)
{
	EphyPermissionManagerIFace *iface = EPHY_PERMISSION_MANAGER_GET_IFACE (manager);
	iface->remove (manager, host, type);
}

/**
 * ephy_permission_manager_clear:
 * @manager: the #EphyPermissionManager
 * 
 * Clears the permissions database.
 **/
void
ephy_permission_manager_clear (EphyPermissionManager *manager)
{
	EphyPermissionManagerIFace *iface = EPHY_PERMISSION_MANAGER_GET_IFACE (manager);
	iface->clear (manager);
}

/**
 * ephy_permission_manager_test:
 * @manager: the #EphyPermissionManager
 * @host: a host name
 * @type: a #EphyPermissionType
 * 
 * Tests whether the host @host is allowed to do the action specified by @type.
 * 
 * Return value: TRUE if allowed
 **/
gboolean
ephy_permission_manager_test (EphyPermissionManager *manager,
			      const char *host,
			      EphyPermissionType type)
{
	EphyPermissionManagerIFace *iface = EPHY_PERMISSION_MANAGER_GET_IFACE (manager);
	return iface->test (manager, host, type);
}

/**
 * ephy_permission_manager_list:
 * @manager: the #EphyPermissionManager
 * @type: a #EphyPermissionType
 * 
 * Lists all permission entries of type @type in the permissions database.
 * 
 * Return value: the list of permission entries
 **/
GList *
ephy_permission_manager_list (EphyPermissionManager *manager,
			      EphyPermissionType type)
{
	EphyPermissionManagerIFace *iface = EPHY_PERMISSION_MANAGER_GET_IFACE (manager);
	return iface->list (manager, type);
}
