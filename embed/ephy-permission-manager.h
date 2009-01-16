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
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_PERMISSION_MANAGER_H
#define EPHY_PERMISSION_MANAGER_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PERMISSION_MANAGER		(ephy_permission_manager_get_type ())
#define EPHY_PERMISSION_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_PERMISSION_MANAGER, EphyPermissionManager))
#define EPHY_PERMISSION_MANAGER_IFACE(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_PERMISSION_MANAGER, EphyPermissionManagerIface))
#define EPHY_IS_PERMISSION_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_PERMISSION_MANAGER))
#define EPHY_IS_PERMISSION_MANAGER_IFACE(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_PERMISSION_MANAGER))
#define EPHY_PERMISSION_MANAGER_GET_IFACE(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_PERMISSION_MANAGER, EphyPermissionManagerIface))

#define EPHY_TYPE_PERMISSION_INFO		(ephy_permission_info_get_type ())

#define EPT_COOKIE	"cookie"
#define EPT_IMAGE	"image"
#define EPT_POPUP	"popup"

typedef enum
{
	EPHY_PERMISSION_ALLOWED,
	EPHY_PERMISSION_DENIED,
	EPHY_PERMISSION_DEFAULT
} EphyPermission;

typedef struct _EphyPermissionInfo		EphyPermissionInfo;

typedef struct _EphyPermissionManager		EphyPermissionManager;
typedef struct _EphyPermissionManagerIface	EphyPermissionManagerIface;

struct _EphyPermissionInfo
{
	char *host;
	GQuark qtype;
	EphyPermission permission;
};

struct _EphyPermissionManagerIface
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
					 const char *type,
					 EphyPermission permission);
	void		(* remove)	(EphyPermissionManager *manager,
					 const char *host,
					 const char *type);
	void		(* clear)	(EphyPermissionManager *manager);
	EphyPermission	(* test)	(EphyPermissionManager *manager,
					 const char *host,
					 const char *type);
	GList *		(* list)	(EphyPermissionManager *manager,
					 const char *type);
};

/* EphyPermissionInfo */

GType			ephy_permission_get_type	(void);

GType			ephy_permission_info_get_type	(void);

EphyPermissionInfo     *ephy_permission_info_new	(const char *host,
							 const char *type,
							 EphyPermission permission);

EphyPermissionInfo     *ephy_permission_info_copy	(const EphyPermissionInfo *info);

void			ephy_permission_info_free	(EphyPermissionInfo *info);

/* EphyPermissionManager */

GType 		ephy_permission_manager_get_type	(void);

void		ephy_permission_manager_add_permission		(EphyPermissionManager *manager,
								 const char *host,
								 const char *type,
								 EphyPermission permission);

void		ephy_permission_manager_remove_permission	(EphyPermissionManager *manager,
								 const char *host,
								 const char *type);

void		ephy_permission_manager_clear_permissions	(EphyPermissionManager *manager);

EphyPermission	ephy_permission_manager_test_permission		(EphyPermissionManager *manager,
								 const char *host,
								 const char *type);

GList *		ephy_permission_manager_list_permissions	(EphyPermissionManager *manager,
								 const char *type);

G_END_DECLS

#endif
