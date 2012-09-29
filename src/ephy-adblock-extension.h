/*
 *  Copyright © 2011 Igalia S.L.
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

#ifndef EPHY_ADBLOCK_EXTENSION_H
#define EPHY_ADBLOCK_EXTENSION_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_ADBLOCK_EXTENSION         (ephy_adblock_extension_get_type ())
#define EPHY_ADBLOCK_EXTENSION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_ADBLOCK_EXTENSION, EphyAdblockExtension))
#define EPHY_ADBLOCK_EXTENSION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_ADBLOCK_EXTENSION, EphyAdblockExtensionClass))
#define EPHY_IS_ADBLOCK_EXTENSION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_ADBLOCK_EXTENSION))
#define EPHY_IS_ADBLOCK_EXTENSION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_ADBLOCK_EXTENSION))
#define EPHY_ADBLOCK_EXTENSION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_ADBLOCK_EXTENSION, EphyAdblockExtensionClass))

typedef struct EphyAdblockExtension        EphyAdblockExtension;
typedef struct EphyAdblockExtensionClass   EphyAdblockExtensionClass;
typedef struct EphyAdblockExtensionPrivate EphyAdblockExtensionPrivate;

struct EphyAdblockExtensionClass
{
  GObjectClass parent_class;
};

struct EphyAdblockExtension
{
  GObject parent_instance;

  /*< private >*/
  EphyAdblockExtensionPrivate *priv;
};

GType ephy_adblock_extension_get_type (void);

G_END_DECLS

#endif
