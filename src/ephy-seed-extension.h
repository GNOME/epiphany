/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2009, Robert Carr <carrr@rpi.edu>
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

#ifndef EPHY_SEED_EXTENSION_H
#define EPHY_SEED_EXTENSION_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SEED_EXTENSION		(ephy_seed_extension_get_type ())
#define EPHY_SEED_EXTENSION(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_SEED_EXTENSION, EphySeedExtension))
#define EPHY_SEED_EXTENSION_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_SEED_EXTENSION, EphySeedExtensionClass))
#define EPHY_IS_SEED_EXTENSION(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_SEED_EXTENSION))
#define EPHY_IS_SEED_EXTENSION_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_SEED_EXTENSION))
#define EPHY_SEED_EXTENSION_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_SEED_EXTENSION, EphySeedExtensionClass))

typedef struct _EphySeedExtension		EphySeedExtension;
typedef struct _EphySeedExtensionClass	EphySeedExtensionClass;
typedef struct _EphySeedExtensionPrivate	EphySeedExtensionPrivate;

struct _EphySeedExtensionClass
{
  GObjectClass parent_class;
};

struct _EphySeedExtension
{
  GObject parent_instance;

  /*< private >*/
  EphySeedExtensionPrivate *priv;
};

GType	ephy_seed_extension_get_type		(void);

G_END_DECLS

#endif
