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

#ifndef UI_PREFS_H
#define UI_PREFS_H

#include "ephy-dialog.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct UIPrefs UIPrefs;
typedef struct UIPrefsClass UIPrefsClass;

#define UI_PREFS_TYPE             (ui_prefs_get_type ())
#define UI_PREFS(obj)             (GTK_CHECK_CAST ((obj), UI_PREFS_TYPE, UIPrefs))
#define UI_PREFS_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), UI_PREFS, UIPrefsClass))
#define IS_UI_PREFS(obj)          (GTK_CHECK_TYPE ((obj), UI_PREFS_TYPE))
#define IS_UI_PREFS_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), UI_PREFS))

typedef struct UIPrefsPrivate UIPrefsPrivate;

struct UIPrefs
{
        EphyDialog parent;
        UIPrefsPrivate *priv;
};

struct UIPrefsClass
{
        EphyDialogClass parent_class;
};

GType         ui_prefs_get_type			(void);

EphyDialog   *ui_prefs_new			(void);

G_END_DECLS

#endif

