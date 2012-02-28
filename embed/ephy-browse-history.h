/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2011 Igalia S.L.
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

#ifndef _EPHY_BROWSE_HISTORY_H
#define _EPHY_BROWSE_HISTORY_H

#include <glib-object.h>

#include "ephy-history-service.h"

G_BEGIN_DECLS

#define EPHY_TYPE_BROWSE_HISTORY ephy_browse_history_get_type()

#define EPHY_BROWSE_HISTORY(obj)                                        \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                   \
                               EPHY_TYPE_BROWSE_HISTORY, EphyBrowseHistory))

#define EPHY_BROWSE_HISTORY_CLASS(klass)                                \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                    \
                            EPHY_TYPE_BROWSE_HISTORY, EphyBrowseHistoryClass))

#define EPHY_IS_BROWSE_HISTORY(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                     \
                               EPHY_TYPE_BROWSE_HISTORY))

#define EPHY_IS_BROWSE_HISTORY_CLASS(klass)             \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                    \
                            EPHY_TYPE_BROWSE_HISTORY))

#define EPHY_BROWSE_HISTORY_GET_CLASS(obj)                              \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                    \
                              EPHY_TYPE_BROWSE_HISTORY, EphyBrowseHistoryClass))

typedef struct _EphyBrowseHistory EphyBrowseHistory;
typedef struct _EphyBrowseHistoryClass EphyBrowseHistoryClass;
typedef struct _EphyBrowseHistoryPrivate EphyBrowseHistoryPrivate;

struct _EphyBrowseHistory
{
  GObject parent;

  EphyBrowseHistoryPrivate *priv;
};

struct _EphyBrowseHistoryClass
{
  GObjectClass parent_class;
};

GType              ephy_browse_history_get_type (void) G_GNUC_CONST;

EphyBrowseHistory *ephy_browse_history_new      (void);

void               ephy_browse_history_add_page (EphyBrowseHistory *history,
                                                 const char *orig_url);

void               ephy_browse_history_set_page_title (EphyBrowseHistory *history,
                                                       const char *url,
                                                       const char *title);

void              ephy_browse_history_set_page_zoom_level (EphyBrowseHistory *history,
                                                           const char *url,
                                                           const double zoom_level);

void              ephy_browse_history_get_url (EphyBrowseHistory *history,
                                               const char *url,
                                               EphyHistoryJobCallback callback,
                                               gpointer user_data);

void             ephy_browse_history_find_urls (EphyBrowseHistory *history,
                                                gint64 from, gint64 to,
                                                guint limit,
                                                GList *substring_list,
                                                EphyHistoryJobCallback callback,
                                                gpointer user_data);

void             ephy_browse_history_delete_urls (EphyBrowseHistory *history,
                                                  GList *urls,
                                                  EphyHistoryJobCallback callback,
                                                  gpointer user_data);

G_END_DECLS

#endif /* _EPHY_BROWSE_HISTORY_H */
