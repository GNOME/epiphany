/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © Jan Alonzo
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

#include "config.h"

#include "webkit-history-item.h"

static void webkit_history_item_finalize   (GObject *object);

EphyHistoryItem*
webkit_history_item_new (WebKitWebHistoryItem *history_item)
{
  WebKitHistoryItem *item;

  if (!history_item) return NULL;

  item = g_object_new (WEBKIT_TYPE_HISTORY_ITEM, NULL);
  item->data = g_object_ref (history_item);

  return EPHY_HISTORY_ITEM (item);
}

static char*
impl_get_url (EphyHistoryItem *item)
{
  const gchar *uri;

  if (!item) return NULL;

  uri = webkit_web_history_item_get_uri (WEBKIT_HISTORY_ITEM (item)->data);

  return g_strdup (uri);
}

static char*
impl_get_title (EphyHistoryItem *item)
{
  const gchar *title;

  if (!item) return NULL;

  title = webkit_web_history_item_get_title (WEBKIT_HISTORY_ITEM (item)->data);

  return g_strdup (title);
}

static void
webkit_history_item_iface_init (EphyHistoryItemIface *iface)
{
  iface->get_url = impl_get_url;
  iface->get_title = impl_get_title;
}

G_DEFINE_TYPE_WITH_CODE (WebKitHistoryItem, webkit_history_item, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_HISTORY_ITEM,
                                                webkit_history_item_iface_init))


static void
webkit_history_item_finalize (GObject *object)
{
  WebKitHistoryItem *item = WEBKIT_HISTORY_ITEM (object);

  g_object_unref (item->data);

  G_OBJECT_CLASS (webkit_history_item_parent_class)->finalize (object);
}

static void
webkit_history_item_class_init (WebKitHistoryItemClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass*)klass;

  gobject_class->finalize = webkit_history_item_finalize;
}

static void
webkit_history_item_init (WebKitHistoryItem *self)
{
}
