/*
 *  arch-tag: Header file for code cut and pasted from elsewhere
 *
 *  Copyright © 2002 Jorn Baayen
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
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 */

/* Following code is copied from Rhythmbox rb-cut-and-paste-code.h */

#include <time.h>

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_TIME_HELPERS_H
#define EPHY_TIME_HELPERS_H

G_BEGIN_DECLS

char      *eel_strdup_strftime			(const char *format,
						 struct tm *time_pieces);

char *     ephy_time_helpers_utf_friendly_time	(time_t date);

G_END_DECLS

#endif /* EPHY_TIME_HELPERS_H */
