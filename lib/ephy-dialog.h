/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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
 */

#ifndef EPHY_DIALOG_H
#define EPHY_DIALOG_H

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

typedef struct EphyDialogClass EphyDialogClass;

#define EPHY_DIALOG_TYPE             (ephy_dialog_get_type ())
#define EPHY_DIALOG(obj)             (GTK_CHECK_CAST ((obj), EPHY_DIALOG_TYPE, EphyDialog))
#define EPHY_DIALOG_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), EPHY_DIALOG_TYPE, EphyDialogClass))
#define IS_EPHY_DIALOG(obj)          (GTK_CHECK_TYPE ((obj), EPHY_DIALOG_TYPE))
#define IS_EPHY_DIALOG_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), EPHY_DIALOG))
#define EPHY_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_DIALOG_TYPE, EphyDialogClass))

typedef struct EphyDialog EphyDialog;
typedef struct EphyDialogPrivate EphyDialogPrivate;

#define SY_BEGIN_GROUP -20
#define SY_END_GROUP -21
#define SY_END -22
#define SY_BEGIN_GROUP_INVERSE -23

struct EphyDialog
{
        GObject parent;
        EphyDialogPrivate *priv;
};

typedef enum
{
	PT_NORMAL,
	PT_AUTOAPPLY
} PropertyType;

typedef struct
{
	int id;
	const char *control_name;
	const char *state_pref;
	PropertyType type;
	int *sg;
} EphyDialogProperty;

struct EphyDialogClass
{
        GObjectClass parent_class;

	void        (* construct)       (EphyDialog *dialog,
					 const EphyDialogProperty *properties,
					 const char *file,
			                 const char *name);
	gint        (* run)		(EphyDialog *dialog);
	void        (* show)		(EphyDialog *dialog);
	GtkWidget * (* get_control)     (EphyDialog *dialog,
				         int property_id);
	void        (* get_value)       (EphyDialog *dialog,
					 int property_id,
					 GValue *value);
};

GType         ephy_dialog_get_type		(void);

EphyDialog   *ephy_dialog_new			(void);

EphyDialog   *ephy_dialog_new_with_parent	(GtkWidget *parent_window);

void	      ephy_dialog_construct		(EphyDialog *dialog,
						 const EphyDialogProperty *properties,
						 const char *file,
						 const char *name);

void	      ephy_dialog_add_enum		(EphyDialog *dialog,
						 int id,
						 guint n_items,
						 const char **items);

void	      ephy_dialog_set_size_group	(EphyDialog *dialog,
						 int *controls_id,
						 guint n_controls);

gint          ephy_dialog_run			(EphyDialog *dialog);

void          ephy_dialog_show			(EphyDialog *dialog);

void          ephy_dialog_show_embedded		(EphyDialog *dialog,
						 GtkWidget *container);

void	      ephy_dialog_remove_embedded	(EphyDialog *dialog);

void          ephy_dialog_set_modal		(EphyDialog *dialog,
						 gboolean is_modal);

GtkWidget    *ephy_dialog_get_control		(EphyDialog *dialog,
						 int property_id);

void          ephy_dialog_get_value		(EphyDialog *dialog,
						 int property_id,
						 GValue *value);

G_END_DECLS

#endif

