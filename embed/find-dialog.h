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
 */

#ifndef FIND_DIALOG_H
#define FIND_DIALOG_H

#include "ephy-embed-dialog.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct FindDialog FindDialog;
typedef struct FindDialogClass FindDialogClass;

#define FIND_DIALOG_TYPE             (find_dialog_get_type ())
#define FIND_DIALOG(obj)             (GTK_CHECK_CAST ((obj), FIND_DIALOG_TYPE, FindDialog))
#define FIND_DIALOG_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), FIND_DIALOG, FindDialogClass))
#define IS_FIND_DIALOG(obj)          (GTK_CHECK_TYPE ((obj), FIND_DIALOG_TYPE))
#define IS_FIND_DIALOG_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), FIND_DIALOG))

typedef struct FindDialogPrivate FindDialogPrivate;

struct FindDialog
{
        EphyEmbedDialog parent;
        FindDialogPrivate *priv;
};

struct FindDialogClass
{
        EphyEmbedDialogClass parent_class;

	void (* search)    (FindDialog *dialog);
};

GType           find_dialog_get_type           (void);

EphyDialog   *find_dialog_new                  (EphyEmbed *embed);

EphyDialog	*find_dialog_new_with_parent   (GtkWidget *window,
						EphyEmbed *embed);


gboolean	find_dialog_can_go_next	       (FindDialog *dialog);

gboolean	find_dialog_can_go_prev	       (FindDialog *dialog);

void		find_dialog_go_next	       (FindDialog *dialog,
						gboolean interactive);

void		find_dialog_go_prev	       (FindDialog *dialog,
						gboolean interactive);

G_END_DECLS

#endif

