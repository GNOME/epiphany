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

#ifndef GENERAL_PREFS_H
#define GENERAL_PREFS_H

#include "ephy-embed-dialog.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct GeneralPrefs GeneralPrefs;
typedef struct GeneralPrefsClass GeneralPrefsClass;

#define GENERAL_PREFS_TYPE             (general_prefs_get_type ())
#define GENERAL_PREFS(obj)             (GTK_CHECK_CAST ((obj), GENERAL_PREFS_TYPE, GeneralPrefs))
#define GENERAL_PREFS_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), GENERAL_PREFS, GeneralPrefsClass))
#define IS_GENERAL_PREFS(obj)          (GTK_CHECK_TYPE ((obj), GENERAL_PREFS_TYPE))
#define IS_GENERAL_PREFS_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), GENERAL_PREFS))

typedef struct GeneralPrefsPrivate GeneralPrefsPrivate;

struct GeneralPrefs
{
        EphyDialog parent;
        GeneralPrefsPrivate *priv;
};

struct GeneralPrefsClass
{
        EphyDialogClass parent_class;
};

GType         general_prefs_get_type		(void);

EphyDialog   *general_prefs_new			(void);

G_END_DECLS

#endif

