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
void  ephy_base_embed_set_title (EphyBaseEmbed *embed,
                                 char *title);
void  ephy_base_embed_set_loading_title (EphyBaseEmbed *embed,
                                         const char *title,
                                         gboolean is_address);
void  ephy_base_embed_set_address (EphyBaseEmbed *embed, char *address);
void  ephy_base_embed_location_changed (EphyBaseEmbed *embed,
                                        char *location);
void  ephy_base_embed_load_icon (EphyBaseEmbed *embed);
void  ephy_base_embed_set_icon_address (EphyBaseEmbed *embed,
                                        const char *address);
void  ephy_base_embed_set_link_message (EphyBaseEmbed *embed,
                                        char *link_message);
void  ephy_base_embed_set_security_level (EphyBaseEmbed *embed,
                                          EphyEmbedSecurityLevel level);
void  ephy_base_embed_restore_zoom_level (EphyBaseEmbed *membed,
                                          const char *address);
void  ephy_base_embed_update_from_net_state (EphyBaseEmbed *embed,
                                             const char *uri,
                                             EphyEmbedNetState state);
void  ephy_base_embed_set_load_percent (EphyBaseEmbed *embed, int percent);

G_END_DECLS

#endif /* __EPHY_BASE_EMBED_H__ */
