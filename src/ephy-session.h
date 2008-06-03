/*
 *  Copyright © 2002 Jorn Baayen
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003 Christian Persch
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_SESSION_H
#define EPHY_SESSION_H

#include "ephy-extension.h"

#include "ephy-window.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SESSION		(ephy_session_get_type ())
#define EPHY_SESSION(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_SESSION, EphySession))
#define EPHY_SESSION_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_SESSION, EphySessionClass))
#define EPHY_IS_SESSION(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_SESSION))
#define EPHY_IS_SESSION_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_SESSION))
#define EPHY_SESSION_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_SESSION, EphySessionClass))

typedef struct _EphySession		EphySession;
typedef struct _EphySessionPrivate	EphySessionPrivate;
typedef struct _EphySessionClass	EphySessionClass;

typedef enum
{
	EPHY_SESSION_CMD_RESUME_SESSION,
	EPHY_SESSION_CMD_LOAD_SESSION,
	EPHY_SESSION_CMD_OPEN_BOOKMARKS_EDITOR,
	EPHY_SESSION_CMD_OPEN_URIS,
	EPHY_SESSION_CMD_MAYBE_OPEN_WINDOW,
	EPHY_SESSION_CMD_LAST
	
} EphySessionCommand;

struct _EphySession
{
        GObject parent;

	/*< private >*/
        EphySessionPrivate *priv;
};

struct _EphySessionClass
{
        GObjectClass parent_class;
};

GType		 ephy_session_get_type		(void);

EphyWindow	*ephy_session_get_active_window	(EphySession *session);

gboolean	 ephy_session_save		(EphySession *session,
						 const char *filename);

gboolean	 ephy_session_load		(EphySession *session,
						 const char *filename,
						 guint32 user_time);

void		 ephy_session_close		(EphySession *session);

GList		*ephy_session_get_windows	(EphySession *session);

void		 ephy_session_add_window	(EphySession *session,
						 GtkWindow *window);

void		 ephy_session_remove_window	(EphySession *session,
						 GtkWindow *window);

void		 ephy_session_queue_command	(EphySession *session,
						 EphySessionCommand op,
						 const char *arg,
						 char **args,
						 guint32 user_time,
						 gboolean priority);

G_END_DECLS

#endif
