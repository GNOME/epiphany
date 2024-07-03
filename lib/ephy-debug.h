/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS
#define LOG(msg, args...) G_STMT_START { \
    g_autofree char *ephy_log_file_basename = g_path_get_basename (__FILE__); \
    g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "[ %s ] " msg, ephy_log_file_basename, ## args); \
  } G_STMT_END

#define START_PROFILER(name)	ephy_profiler_start (name, __FILE__);
#define STOP_PROFILER(name)   ephy_profiler_stop (name);

typedef struct
{
	GTimer *timer;
	char *name;
	char *module;
} EphyProfiler;

void		ephy_debug_init		(void);

void		ephy_profiler_start	(const char *name,
					 const char *module);

void		ephy_profiler_stop	(const char *name);

void		ephy_debug_set_fatal_criticals ();

G_END_DECLS
