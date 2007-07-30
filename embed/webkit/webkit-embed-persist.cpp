/*
 *  Copyright © 2007 Xan Lopez <xan@gnome.org>
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
 *  $Id$
 */

#include "webkit-embed-persist.h"

static void
webkit_embed_persist_class_init (WebKitEmbedPersistClass *klass);
static void
webkit_embed_persist_init (WebKitEmbedPersist *ges);
static void
webkit_embed_persist_finalize (GObject *object);

#define WEBKIT_EMBED_PERSIST_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), WEBKIT_TYPE_EMBED_PERSIST, WebKitEmbedPersistPrivate))

G_DEFINE_TYPE (WebKitEmbedPersist, webkit_embed_persist, EPHY_TYPE_EMBED_PERSIST)

static void
webkit_embed_persist_init (WebKitEmbedPersist *persist)
{
}

static void
webkit_embed_persist_finalize (GObject *object)
{
  G_OBJECT_CLASS (webkit_embed_persist_parent_class)->finalize (object);
}

void
webkit_embed_persist_completed (WebKitEmbedPersist *persist)
{
  g_signal_emit_by_name (persist, "completed");
  g_object_unref (persist);
}

void
webkit_embed_persist_cancelled (WebKitEmbedPersist *persist)
{
  g_signal_emit_by_name (persist, "cancelled");
  g_object_unref (persist);
}

static void
impl_cancel (EphyEmbedPersist *persist)
{
  g_object_unref (persist);
}

static gboolean
impl_save (EphyEmbedPersist *persist)
{
  g_object_ref (persist);
}

static char *
impl_to_string (EphyEmbedPersist *persist)
{
  return NULL;
}

static void
webkit_embed_persist_class_init (WebKitEmbedPersistClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  EphyEmbedPersistClass *persist_class = EPHY_EMBED_PERSIST_CLASS (klass);
	
  object_class->finalize = webkit_embed_persist_finalize;

  persist_class->save = impl_save;
  persist_class->cancel = impl_cancel;
  persist_class->to_string = impl_to_string;

  //  g_type_class_add_private (object_class, sizeof(WebKitEmbedPersistPrivate));
}
