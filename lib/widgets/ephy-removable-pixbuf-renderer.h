/*
 * Copyright (c) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef _EPHY_REMOVABLE_PIXBUF_RENDERER_H
#define _EPHY_REMOVABLE_PIXBUF_RENDERER_H

#include "gd-toggle-pixbuf-renderer.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_REMOVABLE_PIXBUF_RENDERER            (ephy_removable_pixbuf_renderer_get_type())
#define EPHY_REMOVABLE_PIXBUF_RENDERER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_REMOVABLE_PIXBUF_RENDERER, EphyRemovablePixbufRenderer))
#define EPHY_REMOVABLE_PIXBUF_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_REMOVABLE_PIXBUF_RENDERER, EphyRemovablePixbufRendererClass))
#define EPHY_IS_REMOVABLE_PIXBUF_RENDERER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_REMOVABLE_PIXBUF_RENDERER))
#define EPHY_IS_REMOVABLE_PIXBUF_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_REMOVABLE_PIXBUF_RENDERER))
#define EPHY_REMOVABLE_PIXBUF_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_REMOVABLE_PIXBUF_RENDERER, EphyRemovablePixbufRendererClass))

typedef struct _EphyRemovablePixbufRenderer EphyRemovablePixbufRenderer;
typedef struct _EphyRemovablePixbufRendererClass EphyRemovablePixbufRendererClass;
typedef struct _EphyRemovablePixbufRendererPrivate EphyRemovablePixbufRendererPrivate;

typedef enum {
  EPHY_REMOVABLE_PIXBUF_RENDER_PRELIT = 0,
  EPHY_REMOVABLE_PIXBUF_RENDER_NEVER,
  EPHY_REMOVABLE_PIXBUF_RENDER_ALWAYS
} EphyRemovablePixbufRenderPolicy;

struct _EphyRemovablePixbufRenderer
{
  GdTogglePixbufRenderer parent;

  EphyRemovablePixbufRendererPrivate *priv;
};

struct _EphyRemovablePixbufRendererClass
{
  GdTogglePixbufRendererClass parent_class;
};

GType ephy_removable_pixbuf_renderer_get_type (void) G_GNUC_CONST;

GtkCellRenderer *ephy_removable_pixbuf_renderer_new (void);

G_END_DECLS

#endif /* _EPHY_REMOVABLE_PIXBUF_RENDERER_H */
