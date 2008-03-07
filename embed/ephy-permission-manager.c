/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 Christian Persch
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#include "config.h"

#include "ephy-permission-manager.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-marshal.h"
#include "ephy-debug.h"

GType
ephy_permission_info_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
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
 * @type: an #EphyPermissionType
 * @allowed: whether @host should be allowed to do what @type specifies
 *
 * Return value: the new #EphyPermissionInfo
 **/
EphyPermissionInfo *
ephy_permission_info_new (const char *host,
			  const char *type,
			  EphyPermission permission)
{
	EphyPermissionInfo *info = g_slice_new0 (EphyPermissionInfo);

	info->host = g_strdup (host);
	info->qtype = g_quark_from_string (type);
	info->permission = permission;

	return info;
}

/**
 * ephy_permission_info_copy:
 * @info: an #EphyPermissionInfo
 *
 * Return value: a copy of @info
 **/
EphyPermissionInfo *
ephy_permission_info_copy (const EphyPermissionInfo *info)
{
	EphyPermissionInfo *copy = g_slice_new0 (EphyPermissionInfo);

	copy->host = g_strdup (info->host);
	copy->qtype = info->qtype;
	copy->permission = info->permission;

	return copy;
}

/**
 * ephy_permission_info_free:
 * @info: an #EphyPermissionInfo
 *
 * Frees @info.
 **/
void
ephy_permission_info_free (EphyPermissionInfo *info)
{
	if (info != NULL)
	{
		g_free (info->host);
		g_slice_free (EphyPermissionInfo, info);
	}
}

/* EphyPermissionManager */

static void ephy_permission_manager_base_init (gpointer g_class);

GType
ephy_permission_manager_get_type (void)
{
       static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
		sizeof (EphyPermissionManagerIface),
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
		      G_STRUCT_OFFSET (EphyPermissionManagerIface, added),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__BOXED,
		      G_TYPE_NONE,
		      1,
		      EPHY_TYPE_PERMISSION_INFO | G_SIGNAL_TYPE_STATIC_SCOPE);

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
		      G_STRUCT_OFFSET (EphyPermissionManagerIface, changed),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__BOXED,
		      G_TYPE_NONE,
		      1,
		      EPHY_TYPE_PERMISSION_INFO | G_SIGNAL_TYPE_STATIC_SCOPE);

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
		      G_STRUCT_OFFSET (EphyPermissionManagerIface, deleted),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__BOXED,
		      G_TYPE_NONE,
		      1,
		      EPHY_TYPE_PERMISSION_INFO | G_SIGNAL_TYPE_STATIC_SCOPE);

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
		      G_STRUCT_OFFSET (EphyPermissionManagerIface, cleared),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0);

	initialised = TRUE;
	}
}

/**
 * ephy_permission_manager_add_permission:
 * @manager: the #EphyPermissionManager
 * @host: a website URL
 * @type: a string to identify the type of the permission
 * @permission: either %EPHY_PERMISSION_ALLOWED or %EPHY_PERMISSION_DENIED
 * 
 * Adds the specified permission to the permissions database.
 **/
void
ephy_permission_manager_add_permission (EphyPermissionManager *manager,
					const char *host,
					const char *type,
					EphyPermission permission)
{
	EphyPermissionManagerIface *iface = EPHY_PERMISSION_MANAGER_GET_IFACE (manager);
	iface->add (manager, host, type, permission);
}

/**
 * ephy_permission_manager_remove_permission:
 * @manager: the #EphyPermissionManager
 * @host: a website URL
 * @type: a string to identify the type of the permission
 * 
 * Removes the specified permission from the permissions database. This implies
 * that the browser should use defaults when next visiting the specified
 * @host's web pages.
 **/
void
ephy_permission_manager_remove_permission (EphyPermissionManager *manager,
					   const char *host,
					   const char *type)
{
	EphyPermissionManagerIface *iface = EPHY_PERMISSION_MANAGER_GET_IFACE (manager);
	iface->remove (manager, host, type);
}

/**
 * ephy_permission_manager_clear_permission:
 * @manager: the #EphyPermissionManager
 * 
 * Clears the permissions database. This cannot be undone.
 **/
void
ephy_permission_manager_clear_permissions (EphyPermissionManager *manager)
{
	EphyPermissionManagerIface *iface = EPHY_PERMISSION_MANAGER_GET_IFACE (manager);
	iface->clear (manager);
}

/**
 * ephy_permission_manager_test_permission:
 * @manager: the #EphyPermissionManager
 * @host: a website URL
 * @type: a string to identify the type of the permission
 * 
 * Retrieves an #EphyPermissionType from the permissions database. If there is
 * no entry for this @type and @host, it will return %EPHY_PERMISSION_DEFAULT.
 * In that case, the caller may need to determine the appropriate default
 * behavior.
 *
 * Return value: the permission of type #EphyPermission
 **/
EphyPermission
ephy_permission_manager_test_permission (EphyPermissionManager *manager,
					 const char *host,
					 const char *type)
{
	EphyPermissionManagerIface *iface = EPHY_PERMISSION_MANAGER_GET_IFACE (manager);
	return iface->test (manager, host, type);
}

/**
 * ephy_permission_manager_list_permission:
 * @manager: the #EphyPermissionManager
 * @type: a string to identify the type of the permission
 * 
 * Lists all permission entries of type @type in the permissions database, each
 * as its own #EphyPermissionInfo. These entries must be freed using
 * ephy_permission_info_free().
 * 
 * Return value: the list of permission database entries
 **/
GList *
ephy_permission_manager_list_permissions (EphyPermissionManager *manager,
					 const char *type)
{
	EphyPermissionManagerIface *iface = EPHY_PERMISSION_MANAGER_GET_IFACE (manager);
	return iface->list (manager, type);
}
