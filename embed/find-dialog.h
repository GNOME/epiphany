/*
 *  Copyright (C) 2002 Jorn Baayen
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

#ifndef FIND_DIALOG_H
#define FIND_DIALOG_H

#include "ephy-embed-dialog.h"

G_BEGIN_DECLS

#define EPHY_TYPE_FIND_DIALOG		(find_dialog_get_type ())
#define EPHY_FIND_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_FIND_DIALOG, FindDialog))
#define EPHY_FIND_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_FIND_DIALOG, FindDialogClass))
#define EPHY_IS_FIND_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_FIND_DIALOG))
#define EPHY_IS_FIND_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_FIND_DIALOG))
#define EPHY_FIND_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_FIND_DIALOG, FindDialogClass))

typedef struct FindDialogClass FindDialogClass;
typedef struct FindDialog FindDialog;
typedef struct FindDialogPrivate FindDialogPrivate;

struct FindDialog
{
	EphyEmbedDialog parent;

	/*< private >*/
	FindDialogPrivate *priv;
};

struct FindDialogClass
{
	EphyEmbedDialogClass parent_class;
};

GType			find_dialog_get_type		 (void);

EphyDialog*		find_dialog_new			 (EphyEmbed *embed);

EphyDialog *		find_dialog_new_with_parent	 (GtkWidget *window,
							  EphyEmbed *embed);

G_END_DECLS

#endif

