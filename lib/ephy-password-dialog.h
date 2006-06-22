/*
 *  Copyright (C) 2005 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef EPHY_PASSWORD_DIALOG_H
#define EPHY_PASSWORD_DIALOG_H

#include <gtk/gtkmessagedialog.h>
#include <libgnomeui/gnome-password-dialog.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PASSWORD_DIALOG         (ephy_password_dialog_get_type ())
#define EPHY_PASSWORD_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_PASSWORD_DIALOG, EphyPasswordDialog))
#define EPHY_PASSWORD_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_PASSWORD_DIALOG, EphyPasswordDialogClass))
#define EPHY_IS_PASSWORD_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_PASSWORD_DIALOG))
#define EPHY_IS_PASSWORD_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_PASSWORD_DIALOG))
#define EPHY_PASSWORD_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_PASSWORD_DIALOG, EphyPasswordDialogClass))

typedef struct _EphyPasswordDialog		EphyPasswordDialog;
typedef struct _EphyPasswordDialogPrivate	EphyPasswordDialogPrivate;
typedef struct _EphyPasswordDialogClass		EphyPasswordDialogClass;

struct _EphyPasswordDialog
{
	GtkMessageDialog parent_instance;

	/*< private >*/
	EphyPasswordDialogPrivate *priv;
};

struct _EphyPasswordDialogClass
{
	GtkMessageDialogClass parent_class;
};

typedef enum
{
	EPHY_PASSWORD_DIALOG_FLAGS_SHOW_USERNAME	= 1 << 0,
	EPHY_PASSWORD_DIALOG_FLAGS_EDIT_USERNAME	= 1 << 1,
	EPHY_PASSWORD_DIALOG_FLAGS_SHOW_DOMAIN		= 1 << 2,
	EPHY_PASSWORD_DIALOG_FLAGS_EDIT_DOMAIN		= 1 << 3,
	EPHY_PASSWORD_DIALOG_FLAGS_SHOW_PASSWORD	= 1 << 4,
	EPHY_PASSWORD_DIALOG_FLAGS_SHOW_NEW_PASSWORD	= 1 << 5,
	EPHY_PASSWORD_DIALOG_FLAGS_SHOW_QUALITY_METER	= 1 << 6,
	EPHY_PASSWORD_DIALOG_FLAGS_SHOW_REMEMBER	= 1 << 7,
} EphyPasswordDialogFlags;

#define EPHY_PASSWORD_DIALOG_FLAGS_DEFAULT 0

GType		ephy_password_dialog_get_type	(void);

GtkWidget      *ephy_password_dialog_new	(GtkWidget *parent,
						 const char *title,
						 EphyPasswordDialogFlags flags);

void		ephy_password_dialog_set_remember	(EphyPasswordDialog *dialog,
							 GnomePasswordDialogRemember remember);

GnomePasswordDialogRemember ephy_password_dialog_get_remember (EphyPasswordDialog *dialog);

void		ephy_password_dialog_set_label	(EphyPasswordDialog *dialog,
						 const char *markup);

GtkWidget      *ephy_password_dialog_get_username_entry	(EphyPasswordDialog *dialog);

const char     *ephy_password_dialog_get_username	(EphyPasswordDialog *dialog);

void		ephy_password_dialog_set_username	(EphyPasswordDialog *dialog,
							 const char *text);

const char     *ephy_password_dialog_get_domain		(EphyPasswordDialog *dialog);

void		ephy_password_dialog_set_domain		(EphyPasswordDialog *dialog,
							 const char *text);

const char     *ephy_password_dialog_get_password	(EphyPasswordDialog *dialog);

void		ephy_password_dialog_set_password	(EphyPasswordDialog *dialog,
							 const char *text);

const char     *ephy_password_dialog_get_new_password	(EphyPasswordDialog *dialog);

G_END_DECLS

#endif /* !EPHY_PASSWORD_DIALOG_H */
