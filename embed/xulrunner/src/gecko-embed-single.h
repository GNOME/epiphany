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

#ifndef gecko_embed_single_h
#define gecko_embed_single_h

#include "gecko-embed-type-builtins.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define GECKO_TYPE_EMBED_SINGLE         (gecko_embed_single_get_type())
#define GECKO_EMBED_SINGLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GECKO_TYPE_EMBED_SINGLE, GeckoEmbedSingle))
#define GECKO_EMBED_SINGLE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GECKO_TYPE_EMBED_SINGLE, GeckoEmbedSingleClass))
#define GECKO_IS_EMBED_SINGLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GECKO_TYPE_EMBED_SINGLE))
#define GECKO_IS_EMBED_SINGLE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GECKO_TYPE_EMBED_SINGLE))
#define GECKO_EMBED_SINGLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GECKO_TYPE_EMBED_SINGLE, GeckoEmbedSingleClass))

typedef struct _GeckoEmbedSingle        GeckoEmbedSingle;
typedef struct _GeckoEmbedSinglePrivate GeckoEmbedSinglePrivate;
typedef struct _GeckoEmbedSingleClass   GeckoEmbedSingleClass;

/* circular dependency */
#include "gecko-embed.h"

struct _GeckoEmbedSingle
{
  GObject parent_instance;

  /*< private >*/
  GeckoEmbedSinglePrivate *priv;
};

struct _GeckoEmbedSingleClass
{
  GObjectClass parent_class;

  void (* new_window_orphan)   (GeckoEmbedSingle *single,
				GeckoEmbed **newEmbed,
				guint chromemask);
};

GType             gecko_embed_single_get_type         (void);

GeckoEmbedSingle *gecko_embed_single_get              (void);

void              gecko_embed_single_push_startup     (void);

void              gecko_embed_single_pop_startup      (void);


G_END_DECLS

#endif /* gecko_embed_single_h */
