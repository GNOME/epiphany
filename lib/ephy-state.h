/*
 *  Copyright (C) 2001 Matthew Mueller
 *            (C) 2002 Jorn Baayen <jorn@nl.linux.org>
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
 *  
 *  $Id$
 */

#ifndef EPHY_STATE_H
#define EPHY_STATE_H

G_BEGIN_DECLS

void ephy_state_save_window	(GtkWidget *window,
				 const gchar *name);

void ephy_state_load_window	(GtkWidget *window,
				 const gchar *name,
				 int default_width,
				 int default_heigth,
				 gboolean position);

void ephy_state_save_pane_pos	(GtkWidget *pane,
				 const gchar *name);

void ephy_state_load_pane_pos	(GtkWidget *pane,
				 const gchar *name);

G_END_DECLS

#endif /* EPHY_STATE_H */
