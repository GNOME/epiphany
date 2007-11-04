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

#ifndef EPHY_PASSWORD_MANAGER_H
#define EPHY_PASSWORD_MANAGER_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PASSWORD_MANAGER		(ephy_password_manager_get_type ())
#define EPHY_PASSWORD_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_PASSWORD_MANAGER, EphyPasswordManager))
#define EPHY_PASSWORD_MANAGER_IFACE(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_PASSWORD_MANAGER, EphyPasswordManagerIface))
#define EPHY_IS_PASSWORD_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_PASSWORD_MANAGER))
#define EPHY_IS_PASSWORD_MANAGER_IFACE(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_PASSWORD_MANAGER))
#define EPHY_PASSWORD_MANAGER_GET_IFACE(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_PASSWORD_MANAGER, EphyPasswordManagerIface))

#define EPHY_TYPE_PASSWORD_INFO			(ephy_password_info_get_type ())

typedef struct _EphyPasswordManager		EphyPasswordManager;
typedef struct _EphyPasswordManagerIface	EphyPasswordManagerIface;

typedef struct
{
	char *host;
	char *username;
	char *password;
} EphyPasswordInfo;

struct _EphyPasswordManagerIface
{
	GTypeInterface base_iface;

	/* Signals */
	void	(* changed)	(EphyPasswordManager *manager);

	/* Methods */
	void	(* add)		(EphyPasswordManager *manager,
				 EphyPasswordInfo *info);
	void	(* remove)	(EphyPasswordManager *manager,
				 EphyPasswordInfo *info);
	void	(* remove_all)	(EphyPasswordManager *manager);
	GList *	(* list)	(EphyPasswordManager *manager);
};

/* EphyPasswordInfo */

GType			ephy_password_info_get_type	(void);

EphyPasswordInfo       *ephy_password_info_new		(const char *host,
							 const char *username,
							 const char *password);

EphyPasswordInfo       *ephy_password_info_copy		(const EphyPasswordInfo *info);

void			ephy_password_info_free		(EphyPasswordInfo *info);

/* EphyPasswordManager */

GType 		ephy_password_manager_get_type	(void);

void		ephy_password_manager_add_password	(EphyPasswordManager *manager,
							 EphyPasswordInfo *info);

void		ephy_password_manager_remove_password	(EphyPasswordManager *manager,
							 EphyPasswordInfo *info);

void		ephy_password_manager_remove_all_passwords (EphyPasswordManager *manager);

GList *		ephy_password_manager_list_passwords	(EphyPasswordManager *manager);

G_END_DECLS

#endif
