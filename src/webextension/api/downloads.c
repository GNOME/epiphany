/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2022 Igalia S.L.
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

#include "ephy-file-helpers.h"
#include "ephy-web-extension-manager.h"

#include "api-utils.h"
#include "downloads.h"

static EphyDownloadsManager *
get_downloads_manager (void)
{
  return ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ());
}

static void
downloads_handler_download (EphyWebExtensionSender *sender,
                            const char             *method_name,
                            JsonArray              *args,
                            GTask                  *task)
{
  JsonObject *options = ephy_json_array_get_object (args, 0);
  EphyDownloadsManager *downloads_manager = get_downloads_manager ();
  g_autoptr (EphyDownload) download = NULL;
  g_autofree char *suggested_filename = NULL;
  g_autofree char *suggested_directory = NULL;
  const char *url;
  const char *filename;
  const char *conflict_action;

  if (!options) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "downloads.download(): Missing options object");
    return;
  }

  url = ephy_json_object_get_string (options, "url");
  if (!url) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "downloads.download(): Missing url");
    return;
  }

  filename = ephy_json_object_get_string (options, "filename");
  if (filename) {
    g_autoptr (GFile) downloads_dir = g_file_new_for_path (ephy_file_get_downloads_dir ());
    g_autoptr (GFile) destination = g_file_resolve_relative_path (downloads_dir, filename);
    g_autoptr (GFile) parent_dir = g_file_get_parent (destination);

    /* Relative paths are allowed however it cannot escape the parent directory. */
    if (!g_file_has_prefix (destination, downloads_dir)) {
      g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "downloads.download(): Relative filename cannot contain escape parent directory");
      return;
    }

    suggested_filename = g_file_get_basename (destination);
    suggested_directory = g_file_get_path (parent_dir);
  }

  conflict_action = ephy_json_object_get_string (options, "conflictAction");

  download = ephy_download_new_for_uri (url);
  ephy_download_set_allow_overwrite (download, g_strcmp0 (conflict_action, "overwrite") == 0);
  ephy_download_set_choose_filename (download, TRUE);
  ephy_download_set_suggested_destination (download, suggested_directory, suggested_filename);
  ephy_download_set_always_ask_destination (download, ephy_json_object_get_boolean (options, "saveAs", FALSE));
  ephy_download_set_initiating_web_extension_info (download, ephy_web_extension_get_guid (sender->extension), ephy_web_extension_get_name (sender->extension));
  ephy_downloads_manager_add_download (downloads_manager, download);

  /* FIXME: We should wait to return until after the user has been prompted to error if they cancelled it. */

  /* FIXME: The id is supposed to be persistent across sessions. */
  g_task_return_pointer (task, g_strdup_printf ("%" G_GUINT64_FORMAT, ephy_download_get_uid (download)), g_free);
}

static void
downloads_handler_cancel (EphyWebExtensionSender *sender,
                          const char             *method_name,
                          JsonArray              *args,
                          GTask                  *task)
{
  gint64 download_id = ephy_json_array_get_int (args, 0);
  EphyDownloadsManager *downloads_manager = get_downloads_manager ();
  EphyDownload *download;

  if (download_id < 0) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "downloads.cancel(): Missing downloadId");
    return;
  }

  download = ephy_downloads_manager_find_download_by_id (downloads_manager, (guint64)download_id);
  /* If we fail to find one its possible it was removed already. So instead of erroring just consider it a success. */
  if (!download) {
    g_task_return_pointer (task, NULL, NULL);
    return;
  }

  ephy_download_cancel (download);
  g_task_return_pointer (task, NULL, NULL);
}

static void
downloads_handler_open_or_show (EphyWebExtensionSender *sender,
                                const char             *method_name,
                                JsonArray              *args,
                                GTask                  *task)
{
  gint64 download_id = ephy_json_array_get_int (args, 0);
  EphyDownloadsManager *downloads_manager = get_downloads_manager ();
  EphyDownloadActionType action;
  EphyDownload *download;

  /* We reuse this method for both downloads.open() and downloads.show() as they are identical other than the action. */

  if (download_id < 0) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "downloads.%s(): Missing downloadId", method_name);
    return;
  }

  download = ephy_downloads_manager_find_download_by_id (downloads_manager, (guint64)download_id);
  if (!download) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "downloads.%s(): Failed to find downloadId", method_name);
    return;
  }

  if (strcmp (method_name, "open") == 0)
    action = EPHY_DOWNLOAD_ACTION_OPEN;
  else
    action = EPHY_DOWNLOAD_ACTION_BROWSE_TO;

  if (!ephy_download_do_download_action (download, action)) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "downloads.%s(): Failed to %s download", method_name, method_name);
    return;
  }

  g_task_return_pointer (task, NULL, NULL);
}

static GDateTime *
get_download_time_property (JsonObject *obj,
                            const char *name)
{
  /* https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/downloads/DownloadTime */
  JsonNode *node = json_object_get_member (obj, name);

  if (!node || !JSON_NODE_HOLDS_VALUE (node))
    return NULL;

  if (json_node_get_value_type (node) == G_TYPE_STRING) {
    const char *string = json_node_get_string (node);
    char *end = NULL;
    guint64 timestamp;

    /* This can be a number that's a timestamp. */
    timestamp = g_ascii_strtoull (string, &end, 10);
    if ((gsize)(end - string) == strlen (string))
      return g_date_time_new_from_unix_local (timestamp);

    return g_date_time_new_from_iso8601 (string, NULL);
  }

  if (json_node_get_value_type (node) == G_TYPE_INT64)
    return g_date_time_new_from_unix_local (json_node_get_int (node));

  return NULL;
}

typedef enum {
  DOWNLOAD_STATE_ANY,
  DOWNLOAD_STATE_IN_PROGRESS,
  DOWNLOAD_STATE_INTERRUPTED,
  DOWNLOAD_STATE_COMPLETE,
} DownloadState;

typedef struct {
  GPtrArray *query;
  GPtrArray *order_by;
  GDateTime *start_time;
  GDateTime *started_before;
  GDateTime *started_after;
  GDateTime *end_time;
  GDateTime *ended_before;
  GDateTime *ended_after;
  char *filename_regex;
  char *url_regex;
  char *filename;
  char *url;
  char *content_type;
  char *interrupt_reason;
  gint64 limit;
  gint64 id;
  gint64 bytes_received;
  gint64 total_bytes;
  gint64 file_size;
  gint64 total_bytes_greater;
  gint64 total_bytes_less;
  DownloadState state;
  ApiTriStateValue paused;
  ApiTriStateValue exists;
  ApiTriStateValue dangerous_only;
} DownloadQuery;

static void
download_query_free (DownloadQuery *query)
{
  g_clear_pointer (&query->start_time, g_date_time_unref);
  g_clear_pointer (&query->started_before, g_date_time_unref);
  g_clear_pointer (&query->started_after, g_date_time_unref);
  g_clear_pointer (&query->end_time, g_date_time_unref);
  g_clear_pointer (&query->ended_before, g_date_time_unref);
  g_clear_pointer (&query->ended_after, g_date_time_unref);
  g_ptr_array_free (query->query, TRUE);
  g_ptr_array_free (query->order_by, TRUE);
  g_free (query->filename);
  g_free (query->filename_regex);
  g_free (query->url);
  g_free (query->url_regex);
  g_free (query->interrupt_reason);
  g_free (query->content_type);
  g_free (query);
}

static DownloadQuery *
download_query_new (JsonObject *object)
{
  DownloadQuery *query = g_new (DownloadQuery, 1);
  const char *danger;
  const char *state;
  const char *mime;

  query->filename = ephy_json_object_dup_string (object, "filename");
  query->filename_regex = ephy_json_object_dup_string (object, "filenameRegex");
  query->url = ephy_json_object_dup_string (object, "url");
  query->url_regex = ephy_json_object_dup_string (object, "urlRegex");
  query->interrupt_reason = ephy_json_object_dup_string (object, "error");
  mime = ephy_json_object_get_string (object, "mime");
  query->content_type = mime ? g_content_type_from_mime_type (mime) : NULL;

  query->total_bytes_greater = ephy_json_object_get_int (object, "totalBytesGreater");
  query->total_bytes_less = ephy_json_object_get_int (object, "totalBytesLess");
  query->limit = ephy_json_object_get_int (object, "limit");
  query->bytes_received = ephy_json_object_get_int (object, "bytesReceived");
  query->total_bytes = ephy_json_object_get_int (object, "totalBytes");
  query->file_size = ephy_json_object_get_int (object, "fileSize");
  query->id = ephy_json_object_get_int (object, "id");

  query->start_time = get_download_time_property (object, "startTime");
  query->started_before = get_download_time_property (object, "startedBefore");
  query->started_after = get_download_time_property (object, "startedAfter");
  query->end_time = get_download_time_property (object, "endTime");
  query->ended_before = get_download_time_property (object, "endedBefore");
  query->ended_after = get_download_time_property (object, "endedAfter");

  query->query = ephy_json_object_get_string_array (object, "query");
  query->order_by = ephy_json_object_get_string_array (object, "orderBy");

  query->paused = ephy_json_object_get_boolean (object, "paused", -1);
  query->exists = ephy_json_object_get_boolean (object, "exists", -1);

  /* Epiphany doesn't detect dangerous files so we only care if the query wanted to
   * filter out *safe* files. */
  danger = ephy_json_object_get_string (object, "danger");
  query->dangerous_only = danger ? strcmp (danger, "safe") != 0 : API_VALUE_UNSET;

  query->state = DOWNLOAD_STATE_ANY;
  state = ephy_json_object_get_string (object, "state");
  if (state) {
    if (strcmp (state, "in_progress") == 0)
      query->state = DOWNLOAD_STATE_IN_PROGRESS;
    else if (strcmp (state, "interrupted") == 0)
      query->state = DOWNLOAD_STATE_INTERRUPTED;
    else if (strcmp (state, "complete") == 0)
      query->state = DOWNLOAD_STATE_COMPLETE;
  }

  return query;
}

static char *
download_get_filename (EphyDownload *download)
{
  const char *destination_path = ephy_download_get_destination (download);
  g_autoptr (GFile) dest_file = NULL;

  if (!destination_path)
    return NULL;

  dest_file = g_file_new_for_path (destination_path);
  return g_file_get_path (dest_file);
}

static const char *
download_get_url (EphyDownload *download)
{
  WebKitDownload *wk_dl = ephy_download_get_webkit_download (download);
  WebKitURIRequest *request = webkit_download_get_request (wk_dl);
  return webkit_uri_request_get_uri (request);
}

static guint64
download_get_received_size (EphyDownload *download)
{
  WebKitDownload *wk_dl = ephy_download_get_webkit_download (download);
  return webkit_download_get_received_data_length (wk_dl);
}

static gboolean
regex_matches (JSCContext *context,
               const char *regex,
               const char *string)
{
  /* WebExtensions can include arbitrary regex; To match expectations we need to run this against
   * the JavaScript implementation of regex rather than PCREs.
   * Note that this is absolutely untrusted code, however @context is private to this single API call
   * so they cannot actually do anything except make this match succeed or fail. */
  /* FIXME: Maybe this can use `jsc_value_constructor_call()` and `jsc_context_evaluate_in_object()` instead of
   * printf to avoid quotes potentially conflicting. */
  g_autofree char *code = g_strdup_printf ("let re = new RegExp('%s'); re.test('%s');", regex, string);
  g_autoptr (JSCValue) ret = jsc_context_evaluate (context, code, -1);
  return jsc_value_to_boolean (ret);
}

static gboolean
matches_filename_or_url (EphyDownload  *download,
                         DownloadQuery *query)
{
  g_autofree char *filename = download_get_filename (download);
  const char *url = download_get_url (download);
  g_autoptr (JSCContext) js_context = NULL;

  /* query contains a list of strings that must be in either the URL or the filename.
   * They may also be prefixed with `-` to require negative matches. */
  for (guint i = 0; i < query->query->len; i++) {
    const char *string = g_ptr_array_index (query->query, i);
    if (*string == '-') {
      if (strstr (url, string + 1) || strstr (filename, string + 1))
        return FALSE;
    } else {
      if (!strstr (url, string) && !strstr (filename, string))
        return FALSE;
    }
  }

  if (query->filename && g_strcmp0 (query->filename, filename))
    return FALSE;

  if (query->url && g_strcmp0 (query->url, url))
    return FALSE;

  if (query->url_regex || query->filename_regex)
    js_context = jsc_context_new ();

  if (query->url_regex && !regex_matches (js_context, query->url_regex, url))
    return FALSE;

  if (query->filename_regex && !regex_matches (js_context, query->filename_regex, filename))
    return FALSE;

  return TRUE;
}

static gboolean
matches_times (EphyDownload  *download,
               DownloadQuery *query)
{
  GDateTime *start_time = ephy_download_get_start_time (download);
  GDateTime *end_time = ephy_download_get_end_time (download);

  if (start_time) {
    if (query->start_time && g_date_time_compare (query->start_time, start_time) != 0)
      return FALSE;

    if (query->started_after && g_date_time_compare (query->started_after, start_time) >= 0)
      return FALSE;

    if (query->started_before && g_date_time_compare (query->started_before, start_time) <= 0)
      return FALSE;
  }

  if (end_time) {
    if (query->end_time && g_date_time_compare (query->end_time, end_time) != 0)
      return FALSE;

    if (query->ended_after && g_date_time_compare (query->ended_after, end_time) >= 0)
      return FALSE;

    if (query->ended_before && g_date_time_compare (query->ended_before, end_time) <= 0)
      return FALSE;
  }

  return TRUE;
}


static gboolean
match_error_to_interrupt_reason (GError     *error,
                                 const char *interrupt_reason)
{
  /* https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/downloads/InterruptReason */
  if (strcmp (interrupt_reason, "USER_CANCELED") == 0)
    return g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

  /* TODO: For now be very liberal, need to track all of these down. */
  return TRUE;
}

static const char *
error_to_interrupt_reason (GError *error)
{
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return "USER_CANCELLED";
  /* TODO */
  return "FILE_FAILED";
}

static int
order_downloads (EphyDownload *d1,
                 EphyDownload *d2,
                 GPtrArray    *order_by)
{
  /* TODO: Implement this...
   * An array of strings representing DownloadItem properties the search results should be sorted by.
   * For example, including startTime then totalBytes in the array would sort the DownloadItems by their start time, then total bytes — in ascending order.
   * To specify sorting by a property in descending order, prefix it with a hyphen, for example -startTime.
   */

  return 0;
}

static GList *
filter_downloads (GList         *downloads,
                  DownloadQuery *query)
{
  GList *matches = NULL;
  GList *extras = NULL;

  for (GList *l = downloads; l; l = g_list_next (l)) {
    EphyDownload *dl = l->data;
    guint64 received_size = download_get_received_size (dl);

    if (query->id != -1 && ephy_download_get_uid (dl) != (guint64)query->id)
      continue;

    if (query->dangerous_only == API_VALUE_TRUE)
      continue; /* We don't track dangerous files. */

    if (query->content_type && !g_content_type_equals (ephy_download_get_content_type (dl), query->content_type))
      continue;

    if (query->paused == API_VALUE_TRUE)
      continue; /* We don't support pausing. */

    if (query->exists != API_VALUE_UNSET && query->exists == ephy_download_get_was_moved (dl))
      continue;

    if (query->state != DOWNLOAD_STATE_ANY) {
      if (query->state == DOWNLOAD_STATE_IN_PROGRESS && !ephy_download_is_active (dl))
        continue;
      if (query->state == DOWNLOAD_STATE_INTERRUPTED && !ephy_download_failed (dl, NULL))
        continue;
      if (query->state == DOWNLOAD_STATE_COMPLETE && !ephy_download_succeeded (dl))
        continue;
    }

    if (query->bytes_received != -1 && (guint64)query->bytes_received != received_size)
      continue;

    /* This represents the file size on disk so far. We don't have easy access to this so
     * for now just treat it as bytes_received. */
    if (query->total_bytes != -1 && (guint64)query->total_bytes != received_size)
      continue;

    if (query->total_bytes_greater != -1 && (guint64)query->total_bytes_greater > received_size)
      continue;

    if (query->total_bytes_less != -1 && (guint64)query->total_bytes_less > received_size)
      continue;

    if (!matches_filename_or_url (dl, query))
      continue;

    if (!matches_times (dl, query))
      continue;

    if (query->interrupt_reason) {
      g_autoptr (GError) error = NULL;
      if (!ephy_download_failed (dl, &error))
        continue;

      if (!match_error_to_interrupt_reason (error, query->interrupt_reason))
        continue;
    }

    /* TODO: Handle file_size */

    matches = g_list_append (matches, dl);
  }

  if (query->order_by->len)
    matches = g_list_sort_with_data (matches, (GCompareDataFunc)order_downloads, query->order_by);

  if (query->limit) {
    extras = g_list_nth (matches, query->limit + 1);
    if (extras) {
      extras = g_list_remove_link (matches, extras);
      g_list_free (extras);
    }
  }

  return matches;
}

static void
add_download_to_json (JsonBuilder  *builder,
                      EphyDownload *download)
{
  GDateTime *end_time, *start_time;
  g_autofree char *end_time_iso8601 = NULL;
  g_autofree char *start_time_iso8601 = NULL;
  const char *content_type;
  g_autofree char *mime_type = NULL;
  g_autofree char *filename = download_get_filename (download);
  g_autoptr (GError) error = NULL;
  const char *extension_id;
  const char *extension_name;

  if ((start_time = ephy_download_get_start_time (download)))
    start_time_iso8601 = g_date_time_format_iso8601 (start_time);
  if ((end_time = ephy_download_get_end_time (download)))
    end_time_iso8601 = g_date_time_format_iso8601 (end_time);

  content_type = ephy_download_get_content_type (download);
  if (content_type)
    mime_type = g_content_type_get_mime_type (content_type);

  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "id");
  json_builder_add_int_value (builder, ephy_download_get_uid (download));
  json_builder_set_member_name (builder, "canResume");
  json_builder_add_boolean_value (builder, FALSE);
  json_builder_set_member_name (builder, "incognito");
  json_builder_add_boolean_value (builder, TRUE); /* We never remember downloads. */
  json_builder_set_member_name (builder, "exists");
  json_builder_add_boolean_value (builder, !ephy_download_get_was_moved (download));
  json_builder_set_member_name (builder, "danger");
  json_builder_add_string_value (builder, "safe");
  json_builder_set_member_name (builder, "url");
  json_builder_add_string_value (builder, download_get_url (download));
  json_builder_set_member_name (builder, "state");
  if (ephy_download_is_active (download))
    json_builder_add_string_value (builder, "in_progress");
  else if (ephy_download_failed (download, NULL))
    json_builder_add_string_value (builder, "interrupted");
  else
    json_builder_add_string_value (builder, "complete");
  if (mime_type) {
    json_builder_set_member_name (builder, "mime");
    json_builder_add_string_value (builder, mime_type);
  }
  json_builder_set_member_name (builder, "paused");
  json_builder_add_boolean_value (builder, FALSE);
  json_builder_set_member_name (builder, "filename");
  json_builder_add_string_value (builder, filename);
  if (start_time_iso8601) {
    json_builder_set_member_name (builder, "startTime");
    json_builder_add_string_value (builder, start_time_iso8601);
  }
  if (end_time_iso8601) {
    json_builder_set_member_name (builder, "endTime");
    json_builder_add_string_value (builder, end_time_iso8601);
  }
  json_builder_set_member_name (builder, "bytesReceived");
  json_builder_add_int_value (builder, (gint64)download_get_received_size (download));
  json_builder_set_member_name (builder, "totalBytes");
  json_builder_add_int_value (builder, -1);
  json_builder_set_member_name (builder, "fileSize");
  json_builder_add_int_value (builder, -1);
  if (ephy_download_failed (download, &error)) {
    json_builder_set_member_name (builder, "error");
    json_builder_add_string_value (builder, error_to_interrupt_reason (error));
  }
  if (ephy_download_get_initiating_web_extension_info (download, &extension_id, &extension_name)) {
    json_builder_set_member_name (builder, "byExtensionId");
    json_builder_add_string_value (builder, extension_id);
    json_builder_set_member_name (builder, "byExtensionName");
    json_builder_add_string_value (builder, extension_name);
  }
  json_builder_end_object (builder);
}

static char *
download_to_json (EphyDownload *download)
{
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;

  add_download_to_json (builder, download);
  root = json_builder_get_root (builder);

  return json_to_string (root, FALSE);
}

static void
downloads_handler_search (EphyWebExtensionSender *sender,
                          const char             *method_name,
                          JsonArray              *args,
                          GTask                  *task)
{
  JsonObject *query_object = ephy_json_array_get_object (args, 0);
  EphyDownloadsManager *downloads_manager = get_downloads_manager ();
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  DownloadQuery *query;
  GList *downloads;

  if (!query_object) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "downloads.query(): Missing query");
    return;
  }

  query = download_query_new (query_object);
  downloads = filter_downloads (ephy_downloads_manager_get_downloads (downloads_manager), query);
  download_query_free (query);

  json_builder_begin_array (builder);
  for (GList *l = downloads; l; l = g_list_next (l))
    add_download_to_json (builder, l->data);
  json_builder_end_array (builder);

  root = json_builder_get_root (builder);
  g_task_return_pointer (task, json_to_string (root, FALSE), g_free);
}

static void
downloads_handler_erase (EphyWebExtensionSender *sender,
                         const char             *method_name,
                         JsonArray              *args,
                         GTask                  *task)
{
  JsonObject *query_object = ephy_json_array_get_object (args, 0);
  EphyDownloadsManager *downloads_manager = get_downloads_manager ();
  g_autoptr (JsonBuilder) builder = json_builder_new ();
  g_autoptr (JsonNode) root = NULL;
  DownloadQuery *query;
  GList *downloads;

  if (!query_object) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "downloads.erase(): Missing query");
    return;
  }

  query = download_query_new (query_object);
  downloads = filter_downloads (ephy_downloads_manager_get_downloads (downloads_manager), query);
  download_query_free (query);

  json_builder_begin_array (builder);
  for (GList *l = downloads; l; l = g_list_next (l)) {
    EphyDownload *download = l->data;

    json_builder_add_int_value (builder, ephy_download_get_uid (download));
    ephy_downloads_manager_remove_download (downloads_manager, download);
  }
  json_builder_end_array (builder);

  root = json_builder_get_root (builder);
  g_task_return_pointer (task, json_to_string (root, FALSE), g_free);
}

static void
downloads_handler_showdefaultfolder (EphyWebExtensionSender *sender,
                                     const char             *method_name,
                                     JsonArray              *args,
                                     GTask                  *task)
{
  g_autoptr (GFile) default_folder = g_file_new_for_path (ephy_file_get_downloads_dir ());
  ephy_file_browse_to (default_folder, gtk_widget_get_display (GTK_WIDGET (sender->view)));
  g_task_return_pointer (task, NULL, NULL);
}

static void
delete_file_ready_cb (GFile        *file,
                      GAsyncResult *result,
                      GTask        *task)
{
  g_autoptr (GError) error = NULL;

  g_file_delete_finish (file, result, &error);

  /* The file not existing sounds like a success. */
  if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
    g_task_return_error (task, g_steal_pointer (&error));
    return;
  }

  g_task_return_pointer (task, NULL, NULL);
}

static void
downloads_handler_removefile (EphyWebExtensionSender *sender,
                              const char             *method_name,
                              JsonArray              *args,
                              GTask                  *task)
{
  gint64 download_id = ephy_json_array_get_int (args, 0);
  EphyDownloadsManager *downloads_manager = get_downloads_manager ();
  const char *destination;
  g_autoptr (GFile) destination_file = NULL;
  EphyDownload *download;

  if (download_id < 0) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "downloads.removeFile(): Missing downloadId");
    return;
  }

  download = ephy_downloads_manager_find_download_by_id (downloads_manager, (guint64)download_id);
  if (!download) {
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_INVALID_ARGUMENT, "downloads.removeFile(): Failed to find downloadId");
    return;
  }

  /* Ensure the download isn't active. */
  ephy_download_cancel (download);

  destination = ephy_download_get_destination (download);
  /* If a destination was never chosen this was never written to disk. */
  if (!destination) {
    g_task_return_pointer (task, NULL, NULL);
    return;
  }

  destination_file = g_file_new_for_path (destination);
  g_file_delete_async (destination_file, G_PRIORITY_DEFAULT, NULL, (GAsyncReadyCallback)delete_file_ready_cb, task);
}

static EphyWebExtensionApiHandler downloads_async_handlers[] = {
  {"download", downloads_handler_download},
  {"removeFile", downloads_handler_removefile},
  {"cancel", downloads_handler_cancel},
  {"open", downloads_handler_open_or_show},
  {"show", downloads_handler_open_or_show},
  {"showDefaultFolder", downloads_handler_showdefaultfolder},
  {"search", downloads_handler_search},
  {"erase", downloads_handler_erase},
};

void
ephy_web_extension_api_downloads_handler (EphyWebExtensionSender *sender,
                                          const char             *method_name,
                                          JsonArray              *args,
                                          GTask                  *task)
{
  if (!ephy_web_extension_has_permission (sender->extension, "downloads")) {
    g_warning ("Extension %s tried to use downloads without permission.", ephy_web_extension_get_name (sender->extension));
    g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_PERMISSION_DENIED, "downloads: Permission Denied");
    return;
  }

  for (guint idx = 0; idx < G_N_ELEMENTS (downloads_async_handlers); idx++) {
    EphyWebExtensionApiHandler handler = downloads_async_handlers[idx];

    if (g_strcmp0 (handler.name, method_name) == 0) {
      handler.execute (sender, method_name, args, task);
      return;
    }
  }

  g_task_return_new_error (task, WEB_EXTENSION_ERROR, WEB_EXTENSION_ERROR_NOT_IMPLEMENTED, "downloads.%s(): Not Implemented", method_name);
}

typedef struct {
  const char *event_name;
  char *json;
} DownloadEventData;

static void
foreach_extension_cb (EphyWebExtension *web_extension,
                      gpointer          user_data)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  DownloadEventData *data = user_data;

  if (!ephy_web_extension_has_permission (web_extension, "downloads"))
    return;

  ephy_web_extension_manager_emit_in_extension_views (manager, web_extension, data->event_name, data->json);
}

static void
download_added_cb (EphyDownloadsManager    *downloads_manager,
                   EphyDownload            *download,
                   EphyWebExtensionManager *manager)
{
  g_autofree char *json = download_to_json (download);
  DownloadEventData data = { "downloads.onCreated", json };
  ephy_web_extension_manager_foreach_extension (manager, foreach_extension_cb, &data);
}

static void
download_completed_cb (EphyDownloadsManager    *downloads_manager,
                       EphyDownload            *download,
                       EphyWebExtensionManager *manager)
{
  g_autofree char *json = download_to_json (download);
  DownloadEventData data = { "downloads.onChanged", json };
  ephy_web_extension_manager_foreach_extension (manager, foreach_extension_cb, &data);
}

static void
download_removed_cb (EphyDownloadsManager    *downloads_manager,
                     EphyDownload            *download,
                     EphyWebExtensionManager *manager)
{
  g_autofree char *json = g_strdup_printf ("%" G_GUINT64_FORMAT, ephy_download_get_uid (download));
  DownloadEventData data = { "downloads.onErased", json };
  ephy_web_extension_manager_foreach_extension (manager, foreach_extension_cb, &data);
}

void
ephy_web_extension_api_downloads_init (EphyWebExtensionManager *manager)
{
  EphyDownloadsManager *downloads_manager = get_downloads_manager ();

  g_signal_connect (downloads_manager, "download-added", G_CALLBACK (download_added_cb), manager);
  g_signal_connect (downloads_manager, "download-completed", G_CALLBACK (download_completed_cb), manager);
  g_signal_connect (downloads_manager, "download-removed", G_CALLBACK (download_removed_cb), manager);
}

void
ephy_web_extension_api_downloads_dispose (EphyWebExtensionManager *manager)
{
  EphyDownloadsManager *downloads_manager = get_downloads_manager ();

  g_signal_handlers_disconnect_by_data (downloads_manager, manager);
}
