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

#include "config.h"

#include "ephy-password-manager.h"
#include "ephy-marshal.h"
#include "ephy-debug.h"

/* EphyPasswordInfo */

GType
ephy_password_info_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		type = g_boxed_type_register_static ("EphyPasswordInfo",
						     (GBoxedCopyFunc) ephy_password_info_copy,
						     (GBoxedFreeFunc) ephy_password_info_free);
	}

	return type;
}

/**
 * ephy_password_info_new:
 * @host: a host name
 * @username: a user name
 * @password: a password, or NULL
 *
 * Generates a new #EphyPasswordInfo.
 *
 * Return value: the new password info.
 **/
EphyPasswordInfo *
ephy_password_info_new (const char *host,
			const char *username,
			const char *password)
{
	EphyPasswordInfo *info = g_new0 (EphyPasswordInfo, 1);

	info->host = g_strdup (host);
	info->username = g_strdup (username);
	info->password = g_strdup (password);

	return info;
}

/**
 * ephy_password_info_copy:
 * @info: a #EphyPasswordInfo
 *
 * Return value: a copy of @info
 **/
EphyPasswordInfo *
ephy_password_info_copy (const EphyPasswordInfo *info)
{
	EphyPasswordInfo *copy = g_new0 (EphyPasswordInfo, 1);

	copy->host = g_strdup (info->host);
	copy->username = g_strdup (info->username);
	copy->password = g_strdup (info->password);

	return copy;
}

/**
 * ephy_password_info_free:
 * @info:
 *
 * Frees @info.
 **/
void
ephy_password_info_free (EphyPasswordInfo *info)
{
	if (info != NULL)
	{
		g_free (info->host);
		g_free (info->username);
		g_free (info->password);
		g_free (info);
	}
}

/* EphyPasswordManager */

static void ephy_password_manager_base_init (gpointer g_class);

GType
ephy_password_manager_get_type (void)
{
       static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyPasswordManagerIface),
			ephy_password_manager_base_init,
			NULL,
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EphyPasswordManager",
					       &our_info,
					       (GTypeFlags) 0);
	}

	return type;
}

static void
ephy_password_manager_base_init (gpointer g_class)
{
	static gboolean initialised = FALSE;

	if (initialised == FALSE)
	{
	/**
	 * EphyPasswordManager::changed
	 * @manager: the #EphyPermissionManager
	 *
	 * The ::passwords-changed signal is emitted when the list of passwords
	 * has changed.
	 */
	g_signal_new ("passwords-changed",
		      EPHY_TYPE_PASSWORD_MANAGER,
		      G_SIGNAL_RUN_FIRST,
		      G_STRUCT_OFFSET (EphyPasswordManagerIface, changed),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0);

	initialised = TRUE;
	}
}

/**
 * ephy_password_manager_add_password:
 * @manager: the #EphyPasswordManager
 * @info: a #EphyPasswordInfo
 * 
 * Adds the password entry @info to the the passwords database.
 **/
void
ephy_password_manager_add_password (EphyPasswordManager *manager,
			            EphyPasswordInfo *info)
{
	EphyPasswordManagerIface *iface = EPHY_PASSWORD_MANAGER_GET_IFACE (manager);
	iface->add (manager, info);
}

/**
 * ephy_password_manager_remove_password:
 * @manager: the #EphyPasswordManager
 * @info: a #EphyPasswordInfo
 * 
 * Removes the password entry @info from the passwords database.
 **/
void
ephy_password_manager_remove_password (EphyPasswordManager *manager,
			               EphyPasswordInfo *info)
{
	EphyPasswordManagerIface *iface = EPHY_PASSWORD_MANAGER_GET_IFACE (manager);
	iface->remove (manager, info);
}

/**
 * ephy_password_manager_list_passwords:
 * @manager: the #EphyPasswordManager
 * 
 * Lists all password entries in the passwords database.
 * 
 * Return value: the list of password entries
 **/
GList *
ephy_password_manager_list_passwords(EphyPasswordManager *manager)
{
	EphyPasswordManagerIface *iface = EPHY_PASSWORD_MANAGER_GET_IFACE (manager);
	return iface->list (manager);
}
