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

#include "config.h"

#include "ephy-debug.h"

#include <string.h>
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

/**
 * SECTION:ephy-debug
 * @short_description: Epiphany debugging and profiling facilities
 *
 * Epiphany includes powerful profiling and debugging facilities to log and
 * analyze modules. Refer to doc/debugging.txt for more information.
 */

static const char *ephy_debug_break = NULL;

#ifndef DISABLE_PROFILING
static GHashTable *ephy_profilers_hash = NULL;
static char **ephy_profile_modules;
static gboolean ephy_profile_all_modules;
#endif /* !DISABLE_PROFILING */

#ifndef NDEBUG

static char **
build_modules (const char *name,
	       gboolean* is_all)
{
	const char *env;

	*is_all = FALSE;

	env = g_getenv (name);
	if (env == NULL) return NULL;

	if (strcmp (env, "all") == 0)
	{
		*is_all = TRUE;
		return NULL;
	}

	return g_strsplit (g_getenv (name), ":", -1);
}

#endif

#ifndef DISABLE_LOGGING

static char **ephy_log_modules;
static gboolean ephy_log_all_modules;

static void
log_module (const gchar *log_domain,
	    GLogLevelFlags log_level,
	    const char *message,
	    gpointer user_data)
{
	gboolean should_log = ephy_log_all_modules;

	if (!ephy_log_all_modules && !ephy_log_modules) return;

	if (ephy_log_modules != NULL)
	{
		guint i;

		for (i = 0; ephy_log_modules[i] != NULL; i++)
		{
			if (strstr (message, ephy_log_modules [i]) != NULL)
			{
				should_log = TRUE;
				break;
			}
		}
	}

	if (should_log)
	{
		g_print ("%s\n", message);
	}
}

#endif /* !DISABLE_LOGGING */

#define MAX_DEPTH 200

static void 
trap_handler (const char *log_domain,
	      GLogLevelFlags log_level,
	      const char *message,
	      gpointer user_data)
{
	g_log_default_handler (log_domain, log_level, message, user_data);

	if (ephy_debug_break != NULL &&
	    (log_level & (G_LOG_LEVEL_WARNING |
			  G_LOG_LEVEL_ERROR |
			  G_LOG_LEVEL_CRITICAL |
			  G_LOG_FLAG_FATAL)))
	{
		if (strcmp (ephy_debug_break, "suspend") == 0)
		{
			/* the suspend case is first because we wanna send the signal before 
			 * other threads have had a chance to get too far from the state that
			 * caused this assertion (in case they happen to have been involved).
			 */
			g_print ("Suspending program; attach with the debugger.\n");

			raise (SIGSTOP);
		}
		else if (strcmp (ephy_debug_break, "stack") == 0)
		{
#ifdef HAVE_EXECINFO_H
			void *array[MAX_DEPTH];
			size_t size;
			
			size = backtrace (array, MAX_DEPTH);
			backtrace_symbols_fd (array, size, 2);
#else
			g_on_error_stack_trace (g_get_prgname ());
#endif /* HAVE_EXECINFO_H */
		}
		else if (strcmp (ephy_debug_break, "trap") == 0)
		{
			/* FIXME: disable the handler for a moment so we 
			 * don't crash if we don't actually run under gdb
			 */
			G_BREAKPOINT ();
		}
		else if (strcmp (ephy_debug_break, "warn") == 0)
		{
			/* default behaviour only */
		}
		else if (ephy_debug_break[0] != '\0')
		{
			g_print ("Unrecognised value of EPHY_DEBUG_BREAK env var: %s!\n",
				 ephy_debug_break);
		}
	}
}

/**
 * ephy_debug_init:
 *
 * Starts the debugging facility, see doc/debugging.txt in Epiphany's source for
 * more information. It also starts module logging and profiling if the
 * appropiate variables are set: EPHY_LOG_MODULES and EPHY_PROFILE_MODULES.
 **/
void
ephy_debug_init (void)
{
#ifndef DISABLE_LOGGING
	ephy_log_modules = build_modules ("EPHY_LOG_MODULES", &ephy_log_all_modules);

	g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, log_module, NULL);

#endif

	ephy_debug_break = g_getenv ("EPHY_DEBUG_BREAK");
	g_log_set_default_handler (trap_handler, NULL);

#ifndef DISABLE_PROFILING
	ephy_profile_modules = build_modules ("EPHY_PROFILE_MODULES", &ephy_profile_all_modules);
#endif
}

#ifndef DISABLE_PROFILING

static EphyProfiler *
ephy_profiler_new (const char *name, const char *module)
{
	EphyProfiler *profiler;

	profiler = g_new0 (EphyProfiler, 1);
	profiler->timer = g_timer_new ();
	profiler->name  = g_strdup (name);
	profiler->module  = g_strdup (module);

	g_timer_start (profiler->timer);

	return profiler;
}

static gboolean
ephy_should_profile (const char *module)
{
	char *slash;
	gboolean result = FALSE;
	guint i;

	slash = strrchr (module, '/');

	/* Happens on builddir != srcdir builds */
	if (slash != NULL) module = slash + 1;

	for (i = 0; ephy_profile_modules[i] != NULL; i++)
	{
		if (strcmp (ephy_profile_modules[i], module) == 0)
		{
			result = TRUE;
			break;
		}
	}

	return result;
}

static void
ephy_profiler_dump (EphyProfiler *profiler)
{
	double seconds;

	g_return_if_fail (profiler != NULL);

	seconds = g_timer_elapsed (profiler->timer, NULL);

	g_print ("[ %s ] %s %f s elapsed\n",
		 profiler->module, profiler->name,
		 seconds);
}

static void
ephy_profiler_free (EphyProfiler *profiler)
{
	g_return_if_fail (profiler != NULL);

	g_timer_destroy (profiler->timer);
	g_free (profiler->name);
	g_free (profiler->module);
	g_free (profiler);
}

/**
 * ephy_profiler_start:
 * @name: name of this new profiler
 * @module: Epiphany module to profile
 *
 * Starts a new profiler on @module naming it @name.
 **/
void
ephy_profiler_start (const char *name, const char *module)
{
	EphyProfiler *profiler;

	if (ephy_profilers_hash == NULL)
	{
		ephy_profilers_hash =
			g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, NULL);
	}

	if (!ephy_profile_all_modules &&
	    (ephy_profile_modules == NULL || !ephy_should_profile (module))) return;

	profiler = ephy_profiler_new (name, module);

	g_hash_table_insert (ephy_profilers_hash, g_strdup (name), profiler);
}

/**
 * ephy_profiler_stop:
 * @name: name of the profiler to stop
 *
 * Stops the profiler named @name.
 **/
void
ephy_profiler_stop (const char *name)
{
	EphyProfiler *profiler;

	profiler = g_hash_table_lookup (ephy_profilers_hash, name);
	if (profiler == NULL) return;
	g_hash_table_remove (ephy_profilers_hash, name);

	ephy_profiler_dump (profiler);
	ephy_profiler_free (profiler);
}

#endif
