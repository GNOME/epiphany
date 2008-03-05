/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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

#include "ephy-cookie-manager.h"

GType
ephy_cookie_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		type = g_boxed_type_register_static ("EphyCookie",
						     (GBoxedCopyFunc) ephy_cookie_copy,
						     (GBoxedFreeFunc) ephy_cookie_free);
	}

	return type;
}

/**
 * ephy_cookie_new:
 *
 * Return value: a new #EphyCookie.
 **/
EphyCookie *
ephy_cookie_new (void)
{
	return g_slice_new0 (EphyCookie);
}

/**
 * ephy_cookie_copy:
 * @cookie: a #EphyCookie
 *
 * Return value: a copy of @cookie.
 **/
EphyCookie *
ephy_cookie_copy (const EphyCookie *cookie)
{
	EphyCookie *copy = g_slice_new0 (EphyCookie);

	copy->name = g_strdup (cookie->name);
	copy->value = g_strdup (cookie->value);
	copy->domain = g_strdup (cookie->domain);
	copy->path = g_strdup (cookie->path);
	copy->expires = cookie->expires;
	copy->real_expires = cookie->real_expires;
	copy->is_secure = cookie->is_secure;
	copy->is_session = cookie->is_session;

	return copy;
}

/**
 * ephy_cookie_free:
 * @cookie: a #EphyCookie
 * 
 * Frees @cookie.
 **/
void
ephy_cookie_free (EphyCookie *cookie)
{
	if (cookie != NULL)
	{
		g_free (cookie->name);
		g_free (cookie->value);
		g_free (cookie->domain);
		g_free (cookie->path);

		g_slice_free (EphyCookie, cookie);
	}
}

/* EphyCookieManager */

static void ephy_cookie_manager_base_init (gpointer base_iface);

GType
ephy_cookie_manager_get_type (void)
{
       static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyCookieManagerIface),
			ephy_cookie_manager_base_init,
			NULL,
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EphyCookieManager",
					       &our_info,
					       (GTypeFlags) 0);
	}

	return type;
}

static void
ephy_cookie_manager_base_init (gpointer base_iface)
{
	static gboolean initialised = FALSE;

	if (initialised == FALSE)
	{
	/**
	 * EphyCookieManager::cookie-added
	 * @manager: the #EphyCookieManager
	 * @cookie: the added #EphyCookie
	 *
	 * The cookie-added signal is emitted when a cookie has been added.
	 **/
	g_signal_new ("cookie-added",
		      EPHY_TYPE_COOKIE_MANAGER,
		      G_SIGNAL_RUN_FIRST,
		      G_STRUCT_OFFSET (EphyCookieManagerIface, added),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__BOXED,
		      G_TYPE_NONE,
		      1,
		      EPHY_TYPE_COOKIE | G_SIGNAL_TYPE_STATIC_SCOPE);

	/**
	 * EphyCookieManager::cookie-changed
	 * @manager: the #EphyCookieManager
	 * @cookie: the changed #EphyCookie
	 *
	 * The cookie-changed signal is emitted when a cookie has been changed.
	 **/
	g_signal_new ("cookie-changed",
		      EPHY_TYPE_COOKIE_MANAGER,
		      G_SIGNAL_RUN_FIRST,
		      G_STRUCT_OFFSET (EphyCookieManagerIface, changed),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__BOXED,
		      G_TYPE_NONE,
		      1,
		      EPHY_TYPE_COOKIE | G_SIGNAL_TYPE_STATIC_SCOPE);

	/**
	 * EphyCookieManager::cookie-deleted
	 * @manager: the #EphyCookieManager
	 * @cookie: the deleted #EphyCookie
	 *
	 * The cookie-deleted signal is emitted when a cookie has been deleted.
	 **/
	g_signal_new ("cookie-deleted",
		      EPHY_TYPE_COOKIE_MANAGER,
		      G_SIGNAL_RUN_FIRST,
		      G_STRUCT_OFFSET (EphyCookieManagerIface, deleted),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__BOXED,
		      G_TYPE_NONE,
		      1,
		      EPHY_TYPE_COOKIE | G_SIGNAL_TYPE_STATIC_SCOPE);

	/**
	 * EphyCookieManager::cookie-rejected
	 * @manager: the #EphyCookieManager
	 * @address: the address of the page that wanted to set the cookie
	 *
	 * The cookie-rejected signal is emitted when a cookie has been rejected.
	 **/
	g_signal_new ("cookie-rejected",
		      EPHY_TYPE_COOKIE_MANAGER,
		      G_SIGNAL_RUN_FIRST,
		      G_STRUCT_OFFSET (EphyCookieManagerIface, rejected),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__STRING,
		      G_TYPE_NONE,
		      1,
		      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

	/**
	 * EphyCookieManager::cookies-cleared
	 * @manager: the #EphyCookieManager
	 *
	 * The cookies-cleared signal is emitted when the cookie database has
	 * been cleared.
	 **/
	g_signal_new ("cookies-cleared",
		      EPHY_TYPE_COOKIE_MANAGER,
		      G_SIGNAL_RUN_FIRST,
		      G_STRUCT_OFFSET (EphyCookieManagerIface, cleared),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__VOID,
		      G_TYPE_NONE,
		      0);

	initialised = TRUE;
	}
}

/**
 * ephy_cookie_manager_list_cookies:
 * @manager: the #EphyCookieManager
 * 
 * Lists all cookies in the cookies database.
 * 
 * Return value: the cookies list
 **/
GList *
ephy_cookie_manager_list_cookies (EphyCookieManager *manager)
{
	EphyCookieManagerIface *iface = EPHY_COOKIE_MANAGER_GET_IFACE (manager);
	return iface->list (manager);
}

/**
 * ephy_cookie_manager_remove_cookie:
 * @manager: the #EphyCookieManager
 * @cookie: a #EphyCookie
 * 
 * Removes @cookie from the cookies database. You must free @cookie yourself.
 **/
void
ephy_cookie_manager_remove_cookie (EphyCookieManager *manager,
				   const EphyCookie *cookie)
{
	EphyCookieManagerIface *iface = EPHY_COOKIE_MANAGER_GET_IFACE (manager);
	iface->remove (manager, cookie);
}

/**
 * ephy_cookie_manager_clear:
 * @manager: the #EphyCookieManager
 * 
 * Clears the cookies database.
 **/
void
ephy_cookie_manager_clear (EphyCookieManager *manager)
{
	EphyCookieManagerIface *iface = EPHY_COOKIE_MANAGER_GET_IFACE (manager);
	iface->clear (manager);
}
