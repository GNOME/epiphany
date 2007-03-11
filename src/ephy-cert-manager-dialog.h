/*
 *  Copyright © 2003 Robert Marcano
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

#ifndef EPHY_CERT_MANAGER_DIALOG_H
#define EPHY_CERT_MANAGER_DIALOG_H

#include <glib.h>
#include "ephy-dialog.h"

G_BEGIN_DECLS

#define EPHY_TYPE_CERTS_MANAGER_DIALOG		(certs_manager_dialog_get_type ())
#define EPHY_CERTS_MANAGER_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_CERTS_MANAGER_DIALOG, CertsManagerDialog))
#define EPHY_CERTS_MANAGER_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_CERTS_MANAGER_DIALOG, CertsManagerDialogClass))
#define EPHY_IS_CERTS_MANAGER_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_CERTS_MANAGER_DIALOG))
#define EPHY_IS_CERTS_MANAGER_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_CERTS_MANAGER_DIALOG))
#define EPHY_CERTS_MANAGER_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_CERTS_MANAGER_DIALOG, CertsManagerDialogClass))

typedef struct _CertsManagerDialogClass		CertsManagerDialogClass;
typedef struct _CertsManagerDialog		CertsManagerDialog;
typedef struct _CertsManagerDialogPrivate	CertsManagerDialogPrivate;

struct _CertsManagerDialogClass
{
	EphyDialogClass parent_class;
};

struct _CertsManagerDialog
{
	EphyDialog parent;

	/*< private >*/
	CertsManagerDialogPrivate *priv;
};

GType	    certs_manager_dialog_get_type	(void);

EphyDialog *certs_manager_dialog_new		(void);

G_END_DECLS

#endif
