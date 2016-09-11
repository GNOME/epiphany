/*
 *  Copyright © 2003, 2004 Christian Persch
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_FILE_CHOOSER (ephy_file_chooser_get_type ())

G_DECLARE_FINAL_TYPE (EphyFileChooser, ephy_file_chooser, EPHY, FILE_CHOOSER, GtkFileChooserDialog)

typedef enum
{
	EPHY_FILE_FILTER_ALL_SUPPORTED,
	EPHY_FILE_FILTER_WEBPAGES,
	EPHY_FILE_FILTER_IMAGES,
	EPHY_FILE_FILTER_ALL,
	EPHY_FILE_FILTER_NONE,
	EPHY_FILE_FILTER_LAST = EPHY_FILE_FILTER_NONE
} EphyFileFilterDefault;

EphyFileChooser	*ephy_file_chooser_new			(const char *title,
							 GtkWidget *parent,
							 GtkFileChooserAction action,
							 EphyFileFilterDefault default_filter);

GtkFileFilter	*ephy_file_chooser_add_pattern_filter	(EphyFileChooser *dialog,
							 const char *title,
							 const char *first_pattern,
							 ...);

GtkFileFilter	*ephy_file_chooser_add_mime_filter	(EphyFileChooser *dialog,
							 const char *title,
							 const char *first_mimetype,
							 ...);

G_END_DECLS
