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

#include "ephy-window.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SESSION		(session_get_type ())
#define EPHY_SESSION(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_SESSION, Session))
#define EPHY_SESSION_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_SESSION, SessionClass))
#define EPHY_IS_SESSION(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_SESSION))
#define EPHY_IS_SESSION_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_SESSION))
#define EPHY_SESSION_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_SESSION, SessionClass))

#define SESSION_CRASHED "type:session_crashed"
#define SESSION_GNOME "type:session_gnome"

typedef struct Session Session;
typedef struct SessionClass SessionClass;
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
