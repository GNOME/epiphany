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

#ifndef PREFS_DIALOG_H
#define PREFS_DIALOG_H

#include "ephy-dialog.h"

#include <glib-object.h>
#include <glib.h>

typedef struct PrefsDialog PrefsDialog;
typedef struct PrefsDialogClass PrefsDialogClass;

#define PREFS_DIALOG_TYPE             (prefs_dialog_get_type ())
#define PREFS_DIALOG(obj)             (GTK_CHECK_CAST ((obj), PREFS_DIALOG_TYPE, PrefsDialog))
#define PREFS_DIALOG_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), PREFS_DIALOG, PrefsDialogClass))
#define IS_PREFS_DIALOG(obj)          (GTK_CHECK_TYPE ((obj), PREFS_DIALOG_TYPE))
#define IS_PREFS_DIALOG_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), PREFS_DIALOG))

typedef struct PrefsDialogPrivate PrefsDialogPrivate;

struct PrefsDialog
{
        EphyDialog parent;
        PrefsDialogPrivate *priv;
};

struct PrefsDialogClass
{
        EphyDialogClass parent_class;
};

GType         prefs_dialog_get_type           (void);

EphyDialog   *prefs_dialog_new		      (GtkWidget *parent);

#endif
