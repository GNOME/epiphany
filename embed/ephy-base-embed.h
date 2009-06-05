/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003-2007 Christian Persch
 *  Copyright © 2007  Xan Lopez
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

#ifndef __EPHY_BASE_EMBED_H__
#define __EPHY_BASE_EMBED_H__

#include "ephy-embed.h"

#include <gtk/gtk.h>


G_BEGIN_DECLS

#define EPHY_TYPE_BASE_EMBED (ephy_base_embed_get_type ())
#define EPHY_BASE_EMBED(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_BASE_EMBED, EphyBaseEmbed))
#define EPHY_BASE_EMBED_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_BASE_EMBED, EphyBaseEmbedClass))
#define EPHY_IS_BASE_EMBED(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_BASE_EMBED))
#define EPHY_IS_BASE_EMBED_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_BASE_EMBED))
#define EPHY_BASE_EMBED_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_BASE_EMBED, EphyBaseEmbedClass))

typedef struct _EphyBaseEmbed EphyBaseEmbed;
typedef struct _EphyBaseEmbedClass EphyBaseEmbedClass;
typedef struct _EphyBaseEmbedPrivate EphyBaseEmbedPrivate;

struct _EphyBaseEmbedClass {
  GtkBinClass parent_class;
};

struct _EphyBaseEmbed {
  GtkBin parent_instance;

  /*< private >*/
  EphyBaseEmbedPrivate *priv;
};

GType ephy_base_embed_get_type (void) G_GNUC_CONST;

G_END_DECLS

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#endif /* __EPHY_BASE_EMBED_H__ */
