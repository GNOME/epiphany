/*
 *  Copyright © 2006 Christian Persch
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

#ifndef EPHY_PRINT_UTILS_H
#define EPHY_PRINT_UTILS_H

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkPageSetup	 *ephy_print_utils_page_setup_new_from_file	(const gchar       *file_name,
							 	 GError           **error);

GtkPageSetup	 *ephy_print_utils_page_setup_new_from_key_file	(GKeyFile          *key_file,
								 GError           **error);

gboolean	  ephy_print_utils_page_setup_to_file		(GtkPageSetup      *setup,
								 const char        *file_name,
								 GError           **error);

gboolean	  ephy_print_utils_page_setup_to_key_file	(GtkPageSetup      *setup,
								 GKeyFile          *key_file,
								 GError           **error);

G_END_DECLS

#endif
