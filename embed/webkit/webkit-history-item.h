/*  -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2008 Jan Alonzo
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

#ifndef __WEBKIT_HISTORY_ITEM_H__
#define __WEBKIT_HISTORY_ITEM_H__

#include <glib.h>
#include <glib-object.h>
#include <webkit/webkit.h>

#include "ephy-history-item.h"

G_BEGIN_DECLS

#define WEBKIT_TYPE_HISTORY_ITEM	           (webkit_history_item_get_type ())
#define WEBKIT_HISTORY_ITEM(o)		           (G_TYPE_CHECK_INSTANCE_CAST ((o), WEBKIT_TYPE_HISTORY_ITEM, WebKitHistoryItem))
#define WEBKIT_HISTORY_ITEM_CLASS(k)	       (G_TYPE_CHECK_CLASS_CAST((k), WEBKIT_TYPE_HISTORY_ITEM, WebKitHistoryItemClass))
#define WEBKIT_IS_HISTORY_ITEM(o)	           (G_TYPE_CHECK_INSTANCE_TYPE ((o), WEBKIT_TYPE_HISTORY_ITEM))
#define WEBKIT_IS_HISTORY_ITEM_CLASS(k)	     (G_TYPE_CHECK_CLASS_TYPE ((k), WEBKIT_TYPE_HISTORY_ITEM))
#define WEBKIT_HISTORY_ITEM_GET_CLASS(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), WEBKIT_TYPE_HISTORY_ITEM, WebKitHistoryItemClass))

typedef struct _WebKitHistoryItem	WebKitHistoryItem;
typedef struct _WebKitHistoryItemClass	WebKitHistoryItemClass;

struct _WebKitHistoryItemClass
{
  GObjectClass parent_class;
};

struct _WebKitHistoryItem
{
  GObject parent_instance;
  WebKitWebHistoryItem *data;
};

GType            webkit_history_item_get_type (void) G_GNUC_CONST;
EphyHistoryItem *webkit_history_item_new      (WebKitWebHistoryItem *item) G_GNUC_MALLOC;

G_END_DECLS

#endif /* __WEBKIT_HISTORY_ITEM_H__ */
