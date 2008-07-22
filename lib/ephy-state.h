/*
 *  Copyright © 2001 Matthew Mueller
 *  Copyright © 2002 Jorn Baayen <jorn@nl.linux.org>
 *  Copyright © 2003 Marco Pesenti Gritti <mpeseng@tin.it>
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

#ifndef EPHY_STATE_H
#define EPHY_STATE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
	EPHY_STATE_WINDOW_SAVE_NONE = 0,
	EPHY_STATE_WINDOW_SAVE_SIZE = 1 << 0,
	EPHY_STATE_WINDOW_SAVE_POSITION = 1 << 1
} EphyStateWindowFlags;

void  ephy_state_add_window	(GtkWidget *window,
				 const char *name,
				 int default_width,
				 int default_heigth,
				 gboolean maximize,
				 EphyStateWindowFlags flags);

void  ephy_state_add_paned	(GtkWidget *paned,
				 const char *name,
				 int default_width);

void  ephy_state_add_expander	(GtkWidget *widget,
				 const char *name,
				 gboolean default_state);

void  ephy_state_save		(void);

G_END_DECLS

#endif /* EPHY_STATE_H */
