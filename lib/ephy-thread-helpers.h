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
 *
 *  $Id$
 */

#include <glib/gthread.h>

#ifndef EPHY_THREAD_HELPERS_H
#define EPHY_THREAD_HELPERS_H

G_BEGIN_DECLS

void     ephy_thread_helpers_init           (void);

gboolean ephy_thread_helpers_in_main_thread (void);

G_END_DECLS

#endif /* EPHY_THREAD_HELPERS_H */
