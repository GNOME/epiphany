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

#include <mozilla-config.h>
#include "config.h"

#include "gecko-embed-single.h"
#include "gecko-embed-private.h"
#include "gecko-embed-signals.h"
#include "gecko-embed-marshal.h"

#include "GeckoSingle.h"

#define GECKO_EMBED_SINGLE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), GECKO_TYPE_EMBED_SINGLE, GeckoEmbedSinglePrivate))

struct _GeckoEmbedSinglePrivate
{
  GeckoSingle *single;
};

enum
{
  NEW_WINDOW_ORPHAN,
  LAST_SINGLE_SIGNAL
};

static guint gecko_embed_single_signals[LAST_SINGLE_SIGNAL] = { 0 };

static void gecko_embed_single_class_init (GeckoEmbedSingleClass *klass);
static void gecko_embed_single_init       (GeckoEmbedSingle *embed);

static GObjectClass *parent_class = NULL;

GType
gecko_embed_single_get_type(void)
{
  static GType type = 0;

  if (!type)
  {
    const GTypeInfo info =
    {
      sizeof (GeckoEmbedSingleClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) gecko_embed_single_class_init,
      NULL,
      NULL, /* class_data */
      sizeof (GeckoEmbedSingle),
      0, /* n_preallocs */
      (GInstanceInitFunc) gecko_embed_single_init
    };

    type = g_type_register_static (G_TYPE_OBJECT, "GeckoEmbedSingle",
                                   &info, (GTypeFlags) 0);
  }

  return type;
}

GeckoEmbedSingle *
gecko_embed_single_get (void)
{
  static GeckoEmbedSingle *single = NULL;

  if (!single)
  {
    single = GECKO_EMBED_SINGLE (g_object_new (GECKO_TYPE_EMBED_SINGLE, NULL));

    g_object_add_weak_pointer (G_OBJECT (single), (gpointer *) &single);
  }

  return single;
}

extern "C" void
gecko_embed_single_create_window (GeckoEmbed **aNewEmbed,
                                  guint aChromeFlags)
{
  GeckoEmbedSingle *single = gecko_embed_single_get ();

  *aNewEmbed = nsnull;

  if (!single)
    return;

  g_signal_emit (single, gecko_embed_single_signals[NEW_WINDOW_ORPHAN], 0,
                 (void **) aNewEmbed, aChromeFlags);
}

void
gecko_embed_single_push_startup(void)
{
  GeckoEmbedSingle *single = gecko_embed_single_get ();

  single->priv->single->PushStartup();
}

void
gecko_embed_single_pop_startup(void)
{
  GeckoEmbedSingle *single = gecko_embed_single_get ();

  single->priv->single->PopStartup();
}

static void
gecko_embed_single_init(GeckoEmbedSingle *embed)
{
  embed->priv = GECKO_EMBED_SINGLE_GET_PRIVATE (embed);

  embed->priv->single = new GeckoSingle ();
}

static void
gecko_embed_single_finalize (GObject *object)
{
  GeckoEmbedSingle *single = GECKO_EMBED_SINGLE (object);

  delete single->priv->single;
  single->priv->single = nsnull;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gecko_embed_single_class_init (GeckoEmbedSingleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

  object_class->finalize = gecko_embed_single_finalize;

  gecko_embed_single_signals[NEW_WINDOW] =
    g_signal_new ("new_window_orphan",
                  GECKO_TYPE_EMBED_SINGLE,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GeckoEmbedSingleClass, new_window_orphan),
                  NULL, NULL,
                  gecko_embed_marshal_VOID__OBJECT_UINT,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_OBJECT,
                  G_TYPE_UINT);

  g_type_class_add_private (object_class, sizeof (GeckoEmbedSinglePrivate));
}
