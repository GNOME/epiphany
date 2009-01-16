/*
 *  Copyright © 2000, 2001, 2002, 2003 Marco Pesenti Gritti
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

#ifndef EPHY_ENCODING_DIALOG_H
#define EPHY_ENCODING_DIALOG_H

#include "ephy-embed-dialog.h"
#include "ephy-window.h"

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_ENCODING_DIALOG		(ephy_encoding_dialog_get_type ())
#define EPHY_ENCODING_DIALOG(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_ENCODING_DIALOG, EphyEncodingDialog))
#define EPHY_ENCODING_DIALOG_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_ENCODING_DIALOG, EphyEncodingDialogClass))
#define EPHY_IS_ENCODING_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_ENCODING_DIALOG))
#define EPHY_IS_ENCODING_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_ENCODING_DIALOG))
#define EPHY_ENCODING_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_ENCODING_DIALOG, EphyEncodingDialogClass))

typedef struct _EphyEncodingDialog		EphyEncodingDialog;
typedef struct _EphyEncodingDialogClass		EphyEncodingDialogClass;
typedef struct _EphyEncodingDialogPrivate	EphyEncodingDialogPrivate;

struct _EphyEncodingDialog
{
	EphyEmbedDialog parent;

	/*< private >*/
	EphyEncodingDialogPrivate *priv;
};

struct _EphyEncodingDialogClass
{
	EphyEmbedDialogClass parent_class;
};

GType			 ephy_encoding_dialog_get_type	(void);

EphyEncodingDialog	*ephy_encoding_dialog_new	(EphyWindow *window);

G_END_DECLS

#endif
