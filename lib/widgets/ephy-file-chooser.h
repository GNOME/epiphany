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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_FILE_CHOOSER_H
#define EPHY_FILE_CHOOSER_H

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_FILE_CHOOSER		(ephy_file_chooser_get_type ())
#define EPHY_FILE_CHOOSER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_FILE_CHOOSER, EphyFileChooser))
#define EPHY_FILE_CHOOSER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), EPHY_TYPE_FILE_CHOOSER, EphyFileChooserClass))
#define EPHY_IS_FILE_CHOOSER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_FILE_CHOOSER))
#define EPHY_IS_FILE_CHOOSER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_FILE_CHOOSER))
#define EPHY_FILE_CHOOSER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_FILE_CHOOSER, EphyFileChooserClass))

typedef struct _EphyFileChooser		EphyFileChooser;
typedef struct _EphyFileChooserClass	EphyFileChooserClass;

typedef enum
{
	EPHY_FILE_FILTER_ALL_SUPPORTED,
	EPHY_FILE_FILTER_WEBPAGES,
	EPHY_FILE_FILTER_IMAGES,
	EPHY_FILE_FILTER_ALL,
	EPHY_FILE_FILTER_NONE,
	EPHY_FILE_FILTER_LAST = EPHY_FILE_FILTER_NONE
} EphyFileFilterDefault;

struct _EphyFileChooser
{
	GtkFileChooserDialog parent;
};

struct _EphyFileChooserClass
{
	GtkFileChooserDialogClass parent_class;
};

GType		 ephy_file_chooser_get_type		(void);

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

#endif
