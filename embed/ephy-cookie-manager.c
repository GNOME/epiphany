/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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

#include "ephy-cookie-manager.h"

GType
ephy_cookie_get_type (void)
{
	static GType type = 0;

	if (type == 0)
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
	return g_new0 (EphyCookie, 1);
}

/**
 * ephy_cookie_copy:
 * @cookie: a #EphyCookie
 *
 * Return value: a copy of @cookie.
 **/
EphyCookie *
ephy_cookie_copy (EphyCookie *cookie)
{
	EphyCookie *copy = g_new0 (EphyCookie, 1);

	copy->name = g_strdup (cookie->name);
	copy->value = g_strdup (cookie->value);
	copy->domain = g_strdup (cookie->domain);
	copy->path = g_strdup (cookie->path);
	copy->expires = cookie->expires;
	copy->real_expires = cookie->real_expires;
	copy->is_secure = cookie->is_secure;
	copy->is_session = cookie->is_session;
	copy->p3p_state = cookie->p3p_state;
	copy->p3p_policy = cookie->p3p_policy;

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

		g_free (cookie);
	}
}

/* EphyCookieManager */

static void ephy_cookie_manager_base_init (gpointer base_iface);

GType
ephy_cookie_manager_get_type (void)
{
       static GType type = 0;

	if (type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyCookieManagerIFace),
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
	 * EphyCookieManager::added
	 * @manager: the #EphyCookieManager
	 * @cookie: the added #EphyCookie
	 *
	 * The added signal is emitted when a cookie has been added.
	 **/
	g_signal_new ("added",
		      EPHY_TYPE_COOKIE_MANAGER,
		      G_SIGNAL_RUN_FIRST,
		      G_STRUCT_OFFSET (EphyCookieManagerIFace, added),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__POINTER,
		      G_TYPE_NONE,
		      1,
		      EPHY_TYPE_COOKIE);

	/**
	 * EphyCookieManager::changed
	 * @manager: the #EphyCookieManager
	 * @cookie: the changed #EphyCookie
	 *
	 * The changed signal is emitted when a cookie has been changed.
	 **/
	g_signal_new ("changed",
		      EPHY_TYPE_COOKIE_MANAGER,
		      G_SIGNAL_RUN_FIRST,
		      G_STRUCT_OFFSET (EphyCookieManagerIFace, changed),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__BOXED,
		      G_TYPE_NONE,
		      1,
		      EPHY_TYPE_COOKIE);

	/**
	 * EphyCookieManager::deleted
	 * @manager: the #EphyCookieManager
	 * @cookie: the deleted #EphyCookie
	 *
	 * The deleted signal is emitted when a cookie has been deleted.
	 **/
	g_signal_new ("deleted",
		      EPHY_TYPE_COOKIE_MANAGER,
		      G_SIGNAL_RUN_FIRST,
		      G_STRUCT_OFFSET (EphyCookieManagerIFace, deleted),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__BOXED,
		      G_TYPE_NONE,
		      1,
		      EPHY_TYPE_COOKIE);

	/**
	 * EphyCookieManager::rejected
	 * @manager: the #EphyCookieManager
	 * @address: the address of the page that wanted to set the cookie
	 *
	 * The rejected signal is emitted when a cookie has been rejected.
	 **/
	g_signal_new ("rejected",
		      EPHY_TYPE_COOKIE_MANAGER,
		      G_SIGNAL_RUN_FIRST,
		      G_STRUCT_OFFSET (EphyCookieManagerIFace, rejected),
		      NULL, NULL,
		      g_cclosure_marshal_VOID__STRING,
		      G_TYPE_NONE,
		      1,
		      G_TYPE_STRING);

	/**
	 * EphyCookieManager::cleared
	 * @manager: the #EphyCookieManager
	 *
	 * The cleared signal is emitted when the cookie database has been
	 * cleared.
	 **/
	g_signal_new ("cleared",
		      EPHY_TYPE_COOKIE_MANAGER,
		      G_SIGNAL_RUN_FIRST,
		      G_STRUCT_OFFSET (EphyCookieManagerIFace, cleared),
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
	EphyCookieManagerIFace *iface = EPHY_COOKIE_MANAGER_GET_CLASS (manager);
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
				   EphyCookie *cookie)
{
	EphyCookieManagerIFace *iface = EPHY_COOKIE_MANAGER_GET_CLASS (manager);
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
	EphyCookieManagerIFace *iface = EPHY_COOKIE_MANAGER_GET_CLASS (manager);
	iface->clear (manager);
}
