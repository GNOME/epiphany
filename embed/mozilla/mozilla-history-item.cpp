/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2007 Xan Lopez
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

#include "mozilla-config.h"
#include "config.h"

#include "mozilla-history-item.h"
#include "ephy-history-item.h"
#include "EphyBrowser.h"

#include <nsStringAPI.h>
#include <nsMemory.h>

static void mozilla_history_item_finalize (GObject *object);

static char*
impl_get_url (EphyHistoryItem *item)
{
  char *url = NULL;
  nsresult rv;
  nsCString nsUrl;
  MozillaHistoryItem *mitem = MOZILLA_HISTORY_ITEM (item);

  if (!mitem->embed)
    return NULL;

  EphyBrowser *browser = (EphyBrowser*)_mozilla_embed_get_browser (mitem->embed);

  rv = browser->GetSHUrlAtIndex(mitem->nth, nsUrl);

  if (NS_SUCCEEDED (rv) && nsUrl.Length()) {
    url = g_strdup(nsUrl.get());
  }

  return url;
}

static char*
impl_get_title (EphyHistoryItem *item)
{
  char *title = NULL;
  nsresult rv;
  PRUnichar *nsTitle;

  MozillaHistoryItem *mitem = MOZILLA_HISTORY_ITEM (item);

  if (!mitem->embed)
    return NULL;

  EphyBrowser *browser = (EphyBrowser*)_mozilla_embed_get_browser (mitem->embed);

  rv = browser->GetSHTitleAtIndex(mitem->nth, &nsTitle);

  if (NS_SUCCEEDED (rv) && nsTitle)
    {
      nsCString cTitle;
      NS_UTF16ToCString (nsString(nsTitle),
                         NS_CSTRING_ENCODING_UTF8, cTitle);
      title = g_strdup (cTitle.get());
      nsMemory::Free (nsTitle);
    }

  return title;
}

static void
mozilla_history_item_iface_init (EphyHistoryItemIface *iface)
{
  iface->get_url = impl_get_url;
  iface->get_title = impl_get_title;
}

G_DEFINE_TYPE_WITH_CODE (MozillaHistoryItem, mozilla_history_item, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_HISTORY_ITEM,
                                                mozilla_history_item_iface_init))

static void
mozilla_history_item_class_init (MozillaHistoryItemClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->finalize = mozilla_history_item_finalize;
}

static void
mozilla_history_item_init (MozillaHistoryItem *self)
{
}

static void
mozilla_history_item_finalize (GObject *object)
{
  MozillaHistoryItem *item = MOZILLA_HISTORY_ITEM (object);
  MozillaEmbed **ptr = &item->embed;

  g_object_remove_weak_pointer (G_OBJECT (item->embed),
                                (gpointer*)ptr);

  G_OBJECT_CLASS (mozilla_history_item_parent_class)->finalize (object);
}

MozillaHistoryItem*
mozilla_history_item_new (MozillaEmbed *embed, int index)
{
  MozillaHistoryItem *item;
  MozillaEmbed **ptr;

  item = (MozillaHistoryItem*) g_object_new (MOZILLA_TYPE_HISTORY_ITEM, NULL);
  item->embed = embed;
  ptr = &item->embed;

  g_object_add_weak_pointer (G_OBJECT (embed), (gpointer*)ptr);
  item->nth = index;

  return item;
}
