/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2015 Igalia S.L.
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-downloads-manager.h"

#include "ephy-embed-shell.h"

enum {
  DOWNLOAD_ADDED,
  DOWNLOAD_COMPLETED,
  DOWNLOAD_REMOVED,

  ESTIMATED_PROGRESS_CHANGED,
  SHOW_DOWNLOADS,

  LAST_SIGNAL
};

struct _EphyDownloadsManager {
  GObject parent_instance;

  GList *downloads;

  guint inhibitors;
  guint inhibitor_cookie;
};

static guint signals[LAST_SIGNAL];

G_DEFINE_FINAL_TYPE (EphyDownloadsManager, ephy_downloads_manager, G_TYPE_OBJECT)

static void
ephy_downloads_manager_acquire_session_inhibitor (EphyDownloadsManager *manager)
{
  manager->inhibitors++;
  if (manager->inhibitors > 1)
    return;

  g_assert (manager->inhibitor_cookie == 0);
  manager->inhibitor_cookie = gtk_application_inhibit (GTK_APPLICATION (ephy_embed_shell_get_default ()),
                                                       NULL,
                                                       GTK_APPLICATION_INHIBIT_LOGOUT | GTK_APPLICATION_INHIBIT_SUSPEND,
                                                       "Downloading");

  if (manager->inhibitor_cookie == 0)
    g_warning ("Failed to acquire session inhibitor for active download. Is gnome-session running?");
}

static void
ephy_downloads_manager_release_session_inhibitor (EphyDownloadsManager *manager)
{
  g_assert (manager->inhibitors > 0);
  manager->inhibitors--;

  if (manager->inhibitors > 0)
    return;

  if (manager->inhibitor_cookie > 0) {
    gtk_application_uninhibit (GTK_APPLICATION (ephy_embed_shell_get_default ()),
                               manager->inhibitor_cookie);
    manager->inhibitor_cookie = 0;
  }
}

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
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  EPHY_TYPE_DOWNLOAD);

  signals[DOWNLOAD_COMPLETED] =
    g_signal_new ("download-completed",
                  EPHY_TYPE_DOWNLOADS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  EPHY_TYPE_DOWNLOAD);

  signals[DOWNLOAD_REMOVED] =
    g_signal_new ("download-removed",
                  EPHY_TYPE_DOWNLOADS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  EPHY_TYPE_DOWNLOAD);

  signals[ESTIMATED_PROGRESS_CHANGED] =
    g_signal_new ("estimated-progress-changed",
                  EPHY_TYPE_DOWNLOADS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[SHOW_DOWNLOADS] =
    g_signal_new ("show-downloads",
                  EPHY_TYPE_DOWNLOADS_MANAGER,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
download_completed_cb (EphyDownload         *download,
                       EphyDownloadsManager *manager)
{
  g_signal_emit (manager, signals[ESTIMATED_PROGRESS_CHANGED], 0);
  g_signal_emit (manager, signals[DOWNLOAD_COMPLETED], 0, download);
  ephy_downloads_manager_release_session_inhibitor (manager);
}

static void
download_failed_cb (EphyDownload         *download,
                    GError               *error,
                    EphyDownloadsManager *manager)
{
  if (g_error_matches (error, WEBKIT_DOWNLOAD_ERROR, WEBKIT_DOWNLOAD_ERROR_CANCELLED_BY_USER))
    ephy_downloads_manager_remove_download (manager, download);
  g_signal_emit (manager, signals[ESTIMATED_PROGRESS_CHANGED], 0);
  ephy_downloads_manager_release_session_inhibitor (manager);
}

static void
download_estimated_progress_changed_cb (EphyDownloadsManager *manager)
{
  g_signal_emit (manager, signals[ESTIMATED_PROGRESS_CHANGED], 0);
}

void
ephy_downloads_manager_add_download (EphyDownloadsManager *manager,
                                     EphyDownload         *download)
{
  WebKitDownload *wk_download;

  g_assert (EPHY_IS_DOWNLOADS_MANAGER (manager));
  g_assert (EPHY_IS_DOWNLOAD (download));

  if (g_list_find (manager->downloads, download))
    return;

  ephy_downloads_manager_acquire_session_inhibitor (manager);

  manager->downloads = g_list_prepend (manager->downloads, g_object_ref (download));
  g_signal_connect (download, "completed",
                    G_CALLBACK (download_completed_cb),
                    manager);
  g_signal_connect (download, "error",
                    G_CALLBACK (download_failed_cb),
                    manager);

  wk_download = ephy_download_get_webkit_download (download);
  g_signal_connect_swapped (wk_download, "notify::estimated-progress",
                            G_CALLBACK (download_estimated_progress_changed_cb),
                            manager);
  g_signal_emit (manager, signals[DOWNLOAD_ADDED], 0, download);
  g_signal_emit (manager, signals[ESTIMATED_PROGRESS_CHANGED], 0);
}

void
ephy_downloads_manager_remove_download (EphyDownloadsManager *manager,
                                        EphyDownload         *download)
{
  GList *download_link;

  g_assert (EPHY_IS_DOWNLOADS_MANAGER (manager));
  g_assert (EPHY_IS_DOWNLOAD (download));

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

  g_assert (EPHY_IS_DOWNLOADS_MANAGER (manager));

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
  g_assert (EPHY_IS_DOWNLOADS_MANAGER (manager));

  return manager->downloads;
}

gdouble
ephy_downloads_manager_get_estimated_progress (EphyDownloadsManager *manager)
{
  GList *l;
  guint n_active = 0;
  gdouble progress = 0;

  g_assert (EPHY_IS_DOWNLOADS_MANAGER (manager));

  for (l = manager->downloads; l; l = g_list_next (l)) {
    EphyDownload *download = EPHY_DOWNLOAD (l->data);

    if (!ephy_download_is_active (download))
      continue;

    n_active++;
    progress += webkit_download_get_estimated_progress (ephy_download_get_webkit_download (download));
  }

  return n_active > 0 ? progress / n_active : 1;
}

EphyDownload *
ephy_downloads_manager_find_download_by_id (EphyDownloadsManager *manager,
                                            guint64               id)
{
  g_assert (EPHY_IS_DOWNLOADS_MANAGER (manager));

  for (GList *l = manager->downloads; l; l = g_list_next (l)) {
    EphyDownload *download = EPHY_DOWNLOAD (l->data);

    if (ephy_download_get_uid (download) == id)
      return download;
  }

  return NULL;
}
