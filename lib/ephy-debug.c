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
 */

#include "ephy-debug.h"

#include <string.h>


static const char *ephy_debug_modules;

static void
log_func (const gchar *log_domain,
	  GLogLevelFlags log_level,
	  const gchar *message,
	  gpointer user_data)
{
	if (!ephy_debug_modules) return;

	if ((strcmp (ephy_debug_modules, "all") == 0) ||
	    strstr (message, ephy_debug_modules) != NULL)
	{
		g_print ("%s\n", message);
	}
}

void
ephy_debug_init (void)
{
	ephy_debug_modules = g_getenv ("EPHY_DEBUG_MODULES");

	g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, log_func, NULL);
}
