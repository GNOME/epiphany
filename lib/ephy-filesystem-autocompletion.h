/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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

#ifndef EPHY_FILESYSTEM_AUTOCOMPLETION_H
#define EPHY_FILESYSTEM_AUTOCOMPLETION_H

#include <glib-object.h>

G_BEGIN_DECLS

/* object forward declarations */

typedef struct _EphyFilesystemAutocompletion EphyFilesystemAutocompletion;
typedef struct _EphyFilesystemAutocompletionClass EphyFilesystemAutocompletionClass;
typedef struct _EphyFilesystemAutocompletionPrivate EphyFilesystemAutocompletionPrivate;

/**
 * FilesystemAutocompletion object
 */

#define GUL_TYPE_FILESYSTEM_AUTOCOMPLETION		(ephy_filesystem_autocompletion_get_type())
#define GUL_FILESYSTEM_AUTOCOMPLETION(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), \
							 GUL_TYPE_FILESYSTEM_AUTOCOMPLETION,\
							 EphyFilesystemAutocompletion))
#define GUL_FILESYSTEM_AUTOCOMPLETION_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), \
							 GUL_TYPE_FILESYSTEM_AUTOCOMPLETION,\
							 EphyFilesystemAutocompletionClass))
#define GUL_IS_FILESYSTEM_AUTOCOMPLETION(object)	(G_TYPE_CHECK_INSTANCE_TYPE((object), \
							 GUL_TYPE_FILESYSTEM_AUTOCOMPLETION))
#define GUL_IS_FILESYSTEM_AUTOCOMPLETION_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), \
							 GUL_TYPE_FILESYSTEM_AUTOCOMPLETION))
#define GUL_FILESYSTEM_AUTOCOMPLETION_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), \
							 GUL_TYPE_FILESYSTEM_AUTOCOMPLETION,\
							 EphyFilesystemAutocompletionClass))

struct _EphyFilesystemAutocompletionClass 
{
	GObjectClass parent_class;

};

struct _EphyFilesystemAutocompletion
{
	GObject parent_object;
	EphyFilesystemAutocompletionPrivate *priv;
};

GType				ephy_filesystem_autocompletion_get_type		(void);
EphyFilesystemAutocompletion *	ephy_filesystem_autocompletion_new		(void);
void				ephy_filesystem_autocompletion_set_base_dir	(EphyFilesystemAutocompletion *fa,
										 const gchar *d);

G_END_DECLS

#endif
