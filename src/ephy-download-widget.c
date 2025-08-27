/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011, 2015 Igalia S.L.
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
#include "ephy-download-widget.h"

#include "ephy-debug.h"
#include "ephy-downloads-manager.h"
#include "ephy-embed-shell.h"
#include "ephy-uri-helpers.h"

#include <glib/gi18n.h>
#include <webkit/webkit.h>

struct _EphyDownloadWidget {
  AdwBin parent_instance;

  EphyDownload *download;

  GtkWidget *filename;
  GtkWidget *status;
  GtkWidget *icon;
  GtkWidget *progress;
  GtkWidget *action_button;
};

G_DEFINE_FINAL_TYPE (EphyDownloadWidget, ephy_download_widget, ADW_TYPE_BIN)

enum {
  PROP_0,
  PROP_DOWNLOAD,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static char *
get_destination_basename_from_download (EphyDownload *ephy_download)
{
  WebKitDownload *download;
  const char *dest;

  download = ephy_download_get_webkit_download (ephy_download);
  dest = webkit_download_get_destination (download);
  if (!dest)
    return NULL;

  return g_filename_display_basename (dest);
}

/* modified from telepathy-account-widgets/tpaw-time.c */
static gchar *
duration_to_string (guint seconds)
{
  if (seconds < 60) {
    return g_strdup_printf (ngettext ("%d second left",
                                      "%d seconds left", seconds), seconds);
  } else if (seconds < (60 * 60)) {
    seconds /= 60;
    return g_strdup_printf (ngettext ("%d minute left",
                                      "%d minutes left", seconds), seconds);
  } else if (seconds < (60 * 60 * 24)) {
    seconds /= 60 * 60;
    return g_strdup_printf (ngettext ("%d hour left",
                                      "%d hours left", seconds), seconds);
  } else if (seconds < (60 * 60 * 24 * 7)) {
    seconds /= 60 * 60 * 24;
    return g_strdup_printf (ngettext ("%d day left",
                                      "%d days left", seconds), seconds);
  } else if (seconds < (60 * 60 * 24 * 30)) {
    seconds /= 60 * 60 * 24 * 7;
    return g_strdup_printf (ngettext ("%d week left",
                                      "%d weeks left", seconds), seconds);
  } else {
    seconds /= 60 * 60 * 24 * 30;
    return g_strdup_printf (ngettext ("%d month left",
                                      "%d months left", seconds), seconds);
  }
}

static gdouble
get_remaining_time (guint64 content_length,
                    guint64 received_length,
                    gdouble elapsed_time)
{
  gdouble remaining_time;
  gdouble per_byte_time;

  per_byte_time = elapsed_time / received_length;
  remaining_time = per_byte_time * (content_length - received_length);

  return remaining_time;
}

static void
update_download_icon (EphyDownloadWidget *widget)
{
  g_autoptr (GIcon) icon = NULL;
  const char *content_type;

  content_type = ephy_download_get_content_type (widget->download);
  if (content_type) {
    icon = g_content_type_get_symbolic_icon (content_type);
    /* g_content_type_get_symbolic_icon() always creates a GThemedIcon, but we check it
     * here just in case that changes in GLib eventually.
     */
    if (G_IS_THEMED_ICON (icon)) {
      /* Ensure we always fallback to package-x-generic-symbolic if all other icons are
       * missing in the theme.
       */
      g_themed_icon_append_name (G_THEMED_ICON (icon), "package-x-generic-symbolic");
    }
  } else
    icon = g_icon_new_for_string ("package-x-generic-symbolic", NULL);

  gtk_image_set_from_gicon (GTK_IMAGE (widget->icon), icon);
}

static void
update_download_destination (EphyDownloadWidget *widget)
{
  g_autofree char *dest = NULL;

  dest = get_destination_basename_from_download (widget->download);
  if (!dest)
    return;

  gtk_label_set_label (GTK_LABEL (widget->filename), dest);
}

static void
update_status_label (EphyDownloadWidget *widget,
                     const char         *download_label)
{
  g_autofree char *markup = NULL;

  markup = g_markup_printf_escaped ("<span size='small'>%s</span>", download_label);
  gtk_label_set_markup (GTK_LABEL (widget->status), markup);
}

static void
download_progress_cb (WebKitDownload     *download,
                      GParamSpec         *pspec,
                      EphyDownloadWidget *widget)
{
  gdouble progress;
  WebKitURIResponse *response;
  guint64 content_length;
  guint64 received_length;
  g_autofree char *download_label = NULL;

  if (!webkit_download_get_destination (download))
    return;

  progress = webkit_download_get_estimated_progress (download);
  response = webkit_download_get_response (download);
  content_length = webkit_uri_response_get_content_length (response);
  received_length = webkit_download_get_received_data_length (download);

  if (content_length > 0 && received_length > 0) {
    gdouble time;
    g_autofree char *remaining = NULL;
    g_autofree char *received = NULL;
    g_autofree char *total = NULL;

    received = g_format_size (received_length);
    total = g_format_size (content_length);

    time = get_remaining_time (content_length, received_length,
                               webkit_download_get_elapsed_time (download));
    remaining = duration_to_string ((guint)time);
    download_label = g_strdup_printf ("%s / %s — %s", received, total, remaining);

    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (widget->progress),
                                   progress);
  } else if (received_length > 0) {
    download_label = g_format_size (received_length);
    gtk_progress_bar_pulse (GTK_PROGRESS_BAR (widget->progress));
  }

  if (download_label) {
    update_status_label (widget, download_label);
  }
}

static void
download_finished_cb (EphyDownload       *download,
                      EphyDownloadWidget *widget)
{
  gtk_widget_set_visible (widget->progress, FALSE);
  update_status_label (widget, _("Finished"));
  gtk_button_set_icon_name (GTK_BUTTON (widget->action_button),
                            "folder-open-symbolic");
}

static void
download_moved_cb (EphyDownload       *download,
                   EphyDownloadWidget *widget)
{
  update_status_label (widget, _("Moved or deleted"));
  gtk_widget_set_sensitive (widget->action_button, FALSE);
}

static void
download_failed_cb (EphyDownload       *download,
                    GError             *error,
                    EphyDownloadWidget *widget)
{
  g_autofree char *error_msg = NULL;

  g_signal_handlers_disconnect_by_func (download, download_progress_cb, widget);

  gtk_widget_set_visible (widget->progress, FALSE);

  error_msg = g_strdup_printf (_("Error downloading: %s"), error->message);
  update_status_label (widget, error_msg);
  gtk_button_set_icon_name (GTK_BUTTON (widget->action_button),
                            "list-remove-symbolic");
}

static void
download_content_type_changed_cb (EphyDownload       *download,
                                  GParamSpec         *spec,
                                  EphyDownloadWidget *widget)
{
  update_download_icon (widget);
}

static void
widget_action_button_clicked_cb (EphyDownloadWidget *widget)
{
  if (ephy_download_is_active (widget->download)) {
    WebKitDownload *download;

    download = ephy_download_get_webkit_download (widget->download);
    g_signal_handlers_disconnect_matched (download, G_SIGNAL_MATCH_DATA, 0, 0,
                                          NULL, NULL, widget);
    g_signal_handlers_disconnect_matched (widget->download, G_SIGNAL_MATCH_DATA, 0, 0,
                                          NULL, NULL, widget);
    update_status_label (widget, _("Cancelling…"));
    gtk_widget_set_sensitive (widget->action_button, FALSE);

    ephy_download_cancel (widget->download);
  } else if (ephy_download_failed (widget->download, NULL)) {
    EphyDownloadsManager *manager;

    manager = ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ());
    ephy_downloads_manager_remove_download (manager, widget->download);
  } else {
    ephy_download_do_download_action (widget->download,
                                      EPHY_DOWNLOAD_ACTION_BROWSE_TO);
  }
}

static void
download_destination_changed_cb (WebKitDownload     *download,
                                 GParamSpec         *pspec,
                                 EphyDownloadWidget *widget)
{
  update_download_destination (widget);
}

static GdkContentProvider *
download_drag_prepare (WebKitDownload *download)
{
  const char *dest = webkit_download_get_destination (download);
  return gdk_content_provider_new_typed (G_TYPE_FILE, g_file_new_for_path (dest));
}

static void
ephy_download_widget_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  EphyDownloadWidget *widget;

  widget = EPHY_DOWNLOAD_WIDGET (object);

  switch (property_id) {
    case PROP_DOWNLOAD:
      g_value_set_object (value, ephy_download_widget_get_download (widget));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
ephy_download_widget_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  EphyDownloadWidget *widget;
  widget = EPHY_DOWNLOAD_WIDGET (object);

  switch (property_id) {
    case PROP_DOWNLOAD:
      widget->download = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
ephy_download_widget_dispose (GObject *object)
{
  EphyDownloadWidget *widget;

  LOG ("EphyDownloadWidget %p dispose", object);

  widget = EPHY_DOWNLOAD_WIDGET (object);

  if (widget->download) {
    WebKitDownload *download = ephy_download_get_webkit_download (widget->download);

    g_signal_handlers_disconnect_matched (download, G_SIGNAL_MATCH_DATA, 0, 0,
                                          NULL, NULL, widget);
    g_signal_handlers_disconnect_matched (widget->download, G_SIGNAL_MATCH_DATA, 0, 0,
                                          NULL, NULL, widget);
    g_object_unref (widget->download);
    widget->download = NULL;
  }

  G_OBJECT_CLASS (ephy_download_widget_parent_class)->dispose (object);
}

static void
ephy_download_widget_constructed (GObject *object)
{
  EphyDownloadWidget *widget = EPHY_DOWNLOAD_WIDGET (object);
  WebKitDownload *download;
  const char *action_icon_name = NULL;
  GError *error = NULL;
  PangoAttrList *status_attrs;
  GtkWidget *grid;
  GtkDragSource *source;

  G_OBJECT_CLASS (ephy_download_widget_parent_class)->constructed (object);

  grid = gtk_grid_new ();
  gtk_widget_set_visible (grid, TRUE);
  adw_bin_set_child (ADW_BIN (widget), grid);

  widget->icon = gtk_image_new ();
  gtk_widget_set_margin_end (widget->icon, 4);
  update_download_icon (widget);
  gtk_grid_attach (GTK_GRID (grid), widget->icon, 0, 0, 1, 1);

  widget->filename = gtk_label_new (NULL);
  gtk_widget_set_hexpand (widget->filename, true);
  gtk_label_set_xalign (GTK_LABEL (widget->filename), 0);
  gtk_label_set_max_width_chars (GTK_LABEL (widget->filename), 30);
  gtk_label_set_ellipsize (GTK_LABEL (widget->filename), PANGO_ELLIPSIZE_END);
  update_download_destination (widget);
  gtk_grid_attach (GTK_GRID (grid), widget->filename, 1, 0, 1, 1);

  widget->progress = gtk_progress_bar_new ();
  gtk_widget_set_margin_top (widget->progress, 6);
  gtk_widget_set_margin_bottom (widget->progress, 6);
  gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR (widget->progress), 0.05);
  gtk_grid_attach (GTK_GRID (grid), widget->progress, 0, 1, 2, 1);

  widget->status = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (widget->status), 0);
  g_object_set (widget->status, "width-request", 260, NULL);
  gtk_label_set_max_width_chars (GTK_LABEL (widget->status), 30);
  gtk_label_set_ellipsize (GTK_LABEL (widget->status), PANGO_ELLIPSIZE_END);
  status_attrs = pango_attr_list_new ();
  pango_attr_list_insert (status_attrs, pango_attr_font_features_new ("tnum=1"));
  gtk_label_set_attributes (GTK_LABEL (widget->status), status_attrs);
  pango_attr_list_unref (status_attrs);

  if (ephy_download_failed (widget->download, &error)) {
    g_autofree char *error_msg = NULL;

    error_msg = g_strdup_printf (_("Error downloading: %s"), error->message);
    update_status_label (widget, error_msg);
  } else if (ephy_download_succeeded (widget->download)) {
    update_status_label (widget, _("Finished"));
  } else {
    update_status_label (widget, _("Starting…"));
  }
  gtk_grid_attach (GTK_GRID (grid), widget->status, 0, 2, 2, 1);

  if (ephy_download_succeeded (widget->download))
    action_icon_name = "folder-open-symbolic";
  else if (ephy_download_failed (widget->download, NULL))
    action_icon_name = "list-remove-symbolic";
  else
    action_icon_name = "window-close-symbolic";
  widget->action_button = gtk_button_new_from_icon_name (action_icon_name);
  g_signal_connect_swapped (widget->action_button, "clicked",
                            G_CALLBACK (widget_action_button_clicked_cb),
                            widget);
  gtk_widget_set_valign (widget->action_button, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start (widget->action_button, 10);
  gtk_widget_add_css_class (widget->action_button, "circular");
  gtk_grid_attach (GTK_GRID (grid), widget->action_button, 3, 0, 1, 3);

  download = ephy_download_get_webkit_download (widget->download);
  g_signal_connect (download, "notify::estimated-progress",
                    G_CALLBACK (download_progress_cb),
                    widget);
  g_signal_connect (download, "notify::destination",
                    G_CALLBACK (download_destination_changed_cb),
                    widget);
  g_signal_connect (widget->download, "completed",
                    G_CALLBACK (download_finished_cb),
                    widget);
  g_signal_connect (widget->download, "error",
                    G_CALLBACK (download_failed_cb),
                    widget);
  g_signal_connect (widget->download, "moved",
                    G_CALLBACK (download_moved_cb),
                    widget);
  g_signal_connect (widget->download, "notify::content-type",
                    G_CALLBACK (download_content_type_changed_cb),
                    widget);

  source = gtk_drag_source_new ();
  gtk_drag_source_set_actions (source, GDK_ACTION_COPY);
  g_signal_connect_swapped (source, "prepare", G_CALLBACK (download_drag_prepare), download);

  gtk_widget_add_controller (GTK_WIDGET (widget), GTK_EVENT_CONTROLLER (source));
}

static void
ephy_download_widget_class_init (EphyDownloadWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ephy_download_widget_constructed;
  object_class->get_property = ephy_download_widget_get_property;
  object_class->set_property = ephy_download_widget_set_property;
  object_class->dispose = ephy_download_widget_dispose;

  /**
   * EphyDownloadWidget::download:
   *
   * The EphyDownload that this widget is showing.
   */
  obj_properties[PROP_DOWNLOAD] =
    g_param_spec_object ("download",
                         NULL, NULL,
                         G_TYPE_OBJECT,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static void
ephy_download_widget_init (EphyDownloadWidget *widget)
{
}

/**
 * ephy_download_widget_get_download:
 * @widget: an #EphyDownloadWidget
 *
 * Gets the #EphyDownload that @widget is showing.
 *
 * Returns: (transfer none): an #EphyDownload.
 **/
EphyDownload *
ephy_download_widget_get_download (EphyDownloadWidget *widget)
{
  g_assert (EPHY_IS_DOWNLOAD_WIDGET (widget));
  return widget->download;
}

/**
 * ephy_download_widget_new:
 * @ephy_download: the #EphyDownload that @widget is wrapping
 *
 * Creates an #EphyDownloadWidget to wrap @ephy_download. It also associates
 * @ephy_download to it.
 *
 * Returns: a new #EphyDownloadWidget
 **/
GtkWidget *
ephy_download_widget_new (EphyDownload *ephy_download)
{
  EphyDownloadWidget *widget;

  g_assert (EPHY_IS_DOWNLOAD (ephy_download));

  widget = g_object_new (EPHY_TYPE_DOWNLOAD_WIDGET,
                         "download", ephy_download, NULL);

  return GTK_WIDGET (widget);
}
