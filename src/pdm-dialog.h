/*
 *  Copyright © 2002 Jorn Baayen
 *  Copyright © 2003 Marco Pesenti Gritti
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

typedef enum
{
	CLEAR_ALL_NONE = 0,
	CLEAR_ALL_CACHE = 1 << 0,
	CLEAR_ALL_PASSWORDS = 1 << 1,
	CLEAR_ALL_HISTORY = 1 << 2,
	CLEAR_ALL_COOKIES = 1 << 4
} PdmClearAllDialogFlags;

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

void	pdm_dialog_show_clear_all_dialog (EphyDialog *dialog,
					  GtkWidget *parent,
					  PdmClearAllDialogFlags flags);

G_END_DECLS

#endif
