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

#ifndef HISTORY_DIALOG_H
#define HISTORY_DIALOG_H

#include "ephy-embed-dialog.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct HistoryDialog HistoryDialog;
typedef struct HistoryDialogClass HistoryDialogClass;

#define HISTORY_DIALOG_TYPE             (history_dialog_get_type ())
#define HISTORY_DIALOG(obj)             (GTK_CHECK_CAST ((obj), HISTORY_DIALOG_TYPE, HistoryDialog))
#define HISTORY_DIALOG_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), HISTORY_DIALOG, HistoryDialogClass))
#define IS_HISTORY_DIALOG(obj)          (GTK_CHECK_TYPE ((obj), HISTORY_DIALOG_TYPE))
#define IS_HISTORY_DIALOG_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), HISTORY_DIALOG))

typedef struct HistoryDialogPrivate HistoryDialogPrivate;

struct HistoryDialog
{
        EphyEmbedDialog parent;
        HistoryDialogPrivate *priv;
};

struct HistoryDialogClass
{
        EphyEmbedDialogClass parent_class;
};

GType         history_dialog_get_type         (void);

EphyDialog   *history_dialog_new              (EphyEmbed *embed,
					       gboolean embedded);

EphyDialog   *history_dialog_new_with_parent  (GtkWidget *window,
					       EphyEmbed *embed,
					       gboolean embedded);

G_END_DECLS

#endif

