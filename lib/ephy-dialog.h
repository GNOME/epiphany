/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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

#ifndef EPHY_DIALOG_H
#define EPHY_DIALOG_H

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_DIALOG		(ephy_dialog_get_type ())
#define EPHY_DIALOG(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_DIALOG, EphyDialog))
#define EPHY_DIALOG_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_DIALOG, EphyDialogClass))
#define EPHY_IS_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_DIALOG))
#define EPHY_IS_DIALOG_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_DIALOG))
#define EPHY_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_DIALOG, EphyDialogClass))

typedef struct _EphyDialogClass		EphyDialogClass;
typedef struct _EphyDialog		EphyDialog;
typedef struct _EphyDialogPrivate	EphyDialogPrivate;
typedef struct _EphyDialogProperty	EphyDialogProperty;

struct _EphyDialogClass
{
	GObjectClass parent_class;

	/* Signals */

	void	(* changed)	(EphyDialog *dialog,
				 const GValue *value);

	/* Methods */
	void	(* construct)	(EphyDialog *dialog,
				 const char *resource,
				 const char *name,
				 const char *domain);
	void	(* show)	(EphyDialog *dialog);
};

struct _EphyDialog
{
	GObject parent;

	/*< private >*/
	EphyDialogPrivate *priv;
};

GType		ephy_dialog_get_type		(void);

EphyDialog     *ephy_dialog_new			(void);

EphyDialog     *ephy_dialog_new_with_parent	(GtkWidget *parent_window);

void		ephy_dialog_construct		(EphyDialog *dialog,
						 const char *resource,
						 const char *name,
						 const char *domain);

void		ephy_dialog_set_size_group	(EphyDialog *dialog,
						 const char *first_id,
						 ...);

int		ephy_dialog_run			(EphyDialog *dialog);

void		ephy_dialog_show		(EphyDialog *dialog);

void		ephy_dialog_hide		(EphyDialog *dialog);

void		ephy_dialog_set_parent		(EphyDialog *dialog,
						 GtkWidget *parent);

GtkWidget      *ephy_dialog_get_parent		(EphyDialog *dialog);

GtkWidget      *ephy_dialog_get_control		(EphyDialog *dialog,
						 const char *property_id);

void		ephy_dialog_get_controls	(EphyDialog *dialog,
						 const char *first_id,
						 ...);

G_END_DECLS

#endif
