/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2017 Igalia S.L.
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

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
        EPHY_FILE_FILTER_ALL_SUPPORTED,
        EPHY_FILE_FILTER_WEBPAGES,
        EPHY_FILE_FILTER_IMAGES,
        EPHY_FILE_FILTER_ALL,
        EPHY_FILE_FILTER_NONE,
        EPHY_FILE_FILTER_LAST = EPHY_FILE_FILTER_NONE
} EphyFileFilterDefault;

GtkFileChooser *ephy_create_file_chooser (const char *title,
                                          GtkWidget *parent,
                                          GtkFileChooserAction action,
                                          EphyFileFilterDefault default_filter);

G_END_DECLS
