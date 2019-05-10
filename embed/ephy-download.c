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
#include "ephy-file-chooser.h"
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

  char *destination;
  char *content_type;

  gboolean show_notification;

  EphyDownloadActionType action;
  guint32 start_time;
  gboolean finished;
  GError *error;
  GFileMonitor *file_monitor;
};

G_DEFINE_TYPE (EphyDownload, ephy_download, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DOWNLOAD,
  PROP_DESTINATION,
  PROP_ACTION,
  PROP_START_TIME,
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
      g_value_set_string (value, ephy_download_get_destination_uri (download));
      break;
    case PROP_ACTION:
      g_value_set_enum (value, ephy_download_get_action (download));
      break;
    case PROP_START_TIME:
      g_value_set_uint (value, ephy_download_get_start_time (download));
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
      ephy_download_set_destination_uri (download, g_value_get_string (value));
      break;
    case PROP_ACTION:
      ephy_download_set_action (download, g_value_get_enum (value));
      break;
    case PROP_DOWNLOAD:
    case PROP_START_TIME:
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

  for (i = 0; compression[i] != NULL; i++) {
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
  if (compression != NULL) {
    int i;
    static const char * const extensions[] = { "tar", "ps", "xcf", "dvi", "txt", "text", NULL };

    for (i = 0; extensions[i] != NULL; i++) {
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
set_destination_uri_for_suggested_filename (EphyDownload *download, const char *suggested_filename)
{
  char *dest_dir;
  char *dest_name;
  char *destination_filename;
  char *destination_uri;

  dest_dir = ephy_file_get_downloads_dir ();

  /* Make sure the download directory exists */
  if (g_mkdir_with_parents (dest_dir, 0700) == -1) {
    g_critical ("Could not create downloads directory \"%s\": %s",
                dest_dir, strerror (errno));
    g_free (dest_dir);
    return FALSE;
  }

  if (suggested_filename != NULL) {
    dest_name = ephy_sanitize_filename (g_strdup (suggested_filename));
  } else {
    dest_name = ephy_file_tmp_filename (".ephy-download-XXXXXX", NULL);
  }

  destination_filename = g_build_filename (dest_dir, dest_name, NULL);
  g_free (dest_dir);
  g_free (dest_name);

  /* Append (n) as needed. */
  if (g_file_test (destination_filename, G_FILE_TEST_EXISTS)) {
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

    destination_filename = g_strdup (tmp_filename->str);
    g_string_free (tmp_filename, TRUE);
  }

  destination_uri = g_filename_to_uri (destination_filename, NULL, NULL);
  g_free (destination_filename);

  g_assert (destination_uri);
  webkit_download_set_destination (download->download, destination_uri);
  g_free (destination_uri);

  return TRUE;
}

/**
 * ephy_download_set_destination_uri:
 * @download: an #EphyDownload
 * @destination: URI where to save @download
 *
 * Sets the destination URI of @download. It must be a proper URI, with a
 * scheme like file:/// or similar.
 **/
void
ephy_download_set_destination_uri (EphyDownload *download,
                                   const char   *destination)
{
  g_assert (EPHY_IS_DOWNLOAD (download));
  g_assert (destination != NULL);

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
ephy_download_set_action (EphyDownload          *download,
                          EphyDownloadActionType action)
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
 * ephy_download_get_destination_uri:
 * @download: an #EphyDownload
 *
 * Gets the destination URI where the download is being saved.
 *
 * Returns: (transfer none): destination URI.
 **/
const char *
ephy_download_get_destination_uri (EphyDownload *download)
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
 * ephy_download_get_start_time:
 * @download: an #EphyDownload
 *
 * Gets the time (returned by gtk_get_current_event_time ()) when @download was
 * started. Defaults to 0.
 *
 * Returns: the time when @download was started.
 **/
guint32
ephy_download_get_start_time (EphyDownload *download)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  return download->start_time;
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
ephy_download_failed (EphyDownload *download,
                      GError      **error)
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
 * @user_time: GDK timestamp, for focus-stealing prevention
 *
 * Executes the given @action for @download, this can be any of
 * #EphyDownloadActionType.
 *
 * Returns: %TRUE if the action was executed succesfully.
 *
 **/
gboolean
ephy_download_do_download_action (EphyDownload          *download,
                                  EphyDownloadActionType action,
                                  guint32                user_time)
{
  GFile *destination;
  const char *destination_uri;
  gboolean ret = FALSE;

  destination_uri = webkit_download_get_destination (download->download);
  destination = g_file_new_for_uri (destination_uri);

  switch ((action ? action : download->action)) {
    case EPHY_DOWNLOAD_ACTION_BROWSE_TO:
      LOG ("ephy_download_do_download_action: browse_to");
      /* Must not use this action type under flatpak! */
      ret = ephy_file_browse_to (destination, user_time,
                                 EPHY_FILE_HELPERS_I_UNDERSTAND_I_MUST_NOT_USE_THIS_FUNCTION_UNDER_FLATPAK);
      break;
    case EPHY_DOWNLOAD_ACTION_OPEN:
      LOG ("ephy_download_do_download_action: open");
      ret = ephy_file_launch_handler (destination, user_time);
      if (!ret && !ephy_is_running_inside_flatpak ())
        ret = ephy_file_browse_to (destination, user_time,
                                   EPHY_FILE_HELPERS_I_UNDERSTAND_I_MUST_NOT_USE_THIS_FUNCTION_UNDER_FLATPAK);
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

  g_clear_object (&download->file_monitor);
  g_clear_error (&download->error);
  g_clear_pointer (&download->content_type, g_free);

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
                         "Internal WebKitDownload",
                         "The WebKitDownload used internally by EphyDownload",
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
                         "Destination",
                         "Destination file URI",
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
                       "Download action",
                       "Action to take when download finishes",
                       EPHY_TYPE_DOWNLOAD_ACTION_TYPE,
                       EPHY_DOWNLOAD_ACTION_NONE,
                       G_PARAM_READABLE |
                       G_PARAM_STATIC_STRINGS);

  /**
   * EphyDownload::start-time:
   *
   * User time when the download started, useful for launching applications
   * aware of focus stealing.
   */
  obj_properties[PROP_START_TIME] =
    g_param_spec_uint ("start-time",
                       "Event start time",
                       "Time for focus-stealing prevention.",
                       0, G_MAXUINT32, 0,
                       G_PARAM_READABLE |
                       G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_CONTENT_TYPE] =
    g_param_spec_string ("content-type",
                         "Content Type",
                         "The download content type",
                         NULL,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  /**
   * EphyDownload::filename-suggested:
   *
   * The ::filename-suggested signal is emitted when we have received the
   * suggested filename from WebKit.
   **/
  signals[FILENAME_SUGGESTED] = g_signal_new ("filename-suggested",
                                              G_OBJECT_CLASS_TYPE (object_class),
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              NULL, NULL, NULL,
                                              G_TYPE_NONE,
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

  download->action = EPHY_DOWNLOAD_ACTION_NONE;

  download->start_time = gtk_get_current_event_time ();

  download->show_notification = TRUE;
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
                                const gchar    *suggested_filename,
                                EphyDownload   *download)
{
  if (webkit_download_get_destination (wk_download))
    return TRUE;

  g_signal_emit (download, signals[FILENAME_SUGGESTED], 0, suggested_filename);

  if (webkit_download_get_destination (wk_download))
    return TRUE;

  return set_destination_uri_for_suggested_filename (download, suggested_filename);
}

static void
download_created_destination_cb (WebKitDownload *wk_download,
                                 const char     *destination,
                                 EphyDownload   *download)
{
  char *filename;
  char *content_type;

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
      (download->content_type && !g_content_type_equals (download->content_type, content_type))) {
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

  if (!gtk_window_is_active (toplevel) && dest != NULL) {
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
  if (strcmp (g_file_get_uri (file), webkit_download_get_destination (download->download)))
    return;

  if (event_type == G_FILE_MONITOR_EVENT_DELETED || event_type == G_FILE_MONITOR_EVENT_MOVED)
    g_signal_emit (download, signals[MOVED], 0);
}

static void
download_finished_cb (WebKitDownload *wk_download,
                      EphyDownload   *download)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;

  download->finished = TRUE;

  ephy_download_do_download_action (download, download->action, download->start_time);

  if (download->show_notification)
    display_download_finished_notification (wk_download);

  g_signal_emit (download, signals[COMPLETED], 0);

  file = g_file_new_for_uri (webkit_download_get_destination (wk_download));
  download->file_monitor = g_file_monitor (file, G_FILE_MONITOR_NONE, NULL, &error);
  if (!download->file_monitor)
    g_warning ("Could not add a file monitor for %s, error: %s\n", g_file_get_uri (file), error->message);
  else
    g_signal_connect (download->file_monitor, "changed", G_CALLBACK (download_file_monitor_changed), download);
}

static void
download_failed_cb (WebKitDownload *wk_download,
                    GError         *error,
                    EphyDownload   *download)
{
  g_signal_handlers_disconnect_by_func (wk_download, download_finished_cb, download);

  LOG ("error (%d - %d)! %s", error->code, 0, error->message);
  download->finished = TRUE;
  download->error = g_error_copy (error);
  g_signal_emit (download, signals[ERROR], 0, download->error);
}

static void
filename_suggested_cb (EphyDownload *download,
                       const char   *suggested_filename,
                       gpointer      user_data)
{
  GApplication *application;
  GtkWidget *dialog = NULL;
  GtkWidget *message_area;
  GtkWidget *box;
  GtkWindow *toplevel;
  GtkWidget *type_label;
  GtkWidget *from_label;
  GtkWidget *question_label;
  GtkWidget *filechooser;
  WebKitDownload *webkit_download;
  WebKitURIResponse *response;
  g_autofree gchar *sanitized_filename = NULL;
  g_autofree gchar *type_text = NULL;
  g_autofree gchar *from_text = NULL;
  g_autofree gchar *content_length = NULL;
  const gchar *content_type;

  application = G_APPLICATION (ephy_embed_shell_get_default ());
  toplevel = gtk_application_get_active_window (GTK_APPLICATION (application));

  dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
                                   GTK_DIALOG_USE_HEADER_BAR | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   "%s",
                                   _("Download requested"));
  gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Download"), GTK_RESPONSE_OK, NULL);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", suggested_filename);
  message_area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dialog));

  webkit_download = ephy_download_get_webkit_download (download);
  response = webkit_download_get_response (webkit_download);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (message_area), box, TRUE, TRUE, 0);

  /* Type */
  content_length = g_format_size (webkit_uri_response_get_content_length (response));
  content_type = ephy_download_get_content_type (download);
  type_text = g_strdup_printf (_("Type: %s (%s)"), g_content_type_get_description (content_type), content_length);
  type_label = gtk_label_new (type_text);
  gtk_widget_set_margin_top (type_label, 12);
  gtk_box_pack_start (GTK_BOX (box), type_label, TRUE, TRUE, 0);

  /* From */
  from_text = g_strdup_printf (_("From: %s"), ephy_string_get_host_name (webkit_uri_response_get_uri (response)));
  from_label = gtk_label_new (from_text);
  gtk_box_pack_start (GTK_BOX (box), from_label, TRUE, TRUE, 0);

  /* Question */
  question_label = gtk_label_new (_("Where do you want to save the file?"));
  gtk_widget_set_margin_top (question_label, 12);
  gtk_box_pack_start (GTK_BOX (box), question_label, TRUE, TRUE, 0);

  /* File Chooser Button */
  filechooser = gtk_file_chooser_button_new (_("Save file"), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (filechooser), g_settings_get_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY));
  gtk_box_pack_start (GTK_BOX (box), filechooser, TRUE, TRUE, 0);

  gtk_widget_show_all (box);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
    g_autofree gchar *uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (filechooser));
    g_autofree gchar *folder = g_filename_from_uri (uri, NULL, NULL);
    g_autofree gchar *path = g_build_filename (uri, suggested_filename, NULL);

    ephy_download_set_destination_uri (download, path);

    webkit_download_set_allow_overwrite (webkit_download, TRUE);

    ephy_downloads_manager_add_download (ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ()),
                                         download);

    g_settings_set_string (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_LAST_DOWNLOAD_DIRECTORY, folder);
  } else {
    ephy_download_cancel (download);
  }

  gtk_widget_destroy (dialog);
}

EphyDownload *
ephy_download_new_internal (WebKitDownload *download)
{
  EphyDownload *ephy_download;

  g_assert (WEBKIT_IS_DOWNLOAD (download));

  ephy_download = g_object_new (EPHY_TYPE_DOWNLOAD, NULL);

  g_signal_connect (download, "notify::response",
                    G_CALLBACK (download_response_changed_cb),
                    ephy_download);
  g_signal_connect (download, "decide-destination",
                    G_CALLBACK (download_decide_destination_cb),
                    ephy_download);
  g_signal_connect (download, "created-destination",
                    G_CALLBACK (download_created_destination_cb),
                    ephy_download);
  g_signal_connect (download, "finished",
                    G_CALLBACK (download_finished_cb),
                    ephy_download);
  g_signal_connect (download, "failed",
                    G_CALLBACK (download_failed_cb),
                    ephy_download);

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

  if (!ephy_is_running_inside_flatpak() && g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ASK_ON_DOWNLOAD)) {
    g_signal_connect (ephy_download, "filename-suggested",
                      G_CALLBACK (filename_suggested_cb),
                      NULL);
  }

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

  g_assert (uri != NULL);

  download = webkit_web_context_download_uri (ephy_embed_shell_get_web_context (shell), uri);
  ephy_download = ephy_download_new (download);
  g_object_unref (download);

  return ephy_download;
}

EphyDownload *
ephy_download_new_for_uri_internal (const char *uri)
{
  EphyDownload *ephy_download;
  g_autoptr(WebKitDownload) download = NULL;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  g_assert (uri != NULL);

  download = webkit_web_context_download_uri (ephy_embed_shell_get_web_context (shell), uri);
  ephy_download = ephy_download_new_internal (download);

  return ephy_download;
}

void
ephy_download_disable_desktop_notification (EphyDownload *download)
{
  g_assert (EPHY_IS_DOWNLOAD (download));

  download->show_notification = FALSE;
}
