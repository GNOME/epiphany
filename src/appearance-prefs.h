/*
 *  Copyright (C) 2002 Marco Pesenti Gritti
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

#ifndef APPEARANCE_PREFS_H
#define APPEARANCE_PREFS_H

#include "ephy-dialog.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct AppearancePrefs AppearancePrefs;
typedef struct AppearancePrefsClass AppearancePrefsClass;

#define appearance_PREFS_TYPE             (appearance_prefs_get_type ())
#define appearance_PREFS(obj)             (GTK_CHECK_CAST ((obj), appearance_PREFS_TYPE, AppearancePrefs))
#define appearance_PREFS_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), appearance_PREFS, AppearancePrefsClass))
#define IS_appearance_PREFS(obj)          (GTK_CHECK_TYPE ((obj), appearance_PREFS_TYPE))
#define IS_appearance_PREFS_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), appearance_PREFS))

typedef struct AppearancePrefsPrivate AppearancePrefsPrivate;

struct AppearancePrefs
{
        EphyDialog parent;
        AppearancePrefsPrivate *priv;
};

struct AppearancePrefsClass
{
        EphyDialogClass parent_class;
};

GType         appearance_prefs_get_type			(void);

EphyDialog   *appearance_prefs_new			(void);

G_END_DECLS

#endif

