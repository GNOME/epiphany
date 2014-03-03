/*
 *  Copyright © 2014 Igalia S.L.
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
 */

#ifndef EPHY_WEB_EXTENSION_H
#define EPHY_WEB_EXTENSION_H

#include <glib-object.h>
#include <webkit2/webkit-web-extension.h>

G_BEGIN_DECLS

#define EPHY_TYPE_WEB_EXTENSION            (ephy_web_extension_get_type())
#define EPHY_WEB_EXTENSION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_WEB_EXTENSION, EphyWebExtension))
#define EPHY_IS_WEB_EXTENSION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_WEB_EXTENSION))
#define EPHY_WEB_EXTENSION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_WEB_EXTENSION, EphyWebExtensionClass))
#define EPHY_IS_WEB_EXTENSION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_WEB_EXTENSION))
#define EPHY_WEB_EXTENSION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_WEB_EXTENSION, EphyWebExtensionClass))

typedef struct _EphyWebExtension        EphyWebExtension;
typedef struct _EphyWebExtensionClass   EphyWebExtensionClass;
typedef struct _EphyWebExtensionPrivate EphyWebExtensionPrivate;

struct _EphyWebExtension
{
  GObject parent;

  EphyWebExtensionPrivate *priv;
};

struct _EphyWebExtensionClass
{
  GObjectClass parent_class;
};

GType             ephy_web_extension_get_type       (void) G_GNUC_CONST;

EphyWebExtension *ephy_web_extension_get            (void);
void              ephy_web_extension_initialize     (EphyWebExtension   *extension,
                                                     WebKitWebExtension *wk_extension,
                                                     const char         *dot_dir,
                                                     gboolean            is_private_profile);
void              ephy_web_extension_dbus_register  (EphyWebExtension   *extension,
                                                     GDBusConnection    *connection);


G_END_DECLS

#endif /* EPHY_WEB_EXTENSION_H */
