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

#ifndef PRINT_DIALOG_H
#define PRINT_DIALOG_H

#include "ephy-embed-dialog.h"
#include "ephy-embed.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct PrintDialog PrintDialog;
typedef struct PrintDialogClass PrintDialogClass;

#define PRINT_DIALOG_TYPE             (print_dialog_get_type ())
#define PRINT_DIALOG(obj)             (GTK_CHECK_CAST ((obj), PRINT_DIALOG_TYPE, PrintDialog))
#define PRINT_DIALOG_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), PRINT_DIALOG, PrintDialogClass))
#define IS_PRINT_DIALOG(obj)          (GTK_CHECK_TYPE ((obj), PRINT_DIALOG_TYPE))
#define IS_PRINT_DIALOG_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), PRINT_DIALOG))

typedef struct PrintDialogPrivate PrintDialogPrivate;

struct PrintDialog
{
        EphyEmbedDialog parent;
        PrintDialogPrivate *priv;
        //FIXME: These should be gobject properties
        gboolean only_collect_info;
        EmbedPrintInfo **ret_info;
};

struct PrintDialogClass
{
        EphyEmbedDialogClass parent_class;

	void (* preview)    (PrintDialog *dialog);
};

GType		print_dialog_get_type		(void);

EphyDialog     *print_dialog_new		(EphyEmbed *embed,
						 EmbedPrintInfo **ret_info);

EphyDialog     *print_dialog_new_with_parent	(GtkWidget *window,
						 EphyEmbed *embed,
						 EmbedPrintInfo **ret_info);

gboolean	print_dialog_is_preview		(PrintDialog *dialog);

void		print_free_info			(EmbedPrintInfo *info);

G_END_DECLS

#endif

