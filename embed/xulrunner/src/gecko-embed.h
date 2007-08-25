/*
 *  Copyright © Christopher Blizzard
 *  Copyright © Ramiro Estrugo 
 *  Copyright © 2006 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  ---------------------------------------------------------------------------
 *  Derived from Mozilla.org code, which had the following attributions:
 *
 *  The Original Code is mozilla.org code.
 *
 *  The Initial Developer of the Original Code is
 *  Christopher Blizzard. Portions created by Christopher Blizzard are Copyright © Christopher Blizzard.  All Rights Reserved.
 *  Portions created by the Initial Developer are Copyright © 2001
 *  the Initial Developer. All Rights Reserved.
 *
 *  Contributor(s):
 *    Christopher Blizzard <blizzard@mozilla.org>
 *    Ramiro Estrugo <ramiro@eazel.com>
 *  ---------------------------------------------------------------------------
 *
 *  $Id$
 */

#ifndef gecko_embed_h
#define gecko_embed_h

#include <stddef.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GECKO_TYPE_EMBED         (gecko_embed_get_type())
#define GECKO_EMBED(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GECKO_TYPE_EMBED, GeckoEmbed))
#define GECKO_EMBED_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GECKO_TYPE_EMBED, GeckoEmbedClass))
#define GECKO_IS_EMBED(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GECKO_TYPE_EMBED))
#define GECKO_IS_EMBED_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GECKO_TYPE_EMBED))
#define GECKO_EMBED_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GECKO_TYPE_EMBED, GeckoEmbedClass))

typedef struct _GeckoEmbed        GeckoEmbed;
typedef struct _GeckoEmbedPrivate GeckoEmbedPrivate;
typedef struct _GeckoEmbedClass   GeckoEmbedClass;

struct _GeckoEmbed
{
  GtkBin parent_instance;

  /*< private >*/
  GeckoEmbedPrivate *priv;
};

struct _GeckoEmbedClass
{
  GtkBinClass parent_class;

  /* Signals */

  /* Network */
  gboolean (* open_uri)            (GeckoEmbed *embed, const char *aURI);
  void     (* net_start)           (GeckoEmbed *embed);
  void     (* net_stop)            (GeckoEmbed *embed);
  void     (* net_state)           (GeckoEmbed *embed, int state, guint status);
  void     (* net_state_all)       (GeckoEmbed *embed, const char *aURI,
                                    int state, guint status);
  void     (* progress)            (GeckoEmbed *embed, int curprogress,
                                    int maxprogress);
  void     (* progress_all)        (GeckoEmbed *embed, const char *aURI,
                                    int curprogress, int maxprogress);
  void     (* security_change)     (GeckoEmbed *embed, gpointer request,
                                    guint state);
  void     (* status_change)       (GeckoEmbed *embed, gpointer request,
                                    int status, gpointer message);

  /* Document */
  void     (* link_message)        (GeckoEmbed *embed);
  void     (* js_status_message)   (GeckoEmbed *embed);
  void     (* location)            (GeckoEmbed *embed);
  void     (* title)               (GeckoEmbed *embed);
  void     (* visibility)          (GeckoEmbed *embed, gboolean visibility);
  void     (* destroy_browser)     (GeckoEmbed *embed);
  void     (* size_to)             (GeckoEmbed *embed, int width, int height);


  /* misc. */
  void (* new_window)          (GeckoEmbed *embed, GeckoEmbed **newEmbed,
                                guint chromemask);

  /* reserved for future use */
  void (* reserved_0) (void);
  void (* reserved_1) (void);
  void (* reserved_2) (void);
  void (* reserved_3) (void);
  void (* reserved_4) (void);
  void (* reserved_5) (void);
  void (* reserved_6) (void);
  void (* reserved_7) (void);
  void (* reserved_8) (void);
  void (* reserved_9) (void);
  void (* reserved_a) (void);
  void (* reserved_b) (void);
  void (* reserved_c) (void);
  void (* reserved_d) (void);
  void (* reserved_e) (void);
  void (* reserved_f) (void);
};

GType      gecko_embed_get_type         (void);
GtkWidget   *gecko_embed_new              (void);
void         gecko_embed_load_url         (GeckoEmbed *embed, 
					     const char *url);
void         gecko_embed_stop_load        (GeckoEmbed *embed);
gboolean     gecko_embed_can_go_back      (GeckoEmbed *embed);
gboolean     gecko_embed_can_go_forward   (GeckoEmbed *embed);
void         gecko_embed_go_back          (GeckoEmbed *embed);
void         gecko_embed_go_forward       (GeckoEmbed *embed);
void         gecko_embed_render_data      (GeckoEmbed *embed, 
					     const char *data,
					     guint32 len,
					     const char *base_uri, 
					     const char *mime_type);
void         gecko_embed_open_stream      (GeckoEmbed *embed,
					     const char *base_uri,
					     const char *mime_type);
void         gecko_embed_append_data      (GeckoEmbed *embed,
					     const char *data, guint32 len);
void         gecko_embed_close_stream     (GeckoEmbed *embed);
char        *gecko_embed_get_link_message (GeckoEmbed *embed);
char        *gecko_embed_get_js_status    (GeckoEmbed *embed);
char        *gecko_embed_get_title        (GeckoEmbed *embed);
char        *gecko_embed_get_location     (GeckoEmbed *embed);
void         gecko_embed_reload           (GeckoEmbed *embed, gint32 flags);
void         gecko_embed_set_chrome_mask  (GeckoEmbed *embed, 
					     guint32 flags);
guint32      gecko_embed_get_chrome_mask  (GeckoEmbed *embed);

G_END_DECLS

#endif /* gecko_embed_h */
