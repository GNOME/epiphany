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

#ifndef SESSION_H
#define SESSION_H

#define SESSION_CRASHED "type:session_crashed"
#define SESSION_GNOME "type:session_gnome"

#include "ephy-window.h"

G_BEGIN_DECLS

#include <glib-object.h>
#include <glib.h>

typedef struct Session Session;
typedef struct SessionClass SessionClass;

#define SESSION_TYPE             (session_get_type ())
#define SESSION(obj)             (GTK_CHECK_CAST ((obj), SESSION_TYPE, Session))
#define SESSION_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), SESSION, SessionClass))
#define IS_SESSION(obj)          (GTK_CHECK_TYPE ((obj), SESSION_TYPE))
#define IS_SESSION_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), SESSION))

typedef struct SessionPrivate SessionPrivate;

struct Session
{
        GObject parent;
        SessionPrivate *priv;
};

struct SessionClass
{
        GObjectClass parent_class;

	void ( *new_window)         (Session *session,
				     EphyWindow *window);
	void ( *close_window)       (Session *session,
				     EphyWindow *window);
};

GType         session_get_type		(void);

Session      *session_new		(void);

void	      session_close		(Session *session);

void	      session_load		(Session *session,
					 const char *filename);

void	      session_save		(Session *session,
					 const char *filename);

gboolean      session_autoresume	(Session *session);

const GList  *session_get_windows	(Session *session);

void          session_add_window	(Session *session,
					 EphyWindow *window);

void          session_remove_window     (Session *session,
					 EphyWindow *window);

EphyWindow   *session_get_active_window (Session *session);

G_END_DECLS

#endif
