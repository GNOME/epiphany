/*
 *  Copyright © 2003 Marco Pesenti Gritti
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
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_DEBUG_H
#define EPHY_DEBUG_H

#include <glib.h>

G_BEGIN_DECLS

#ifdef NDEBUG
#define DISABLE_LOGGING
#define DISABLE_PROFILING
#endif

#if defined(G_HAVE_GNUC_VARARGS)

#ifdef DISABLE_LOGGING
#define LOG(msg, args...) G_STMT_START { } G_STMT_END
#else
#define LOG(msg, args...)			\
g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,		\
       "[ %s ] " msg,				\
       __FILE__ , ## args)
#endif

#elif defined(G_HAVE_ISO_VARARGS)

#define LOG(...) G_STMT_START { } G_STMT_END

#else /* no varargs macros */

static void LOG(const char *format, ...) {}

#endif

#ifdef DISABLE_PROFILING
#define START_PROFILER(name)
#define STOP_PROFILER(name)
#else
#define START_PROFILER(name)	\
ephy_profiler_start (name, __FILE__);
#define STOP_PROFILER(name)	\
ephy_profiler_stop (name);
#endif

typedef struct
{
	GTimer *timer;
	char *name;
	char *module;
} EphyProfiler;

void		ephy_debug_init		(void);

#ifndef DISABLE_PROFILING

void		ephy_profiler_start	(const char *name,
					 const char *module);

void		ephy_profiler_stop	(const char *name);

#endif

G_END_DECLS

#endif
