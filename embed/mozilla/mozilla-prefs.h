/*
 *  Copyright (C) 2000-2002 Marco Pesenti Gritti
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

#ifndef MOZILLA_PREFS_H
#define MOZILLA_PREFS_H

#include "glib/gtypes.h"

gboolean  mozilla_prefs_save        (void);

gboolean  mozilla_prefs_set_string  (const char *preference_name, 
				     const char *new_value);

gboolean  mozilla_prefs_set_boolean (const char *preference_name,
                                     gboolean new_boolean_value);

gboolean  mozilla_prefs_set_int     (const char *preference_name, 
				     int new_int_value);

gboolean  mozilla_prefs_get_boolean (const char *preference_name,
                                     gboolean default_value);

int       mozilla_prefs_get_int     (const char *preference_name);

gchar    *mozilla_prefs_get_string  (const char *preference_name);

gboolean  mozilla_prefs_remove      (const char *preference_name);

#endif
