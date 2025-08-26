/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2017, 2019 Igalia S.L.
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
#include "ephy-filters-manager.h"

#include "ephy-debug.h"
#include "ephy-download.h"
#include "ephy-file-helpers.h"
#include "ephy-langs.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-string.h"
#include "ephy-embed-shell.h"

#include <gio/gio.h>

#include <inttypes.h>

#define ADBLOCK_FILTER_UPDATE_FREQUENCY 24 * 60 * 60 /* In seconds */
#define ADBLOCK_FILTER_UPDATE_FREQUENCY_METERED 28 * 24 * 60 * 60 /* In seconds */
#define ADBLOCK_FILTER_SIDECAR_FILE_SUFFIX ".filterinfo"

struct _EphyFiltersManager {
  GObject parent_instance;
  gboolean is_initialized;

  char *filters_dir;
  GHashTable *filters;  /* (identifier, FilterInfo) */
  gint64 update_time;
  guint update_timeout_id;
  GCancellable *cancellable;
  WebKitUserContentFilter *wk_filter;
  WebKitUserContentFilterStore *store;
  gboolean metered;
};

G_DEFINE_FINAL_TYPE (EphyFiltersManager, ephy_filters_manager, G_TYPE_OBJECT)

enum {
  FILTER_READY,
  FILTER_REMOVED,
  FILTERS_DISABLED,
  LAST_SIGNAL,
};

static guint s_signals[LAST_SIGNAL];

enum {
  PROP_0,
  PROP_FILTERS_DIR,
  PROP_IS_INITIALIZED,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

typedef struct {
  EphyFiltersManager *manager;
  char *identifier;      /* Lazily derived from source_uri. */
  char *source_uri;      /* Saved. */
  char *checksum;        /* Saved. */
  gint64 last_update;    /* Saved, seconds since the Epoch. */

  gboolean found : 1;    /* WebKitUserContentFilter found during lookup. */
  gboolean local : 1;    /* The source_uri is a local file URI. */
  gboolean done  : 1;    /* Filter setup done (successfully or errored). */
} FilterInfo;

/* The "saved" fields from the struct above are stored as versioned sidecar
 * metadata files, using GVariant for serialization. An integer indicating
 * the version of the on-disk format is prepended to the data, and it must
 * be increased by 1 in the source code whenever the GVariant format below
 * changes.
 */
#define FILTER_INFO_VARIANT_VERSION ((uint32_t)2)
#define FILTER_INFO_VARIANT_FORMAT  "(usmsx)"

static void filter_info_setup_done (FilterInfo *self);

static void
filter_info_free (FilterInfo *self)
{
  g_clear_weak_pointer (&self->manager);
  g_clear_pointer (&self->identifier, g_free);
  g_clear_pointer (&self->source_uri, g_free);
  g_clear_pointer (&self->checksum, g_free);
  g_free (self);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FilterInfo, filter_info_free)

static FilterInfo *
filter_info_new (const char         *source_uri,
                 EphyFiltersManager *manager)
{
  g_autoptr (FilterInfo) self = NULL;

  g_assert (source_uri);
  g_assert (manager);

  self = g_new0 (FilterInfo, 1);
  self->source_uri = g_strdup (source_uri);
  self->last_update = G_MININT64;  /* Oldest possible time: never updated. */
  g_set_weak_pointer (&self->manager, manager);
  return g_steal_pointer (&self);
}

static gboolean
filter_info_load_from_bytes (FilterInfo  *self,
                             GBytes      *data,
                             GError     **error)
{
  uint32_t saved_version = 0;
  g_autofree char *source_uri = NULL;
  g_autofree char *checksum = NULL;
  guint64 last_update = 0;

  g_autoptr (GVariantType) value_type = g_variant_type_new (FILTER_INFO_VARIANT_FORMAT);
  g_autoptr (GVariant) value = g_variant_ref_sink (g_variant_new_from_bytes (value_type, data, TRUE));

  if (!value) {
    g_set_error_literal (error,
                         G_IO_ERROR,
                         G_IO_ERROR_INVALID_ARGUMENT,
                         "Cannot decode GVariant from bytes");
    return FALSE;
  }

  g_variant_get_child (value, 0, "u", &saved_version);
  if (saved_version != FILTER_INFO_VARIANT_VERSION) {
    g_set_error (error,
                 G_IO_ERROR,
                 G_IO_ERROR_INVALID_DATA,
                 "Attempted to decode content filter data GVariant with"
                 " format version %" PRIu32 " (expected %" PRIu32 ")",
                 saved_version,
                 FILTER_INFO_VARIANT_VERSION);
    return FALSE;
  }

  g_variant_get (value,
                 FILTER_INFO_VARIANT_FORMAT,
                 NULL,  /* Ignore the version, it has been checked already. */
                 &source_uri,
                 &checksum,
                 &last_update);

  if (strcmp (source_uri, self->source_uri) != 0) {
    g_set_error (error,
                 G_IO_ERROR,
                 G_IO_ERROR_INVALID_DATA,
                 "Attempted to decode content filter data GVariant with"
                 " wrong filter URI <%s> (expected <%s>)",
                 source_uri,
                 self->source_uri);
    return FALSE;
  }

  /* All sanity checks passed. The "source_uri" member does not need to
   * be updated in the struct because at this point it is known to be the
   * same as in the sidecar metadata file, and the same applies to the
   * "identifier" field.
   */
  g_clear_pointer (&self->checksum, g_free);
  self->checksum = g_steal_pointer (&checksum);
  self->last_update = last_update;

  LOG ("Loaded metadata: uri=<%s>, identifier=%s, checksum=%s, last_update=%" PRIu64,
       self->source_uri,
       self->identifier,
       self->checksum,
       self->last_update);

  return TRUE;
}

static char *
filter_info_identifier_for_source_uri (const char *source_uri)
{
  g_assert (source_uri);
  return g_compute_checksum_for_string (G_CHECKSUM_SHA256, source_uri, -1);
}

static const char *
filter_info_get_identifier (FilterInfo *self)
{
  g_assert (self);
  if (!self->identifier)
    self->identifier = filter_info_identifier_for_source_uri (self->source_uri);
  return self->identifier;
}

static GFile *
filter_info_get_sidecar_file (FilterInfo *self)
{
  const char *filters_dir = ephy_filters_manager_get_adblock_filters_dir (self->manager);
  g_autofree char *sidecar_filename = g_strconcat (filter_info_get_identifier (self),
                                                   ADBLOCK_FILTER_SIDECAR_FILE_SUFFIX,
                                                   NULL);
  return g_file_new_build_filename (filters_dir, sidecar_filename, NULL);
}

static void
sidecar_bytes_loaded_cb (GFile        *file,
                         GAsyncResult *result,
                         GTask        *task)
{
  GError *error = NULL;
  FilterInfo *self = g_task_get_task_data (task);
  g_autoptr (GBytes) data = g_file_load_bytes_finish (file,
                                                      result,
                                                      NULL,  /* etag_out */
                                                      &error);
  if (data && filter_info_load_from_bytes (self, data, &error)) {
    g_task_return_boolean (task, TRUE);
  } else {
    g_task_return_error (task, error);
  }
  g_object_unref (task);
}

static void
filter_info_load_sidecar (FilterInfo          *self,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          void                *user_data)
{
  g_autoptr (GFile) sidecar_file = filter_info_get_sidecar_file (self);
  g_autofree char *sidecar_file_path = g_file_get_path (sidecar_file);
  GFileType file_type = g_file_query_file_type (sidecar_file,
                                                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                NULL);
  if (file_type != G_FILE_TYPE_REGULAR) {
    int error_code;
    const char *message;

    if (file_type == G_FILE_TYPE_UNKNOWN) {
      error_code = G_IO_ERROR_NOT_FOUND;
      message = "File not found";
    } else {
      error_code = G_IO_ERROR_NOT_REGULAR_FILE;
      message = "Not a regular file";
    }
    g_task_report_new_error (NULL,
                             callback,
                             user_data,
                             filter_info_load_sidecar,
                             G_IO_ERROR,
                             error_code,
                             "%s: %s",
                             sidecar_file_path,
                             message);
  } else {
    GTask *task = g_task_new (NULL,
                              cancellable,
                              callback,
                              user_data);
    g_autofree char *task_name = g_strconcat ("load sidecar file: ",
                                              sidecar_file_path,
                                              NULL);
    /* The FilterInfo itself as used async task data: it already contains
     * all the bits of information needed by the completion callback.
     */
    g_task_set_task_data (task, self, NULL);
    g_task_set_name (task, task_name);
    g_file_load_bytes_async (sidecar_file,
                             g_task_get_cancellable (task),
                             (GAsyncReadyCallback)sidecar_bytes_loaded_cb,
                             task);
  }
}

static gboolean
filter_info_load_sidecar_finish (GAsyncResult  *result,
                                 GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static GBytes *
filter_info_get_data_as_bytes (FilterInfo *self)
{
  g_autoptr (GVariant) value = g_variant_ref_sink (g_variant_new (FILTER_INFO_VARIANT_FORMAT,
                                                                  FILTER_INFO_VARIANT_VERSION,
                                                                  self->source_uri,
                                                                  self->checksum,
                                                                  self->last_update));
  return g_variant_get_data_as_bytes (value);
}

static void
sidecar_contents_replaced_cb (GFile        *file,
                              GAsyncResult *result,
                              GTask        *task)
{
  GError *error = NULL;
  if (g_file_replace_contents_finish (file,
                                      result,
                                      NULL,  /* new_etag */
                                      &error)) {
    g_task_return_boolean (task, TRUE);
  } else {
    g_task_return_error (task, error);
  }
}

static void
filter_info_save_sidecar (FilterInfo          *self,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          void                *user_data)
{
  g_autoptr (GBytes) data = filter_info_get_data_as_bytes (self);
  g_autoptr (GFile) sidecar_file = filter_info_get_sidecar_file (self);
  g_autofree char *sidecar_file_path = g_file_get_path (sidecar_file);
  g_autofree char *task_name = g_strconcat ("save sidecar file: ",
                                            sidecar_file_path,
                                            NULL);
  GTask *task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_name (task, task_name);

  LOG ("Saving metadata: uri=<%s>, identifier=%s, checksum=%s, last_update=%" PRIu64,
       self->source_uri,
       self->identifier,
       self->checksum,
       self->last_update);

  /* Using G_FILE_CREATE_REPLACE_DESTINATION is needed to ensure that
   * different processes trying to write the same file replace its
   * contents atomically so there is no partially written data. If two
   * processes write the same file, the one which finishes writing later
   * "wins", but that is fine because both would have downloaded the
   * same rule set and their metadata files will only have a slightly
   * different (but very close) timestamp.
   */
  g_file_replace_contents_bytes_async (sidecar_file,
                                       data,
                                       NULL,   /* etag */
                                       FALSE,  /* make_backup */
                                       G_FILE_CREATE_PRIVATE | G_FILE_CREATE_REPLACE_DESTINATION,
                                       g_task_get_cancellable (task),
                                       (GAsyncReadyCallback)sidecar_contents_replaced_cb,
                                       task);
}

static gboolean
filter_info_save_sidecar_finish (GAsyncResult  *result,
                                 GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static GFile *
filter_info_get_source_file (FilterInfo *self)
{
  /* It is possible that different Epiphany processes try to download the
   * same ruleset file simultaneously. Using different file names ensures
   * that they do not step on each other toes when writing the downloaded
   * data to disk.
   */
  g_autofree char *filename = g_strdup_printf ("%s-%" G_PID_FORMAT ".json",
                                               filter_info_get_identifier (self),
                                               getpid ());
  const char *filters_dir = ephy_filters_manager_get_adblock_filters_dir (self->manager);
  return g_file_new_build_filename (filters_dir, filename, NULL);
}

static void
filter_info_setup_enable_compiled_filter (FilterInfo              *self,
                                          WebKitUserContentFilter *wk_filter)
{
  g_assert (self);
  g_assert (wk_filter);

  LOG ("Emitting EphyFiltersManager::filter-ready for %s.", filter_info_get_identifier (self));
  g_signal_emit (self->manager, s_signals[FILTER_READY], 0, wk_filter);
}

static gboolean
filter_info_needs_updating_from_source (const FilterInfo *self)
{
  gboolean ret;

  g_assert (self);

  if (!self->manager)
    return FALSE;

  /* For local files, check whether their modification time is newer
   * than the last update time saved for it.
   */
  if (self->local) {
    g_autoptr (GDateTime) modification_time = NULL;
    g_autoptr (GError) error = NULL;
    g_autoptr (GFile) source_file = g_file_new_for_uri (self->source_uri);
    g_autoptr (GFileInfo) info = g_file_query_info (source_file,
                                                    G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                    NULL,
                                                    &error);
    if (!info) {
      g_warning ("Cannot get file modification time: %s", error->message);
      return TRUE;
    }

    modification_time = g_file_info_get_modification_date_time (info);
    return g_date_time_to_unix (modification_time) > self->last_update;
  }

  /*
   * For remote filters, check the time elapsed since the last fetch.
   *
   * Note that timestamps are signed; calculating (update time - last update)
   * can overflow. Instead find the point in time after which a filter should
   * have been last updated to be considered "recent enough" (that is, current
   * time minus the update frequency). Then a filter needs an update if its
   * last update was before that moment. Also check that the "recent enough"
   * time point can be calculated without underflowing beforehand.
   */
  if (!self->manager->metered)
    ret = (self->manager->update_time < ADBLOCK_FILTER_UPDATE_FREQUENCY) ||
          (self->last_update <= (self->manager->update_time - ADBLOCK_FILTER_UPDATE_FREQUENCY));
  else
    ret = (self->manager->update_time < ADBLOCK_FILTER_UPDATE_FREQUENCY_METERED) ||
          (self->last_update <= (self->manager->update_time - ADBLOCK_FILTER_UPDATE_FREQUENCY_METERED));

  return ret;
}

static void
file_removed_cb (GFile        *file,
                 GAsyncResult *result,
                 void         *user_data)
{
  g_autoptr (GError) error = NULL;

  g_assert (G_IS_FILE (file));
  g_assert (result);

  if (!g_file_delete_finish (file, result, &error) &&
      !g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    g_autofree char *file_path = g_file_get_path (file);
    g_warning ("Cannot delete '%s': %s", file_path, error->message);
  }
}

static void
sidecar_saved_cb (GObject      *source_object,
                  GAsyncResult *result,
                  FilterInfo   *self)
{
  g_autoptr (GError) error = NULL;
  if (filter_info_save_sidecar_finish (result, &error)) {
    LOG ("Sidecar successfully saved for filter %s.",
         filter_info_get_identifier (self));
  } else {
    g_warning ("Cannot save sidecar for filter %s: %s",
               filter_info_get_identifier (self),
               error->message);
  }
}

static void
filter_saved_cb (WebKitUserContentFilterStore *store,
                 GAsyncResult                 *result,
                 FilterInfo                   *self)
{
  g_autoptr (GError) error = NULL;

  if (!self->manager)
    return;

  g_assert (WEBKIT_IS_USER_CONTENT_FILTER_STORE (store));
  g_assert (result);
  g_assert (self);
  g_assert (self->manager->store == store);

  g_clear_pointer (&self->manager->wk_filter, webkit_user_content_filter_unref);
  self->manager->wk_filter = webkit_user_content_filter_store_save_finish (self->manager->store,
                                                                           result,
                                                                           &error);
  if (self->manager->wk_filter) {
    LOG ("Filter %s compiled successfully.", filter_info_get_identifier (self));
    filter_info_setup_enable_compiled_filter (self, self->manager->wk_filter);
    filter_info_save_sidecar (self,
                              self->manager->cancellable,
                              (GAsyncReadyCallback)sidecar_saved_cb,
                              self);
  } else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    g_warning ("Filter %s <%s> cannot be compiled: %s.",
               filter_info_get_identifier (self), self->source_uri,
               error->message);
  }

  /* In either case, setting up this filter is done. */
  filter_info_setup_done (self);
}

static void
filter_info_setup_load_file (FilterInfo *self,
                             GFile      *json_file)
{
  g_autofree char *old_checksum = NULL;
  g_autofree char *json_file_path = NULL;
  g_autoptr (GMappedFile) file_map = NULL;
  g_autoptr (GBytes) json_data = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (self);
  g_assert (G_IS_FILE (json_file));

  if (!self->manager)
    return;

  /* Some filter source JSON files can be big (tens of megabytes), so instead
   * of reading the data for compilation, just map the source file in memory.
   */
  json_file_path = g_file_get_path (json_file);
  file_map = g_mapped_file_new (json_file_path,
                                FALSE,  /* writable */
                                &error);

  /* Immediately unlink a fetched file after it has been mapped. */
  if (!self->local) {
    LOG ("Unlinking fetched JSON file: %s", json_file_path);
    g_file_delete_async (json_file,
                         G_PRIORITY_LOW,
                         self->manager->cancellable,
                         (GAsyncReadyCallback)file_removed_cb,
                         NULL);
  }

  if (!file_map) {
    g_warning ("Cannot map filter %s source file %s: %s",
               filter_info_get_identifier (self),
               json_file_path, error->message);
    filter_info_setup_done (self);
    return;
  }

  json_data = g_mapped_file_get_bytes (file_map);
  old_checksum = g_steal_pointer (&self->checksum);
  self->checksum = g_compute_checksum_for_bytes (G_CHECKSUM_SHA256, json_data);
  self->last_update = self->manager->update_time;

  if (!filter_info_needs_updating_from_source (self) && self->found &&
      old_checksum && strcmp (self->checksum, old_checksum) == 0) {
    /* Even if an update is not needed, the sidecar needs to be updated. */
    filter_info_save_sidecar (self,
                              self->manager->cancellable,
                              (GAsyncReadyCallback)sidecar_saved_cb,
                              self);
    LOG ("Filter %s not stale, source checksum unchanged (%s), recompilation skipped.",
         filter_info_get_identifier (self), self->checksum);
    filter_info_setup_done (self);
  } else {
    webkit_user_content_filter_store_save (self->manager->store,
                                           filter_info_get_identifier (self),
                                           json_data,
                                           self->manager->cancellable,
                                           (GAsyncReadyCallback)filter_saved_cb,
                                           self);
  }
}

static void
json_file_deleted (GObject      *source,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  if (!g_file_delete_finish (G_FILE (source), res, &error))
    g_warning ("Could not delete filter json file: %s", error->message);
}

typedef struct {
  EphyDownload *download;
  FilterInfo *self;
} FilterJsonInfoAsyncData;

static void
json_file_info_callback (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  FilterJsonInfoAsyncData *data = user_data;
  GFile *json_file = G_FILE (source_object);
  g_autoptr (GFileInfo) info = g_file_query_info_finish (json_file, res, &error);
  const char *content_type = NULL;

  if (info)
    content_type = g_file_info_get_content_type (info);
  else
    g_warning ("Couldn't query filter file %s: %s", ephy_download_get_destination (data->download), error->message);

  if (content_type && g_strcmp0 ("application/json", content_type) == 0) {
    filter_info_setup_load_file (data->self, json_file);
  } else {
    g_warning ("Filter source %s has invalid MIME type: %s",
               ephy_download_get_destination (data->download),
               content_type);

    g_file_delete_async (json_file, G_PRIORITY_DEFAULT, NULL, json_file_deleted, NULL);

    filter_info_setup_done (data->self);
  }

  g_object_unref (data->download);
  g_free (data);
}

static void
download_completed_cb (EphyDownload *download,
                       FilterInfo   *self)
{
  g_autoptr (GFile) json_file = NULL;
  FilterJsonInfoAsyncData *data = NULL;

  g_assert (download);
  g_assert (self);

  g_signal_handlers_disconnect_by_data (download, self);

  LOG ("Filter source %s fetched from <%s>", filter_info_get_identifier (self), self->source_uri);

  data = g_new0 (FilterJsonInfoAsyncData, 1);
  data->download = download;
  data->self = self;

  json_file = g_file_new_for_path (ephy_download_get_destination (download));
  g_file_query_info_async (json_file,
                           G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           NULL,
                           json_file_info_callback,
                           data);
}

static void
download_errored_cb (EphyDownload *download,
                     GError       *error,
                     FilterInfo   *self)
{
  g_assert (download);
  g_assert (error);
  g_assert (self);

  g_signal_handlers_disconnect_by_data (download, self);

  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("Cannot fetch source for filter %s from <%s>: %s",
               filter_info_get_identifier (self), self->source_uri,
               error ? error->message : "Unknown error");

  /* There is not much else we can do if the download failed. Note that it
   * is still possible that if a precompiled version of the filter was found
   * that may get used instead.
   */
  filter_info_setup_done (self);

  g_object_unref (download);
}

static void
filter_load_cb (WebKitUserContentFilterStore *store,
                GAsyncResult                 *result,
                FilterInfo                   *self)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) source_file = NULL;
  g_autoptr (GFile) json_file = NULL;
  EphyDownload *download;

  if (!self->manager)
    return;

  g_assert (WEBKIT_IS_USER_CONTENT_FILTER_STORE (store));
  g_assert (result);
  g_assert (self);
  g_assert (store == self->manager->store);

  g_clear_pointer (&self->manager->wk_filter, webkit_user_content_filter_unref);
  self->manager->wk_filter = webkit_user_content_filter_store_load_finish (self->manager->store,
                                                                           result,
                                                                           &error);
  self->found = !!self->manager->wk_filter;

  if (self->manager->wk_filter) {
    LOG ("Found compiled filter %s.", filter_info_get_identifier (self));
    filter_info_setup_enable_compiled_filter (self, self->manager->wk_filter);
    LOG ("Update %sneeded for filter %s (last %" PRIu64 "s ago, interval %us)",
         filter_info_needs_updating_from_source (self) ? "" : "not ",
         filter_info_get_identifier (self),
         (self->manager->update_time - self->last_update),
         self->manager->metered ? ADBLOCK_FILTER_UPDATE_FREQUENCY_METERED : ADBLOCK_FILTER_UPDATE_FREQUENCY);
  } else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    filter_info_setup_done (self);
    return;
  } else if (g_error_matches (error,
                              WEBKIT_USER_CONTENT_FILTER_ERROR,
                              WEBKIT_USER_CONTENT_FILTER_ERROR_NOT_FOUND)) {
    LOG ("Compiled filter %s not found, needs fetching.",
         filter_info_get_identifier (self));
  } else {
    g_warning ("Lookup failed for compiled filter %s: %s.",
               filter_info_get_identifier (self),
               error->message);
  }

  if (!filter_info_needs_updating_from_source (self)) {
    filter_info_setup_done (self);
    return;
  }

  /* Even if a compiled filter was found, we may need to compile an updated
   * version if the local file has changed, or the contents of remote URIs
   * have changed. If an updated ruleset is available, it will replace the
   * precompiled version found above (if any) once it has been compiled.
   */
  LOG ("Loading filter %s from <%s>", filter_info_get_identifier (self), self->source_uri);

  /* Skip fetching local file:// URIs; load them directly. */
  source_file = g_file_new_for_uri (self->source_uri);
  if ((self->local = g_file_is_native (source_file))) {
    filter_info_setup_load_file (self, source_file);
    return;
  }

  /* Download non-local URIs. */
  download = ephy_download_new_for_uri_internal (self->source_uri);

  json_file = filter_info_get_source_file (self);
  ephy_download_set_destination (download, g_file_peek_path (json_file));
  ephy_download_disable_desktop_notification (download);
  webkit_download_set_allow_overwrite (ephy_download_get_webkit_download (download), TRUE);

  g_signal_connect (download, "completed",
                    G_CALLBACK (download_completed_cb), self);
  g_signal_connect (download, "error",
                    G_CALLBACK (download_errored_cb), self);
}

static void
filter_info_setup_start (FilterInfo *self)
{
  g_assert (self);

  if (!self->manager)
    return;

  LOG ("Setup started for <%s> id=%s", self->source_uri, filter_info_get_identifier (self));

  self->done = FALSE;
  webkit_user_content_filter_store_load (self->manager->store,
                                         filter_info_get_identifier (self),
                                         self->manager->cancellable,
                                         (GAsyncReadyCallback)filter_load_cb,
                                         self);
}

static void
filters_manager_ensure_initialized (EphyFiltersManager *manager)
{
  g_assert (EPHY_IS_FILTERS_MANAGER (manager));
  if (manager->is_initialized)
    return;

  LOG ("Setting EphyFiltersManager as initialized.");
  manager->is_initialized = TRUE;
  g_object_notify_by_pspec (G_OBJECT (manager),
                            object_properties[PROP_IS_INITIALIZED]);
}

static void
accumulate_filter_done (const char *identifier,
                        FilterInfo *filter,
                        gboolean   *done)
{
  g_assert (strcmp (identifier, filter_info_get_identifier (filter)) == 0);
  g_assert (g_hash_table_contains (filter->manager->filters, identifier));

  *done = *done && filter->done;
}

static void
filter_info_setup_done (FilterInfo *self)
{
  gboolean done = self->done = TRUE;

  g_hash_table_foreach (self->manager->filters,
                        (GHFunc)accumulate_filter_done,
                        &done);

  LOG ("Setup for filter %s from <%s> completed.",
       filter_info_get_identifier (self), self->source_uri);

  if (done) {
    LOG ("Setup completed for %u filters.",
         g_hash_table_size (self->manager->filters));
    filters_manager_ensure_initialized (self->manager);
  }
}

static void
filter_removed_cb (WebKitUserContentFilterStore *store,
                   GAsyncResult                 *result,
                   void                         *user_data)
{
  g_autoptr (GError) error = NULL;

  g_assert (WEBKIT_IS_USER_CONTENT_FILTER_STORE (store));
  g_assert (result);

  if (!webkit_user_content_filter_store_remove_finish (store,
                                                       result,
                                                       &error) &&
      !g_error_matches (error,
                        WEBKIT_USER_CONTENT_FILTER_ERROR,
                        WEBKIT_USER_CONTENT_FILTER_ERROR_NOT_FOUND)) {
    g_warning ("Cannot remove compiled filter: %s", error->message);
  }
}

static void
remove_unused_filter (const char         *identifier,
                      FilterInfo         *filter,
                      EphyFiltersManager *manager)
{
  g_autoptr (GFile) sidecar_file = filter_info_get_sidecar_file (filter);

  g_assert (strcmp (identifier, filter_info_get_identifier (filter)) == 0);
  g_assert (!g_hash_table_contains (filter->manager->filters, identifier));

  LOG ("Emitting EphyFiltersManager::filter-removed for %s.", identifier);
  g_signal_emit (manager, s_signals[FILTER_REMOVED], 0, identifier);
  g_file_delete_async (sidecar_file,
                       G_PRIORITY_LOW,
                       filter->manager->cancellable,
                       (GAsyncReadyCallback)file_removed_cb,
                       NULL);
  webkit_user_content_filter_store_remove (filter->manager->store,
                                           identifier,
                                           filter->manager->cancellable,
                                           (GAsyncReadyCallback)filter_removed_cb,
                                           NULL);
  LOG ("Filter %s removal scheduled scheduled.", identifier);
}

void
sidecar_loaded_cb (GObject      *source_object,
                   GAsyncResult *result,
                   FilterInfo   *self)
{
  g_autoptr (GError) error = NULL;
  if (!filter_info_load_sidecar_finish (result, &error)) {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      return;

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
      LOG ("Sidecar missing for filter %s: %s",
           filter_info_get_identifier (self),
           error->message);
    } else {
      g_warning ("Cannot load sidecar file for filter %s: %s",
                 filter_info_get_identifier (self),
                 error->message);
    }
  }
  filter_info_setup_start (self);
}

static void
update_filters (EphyFiltersManager  *manager,
                char               **uris)
{
  const gint64 update_time = g_get_real_time () / G_USEC_PER_SEC;
  g_autoptr (GHashTable) old_filters = NULL;

  if ((!g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_ADBLOCK)) ||
      (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_AUTOMATION)) {
    LOG ("Filters are disabled, skipping update.");
    g_signal_emit (manager, s_signals[FILTERS_DISABLED], 0);
    /* If the ad blocker is disabled, initialization is done. */
    filters_manager_ensure_initialized (manager);
    return;
  }

  /* Only once at a time please! Newest set of filters wins. */
  g_cancellable_cancel (manager->cancellable);
  g_object_unref (manager->cancellable);
  manager->cancellable = g_cancellable_new ();
  manager->update_time = update_time;

  old_filters = g_steal_pointer (&manager->filters);
  manager->filters = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            NULL,
                                            (GDestroyNotify)filter_info_free);

  for (unsigned i = 0; uris[i]; i++) {
    g_autofree char *filter_id = filter_info_identifier_for_source_uri (uris[i]);
    FilterInfo *filter_info = NULL;
    char *old_filter_id = NULL;

    /* Check whether there was already a FilterInfo for the URI in the old
     * filters table, and reuse it instead of creating a new one and reloading
     * the sidecar file from disk.
     *
     * Note that the value is stolen from the old hash table in order to
     * look it up and remove it from the old table *without* destroying it.
     */
    if (g_hash_table_steal_extended (old_filters,
                                     filter_id,
                                     (void **)&old_filter_id,
                                     (void **)&filter_info)) {
      g_assert (strcmp (old_filter_id, filter_id) == 0);
      g_assert (strcmp (old_filter_id, filter_info_get_identifier (filter_info)) == 0);

      LOG ("Filter %s in old set, stolen and starting setup.", filter_id);
      filter_info_setup_start (filter_info);
    } else {
      /* Filter was not present in the old hash table: create a FilterInfo
       * for the URI and start by loading its sidecar file.
       */
      LOG ("Filter %s not in old set, creating anew.", filter_id);
      filter_info = filter_info_new (uris[i], manager);
      filter_info->identifier = g_steal_pointer (&filter_id);
      filter_info_load_sidecar (filter_info,
                                manager->cancellable,
                                (GAsyncReadyCallback)sidecar_loaded_cb,
                                filter_info);
    }

    g_hash_table_replace (manager->filters,
                          (void *)filter_info_get_identifier (filter_info),
                          filter_info);
  }

  if (!g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_COOKIE_BANNER)) {
    g_autoptr (GBytes) data = g_resources_lookup_data ("/org/gnome/epiphany/hush.json", 0, NULL);
    g_autofree char *filter_id = filter_info_identifier_for_source_uri ("org/gnome/epiphany/hush.json");
    FilterInfo *filter_info = NULL;

    filter_info = filter_info_new ("/org/gnome/epiphany/hush.json", manager);
    filter_info->identifier = g_steal_pointer (&filter_id);

    webkit_user_content_filter_store_save (manager->store,
                                           filter_info_get_identifier (filter_info),
                                           data,
                                           manager->cancellable,
                                           (GAsyncReadyCallback)filter_saved_cb,
                                           filter_info);
  }

  /* Remove the filters which are no longer in the configured set. */
  g_hash_table_foreach (old_filters,
                        (GHFunc)remove_unused_filter,
                        manager);
}

static void
update_adblock_filter_files_cb (GSettings          *settings,
                                char               *key,
                                EphyFiltersManager *manager)
{
  g_auto (GStrv) uris = NULL;

  g_assert (manager);

  uris = g_settings_get_strv (EPHY_SETTINGS_MAIN, EPHY_PREFS_CONTENT_FILTERS);
  update_filters (manager, uris);
}

static void
ephy_filters_manager_dispose (GObject *object)
{
  EphyFiltersManager *manager = EPHY_FILTERS_MANAGER (object);

  g_clear_handle_id (&manager->update_timeout_id, g_source_remove);

  if (manager->cancellable) {
    g_cancellable_cancel (manager->cancellable);
    g_clear_object (&manager->cancellable);
  }
  g_clear_pointer (&manager->wk_filter, webkit_user_content_filter_unref);
  g_clear_object (&manager->store);

  G_OBJECT_CLASS (ephy_filters_manager_parent_class)->dispose (object);
}

static void
ephy_filters_manager_finalize (GObject *object)
{
  EphyFiltersManager *manager = EPHY_FILTERS_MANAGER (object);

  g_clear_pointer (&manager->filters, g_hash_table_unref);
  g_free (manager->filters_dir);

  G_OBJECT_CLASS (ephy_filters_manager_parent_class)->finalize (object);
}

static gboolean
update_timeout_cb (EphyFiltersManager *manager)
{
  g_assert (EPHY_IS_FILTERS_MANAGER (manager));
  update_adblock_filter_files_cb (NULL, NULL, manager);
  return G_SOURCE_CONTINUE;
}

static void
on_network_metered (GNetworkMonitor    *monitor,
                    GParamSpec         *pspec,
                    EphyFiltersManager *manager)

{
  manager->metered = g_network_monitor_get_network_metered (g_network_monitor_get_default ());
}

/* URLs are provided by https://gitlab.com/eyeo/filterlists/contentblockerlists/-/tree/master  */
struct adblocker_map {
  const char *lang;
  const char *url;
} adblockers[] = {
  { "cn", "https://easylist-downloads.adblockplus.org/easylist+easylistchina-minified.json"},
  { "de", "https://easylist-downloads.adblockplus.org/easylist+easylistgermany-minified.json" },
  { "es", "https://easylist-downloads.adblockplus.org/easylist+easylistspanish-minified.json"},
  { "fr", "https://easylist-downloads.adblockplus.org/easylist+liste_fr-minified.json"},
  { "it", "https://easylist-downloads.adblockplus.org/easylist+easylistitaly-minified.json"},
  { "nl", "https://easylist-downloads.adblockplus.org/easylist+easylistdutch-minified.json"},
  { NULL, NULL }
};

#define EASYLIST_DEFAULT "https://easylist-downloads.adblockplus.org/easylist_min_content_blocker.json"

static void
setup_adblocker_list (EphyFiltersManager *self)
{
  g_auto (GStrv) languages = NULL;
  g_auto (GStrv) filters = NULL;
  g_auto (GStrv) locale_filters = NULL;
  int n_languages;

  if (g_settings_get_user_value (EPHY_SETTINGS_MAIN, EPHY_PREFS_CONTENT_FILTERS)) {
    update_adblock_filter_files_cb (NULL, NULL, self);
    return;
  }

  languages = ephy_langs_get_languages ();
  n_languages = g_strv_length (languages);

  locale_filters = g_malloc0_n (2, sizeof (char *));
  locale_filters[0] = g_strdup (EASYLIST_DEFAULT);
  locale_filters[1] = NULL;

  for (int lang = 0; lang < n_languages; lang++) {
    languages[lang] = g_strdelimit (languages[lang], "_", '\0');
    languages[lang] = g_strdelimit (languages[lang], "-", '\0');
  }

  for (int lang = 0; lang < n_languages; lang++) {
    for (int idx = 0; adblockers[idx].lang; idx++)
      if (g_ascii_strcasecmp (languages[lang], adblockers[idx].lang) == 0 && !g_strv_contains ((const char * const *)locale_filters, adblockers[idx].url)) {
        char **tmp = locale_filters;

        locale_filters = ephy_strv_append ((const char * const *)locale_filters, adblockers[idx].url);
        g_strfreev (tmp);
      }
  }

  update_filters (self, locale_filters);
}

static void
ephy_filters_manager_constructed (GObject *object)
{
  EphyFiltersManager *manager = EPHY_FILTERS_MANAGER (object);
  g_autofree char *saved_filters_dir = NULL;

  G_OBJECT_CLASS (ephy_filters_manager_parent_class)->constructed (object);

  /* Disable filter manager during tests */
  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) == EPHY_EMBED_SHELL_MODE_TEST)
    return;

  if (!manager->filters_dir) {
    g_autofree char *cache_dir = ephy_default_cache_dir ();
    manager->filters_dir = g_build_filename (cache_dir, "adblock", NULL);
  }

  saved_filters_dir = g_build_filename (manager->filters_dir, "compiled", NULL);
  g_mkdir_with_parents (saved_filters_dir, 0700);
  manager->store = webkit_user_content_filter_store_new (saved_filters_dir);

  /* Note: up here because we must connect *before* reading the settings. */
  g_signal_connect_object (EPHY_SETTINGS_MAIN, "changed::" EPHY_PREFS_CONTENT_FILTERS,
                           G_CALLBACK (update_adblock_filter_files_cb), manager, 0);
  g_signal_connect_object (EPHY_SETTINGS_WEB, "changed::" EPHY_PREFS_WEB_ENABLE_ADBLOCK,
                           G_CALLBACK (update_adblock_filter_files_cb), manager, 0);

  setup_adblocker_list (manager);

  g_signal_connect_object (g_network_monitor_get_default (), "notify::network-metered", G_CALLBACK (on_network_metered), manager, 0);
  manager->metered = g_network_monitor_get_network_metered (g_network_monitor_get_default ());

  manager->update_timeout_id = g_timeout_add_seconds (manager->metered ? ADBLOCK_FILTER_UPDATE_FREQUENCY_METERED : ADBLOCK_FILTER_UPDATE_FREQUENCY,
                                                      (GSourceFunc)update_timeout_cb,
                                                      manager);
}

static void
ephy_filters_manager_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  EphyFiltersManager *manager = EPHY_FILTERS_MANAGER (object);

  switch (prop_id) {
    case PROP_FILTERS_DIR:
      manager->filters_dir = g_value_dup_string (value);
      break;
    case PROP_IS_INITIALIZED:
      manager->is_initialized = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_filters_manager_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  EphyFiltersManager *manager = EPHY_FILTERS_MANAGER (object);

  switch (prop_id) {
    case PROP_FILTERS_DIR:
      g_value_set_string (value, manager->filters_dir);
      break;
    case PROP_IS_INITIALIZED:
      g_value_set_boolean (value, manager->is_initialized);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_filters_manager_class_init (EphyFiltersManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ephy_filters_manager_constructed;
  object_class->dispose = ephy_filters_manager_dispose;
  object_class->finalize = ephy_filters_manager_finalize;
  object_class->set_property = ephy_filters_manager_set_property;
  object_class->get_property = ephy_filters_manager_get_property;

  s_signals[FILTER_READY] =
    g_signal_new ("filter-ready",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  WEBKIT_TYPE_USER_CONTENT_FILTER);

  s_signals[FILTER_REMOVED] =
    g_signal_new ("filter-removed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);

  s_signals[FILTERS_DISABLED] =
    g_signal_new ("filters-disabled",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  object_properties[PROP_FILTERS_DIR] =
    g_param_spec_string ("filters-dir",
                         NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  object_properties[PROP_IS_INITIALIZED] =
    g_param_spec_boolean ("is-initialized",
                          NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     object_properties);
}

static void
ephy_filters_manager_init (EphyFiltersManager *manager)
{
  manager->cancellable = g_cancellable_new ();
  manager->filters = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            NULL,
                                            (GDestroyNotify)filter_info_free);
}

EphyFiltersManager *
ephy_filters_manager_new (const char *filters_dir)
{
  return EPHY_FILTERS_MANAGER (g_object_new (EPHY_TYPE_FILTERS_MANAGER,
                                             "filters-dir", filters_dir,
                                             NULL));
}

const char *
ephy_filters_manager_get_adblock_filters_dir (EphyFiltersManager *manager)
{
  return manager->filters_dir;
}

gboolean
ephy_filters_manager_get_is_initialized (EphyFiltersManager *manager)
{
  return manager->is_initialized;
}

void
ephy_filters_manager_set_ucm_forbids_ads (EphyFiltersManager       *manager,
                                          WebKitUserContentManager *ucm,
                                          gboolean                  forbids_ads)
{
  if (!manager->wk_filter)
    return;

  if (forbids_ads)
    webkit_user_content_manager_add_filter (ucm, manager->wk_filter);
  else
    webkit_user_content_manager_remove_filter (ucm, manager->wk_filter);
}
