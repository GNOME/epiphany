/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2017 Igalia S.L.
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

#include "ephy-download.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"

#include <gio/gio.h>

#include <inttypes.h>

#define ADBLOCK_FILTER_UPDATE_FREQUENCY 24 * 60 * 60 /* In seconds */

struct _EphyFiltersManager {
  GObject parent_instance;

  char *filters_dir;
  GHashTable *filters;  /* (identifier, FilterInfo) */
  GCancellable *cancellable;
  WebKitContentFilterManager *filter_manager;
};

G_DEFINE_TYPE (EphyFiltersManager, ephy_filters_manager, G_TYPE_OBJECT)

enum {
    FILTER_READY,
    FILTERS_DISABLED,
    LAST_SIGNAL,
};

static guint s_signals[LAST_SIGNAL];


typedef enum {
    SETUP_STAGE_UNINITILIZED = 0,
    SETUP_STAGE_LOOKUP,
    SETUP_STAGE_DOWNLOAD,
    SETUP_STAGE_COMPILE,
    SETUP_STAGE_DONE,
} SetupStage;

typedef struct {
    SetupStage stage;
    EphyFiltersManager *manager;
    char *source_uri;      /* Saved. */
    char *identifier;      /* Lazily derived from source_uri. */
    char *checksum;        /* Saved. */
    guint64 last_update;   /* Saved. */
    gboolean found   : 1;  /* Found during lookup. */
    gboolean enabled : 1;  /* Already enabled. */
    gboolean dirty   : 1;  /* Changes not written to disk. */
} FilterInfo;

/*
 * The "saved" fields from the struct above are stored as versioned sidecar
 * metadata files, using GVariant for serialization. An integer indicating
 * the version of the on-disk format is prepended to the data, and it must
 * be increased by 1 in the source code whenevr the GVariant format below
 * changes.
 */
#define FILTER_INFO_VARIANT_VERSION ((uint32_t) 1)
#define FILTER_INFO_VARIANT_FORMAT  "(usmst)"


static void
filter_info_free (FilterInfo *self)
{
    g_clear_pointer (&self->identifier, g_free);
    g_clear_pointer (&self->source_uri, g_free);
    g_clear_pointer (&self->checksum, g_free);
    g_free (self);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FilterInfo, filter_info_free)

static FilterInfo*
filter_info_new (const char *source_uri, EphyFiltersManager *manager)
{
    g_assert_nonnull (source_uri);
    g_assert_nonnull (manager);

    g_autoptr(FilterInfo) self = g_new0 (FilterInfo, 1);
    self->source_uri = g_strdup (source_uri);
    self->manager = manager;
    return g_steal_pointer (&self);
}

static gboolean
filter_info_load_from_bytes (FilterInfo  *self,
                             GBytes      *data,
                             GError     **error)
{
  g_assert_nonnull (self);
  g_assert_nonnull (data);

  g_autoptr(GVariantType) value_type =
    g_variant_type_new (FILTER_INFO_VARIANT_FORMAT);
  g_autoptr(GVariant) value =
    g_variant_ref_sink (g_variant_new_from_bytes (value_type, data, TRUE));

  if (!value) {
    g_set_error_literal (error,
                         G_IO_ERROR,
                         G_IO_ERROR_INVALID_ARGUMENT,
                         "Cannot decode GVariant from bytes");
    return FALSE;
  }

  uint32_t saved_version = 0;
  g_variant_get_child (value, 0, "u", &saved_version);
  if (saved_version != FILTER_INFO_VARIANT_VERSION) {
    g_set_error (error,
                 G_IO_ERROR,
                 G_IO_ERROR_INVALID_DATA,
                 "Data with format version %" PRIu32 " (expected %" PRIu32 ")",
                 saved_version,
                 FILTER_INFO_VARIANT_VERSION);
    return FALSE;
  }

  g_autofree char *source_uri = NULL;
  g_autofree char *checksum = NULL;
  guint64 last_update = 0;

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
                 "Data with wrong URI <%s> (expected <%s>)",
                 source_uri,
                 self->source_uri);
    return FALSE;
  }

  /*
   * All sanity checks passed. The "source_uri" member does not need to
   * be updated in the struct because at this point it is known to be the
   * same as in the sidecar metadata file, and the same applies to the
   * "identifier" field.
   */
  g_clear_pointer (&self->checksum, g_free);
  self->checksum = g_steal_pointer (&checksum);
  self->last_update = last_update;

  return TRUE;
}

static inline char*
filter_info_identifier_for_source_uri (const char *uri)
{
  g_assert_nonnull (uri);
  return g_compute_checksum_for_string (G_CHECKSUM_SHA256, uri, -1);
}

static const char*
filter_info_get_identifier (FilterInfo *self)
{
  g_assert_nonnull(self);
  if (!self->identifier)
    self->identifier = filter_info_identifier_for_source_uri (self->source_uri);
  return self->identifier;
}

static inline GFile*
filter_info_get_sidecar_file (FilterInfo *self)
{
  g_assert_nonnull (self);
  const char *filters_dir =
    ephy_filters_manager_get_adblock_filters_dir (self->manager);
  const char *identifier = filter_info_get_identifier (self);
  return g_file_new_build_filename (filters_dir, identifier, NULL);
}

static gboolean
filter_info_load_sidecar (FilterInfo  *self,
                          GError     **error)
{
  g_assert_nonnull (self);

  g_autoptr(GFile) sidecar_file = filter_info_get_sidecar_file (self);
  const GFileType file_type =
    g_file_query_file_type (sidecar_file,
                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                            NULL);
  if (file_type != G_FILE_TYPE_REGULAR) {
    g_autofree char *sidecar_file_path = g_file_get_path (sidecar_file);
    g_set_error (error,
                 G_IO_ERROR,
                 G_IO_ERROR_NOT_REGULAR_FILE,
                 "Not a regular file: %s",
                 sidecar_file_path);
    return FALSE;
  }

  g_autoptr(GBytes) data = g_file_load_bytes (sidecar_file,
                                              NULL,  /* cancellable */
                                              NULL,  /* etag_out */
                                              error);

  if (data && filter_info_load_from_bytes (self, data, error)) {
    /* Loaded correctly, data in memory matches that on disk. */
    self->dirty = FALSE;
    return TRUE;
  }

  return FALSE;
}

static inline GBytes*
filter_info_get_data_as_bytes (FilterInfo *self)
{
  g_assert_nonnull(self);
  g_autoptr(GVariant) value =
    g_variant_ref_sink (g_variant_new (FILTER_INFO_VARIANT_FORMAT,
                                       FILTER_INFO_VARIANT_VERSION,
                                       self->source_uri,
                                       self->checksum,
                                       self->last_update));
  return g_variant_get_data_as_bytes (value);
}

static gboolean
filter_info_save_sidecar (FilterInfo  *self,
                          GError     **error)
{
  g_assert_nonnull (self);

  if (!self->dirty)
    return TRUE;  /* Nothing to do. */

  g_autoptr(GBytes) data = filter_info_get_data_as_bytes (self);
  g_autoptr(GFile) sidecar_file = filter_info_get_sidecar_file (self);
  if (g_file_replace_contents (sidecar_file,
                               g_bytes_get_data (data, NULL),
                               g_bytes_get_size (data),
                               NULL,   /* etag */
                               FALSE,  /* make_backup */
                               G_FILE_CREATE_PRIVATE |
                               G_FILE_CREATE_REPLACE_DESTINATION,
                               NULL,   /* new_etag */
                               NULL,   /* cancellable */
                               error)) {
    self->dirty = FALSE;
    return TRUE;
  }

  return FALSE;
}

static GFile*
filter_info_get_source_file (FilterInfo *self)
{
  g_assert_nonnull (self);
  g_autofree char *filename = g_strconcat (filter_info_get_identifier (self),
                                           ".json",
                                           NULL);
  const char *filters_dir =
    ephy_filters_manager_get_adblock_filters_dir (self->manager);
  return g_file_new_build_filename (filters_dir, filename, NULL);
}

static void
filter_info_setup_enable_compiled_filter (FilterInfo              *self,
                                          WebKitUserContentFilter *wk_filter)
{
  g_assert_nonnull (self);
  g_assert_nonnull (wk_filter);

  g_debug ("Emitting EphyFiltersManager::filter-ready for %s.",
           filter_info_get_identifier (self));
  g_signal_emit (self->manager, s_signals[FILTER_READY], 0, wk_filter);

  self->found = self->enabled = TRUE;
  self->stage = SETUP_STAGE_DONE;
}

static void
file_removed_cb (GFile        *file,
                 GAsyncResult *result,
                 gpointer      user_data G_GNUC_UNUSED)
{
  g_assert (G_IS_FILE(file));
  g_assert_nonnull (result);

  g_autoptr(GError) error = NULL;
  if (!g_file_delete_finish (file, result, &error) &&
      !g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
    g_autofree char *file_path = g_file_get_path (file);
    g_warning ("Cannot delete '%s': %s", file_path, error->message);
  }
}

static void
filter_compiled_cb (WebKitContentFilterManager *filter_manager G_GNUC_UNUSED,
                    GAsyncResult               *result,
                    FilterInfo                 *self)
{
  g_assert (WEBKIT_IS_CONTENT_FILTER_MANAGER (filter_manager));
  g_assert_nonnull (result);
  g_assert_nonnull (self);
  g_assert (self->manager->filter_manager == filter_manager);

  g_autoptr(GError) error = NULL;
  g_autoptr(WebKitUserContentFilter) wk_filter =
    webkit_content_filter_manager_compile_finish (self->manager->filter_manager,
                                                  result,
                                                  &error);
  if (wk_filter) {
    g_debug ("Compiled filter %s successfully @ %p.",
             filter_info_get_identifier (self), wk_filter);
    filter_info_setup_enable_compiled_filter (self, wk_filter);
  } else {
    g_warning ("Filter %s <%s> cannot be compiled: %s.",
               filter_info_get_identifier (self), self->source_uri,
               error->message);
  }
}

static void
filter_info_setup_compile_from_file (FilterInfo *self,
                                     GFile      *json_file)
{
  g_assert_nonnull (self);
  g_assert (G_IS_FILE (json_file));
  g_assert_cmpuint (self->stage, ==, SETUP_STAGE_DOWNLOAD);

  self->stage = SETUP_STAGE_COMPILE;

  /*
   * Some filter source JSON files can be big (tens of megabytes), so instead
   * of reading the data for compilation, just map the source file in memory.
   */
  g_autoptr(GError) error = NULL;
  g_autofree char *json_file_path = g_file_get_path (json_file);
  g_autoptr(GMappedFile) file_map = g_mapped_file_new (json_file_path,
                                                       FALSE,  /* writable */
                                                       &error);

  /* Immediately unlink the file after it has been mapped. */
  g_file_delete_async (json_file,
                       G_PRIORITY_LOW,
                       NULL,  /* cancellable */
                       (GAsyncReadyCallback) file_removed_cb,
                       NULL);

  if (!file_map) {
    g_warning ("Cannot map filter %s source file %s: %s",
               filter_info_get_identifier (self),
               json_file_path, error->message);
    self->stage = SETUP_STAGE_DONE;
    return;
  }

  g_autoptr(GBytes) json_data = g_mapped_file_get_bytes (file_map);
  g_autofree char *old_checksum = g_steal_pointer (&self->checksum);
  self->checksum = g_compute_checksum_for_bytes (G_CHECKSUM_SHA256, json_data);

  /* TODO: update last_update, save metadata */

  gboolean compilation_needed = TRUE;
  if (old_checksum && strcmp (self->checksum, old_checksum) == 0) {
    g_debug ("Filter %s source checksum unchanged (%s)",
             filter_info_get_identifier (self), self->checksum);
    compilation_needed = !self->found;
  }

  if (compilation_needed) {
    webkit_content_filter_manager_compile (self->manager->filter_manager,
                                           filter_info_get_identifier (self),
                                           json_data,
                                           (GAsyncReadyCallback) filter_compiled_cb,
                                           self);
  }
}

static void
download_completed_cb (EphyDownload *download,
                       FilterInfo   *self)
{
  g_assert_nonnull (download);
  g_assert_nonnull (self);
  g_assert_cmpuint (self->stage, ==, SETUP_STAGE_DOWNLOAD);

  g_signal_handlers_disconnect_by_data (download, self);

  g_debug ("Filter source %s fetched from <%s>",
           filter_info_get_identifier (self),
           self->source_uri);

  if (g_strcmp0 ("application/json", ephy_download_get_content_type (download)) == 0) {
    g_autoptr(GFile) json_file =
      g_file_new_for_uri (ephy_download_get_destination_uri (download));
    filter_info_setup_compile_from_file (self, json_file);
  } else {
    g_warning ("Filter source %s has invalid MIME type: %s",
               ephy_download_get_destination_uri (download),
               ephy_download_get_content_type (download));
    self->stage = SETUP_STAGE_DONE;
  }

  g_autoptr(GError) error = NULL;
  if (!filter_info_save_sidecar (self, &error)) {
    g_warning ("Cannot save sidecar for filter %s: %s",
               filter_info_get_identifier (self),
               error->message);
  }

  g_object_unref (download);
}

static void
download_error_cb (EphyDownload *download,
                   GError       *error,
                   FilterInfo   *self)
{
  g_assert_nonnull (download);
  g_assert_nonnull (error);
  g_assert_nonnull (self);
  g_assert_cmpuint (self->stage, ==, SETUP_STAGE_DOWNLOAD);

  g_signal_handlers_disconnect_by_data (download, self);

  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("Cannot fetch source for filter %s from <%s>",
               filter_info_get_identifier (self), self->source_uri);

  /*
   * There is not much else we can do if the download failed. Note that it
   * is still possible that if a precompiled version of the filter was found
   * that may get used instead.
   */
  self->stage = SETUP_STAGE_DONE;

  g_debug ("Done fetching filter %s (enabled: %s)",
           filter_info_get_identifier (self),
           self->enabled ? "yes" : "no");

  g_object_unref (download);
}

static void
filter_lookup_cb (WebKitContentFilterManager *filter_manager G_GNUC_UNUSED,
                  GAsyncResult               *result,
                  FilterInfo                 *self)
{
  g_assert (WEBKIT_IS_CONTENT_FILTER_MANAGER (filter_manager));
  g_assert_nonnull (result);
  g_assert_nonnull (self);
  g_assert (filter_manager == self->manager->filter_manager);

  g_autoptr(GError) error = NULL;
  g_autoptr(WebKitUserContentFilter) wk_filter =
    webkit_content_filter_manager_lookup_finish (self->manager->filter_manager,
                                                 result,
                                                 &error);
  if (wk_filter) {
    g_debug ("Compiled filter %s found @ %p.",
             filter_info_get_identifier (self), wk_filter);
    filter_info_setup_enable_compiled_filter (self, wk_filter);
    return;
  }

  if (!g_error_matches (error,
                       WEBKIT_CONTENT_FILTER_ERROR,
                       WEBKIT_CONTENT_FILTER_ERROR_NOT_FOUND)) {
    g_warning ("Lookup failed for compiled filter %s: %s.",
               filter_info_get_identifier (self),
               error->message);
  } else {
    g_debug ("Compiled filter %s not found, needs fetching.",
             filter_info_get_identifier (self));
  }

  /*
   * Even if a compiled filter was found, for non-local filter source
   * URIs we need to fetch a updated version. If an updated version is
   * fetched, it will replace the precompiled version found above (if
   * any) once it has been compiled.
   *
   * TODO: Check for local file:// URIs and skip straight to compilation.
   */
  g_debug ("Fetching filter %s from <%s>",
           filter_info_get_identifier (self), self->source_uri);

  self->stage = SETUP_STAGE_DOWNLOAD;

  EphyDownload *download = ephy_download_new_for_uri (self->source_uri);
  g_autoptr(GFile) json_file = filter_info_get_source_file (self);
  g_autofree char *json_file_uri = g_file_get_uri (json_file);
  ephy_download_set_destination_uri (download, json_file_uri);
  ephy_download_disable_desktop_notification (download);
  webkit_download_set_allow_overwrite (ephy_download_get_webkit_download (download), TRUE);

  g_signal_connect (download, "completed",
                    G_CALLBACK (download_completed_cb), self);
  g_signal_connect (download, "error",
                    G_CALLBACK (download_error_cb), self);
}

static void
filter_info_setup_start (FilterInfo *self)
{
  g_assert_nonnull (self);
  g_assert_cmpuint (self->stage, ==, SETUP_STAGE_UNINITILIZED);

  g_debug ("Setup started for <%s> id=%s",
           self->source_uri,
           filter_info_get_identifier (self));

  self->stage = SETUP_STAGE_LOOKUP;
  webkit_content_filter_manager_lookup (self->manager->filter_manager,
                                        filter_info_get_identifier (self),
                                        (GAsyncReadyCallback) filter_lookup_cb,
                                        self);
}


enum {
  PROP_0,
  PROP_FILTERS_DIR,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

static gchar*
get_compiled_filters_dir (EphyFiltersManager *manager)
{
    return g_build_filename (manager->filters_dir, "compiled", NULL);
}

static void
filter_removed_cb (WebKitContentFilterManager *filter_manager,
                   GAsyncResult               *result,
                   gpointer                    user_data G_GNUC_UNUSED)
{
  g_assert (WEBKIT_IS_CONTENT_FILTER_MANAGER (filter_manager));
  g_assert_nonnull (result);

  g_autoptr(GError) error = NULL;
  if (!webkit_content_filter_manager_remove_finish (filter_manager,
                                                    result,
                                                    &error) &&
      !g_error_matches (error,
                        WEBKIT_CONTENT_FILTER_ERROR,
                        WEBKIT_CONTENT_FILTER_ERROR_NOT_FOUND)) {
    g_warning ("Cannot remove compiled filter: %s", error->message);
  }
}


static void
check_filter_for_removal (const char         *identifier G_GNUC_UNUSED,
                          FilterInfo         *filter,
                          EphyFiltersManager *manager)
{
  g_assert_nonnull (identifier);
  g_assert_nonnull (filter);
  g_assert_nonnull (manager);
  g_assert_cmpstr (identifier, ==, filter_info_get_identifier (filter));

  if (g_hash_table_contains (manager->filters, identifier)) {
    g_debug ("Filter %s in current filter set, not removing.", identifier);
  } else {
    g_debug ("Filter %s missing from current set, removal scheduled.", identifier);
    g_autoptr(GFile) sidecar_file = filter_info_get_sidecar_file (filter);
    g_file_delete_async (sidecar_file,
                         G_PRIORITY_LOW,
                         NULL,  /* cancellable */
                         (GAsyncReadyCallback) file_removed_cb,
                         NULL);
    webkit_content_filter_manager_remove (filter->manager->filter_manager,
                                          identifier,
                                          (GAsyncReadyCallback) filter_removed_cb,
                                          NULL);
  }
}

static void
update_adblock_filter_files_cb (GSettings          *settings G_GNUC_UNUSED,
                                char               *key G_GNUC_UNUSED,
                                EphyFiltersManager *manager)
{
  g_assert_nonnull (manager);

  g_debug ("Emitting EphyFiltersManager::filters-disabled.");
  g_signal_emit (manager, s_signals[FILTERS_DISABLED], 0);

  if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_ADBLOCK)) {
    /* Only once at a time please! Newest set of filters wins. */
    g_cancellable_cancel (manager->cancellable);
    g_object_unref (manager->cancellable);
    manager->cancellable = g_cancellable_new ();

    g_autoptr(GHashTable) old_filters = g_steal_pointer (&manager->filters);
    manager->filters = g_hash_table_new_full (g_str_hash,
                                              g_str_equal,
                                              NULL,
                                              (GDestroyNotify) filter_info_free);

    g_auto(GStrv) uris = g_settings_get_strv (EPHY_SETTINGS_MAIN,
                                              EPHY_PREFS_ADBLOCK_FILTERS);
    for (unsigned i = 0; uris[i]; i++) {
      FilterInfo *filter_info = filter_info_new (uris[i], manager);

      g_autoptr(GError) error = NULL;
      if (!filter_info_load_sidecar (filter_info, &error)) {
        g_warning ("Cannot load sidecar for filter %s: %s",
                   filter_info_get_identifier (filter_info),
                   error->message);
      }

      g_hash_table_replace (manager->filters,
                            (gpointer) filter_info_get_identifier (filter_info),
                            filter_info);
      filter_info_setup_start (filter_info);
    }

    /* Remove the filters which are no longer in the configured set. */
    g_hash_table_foreach (old_filters,
                          (GHFunc) check_filter_for_removal,
                          manager);
  }
}

static void
ephy_filters_manager_dispose (GObject *object)
{
  EphyFiltersManager *manager = EPHY_FILTERS_MANAGER (object);

  if (manager->cancellable) {
    g_cancellable_cancel (manager->cancellable);
    g_clear_object (&manager->cancellable);
  }
  g_clear_object (&manager->filter_manager);

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

static void
ephy_filters_manager_constructed (GObject *object)
{
  EphyFiltersManager *manager = EPHY_FILTERS_MANAGER (object);

  G_OBJECT_CLASS (ephy_filters_manager_parent_class)->constructed (object);

  gchar *compiled_filters_dir = get_compiled_filters_dir (manager);
  g_mkdir_with_parents (compiled_filters_dir, 0700);
  manager->filter_manager =
      webkit_content_filter_manager_new (compiled_filters_dir);
  g_clear_pointer (&compiled_filters_dir, g_free);

  /* Note: up here because we must connect *before* reading the settings. */
  g_signal_connect (EPHY_SETTINGS_MAIN, "changed::" EPHY_PREFS_ADBLOCK_FILTERS,
                    G_CALLBACK (update_adblock_filter_files_cb), manager);
  g_signal_connect (EPHY_SETTINGS_WEB, "changed::" EPHY_PREFS_WEB_ENABLE_ADBLOCK,
                    G_CALLBACK (update_adblock_filter_files_cb), manager);

  update_adblock_filter_files_cb (NULL, NULL, manager);
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

  s_signals[FILTERS_DISABLED] =
    g_signal_new ("filters-disabled",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  object_properties[PROP_FILTERS_DIR] =
    g_param_spec_string ("filters-dir",
                         "Filters directory",
                         "The directory in which adblock filters are saved",
                         "",
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

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
                                            (GDestroyNotify) filter_info_free);
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
