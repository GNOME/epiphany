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

#ifndef LANGUAGE_EDITOR_H
#define LANGUAGE_EDITOR_H

#include "general-prefs.h"

#include <gtk/gtkwidget.h>
#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct LanguageEditor LanguageEditor;
typedef struct LanguageEditorClass LanguageEditorClass;

#define LANGUAGE_EDITOR_TYPE             (language_editor_get_type ())
#define LANGUAGE_EDITOR(obj)             (GTK_CHECK_CAST ((obj), LANGUAGE_EDITOR_TYPE, LanguageEditor))
#define LANGUAGE_EDITOR_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), LANGUAGE_EDITOR, LanguageEditorClass))
#define IS_LANGUAGE_EDITOR(obj)          (GTK_CHECK_TYPE ((obj), LANGUAGE_EDITOR_TYPE))
#define IS_LANGUAGE_EDITOR_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), LANGUAGE_EDITOR))

typedef struct LanguageEditorPrivate LanguageEditorPrivate;

struct LanguageEditor
{
        EphyDialog parent;
        LanguageEditorPrivate *priv;
};

struct LanguageEditorClass
{
        EphyDialogClass parent_class;

	void (* changed) (GSList *languages);
};

GType           language_editor_get_type	(void);

LanguageEditor *language_editor_new		(GtkWidget *parent);

void		language_editor_set_menu	(LanguageEditor *editor,
						 GtkWidget *menu);

void		language_editor_add		(LanguageEditor *editor,
						 const char *language,
						 int id);

G_END_DECLS

#endif
