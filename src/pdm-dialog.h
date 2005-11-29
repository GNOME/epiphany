/*
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003 Marco Pesenti Gritti
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

#ifndef PDM_DIALOG_H
#define PDM_DIALOG_H

#include "ephy-dialog.h"
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PDM_DIALOG		(pdm_dialog_get_type ())
#define EPHY_PDM_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_PDM_DIALOG, PdmDialog))
#define EPHY_PDM_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_PDM_DIALOG, PdmDialogClass))
#define EPHY_IS_PDM_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_PDM_DIALOG))
#define EPHY_IS_PDM_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_PDM_DIALOG))
#define EPHY_PDM_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_PDM_DIALOG, PdmDialogClass))

typedef struct PdmDialog	PdmDialog;
typedef struct PdmDialogClass	PdmDialogClass;
typedef struct PdmDialogPrivate	PdmDialogPrivate;

struct PdmDialog
{
        EphyDialog parent;

	/*< private >*/
        PdmDialogPrivate *priv;
};

struct PdmDialogClass
{
        EphyDialogClass parent_class;
};

GType	pdm_dialog_get_type	(void);

void	pdm_dialog_open		(PdmDialog *dialog,
				 const char *host);

G_END_DECLS

#endif
