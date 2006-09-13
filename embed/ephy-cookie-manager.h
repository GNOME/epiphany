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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef EPHY_COOKIE_MANAGER_H
#define EPHY_COOKIE_MANAGER_H

#include <glib-object.h>
#include <glib.h>
#include <time.h>

G_BEGIN_DECLS

#define EPHY_TYPE_COOKIE_MANAGER		(ephy_cookie_manager_get_type ())
#define EPHY_COOKIE_MANAGER(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_COOKIE_MANAGER, EphyCookieManager))
#define EPHY_COOKIE_MANAGER_IFACE(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_COOKIE_MANAGER, EphyCookieManagerIface))
#define EPHY_IS_COOKIE_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_COOKIE_MANAGER))
#define EPHY_IS_COOKIE_MANAGER_IFACE(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_COOKIE_MANAGER))
#define EPHY_COOKIE_MANAGER_GET_IFACE(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_COOKIE_MANAGER, EphyCookieManagerIface))

#define EPHY_TYPE_COOKIE			(ephy_cookie_get_type ())

typedef struct _EphyCookieManager	EphyCookieManager;
typedef struct _EphyCookieManagerIface	EphyCookieManagerIface;

typedef struct
{
	char *name;
	char *value;
	char *domain;
	char *path;
	time_t expires;
	glong real_expires;
        guint is_secure : 1;
        guint is_session : 1;
} EphyCookie;

struct _EphyCookieManagerIface
{
	GTypeInterface base_iface;

	/* Signals */
	void	(* added)	(EphyCookieManager *manager,
				 EphyCookie *cookie);
	void	(* changed)	(EphyCookieManager *manager,
				 EphyCookie *cookie);
	void	(* deleted)	(EphyCookieManager *manager,
				 EphyCookie *cookie);
	void	(* rejected)	(EphyCookieManager *manager,
				 const char *url);
	void	(* cleared)	(EphyCookieManager *manager);

	/* Methods */
	GList *	(* list)	(EphyCookieManager *manager);
	void	(* remove)	(EphyCookieManager *manager,
				 const EphyCookie *cookie);
	void	(* clear)	(EphyCookieManager *manager);
};

/* EphyCookie */

GType		ephy_cookie_get_type	(void);

EphyCookie     *ephy_cookie_new		(void);

EphyCookie     *ephy_cookie_copy	(const EphyCookie *cookie);

void		ephy_cookie_free	(EphyCookie *cookie);

/* EphyCookieManager */

GType 		ephy_cookie_manager_get_type		(void);

GList *		ephy_cookie_manager_list_cookies	(EphyCookieManager *manager);

void		ephy_cookie_manager_remove_cookie	(EphyCookieManager *manager,
							 const EphyCookie *cookie);

void		ephy_cookie_manager_clear		(EphyCookieManager *manager);

G_END_DECLS

#endif
