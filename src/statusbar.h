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

#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <gtk/gtkstatusbar.h>

G_BEGIN_DECLS

typedef struct Statusbar Statusbar;
typedef struct StatusbarClass StatusbarClass;

#define STATUSBAR_TYPE             (statusbar_get_type ())
#define STATUSBAR(obj)             (GTK_CHECK_CAST ((obj), STATUSBAR_TYPE, Statusbar))
#define STATUSBAR_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), STATUSBAR, StatusbarClass))
#define IS_STATUSBAR(obj)          (GTK_CHECK_TYPE ((obj), STATUSBAR_TYPE))
#define IS_STATUSBAR_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), STATUSBAR))

typedef struct StatusbarPrivate StatusbarPrivate;

struct Statusbar
{
        GtkStatusbar parent;
        StatusbarPrivate *priv;
};

struct StatusbarClass
{
        GtkStatusbarClass parent_class;
};

GType         statusbar_get_type             (void);

GtkWidget    *statusbar_new                  (void);

void          statusbar_set_security_state   (Statusbar *s,
					      gboolean state,
				              const char *tooltip);

void          statusbar_set_progress         (Statusbar *s,
					      int progress);

void          statusbar_set_message          (Statusbar *s,
					      const gchar *message);

G_END_DECLS

#endif
