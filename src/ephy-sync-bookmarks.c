/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Gabriel Ivascu <ivascu.gabriel59@gmail.com>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-sync-bookmarks.h"

#include "ephy-debug.h"
#include "ephy-shell.h"
#include "ephy-sync-utils.h"

#define EPHY_BOOKMARKS_DUMMY_BSO   "000000000000"
#define EPHY_BOOKMARKS_COLLECTION  "ephy-bookmarks"

static void
create_bso_collection_response_cb (SoupSession *session,
                                   SoupMessage *message,
                                   gpointer     user_data)
{
  EphySyncService *service;
  gchar *endpoint;

  service = ephy_shell_get_global_sync_service (ephy_shell_get_default ());

  /* Status code 412 means the BSO already exists. Since we will delete it
   * anyway, we don't treat this as an error.
   */
  if (message->status_code != 200 && message->status_code != 412) {
    g_warning ("Failed to add dummy BSO to collection, status code: %u, response: %s",
               message->status_code, message->response_body->data);
    return;
  }

  /* The EPHY_BOOKMARKS_COLLECTION collection is now created. We can safely
   * delete the dummy BSO that we've uploaded. No need to check for response.
   */
  endpoint = g_strdup_printf ("storage/%s/%s", EPHY_BOOKMARKS_COLLECTION, EPHY_BOOKMARKS_DUMMY_BSO);
  ephy_sync_service_send_storage_message (service, endpoint, SOUP_METHOD_DELETE,
                                          NULL, -1, -1, NULL, NULL);
  g_free (endpoint);
}

void
ephy_sync_bookmarks_create_storage_collection (void)
{
  EphySyncService *service;
  gchar *endpoint;
  gchar *bso;

  service = ephy_shell_get_global_sync_service (ephy_shell_get_default ());
  endpoint = g_strdup_printf ("storage/%s/%s", EPHY_BOOKMARKS_COLLECTION, EPHY_BOOKMARKS_DUMMY_BSO);
  bso = ephy_sync_utils_create_bso_json (EPHY_BOOKMARKS_DUMMY_BSO, EPHY_BOOKMARKS_DUMMY_BSO);

  /* Send a dummy BSO to the Storage Server so it will create the
   * EPHY_BOOKMARKS_COLLECTION collection if it doesn't exist already.
   * In the response callback we will delete the dummy BSO.
   */
  ephy_sync_service_send_storage_message (service, endpoint, SOUP_METHOD_PUT,
                                          bso, -1, 0,
                                          create_bso_collection_response_cb, NULL);

  g_free (endpoint);
  g_free (bso);
}

static void
server_response_default_cb (SoupSession *session,
                            SoupMessage *message,
                            gpointer     user_data)
{
  LOG ("Server response:");
  LOG ("status_code: %u", message->status_code);
  LOG ("response_body: %s", message->response_body->data);
  LOG ("Retry-After: %s", soup_message_headers_get_one (message->response_headers, "Retry-After"));
  LOG ("X-Weave-Backoff: %s", soup_message_headers_get_one (message->response_headers, "X-Weave-Backoff"));
  LOG ("X-Last-Modified: %s", soup_message_headers_get_one (message->response_headers, "X-Last-Modified"));
  LOG ("X-Weave-Timestamp: %s", soup_message_headers_get_one (message->response_headers, "X-Weave-Timestamp"));
  LOG ("X-Weave-Records: %s", soup_message_headers_get_one (message->response_headers, "X-Weave-Records"));
  LOG ("X-Weave-Next-Offset: %s", soup_message_headers_get_one (message->response_headers, "X-Weave-Next-Offset"));
  LOG ("X-Weave-Quota-Remaining: %s", soup_message_headers_get_one (message->response_headers, "X-Weave-Quota-Remaining"));
  LOG ("X-Weave-Alert: %s", soup_message_headers_get_one (message->response_headers, "X-Weave-Alert"));
}

void
ephy_sync_bookmarks_check_storage_collection (void)
{
  EphySyncService *service;
  gchar *endpoint;

  service = ephy_shell_get_global_sync_service (ephy_shell_get_default ());
  endpoint = g_strdup_printf ("storage/%s", EPHY_BOOKMARKS_COLLECTION);
  ephy_sync_service_send_storage_message (service, endpoint,
                                          SOUP_METHOD_GET, NULL, -1, -1,
                                          server_response_default_cb, NULL);
  g_free (endpoint);
}

void
ephy_sync_bookmarks_delete_storage_collection (void)
{
  EphySyncService *service;
  gchar *endpoint;

  service = ephy_shell_get_global_sync_service (ephy_shell_get_default ());
  endpoint = g_strdup_printf ("storage/%s", EPHY_BOOKMARKS_COLLECTION);
  ephy_sync_service_send_storage_message (service, endpoint,
                                          SOUP_METHOD_DELETE, NULL, -1, -1,
                                          server_response_default_cb, NULL);
  g_free (endpoint);
}
