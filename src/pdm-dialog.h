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

#ifndef PDM_DIALOG_H
#define PDM_DIALOG_H

#include "ephy-dialog.h"
#include <glib.h>

G_BEGIN_DECLS

typedef struct PdmDialog PdmDialog;
typedef struct PdmDialogClass PdmDialogClass;

#define PDM_DIALOG_TYPE             (pdm_dialog_get_type ())
#define PDM_DIALOG(obj)             (GTK_CHECK_CAST ((obj), PDM_DIALOG_TYPE, PdmDialog))
#define PDM_DIALOG_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), PDM_DIALOG, PdmDialogClass))
#define IS_PDM_DIALOG(obj)          (GTK_CHECK_TYPE ((obj), PDM_DIALOG_TYPE))
#define IS_PDM_DIALOG_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), PDM_DIALOG))

typedef struct PdmDialogPrivate PdmDialogPrivate;

struct PdmDialog
{
        EphyDialog parent;
        PdmDialogPrivate *priv;
};

struct PdmDialogClass
{
        EphyDialogClass parent_class;
};

GType         pdm_dialog_get_type         (void);

EphyDialog   *pdm_dialog_new              (GtkWidget *window);

G_END_DECLS

#endif

