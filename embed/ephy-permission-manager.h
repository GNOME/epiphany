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

#ifndef EPHY_PERMISSION_MANAGER_H
#define EPHY_PERMISSION_MANAGER_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PERMISSION_MANAGER		(ephy_permission_manager_get_type ())
#define EPHY_PERMISSION_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_PERMISSION_MANAGER, EphyPermissionManager))
#define EPHY_PERMISSION_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_PERMISSION_MANAGER, EphyPermissionManagerClass))
#define EPHY_IS_PERMISSION_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_PERMISSION_MANAGER))
#define EPHY_IS_PERMISSION_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_PERMISSION_MANAGER))
#define EPHY_PERMISSION_MANAGER_GET_IFACE(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_PERMISSION_MANAGER, EphyPermissionManagerIFace))

#define EPHY_TYPE_PERMISSION_INFO		(ephy_permission_info_get_type ())

typedef struct EphyPermissionManager		EphyPermissionManager;
typedef struct EphyPermissionManagerIFace	EphyPermissionManagerIFace;

typedef enum
{
	EPT_COOKIE,
	EPT_IMAGE,
	EPT_POPUP
} EphyPermissionType;

typedef enum
{
	EPHY_PERMISSION_ALLOWED,
	EPHY_PERMISSION_DENIED,
	EPHY_PERMISSION_DEFAULT
} EphyPermission;

typedef struct
{
	char *host;
	EphyPermissionType type;
	EphyPermission permission;
} EphyPermissionInfo;

struct EphyPermissionManagerIFace
{
	GTypeInterface base_iface;

	/* Signals */
	void	(* added)	(EphyPermissionManager *manager,
				 EphyPermissionInfo *info);
	void	(* changed)	(EphyPermissionManager *manager,
				 EphyPermissionInfo *info);
	void	(* deleted)	(EphyPermissionManager *manager,
				 EphyPermissionInfo *info);
	void	(* cleared)	(EphyPermissionManager *manager);

	/* Methods */
	void		(* add)		(EphyPermissionManager *manager,
					 const char *host,
					 EphyPermissionType type,
					 EphyPermission permission);
	void		(* remove)	(EphyPermissionManager *manager,
					 const char *host,
					 EphyPermissionType type);
	void		(* clear)	(EphyPermissionManager *manager);
	EphyPermission	(* test)	(EphyPermissionManager *manager,
					 const char *host,
					 EphyPermissionType type);
	GList *		(* list)	(EphyPermissionManager *manager,
					 EphyPermissionType type);
};

/* EphyPermissionInfo */

GType			ephy_permission_info_get_type	(void);

EphyPermissionInfo     *ephy_permission_info_new	(const char *host,
							 EphyPermissionType type,
							 EphyPermission permission);

EphyPermissionInfo     *ephy_permission_info_copy	(const EphyPermissionInfo *info);

void			ephy_permission_info_free	(EphyPermissionInfo *info);

/* EphyPermissionManager */

GType 		ephy_permission_manager_get_type	(void);

void		ephy_permission_manager_add		(EphyPermissionManager *manager,
							 const char *host,
							 EphyPermissionType type,
							 EphyPermission permission);

void		ephy_permission_manager_remove		(EphyPermissionManager *manager,
							 const char *host,
							 EphyPermissionType type);

void		ephy_permission_manager_clear		(EphyPermissionManager *manager);

EphyPermission	ephy_permission_manager_test		(EphyPermissionManager *manager,
							 const char *host,
							 EphyPermissionType type);

GList *		ephy_permission_manager_list		(EphyPermissionManager *manager,
							 EphyPermissionType type);

G_END_DECLS

#endif
