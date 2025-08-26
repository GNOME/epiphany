/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011 Igalia S.L.
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

#include "ephy-debug.h"
#include "ephy-download.h"
#include "ephy-embed.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-file-helpers.h"
#include "ephy-flatpak-utils.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-string.h"

#include <errno.h>
#include <glib/gi18n.h>
#include <string.h>

struct _EphyDownload {
  GObject parent_instance;

  WebKitDownload *download;
  GCancellable *cancellable;

  char *content_type;
  char *suggested_directory;
  char *suggested_filename;

  gboolean show_notification;
  gboolean always_ask_destination;
  gboolean choose_filename;

  EphyDownloadActionType action;
  gboolean finished;
  GError *error;
  GFileMonitor *file_monitor;

  guint64 uid;

  char *initiated_by_extension_id;
  char *initiated_by_extension_name;

  GDateTime *start_time;
  GDateTime *end_time;
  gboolean was_moved;
};

G_DEFINE_FINAL_TYPE (EphyDownload, ephy_download, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DOWNLOAD,
  PROP_DESTINATION,
  PROP_ACTION,
  PROP_CONTENT_TYPE,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

enum {
  FILENAME_SUGGESTED,
  ERROR,
  COMPLETED,
  MOVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static guint64 download_uid = 1;

static void
ephy_download_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  EphyDownload *download = EPHY_DOWNLOAD (object);

  switch (property_id) {
    case PROP_DOWNLOAD:
      g_value_set_object (value, ephy_download_get_webkit_download (download));
      break;
    case PROP_DESTINATION:
      g_value_set_string (value, ephy_download_get_destination (download));
      break;
    case PROP_ACTION:
      g_value_set_enum (value, ephy_download_get_action (download));
      break;
    case PROP_CONTENT_TYPE:
      g_value_set_string (value, ephy_download_get_content_type (download));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
ephy_download_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  EphyDownload *download;
  download = EPHY_DOWNLOAD (object);

  switch (property_id) {
    case PROP_DESTINATION:
      ephy_download_set_destination (download, g_value_get_string (value));
      break;
    case PROP_ACTION:
      ephy_download_set_action (download, g_value_get_enum (value));
      break;
    case PROP_DOWNLOAD:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

/**
 * ephy_download_get_content_type:
 * @download: an #EphyDownload
 *
 * Gets content-type information for @download. If the server didn't
 * provide a content type, the destination file is queried.
 *
 * Returns: content-type for @download
 **/
const char *
ephy_download_get_content_type (EphyDownload *download)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  return download->content_type;
}

/* From the old embed/mozilla/MozDownload.cpp */
static const char *
file_is_compressed (const char *filename)
{
  int i;
  static const char * const compression[] = { ".gz", ".bz2", ".Z", ".lz", ".xz", NULL };

  for (i = 0; compression[i]; i++) {
    if (g_str_has_suffix (filename, compression[i]))
      return compression[i];
  }

  return NULL;
}

static const char *
parse_extension (const char *filename)
{
  const char *compression;
  const char *last_separator;

  compression = file_is_compressed (filename);

  /* if the file is compressed we might have a double extension */
  if (compression) {
    int i;
    static const char * const extensions[] = { "tar", "ps", "xcf", "dvi", "txt", "text", NULL };

    for (i = 0; extensions[i]; i++) {
      char *suffix;
      suffix = g_strdup_printf (".%s%s", extensions[i], compression);

      if (g_str_has_suffix (filename, suffix)) {
        char *p;

        p = g_strrstr (filename, suffix);
        g_free (suffix);

        return p;
      }

      g_free (suffix);
    }
  }

  /* no compression, just look for the last dot in the filename */
  last_separator = strrchr (filename, G_DIR_SEPARATOR);
  return strrchr ((last_separator) ? last_separator : filename, '.');
}

static gboolean
set_destination_for_suggested_filename (EphyDownload *download,
                                        const char   *directory,
                                        const char   *suggested_filename)
{
  char *dest_dir;
  char *dest_name;
  g_autofree char *destination_filename = NULL;

  if (directory)
    dest_dir = g_strdup (directory);
  else
    dest_dir = ephy_file_get_downloads_dir ();

  /* Make sure the download directory exists */
  if (g_mkdir_with_parents (dest_dir, 0700) == -1) {
    g_warning ("Could not create downloads directory \"%s\": %s",
               dest_dir, strerror (errno));
    g_free (dest_dir);
    return FALSE;
  }

  if (suggested_filename) {
    dest_name = ephy_sanitize_filename (g_strdup (suggested_filename));
  } else {
    dest_name = ephy_file_tmp_filename (".ephy-download-XXXXXX", NULL);
  }

  destination_filename = g_build_filename (dest_dir, dest_name, NULL);
  g_free (dest_dir);
  g_free (dest_name);

  if (strlen (destination_filename) > NAME_MAX) {
    g_autofree char *truncated_filename = g_utf8_substring (destination_filename, 0,
                                                            g_utf8_strlen (destination_filename, NAME_MAX));
    g_free (destination_filename);
    destination_filename = g_steal_pointer (&truncated_filename);
  }

  /* Append (n) as needed. */
  if (!webkit_download_get_allow_overwrite (download->download) && g_file_test (destination_filename, G_FILE_TEST_EXISTS)) {
    int i = 1;
    const char *dot_pos;
    gssize position;
    char *serial = NULL;
    GString *tmp_filename;

    dot_pos = parse_extension (destination_filename);
    if (dot_pos)
      position = dot_pos - destination_filename;
    else
      position = strlen (destination_filename);

    tmp_filename = g_string_new (NULL);

    do {
      serial = g_strdup_printf ("(%d)", i++);

      g_string_assign (tmp_filename, destination_filename);
      g_string_insert (tmp_filename, position, serial);

      g_free (serial);
    } while (g_file_test (tmp_filename->str, G_FILE_TEST_EXISTS));

    g_free (destination_filename);
    destination_filename = g_strdup (tmp_filename->str);
    g_string_free (tmp_filename, TRUE);
  }

  webkit_download_set_destination (download->download, destination_filename);

  return TRUE;
}

/**
 * ephy_download_set_destination:
 * @download: an #EphyDownload
 * @destination: path at which to save @download
 *
 * Sets the destination of @download.
 **/
void
ephy_download_set_destination (EphyDownload *download,
                               const char   *destination)
{
  g_assert (EPHY_IS_DOWNLOAD (download));
  g_assert (destination);

  webkit_download_set_destination (download->download, destination);
  g_object_notify_by_pspec (G_OBJECT (download), obj_properties[PROP_DESTINATION]);
}

/**
 * ephy_download_set_action:
 * @download: an #EphyDownload
 * @action: #EphyDownloadActionType to execute
 *
 * Sets the @action to be executed when ephy_download_do_download_action () is
 * called on @download or on finish when "Automatically download and open
 * files" is set.
 **/
void
ephy_download_set_action (EphyDownload           *download,
                          EphyDownloadActionType  action)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  download->action = action;
  g_object_notify_by_pspec (G_OBJECT (download), obj_properties[PROP_ACTION]);
}

/**
 * ephy_download_get_webkit_download:
 * @download: an #EphyDownload
 *
 * Gets the #WebKitDownload being wrapped by @download.
 *
 * Returns: (transfer none): a #WebKitDownload.
 **/
WebKitDownload *
ephy_download_get_webkit_download (EphyDownload *download)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  return download->download;
}

/**
 * ephy_download_get_destination:
 * @download: an #EphyDownload
 *
 * Gets the destination where the download is being saved.
 *
 * Returns: (transfer none): destination path
 **/
const char *
ephy_download_get_destination (EphyDownload *download)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  return webkit_download_get_destination (download->download);
}

/**
 * ephy_download_get_action:
 * @download: an #EphyDownload
 *
 * Gets the #EphyDownloadActionType that this download will execute when
 * ephy_download_do_download_action () is called on it. This action is
 * performed automatically is "Automatically download and open files" is
 * enabled.
 *
 * Returns: the #EphyDownloadActionType to be executed
 **/
EphyDownloadActionType
ephy_download_get_action (EphyDownload *download)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  return download->action;
}

/**
 * ephy_download_cancel:
 * @download: an #EphyDownload
 *
 * Cancels the wrapped #WebKitDownload.
 **/
void
ephy_download_cancel (EphyDownload *download)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  webkit_download_cancel (download->download);
}

gboolean
ephy_download_is_active (EphyDownload *download)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  return !download->finished;
}

gboolean
ephy_download_succeeded (EphyDownload *download)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  return download->finished && !download->error;
}

gboolean
ephy_download_failed (EphyDownload  *download,
                      GError       **error)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  if (download->finished && download->error) {
    if (error)
      *error = download->error;
    return TRUE;
  }

  return FALSE;
}

/**
 * ephy_download_do_download_action:
 * @download: an #EphyDownload
 * @action: one of #EphyDownloadActionType
 *
 * Executes the given @action for @download, this can be any of
 * #EphyDownloadActionType.
 *
 * Returns: %TRUE if the action was executed successfully.
 *
 **/
gboolean
ephy_download_do_download_action (EphyDownload           *download,
                                  EphyDownloadActionType  action)
{
  GFile *destination;
  const char *destination_path;
  gboolean ret = FALSE;

  destination_path = webkit_download_get_destination (download->download);
  destination = g_file_new_for_path (destination_path);

  switch ((action ? action : download->action)) {
    case EPHY_DOWNLOAD_ACTION_BROWSE_TO:
      LOG ("ephy_download_do_download_action: browse_to");
      ret = ephy_file_browse_to (destination, NULL);
      break;
    case EPHY_DOWNLOAD_ACTION_OPEN:
      LOG ("ephy_download_do_download_action: open");
      ret = ephy_file_launch_uri_handler (destination, NULL, NULL, EPHY_FILE_LAUNCH_URI_HANDLER_FILE);
      if (!ret)
        ret = ephy_file_browse_to (destination, NULL);
      break;
    case EPHY_DOWNLOAD_ACTION_NONE:
      LOG ("ephy_download_do_download_action: none");
      ret = TRUE;
      break;
    default:
      g_assert_not_reached ();
  }
  g_object_unref (destination);

  return ret;
}

static void
ephy_download_dispose (GObject *object)
{
  EphyDownload *download = EPHY_DOWNLOAD (object);

  LOG ("EphyDownload disposed %p", object);

  if (download->download) {
    g_signal_handlers_disconnect_matched (download->download, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, download);
    g_object_unref (download->download);
    download->download = NULL;
  }

  g_cancellable_cancel (download->cancellable);
  g_clear_object (&download->cancellable);

  g_clear_object (&download->file_monitor);
  g_clear_error (&download->error);
  g_clear_pointer (&download->content_type, g_free);
  g_clear_pointer (&download->suggested_filename, g_free);
  g_clear_pointer (&download->suggested_directory, g_free);
  g_clear_pointer (&download->start_time, g_date_time_unref);
  g_clear_pointer (&download->end_time, g_date_time_unref);
  g_clear_pointer (&download->initiated_by_extension_id, g_free);
  g_clear_pointer (&download->initiated_by_extension_name, g_free);

  G_OBJECT_CLASS (ephy_download_parent_class)->dispose (object);
}

static void
ephy_download_class_init (EphyDownloadClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ephy_download_get_property;
  object_class->set_property = ephy_download_set_property;
  object_class->dispose = ephy_download_dispose;

  /**
   * EphyDownload::download:
   *
   * Internal WebKitDownload.
   */
  obj_properties[PROP_DOWNLOAD] =
    g_param_spec_object ("download",
                         NULL, NULL,
                         WEBKIT_TYPE_DOWNLOAD,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS);

  /**
   * EphyDownload::destination:
   *
   * The destination URI where to store the download.
   */
  obj_properties[PROP_DESTINATION] =
    g_param_spec_string ("destination",
                         NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  /**
   * EphyDownload::action:
   *
   * Action to take when the download finishes or when
   * ephy_download_do_download_action () is called.
   */
  obj_properties[PROP_ACTION] =
    g_param_spec_enum ("action",
                       NULL, NULL,
                       EPHY_TYPE_DOWNLOAD_ACTION_TYPE,
                       EPHY_DOWNLOAD_ACTION_NONE,
                       G_PARAM_READABLE |
                       G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_CONTENT_TYPE] =
    g_param_spec_string ("content-type",
                         NULL, NULL,
                         NULL,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  /**
   * EphyDownload::filename-suggested:
   *
   * The ::filename-suggested signal is emitted when we have received the
   * suggested filename from WebKit. Return %TRUE if you will provide a
   * destination or will %FALSE otherwise. If the destination is not
   * provided before the signal handler returns, the download will not
   * start until provided.
   **/
  signals[FILENAME_SUGGESTED] = g_signal_new ("filename-suggested",
                                              G_OBJECT_CLASS_TYPE (object_class),
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              g_signal_accumulator_true_handled,
                                              NULL, NULL,
                                              G_TYPE_BOOLEAN,
                                              1,
                                              G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * EphyDownload::completed:
   *
   * The ::completed signal is emitted when @download has finished downloading.
   **/
  signals[COMPLETED] = g_signal_new ("completed",
                                     G_OBJECT_CLASS_TYPE (object_class),
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL, NULL,
                                     G_TYPE_NONE,
                                     0);

  /**
   * EphyDownload::moved:
   *
   * The ::moved signal is emitted when a finished @download has been moved (or deleted).
   **/
  signals[MOVED] = g_signal_new ("moved",
                                 G_OBJECT_CLASS_TYPE (object_class),
                                 G_SIGNAL_RUN_LAST,
                                 0,
                                 NULL, NULL, NULL,
                                 G_TYPE_NONE,
                                 0);
  /**
   * EphyDownload::error:
   *
   * The ::error signal wraps the @download ::error signal.
   **/
  signals[ERROR] = g_signal_new ("error",
                                 G_OBJECT_CLASS_TYPE (object_class),
                                 G_SIGNAL_RUN_LAST,
                                 0,
                                 NULL, NULL, NULL,
                                 G_TYPE_NONE,
                                 1, G_TYPE_POINTER);
}

static void
ephy_download_init (EphyDownload *download)
{
  LOG ("EphyDownload initialising %p", download);

  download->download = NULL;
  download->cancellable = g_cancellable_new ();

  download->action = EPHY_DOWNLOAD_ACTION_NONE;

  download->show_notification = TRUE;

  download->uid = download_uid++;
}

typedef struct {
  EphyDownload *download;
  char *suggested_filename;
  AdwDialog *dialog;
  GFile *directory;
  GtkLabel *directory_label;
  gboolean choose_filename;
} SuggestedFilenameData;

static SuggestedFilenameData *
suggested_filename_data_new (EphyDownload *download,
                             const char   *suggested_filename,
                             AdwDialog    *dialog,
                             GFile        *directory,
                             GtkLabel     *directory_label,
                             gboolean      choose_filename)
{
  SuggestedFilenameData *data = g_new (SuggestedFilenameData, 1);

  data->download = g_object_ref (download);
  data->suggested_filename = g_strdup (suggested_filename);
  data->dialog = dialog;
  data->directory = g_object_ref (directory);
  data->directory_label = directory_label;
  data->choose_filename = choose_filename;

  return data;
}

static void
suggested_filename_data_free (SuggestedFilenameData *data)
{
  g_object_unref (data->download);
  g_object_unref (data->directory);
  g_free (data->suggested_filename);
  g_free (data);
}

static void
filename_suggested_dialog_cb (AdwAlertDialog        *dialog,
                              const char            *response,
                              SuggestedFilenameData *data)
{
  if (!strcmp (response, "download")) {
    g_autofree gchar *directory = g_file_get_path (data->directory);
    WebKitDownload *webkit_download = ephy_download_get_webkit_download (data->download);

    set_destination_for_suggested_filename (data->download, directory, data->suggested_filename);

    webkit_download_set_allow_overwrite (webkit_download, TRUE);

    ephy_downloads_manager_add_download (ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ()),
                                         data->download);

    g_settings_set_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY, directory);
  } else {
    ephy_download_cancel (data->download);
  }

  suggested_filename_data_free (data);
}

static void
filename_suggested_file_dialog_cb (GtkFileDialog         *dialog,
                                   GAsyncResult          *result,
                                   SuggestedFilenameData *data)
{
  g_autoptr (GFile) file = NULL;
  g_autofree char *display_name = NULL;
  g_autoptr (GError) error = NULL;

  if (!data->choose_filename)
    file = gtk_file_dialog_select_folder_finish (dialog, result, &error);
  else
    file = gtk_file_dialog_save_finish (dialog, result, &error);

  if (!file) {
    g_warning ("Failed to select download destination: %s", error->message);
    return;
  }

  g_set_object (&data->directory, file);

  display_name = ephy_file_get_display_name (data->directory);
  gtk_label_set_label (data->directory_label, display_name);
}

static void
filename_suggested_button_cb (GtkButton             *button,
                              SuggestedFilenameData *data)
{
  GtkFileDialog *dialog = gtk_file_dialog_new ();

  gtk_file_dialog_set_initial_folder (dialog, data->directory);

  if (!data->choose_filename) {
    gtk_file_dialog_set_title (dialog, _("Select a Directory"));

    gtk_file_dialog_select_folder (dialog,
                                   GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (data->dialog))),
                                   data->download->cancellable,
                                   (GAsyncReadyCallback)filename_suggested_file_dialog_cb,
                                   data);
  } else {
    gtk_file_dialog_set_title (dialog, _("Select the Destination"));
    gtk_file_dialog_set_initial_name (dialog, data->suggested_filename);

    gtk_file_dialog_save (dialog,
                          GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (data->dialog))),
                          data->download->cancellable,
                          (GAsyncReadyCallback)filename_suggested_file_dialog_cb,
                          data);
  }
}

static void
open_download_confirmation_dialog (EphyDownload *download,
                                   const char   *suggested_filename)
{
  GApplication *application;
  AdwDialog *dialog = NULL;
  GtkWidget *grid;
  GtkWindow *toplevel;
  GtkWidget *icon;
  GtkWidget *name_label;
  GtkWidget *type_label;
  GtkWidget *source_label;
  GtkWidget *question_label;
  GtkWidget *button;
  GtkWidget *button_box;
  GtkWidget *button_label;
  WebKitDownload *webkit_download;
  WebKitURIResponse *response;
  g_autofree gchar *type_text = NULL;
  g_autofree gchar *source_text = NULL;
  g_autofree gchar *content_length = NULL;
  g_autofree gchar *display_name = NULL;
  g_autoptr (GIcon) gicon = NULL;
  g_autoptr (GFile) directory = NULL;
  const gchar *content_type;
  const char *directory_path;
  SuggestedFilenameData *data;

  application = G_APPLICATION (ephy_embed_shell_get_default ());
  toplevel = gtk_application_get_active_window (GTK_APPLICATION (application));

  dialog = adw_alert_dialog_new (_("Download Requested"), NULL);
  adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
                                  "cancel", _("_Cancel"),
                                  "download", _("_Download"),
                                  NULL);

  webkit_download = ephy_download_get_webkit_download (download);
  response = webkit_download_get_response (webkit_download);

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);
  gtk_widget_set_margin_top (grid, 6);
  adw_alert_dialog_set_extra_child (ADW_ALERT_DIALOG (dialog), grid);

  content_length = g_format_size (webkit_uri_response_get_content_length (response));
  content_type = ephy_download_get_content_type (download);

  /* Icon */
  gicon = g_content_type_get_symbolic_icon (content_type);
  icon = gtk_image_new_from_gicon (gicon);
  gtk_image_set_pixel_size (GTK_IMAGE (icon), 64);
  gtk_grid_attach (GTK_GRID (grid), icon, 0, 0, 1, 3);

  /* Name */
  name_label = gtk_label_new (suggested_filename);
  gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (name_label), 0);
  gtk_widget_add_css_class (name_label, "heading");
  gtk_grid_attach (GTK_GRID (grid), name_label, 1, 0, 1, 1);

  /* Type */
  type_text = g_strdup_printf (_("Type: %s (%s)"), g_content_type_get_description (content_type), content_length);
  type_label = gtk_label_new (type_text);
  gtk_label_set_xalign (GTK_LABEL (type_label), 0);
  gtk_grid_attach (GTK_GRID (grid), type_label, 1, 1, 1, 1);

  /* Source */
  source_text = g_strdup_printf (_("Source: %s"), ephy_string_get_host_name (webkit_uri_response_get_uri (response)));
  source_label = gtk_label_new (source_text);
  gtk_label_set_xalign (GTK_LABEL (source_label), 0);
  gtk_grid_attach (GTK_GRID (grid), source_label, 1, 2, 1, 1);

  /* Question */
  question_label = gtk_label_new (_("Where do you want to save the file?"));
  gtk_widget_set_margin_top (question_label, 18);
  gtk_widget_set_margin_bottom (question_label, 6);
  gtk_grid_attach (GTK_GRID (grid), question_label, 0, 3, 2, 1);

  /* File Chooser Button */
  button = gtk_button_new ();
  gtk_grid_attach (GTK_GRID (grid), button, 0, 4, 2, 1);

  button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_hexpand (button_box, FALSE);
  gtk_button_set_child (GTK_BUTTON (button), button_box);

  gtk_box_append (GTK_BOX (button_box),
                  gtk_image_new_from_icon_name ("folder-symbolic"));

  button_label = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (button_label), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (button_label), 0);
  gtk_widget_set_hexpand (button_label, TRUE);
  gtk_box_append (GTK_BOX (button_box), button_label);

  directory_path = g_settings_get_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY);
  if (download->suggested_directory)
    directory = g_file_new_for_path (download->suggested_directory);
  else if (!directory_path || !directory_path[0])
    directory = g_file_new_for_path (ephy_file_get_downloads_dir ());
  else
    directory = g_file_new_for_path (directory_path);

  display_name = ephy_file_get_display_name (directory);
  gtk_label_set_label (GTK_LABEL (button_label), display_name);

  data = suggested_filename_data_new (download,
                                      suggested_filename,
                                      dialog,
                                      directory,
                                      GTK_LABEL (button_label),
                                      download->choose_filename);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (filename_suggested_button_cb), data);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (filename_suggested_dialog_cb), data);

  adw_dialog_present (dialog, GTK_WIDGET (toplevel));
}

static void
download_response_changed_cb (WebKitDownload *wk_download,
                              GParamSpec     *spec,
                              EphyDownload   *download)
{
  WebKitURIResponse *response;
  const char *mime_type;

  response = webkit_download_get_response (download->download);
  mime_type = webkit_uri_response_get_mime_type (response);
  if (!mime_type)
    return;

  download->content_type = g_content_type_from_mime_type (mime_type);
  if (download->content_type)
    g_object_notify_by_pspec (G_OBJECT (download), obj_properties[PROP_CONTENT_TYPE]);
}

static gboolean
download_decide_destination_cb (WebKitDownload *wk_download,
                                const gchar    *wk_suggestion,
                                EphyDownload   *download)
{
  const char *suggested_filename = wk_suggestion;
  gboolean will_provide_destination = FALSE;

  if (download->suggested_filename)
    suggested_filename = download->suggested_filename;

  if (webkit_download_get_destination (wk_download))
    return TRUE;

  g_signal_emit (download, signals[FILENAME_SUGGESTED], 0, suggested_filename, &will_provide_destination);

  if (will_provide_destination)
    return TRUE;

  if (!ephy_is_running_inside_sandbox () &&
      (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ASK_ON_DOWNLOAD) ||
       download->always_ask_destination)) {
    open_download_confirmation_dialog (download, suggested_filename);
    return TRUE;
  }

  return set_destination_for_suggested_filename (download, download->suggested_directory, suggested_filename);
}

static void
download_created_destination_cb (WebKitDownload *wk_download,
                                 const char     *destination,
                                 EphyDownload   *download)
{
  char *filename;
  char *content_type;

  download->start_time = g_date_time_new_now_local ();

  if (download->content_type && !g_content_type_is_unknown (download->content_type))
    return;

  /* The server didn't provide a valid content type, let's try to guess it from the
   * destination filename. We use g_content_type_guess() here instead of g_file_query_info(),
   * because we are only using the filename to guess the content type, since it doesn't make
   * sense to sniff the destination URI that will be empty until the download is completed.
   * We can't use g_file_query_info() with the partial download file either, because it will
   * always return application/x-partial-download based on the .wkdownload extension.
   */
  filename = g_filename_from_uri (destination, NULL, NULL);
  if (!filename)
    return;

  content_type = g_content_type_guess (filename, NULL, 0, NULL);
  g_free (filename);

  if (g_content_type_is_unknown (content_type)) {
    /* We could try to connect to received-data signal and sniff the contents when we have
     * enough data written in the file, but I don't think it's worth it.
     */
    g_free (content_type);
    return;
  }

  if (!download->content_type ||
      !g_content_type_equals (download->content_type, content_type)) {
    g_free (download->content_type);
    download->content_type = content_type;
    g_object_notify_by_pspec (G_OBJECT (download), obj_properties[PROP_CONTENT_TYPE]);
    return;
  }

  g_free (content_type);
}

static void
display_download_finished_notification (WebKitDownload *download)
{
  GApplication *application;
  GtkWindow *toplevel;
  const char *dest;

  application = G_APPLICATION (ephy_embed_shell_get_default ());
  toplevel = gtk_application_get_active_window (GTK_APPLICATION (application));
  dest = webkit_download_get_destination (download);

  if (!gtk_window_is_active (toplevel) && dest) {
    char *filename;
    char *message;
    GNotification *notification;

    filename = g_filename_display_basename (dest);
    /* Translators: a desktop notification when a download finishes. */
    message = g_strdup_printf (_("Finished downloading %s"), filename);
    /* Translators: the title of the notification. */
    notification = g_notification_new (_("Download finished"));
    g_notification_set_body (notification, message);
    g_application_send_notification (application, "download-finished", notification);

    g_free (filename);
    g_free (message);
    g_object_unref (notification);
  }
}

static void
download_file_monitor_changed (GFileMonitor      *monitor,
                               GFile             *file,
                               GFile             *other_file,
                               GFileMonitorEvent  event_type,
                               EphyDownload      *download)
{
  /* Skip messages for <file>.wkdownload */
  if (strcmp (g_file_peek_path (file), webkit_download_get_destination (download->download)) != 0)
    return;

  download->was_moved = TRUE;

  if (event_type == G_FILE_MONITOR_EVENT_DELETED || event_type == G_FILE_MONITOR_EVENT_MOVED)
    g_signal_emit (download, signals[MOVED], 0);
}

static void
download_finished_cb (WebKitDownload *wk_download,
                      EphyDownload   *download)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;

  download->finished = TRUE;
  download->end_time = g_date_time_new_now_local ();

  ephy_download_do_download_action (download, download->action);

  if (download->show_notification)
    display_download_finished_notification (wk_download);

  g_signal_emit (download, signals[COMPLETED], 0);

  file = g_file_new_for_path (webkit_download_get_destination (wk_download));
  download->file_monitor = g_file_monitor (file, G_FILE_MONITOR_NONE, NULL, &error);
  if (!download->file_monitor)
    g_warning ("Could not add a file monitor for %s, error: %s", g_file_get_uri (file), error->message);
  else
    g_signal_connect_object (download->file_monitor, "changed", G_CALLBACK (download_file_monitor_changed), download, 0);
}

static void
download_failed_cb (WebKitDownload *wk_download,
                    GError         *error,
                    EphyDownload   *download)
{
  g_signal_handlers_disconnect_by_func (wk_download, download_finished_cb, download);

  LOG ("error (%d - %d)! %s", error->code, 0, error->message);
  download->finished = TRUE;
  download->end_time = g_date_time_new_now_local ();
  download->error = g_error_copy (error);
  g_signal_emit (download, signals[ERROR], 0, download->error);
}

EphyDownload *
ephy_download_new_internal (WebKitDownload *download)
{
  EphyDownload *ephy_download;

  g_assert (WEBKIT_IS_DOWNLOAD (download));

  ephy_download = g_object_new (EPHY_TYPE_DOWNLOAD, NULL);

  g_signal_connect_object (download, "notify::response",
                           G_CALLBACK (download_response_changed_cb),
                           ephy_download, 0);
  g_signal_connect_object (download, "created-destination",
                           G_CALLBACK (download_created_destination_cb),
                           ephy_download, 0);
  g_signal_connect_object (download, "finished",
                           G_CALLBACK (download_finished_cb),
                           ephy_download, 0);
  g_signal_connect_object (download, "failed",
                           G_CALLBACK (download_failed_cb),
                           ephy_download, 0);

  ephy_download->download = g_object_ref (download);
  g_object_set_data (G_OBJECT (download), "ephy-download-set", GINT_TO_POINTER (TRUE));

  return ephy_download;
}

/**
 * ephy_download_new:
 * @download: a #WebKitDownload to wrap
 *
 * Wraps @download in an #EphyDownload.
 *
 * Returns: an #EphyDownload.
 **/
EphyDownload *
ephy_download_new (WebKitDownload *download)
{
  EphyDownload *ephy_download;

  ephy_download = ephy_download_new_internal (download);

  g_signal_connect_object (download, "decide-destination",
                           G_CALLBACK (download_decide_destination_cb),
                           ephy_download, 0);

  return ephy_download;
}

/**
 * ephy_download_new_for_uri:
 * @uri: a source URI from where to download
 *
 * Creates an #EphyDownload to download @uri.
 *
 * Returns: an #EphyDownload.
 **/
EphyDownload *
ephy_download_new_for_uri (const char *uri)
{
  EphyDownload *ephy_download;
  WebKitDownload *download;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  g_assert (uri);

  download = webkit_network_session_download_uri (ephy_embed_shell_get_network_session (shell), uri);
  ephy_download = ephy_download_new (download);
  g_object_unref (download);

  return ephy_download;
}

EphyDownload *
ephy_download_new_for_uri_internal (const char *uri)
{
  EphyDownload *ephy_download;
  g_autoptr (WebKitDownload) download = NULL;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  g_assert (uri);

  download = webkit_network_session_download_uri (ephy_embed_shell_get_network_session (shell), uri);
  ephy_download = ephy_download_new_internal (download);

  return ephy_download;
}

void
ephy_download_disable_desktop_notification (EphyDownload *download)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  download->show_notification = FALSE;
}

guint64
ephy_download_get_uid (EphyDownload *download)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  return download->uid;
}

/**
 * ephy_download_set_always_ask_destination:
 *
 * Bypasses the global user preference for prompting for
 * a save location. This does not bypass EphyDownload:destination
 * being set.
 */
void
ephy_download_set_always_ask_destination (EphyDownload *download,
                                          gboolean      always_ask)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  download->always_ask_destination = always_ask;
}

/**
 * ephy_download_set_choose_filename:
 *
 * Changes the download prompt to select the destination
 * filename rather than only the directory.
 */
void
ephy_download_set_choose_filename (EphyDownload *download,
                                   gboolean      choose_filename)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  download->choose_filename = choose_filename;
}

/**
 * ephy_download_set_suggested_destination:
 * @suggested_directory: (nullable): Default download directory
 * @suggested_filename: (nullable): Default filename
 *
 * This sets recommendations for the directory and filename of
 * the download. If the directory does not exist it will be created.
 * The filename will be sanitized and possibly renamed if needed.
 *
 * If @suggested_directory is %NULL the globally configured download
 * directory is used.
 *
 * If @suggested_filename is %NULL the WebKit recommended filename
 * is used.
 *
 * Note that this does not override EphyDownload:destination and only
 * provides default suggestions.
 */
void
ephy_download_set_suggested_destination (EphyDownload *download,
                                         const char   *suggested_directory,
                                         const char   *suggested_filename)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  g_free (download->suggested_directory);
  download->suggested_directory = g_strdup (suggested_directory);

  g_free (download->suggested_filename);
  download->suggested_filename = g_strdup (suggested_filename);
}

/**
 * ephy_download_set_allow_overwrite:
 *
 * This allows the downloaded file to overwrite files on disk and
 * also disables the automatic renaming (appending "(1)") when the file
 * already exists.
 */
void
ephy_download_set_allow_overwrite (EphyDownload *download,
                                   gboolean      allow_overwrite)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  webkit_download_set_allow_overwrite (download->download, allow_overwrite);
}

/**
 * ephy_download_get_was_moved:
 *
 * Returns: %TRUE if Epiphany detected the file being moved or deleted
 */
gboolean
ephy_download_get_was_moved (EphyDownload *download)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  return download->was_moved;
}

/**
 * ephy_download_get_start_time:
 *
 * Returns: (nullable): The time the download was started or %NULL
 */
GDateTime *
ephy_download_get_start_time (EphyDownload *download)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  return download->start_time;
}

/**
 * ephy_download_get_end_time:
 *
 * Returns: (nullable): The time the download was completed/failed or %NULL if active
 */
GDateTime *
ephy_download_get_end_time (EphyDownload *download)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  return download->end_time;
}

/**
 * ephy_download_get_initiating_web_extension_info:
 * @download: The #EphyDownload
 * @extension_id_out: (nullable): Place to store the extension ID
 * @extension_name_out: (nullable): Place to store the extension name
 *
 * This returns information on which, if any, WebExtension created the download.
 *
 * Returns: %TRUE if web extension info exists
 */
gboolean
ephy_download_get_initiating_web_extension_info (EphyDownload  *download,
                                                 const char   **extension_id_out,
                                                 const char   **extension_name_out)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  if (extension_name_out)
    *extension_name_out = download->initiated_by_extension_name ? download->initiated_by_extension_name : NULL;

  if (extension_id_out)
    *extension_id_out = download->initiated_by_extension_id ? download->initiated_by_extension_id : NULL;

  return download->initiated_by_extension_name || download->initiated_by_extension_id;
}

/**
 * ephy_download_set_initiating_web_extension_info:
 * @download: The #EphyDownload
 * @extension_id: (nullable): Extension ID
 * @extension_name: (nullable): Extension name
 *
 * This sets that @download was created by a WebExtension. This information is exposed to
 * other WebExtensions.
 */
void
ephy_download_set_initiating_web_extension_info (EphyDownload *download,
                                                 const char   *extension_id,
                                                 const char   *extension_name)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  g_free (download->initiated_by_extension_name);
  download->initiated_by_extension_name = g_strdup (extension_name);

  g_free (download->initiated_by_extension_id);
  download->initiated_by_extension_id = g_strdup (extension_id);
}
