/*
 *  Copyright © 2013 Red Hat, Inc.
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CLEAR_DATA_DIALOG_H
#define CLEAR_DATA_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_CLEAR_DATA_DIALOG		(clear_data_dialog_get_type ())
#define EPHY_CLEAR_DATA_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_CLEAR_DATA_DIALOG, ClearDataDialog))
#define EPHY_CLEAR_DATA_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_CLEAR_DATA_DIALOG, ClearDataDialogClass))
#define EPHY_IS_CLEAR_DATA_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_CLEAR_DATA_DIALOG))
#define EPHY_IS_CLEAR_DATA_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_CLEAR_DATA_DIALOG))
#define EPHY_CLEAR_DATA_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_CLEAR_DATA_DIALOG, ClearDataDialogClass))

typedef struct ClearDataDialog		ClearDataDialog;
typedef struct ClearDataDialogClass		ClearDataDialogClass;
typedef struct ClearDataDialogPrivate	ClearDataDialogPrivate;

struct ClearDataDialog
{
        GtkDialog parent;

	/*< private >*/
        ClearDataDialogPrivate *priv;
};

struct ClearDataDialogClass
{
        GtkDialogClass parent_class;
};

GType         clear_data_dialog_get_type           (void);

G_END_DECLS

#endif
