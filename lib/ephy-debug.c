/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-debug.h"

#include <string.h>

#ifndef DISABLE_PROFILING

static GHashTable *ephy_profilers_hash = NULL;
static const char *ephy_profile_modules = NULL;

#endif

#ifndef DISABLE_LOGGING

static const char *ephy_log_modules;

static void
log_module (const gchar *log_domain,
	    GLogLevelFlags log_level,
	    const gchar *message,
	    gpointer user_data)
{
	gboolean should_log = FALSE;

	if (!ephy_log_modules) return;

	if (strcmp (ephy_log_modules, "all") != 0)
	{
		char **modules;
		int i;

		modules = g_strsplit (ephy_log_modules, ":", 100);

		for (i = 0; modules[i] != NULL; i++)
		{
			if (strstr (message, modules [i]) != NULL)
			{
				should_log = TRUE;
				break;
			}
		}

		g_strfreev (modules);
	}
	else
	{
		should_log = TRUE;
	}

	if (should_log)
	{
		g_print ("%s\n", message);
	}
}

#endif

void
ephy_debug_init (void)
{
#ifndef DISABLE_LOGGING
	ephy_log_modules = g_getenv ("EPHY_LOG_MODULES");

	g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, log_module, NULL);
#endif
#ifndef DISABLE_PROFILING
	ephy_profile_modules = g_getenv ("EPHY_PROFILE_MODULES");
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
	char **modules;
	int i;
	gboolean res = FALSE;

	if (!ephy_profile_modules) return FALSE;
	if (strcmp (ephy_profile_modules, "all") == 0) return TRUE;

	modules = g_strsplit (ephy_profile_modules, ":", 100);

	for (i = 0; modules[i] != NULL; i++)
	{
		if (strcmp (module, modules [i]) == 0)
		{
			res = TRUE;
			break;
		}
	}

	g_strfreev (modules);

	return res;
}

static void
ephy_profiler_dump (EphyProfiler *profiler)
{
	long elapsed;
	double seconds;

	g_return_if_fail (profiler != NULL);

	seconds = g_timer_elapsed (profiler->timer, &elapsed);

	g_print ("[ %s ] %s %ld ms (%f s) elapsed\n",
		 profiler->module, profiler->name,
		 elapsed / (G_USEC_PER_SEC / 1000), seconds);
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

	if (!ephy_should_profile (module)) return;

	profiler = ephy_profiler_new (name, module);

	g_hash_table_insert (ephy_profilers_hash, g_strdup (name), profiler);
}

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
