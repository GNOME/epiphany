/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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

#ifndef EPHY_EMBED_SINGLE_H
#define EPHY_EMBED_SINGLE_H

#include "ephy-embed.h"
#include "ephy-web-view.h"

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED_SINGLE    (ephy_embed_single_get_type ())
#define EPHY_EMBED_SINGLE(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSingle))
#define EPHY_EMBED_SINGLE_IFACE(k)  (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSingleIface))
#define EPHY_IS_EMBED_SINGLE(o)   (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_EMBED_SINGLE))
#define EPHY_IS_EMBED_SINGLE_IFACE(k) (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_EMBED_SINGLE))
#define EPHY_EMBED_SINGLE_GET_IFACE(i)  (G_TYPE_INSTANCE_GET_INTERFACE ((i), EPHY_TYPE_EMBED_SINGLE, EphyEmbedSingleIface))

typedef struct _EphyEmbedSingle   EphyEmbedSingle;
typedef struct _EphyEmbedSingleClass    EphyEmbedSingleClass;
typedef struct _EphyEmbedSinglePrivate  EphyEmbedSinglePrivate;

typedef struct {
  char *form_username;
  char *form_password;
  char *username;
} EphyEmbedSingleFormAuthData;

struct _EphyEmbedSingle {
  GObject parent;

  /*< private >*/
  EphyEmbedSinglePrivate *priv;
};

struct _EphyEmbedSingleClass
{
  GObjectClass parent_class;

  /* Signals */

  EphyEmbed * (* new_window)  (EphyEmbedSingle *single,
                               EphyEmbed *parent_embed,
                               EphyWebViewChrome chromemask);

  gboolean (* handle_content) (EphyEmbedSingle *shell,
                               char *mime_type,
                               char *uri);

  gboolean (* add_sidebar)    (EphyEmbedSingle *single,
                               const char *url,
                               const char *title);

  gboolean (* add_search_engine) (EphyEmbedSingle *single,
                                  const char *url,
                                  const char *icon_url,
                                  const char *title);
};

GType           ephy_embed_single_get_type           (void);

gboolean        ephy_embed_single_initialize         (EphyEmbedSingle *single);

GtkWidget      *ephy_embed_single_open_window        (EphyEmbedSingle *single,
                                                      EphyEmbed       *parent,
                                                      const char      *address,
                                                      const char      *name,
                                                      const char      *features);

void            ephy_embed_single_clear_cache        (EphyEmbedSingle *single);

void            ephy_embed_single_clear_auth_cache   (EphyEmbedSingle *single);

void            ephy_embed_single_set_network_status (EphyEmbedSingle *single,
                                                      gboolean         online);

gboolean        ephy_embed_single_get_network_status (EphyEmbedSingle *single);

GSList *        ephy_embed_single_get_form_auth      (EphyEmbedSingle *single,
                                                      const char *uri);

void            ephy_embed_single_add_form_auth      (EphyEmbedSingle *single,
                                                      const char *uri,
                                                      const char *form_username,
                                                      const char *form_password,
                                                      const char *username);

G_END_DECLS

#endif
