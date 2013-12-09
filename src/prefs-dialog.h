/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003-2004 Christian Persch
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

#ifndef PREFS_DIALOG_H
#define PREFS_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PREFS_DIALOG		(prefs_dialog_get_type ())
#define EPHY_PREFS_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_PREFS_DIALOG, PrefsDialog))
#define EPHY_PREFS_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_PREFS_DIALOG, PrefsDialogClass))
#define EPHY_IS_PREFS_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_PREFS_DIALOG))
#define EPHY_IS_PREFS_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_PREFS_DIALOG))
#define EPHY_PREFS_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_PREFS_DIALOG, PrefsDialogClass))

typedef struct PrefsDialog		PrefsDialog;
typedef struct PrefsDialogClass		PrefsDialogClass;
typedef struct PrefsDialogPrivate	PrefsDialogPrivate;

struct PrefsDialog
{
        GtkDialog parent;

	/*< private >*/
        PrefsDialogPrivate *priv;
};

struct PrefsDialogClass
{
        GtkDialogClass parent_class;
};

GType         prefs_dialog_get_type           (void);

G_END_DECLS

#endif
