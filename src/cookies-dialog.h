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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef COOKIES_DIALOG_H
#define COOKIES_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_COOKIES_DIALOG		(cookies_dialog_get_type ())
#define EPHY_COOKIES_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_COOKIES_DIALOG, CookiesDialog))
#define EPHY_COOKIES_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_COOKIES_DIALOG, CookiesDialogClass))
#define EPHY_IS_COOKIES_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_COOKIES_DIALOG))
#define EPHY_IS_COOKIES_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_COOKIES_DIALOG))
#define EPHY_COOKIES_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_COOKIES_DIALOG, CookiesDialogClass))

typedef struct CookiesDialog		CookiesDialog;
typedef struct CookiesDialogClass		CookiesDialogClass;
typedef struct CookiesDialogPrivate	CookiesDialogPrivate;

struct CookiesDialog
{
        GtkDialog parent;

	/*< private >*/
        CookiesDialogPrivate *priv;
};

struct CookiesDialogClass
{
        GtkDialogClass parent_class;
};

GType         cookies_dialog_get_type  (void);

G_END_DECLS

#endif
