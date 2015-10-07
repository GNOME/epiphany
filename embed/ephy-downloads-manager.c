/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2015 Igalia S.L.
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
#include "ephy-downloads-manager.h"

enum {
  DOWNLOAD_ADDED,
  DOWNLOAD_REMOVED,

  LAST_SIGNAL
};

struct _EphyDownloadsManager
{
  GObject parent;

  GList *downloads;
};

struct _EphyDownloadsManagerClass
{
  GObjectClass parent_class;
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EphyDownloadsManager, ephy_downloads_manager, G_TYPE_OBJECT)

static void
ephy_downloads_manager_init (EphyDownloadsManager *manager)
{
}

static void
ephy_downloads_manager_dispose (GObject *object)
{
  EphyDownloadsManager *manager = EPHY_DOWNLOADS_MANAGER (object);

  g_list_free_full (manager->downloads, g_object_unref);

  G_OBJECT_CLASS (ephy_downloads_manager_parent_class)->dispose (object);
}

static void
ephy_downloads_manager_class_init (EphyDownloadsManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_downloads_manager_dispose;

  signals[DOWNLOAD_ADDED] =
    g_signal_new ("download-added",
                  EPHY_TYPE_DOWNLOADS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  EPHY_TYPE_DOWNLOAD);

  signals[DOWNLOAD_REMOVED] =
    g_signal_new ("download-removed",
                  EPHY_TYPE_DOWNLOADS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  EPHY_TYPE_DOWNLOAD);
}

static void
download_failed_cb (EphyDownload         *download,
                    GError               *error,
                    EphyDownloadsManager *manager)
{
  if (g_error_matches (error, WEBKIT_DOWNLOAD_ERROR, WEBKIT_DOWNLOAD_ERROR_CANCELLED_BY_USER))
    ephy_downloads_manager_remove_download (manager, download);
}

void
ephy_downloads_manager_add_download (EphyDownloadsManager *manager,
                                     EphyDownload         *download)
{
  g_return_if_fail (EPHY_IS_DOWNLOADS_MANAGER (manager));
  g_return_if_fail (EPHY_IS_DOWNLOAD (download));

  if (g_list_find (manager->downloads, download))
    return;

  manager->downloads = g_list_prepend (manager->downloads, g_object_ref (download));
  g_signal_connect (download, "error",
                    G_CALLBACK (download_failed_cb),
                    manager);
  g_signal_emit (manager, signals[DOWNLOAD_ADDED], 0, download);
}

void
ephy_downloads_manager_remove_download (EphyDownloadsManager *manager,
                                        EphyDownload         *download)
{
  GList *download_link;

  g_return_if_fail (EPHY_IS_DOWNLOADS_MANAGER (manager));
  g_return_if_fail (EPHY_IS_DOWNLOAD (download));

  download_link = g_list_find (manager->downloads, download);
  if (!download_link)
    return;

  manager->downloads = g_list_remove_link (manager->downloads, download_link);
  g_signal_emit (manager, signals[DOWNLOAD_REMOVED], 0, download);
  g_list_free_full (download_link, g_object_unref);
}

gboolean
ephy_downloads_manager_has_active_downloads (EphyDownloadsManager *manager)
{
  GList *l;

  g_return_val_if_fail (EPHY_IS_DOWNLOADS_MANAGER (manager), FALSE);

  for (l = manager->downloads; l; l = g_list_next (l)) {
    EphyDownload *download = EPHY_DOWNLOAD (l->data);

    if (ephy_download_is_active (download))
      return TRUE;
  }

  return FALSE;
}

GList *
ephy_downloads_manager_get_downloads (EphyDownloadsManager *manager)
{
  g_return_val_if_fail (EPHY_IS_DOWNLOADS_MANAGER (manager), NULL);

  return manager->downloads;
}
