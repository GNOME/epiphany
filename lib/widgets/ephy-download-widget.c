/* vim: set sw=2 ts=2 sts=2 et: */
/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * ephy-download.c
 * This file is part of Epiphany
 *
 * Copyright © 2011 - Igalia S.L.
 *
 * Epiphany is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Epiphany is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Epiphany; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ephy-download-widget.h"

#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-download.h"
#include "totem-glow-button.h"

#include <glib/gi18n.h>
#ifdef HAVE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif

G_DEFINE_TYPE (EphyDownloadWidget, ephy_download_widget, GTK_TYPE_BOX)

#define DOWNLOAD_WIDGET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EPHY_TYPE_DOWNLOAD_WIDGET, EphyDownloadWidgetPrivate))

struct _EphyDownloadWidgetPrivate
{
  EphyDownload *download;

  GtkWidget *text;
  GtkWidget *remaining;
  GtkWidget *button;
  GtkWidget *menu;
  GtkWidget *icon;

  gboolean finished;
};

enum
{
  PROP_0,
  PROP_DOWNLOAD
};

static GIcon *
get_gicon_from_download (EphyDownload *ephy_download)
{
  char *content_type = NULL;
  GIcon *gicon;

  content_type = ephy_download_get_content_type (ephy_download);

  if (content_type != NULL) {
    gicon = g_content_type_get_icon (content_type);
    g_free (content_type);
  } else {
    gicon = g_icon_new_for_string ("package-x-generic", NULL);
  }

  return gicon;
}

static char *
get_destination_basename_from_download (EphyDownload *ephy_download)
{
  WebKitDownload *download;
  const char *dest;
  char *basename;
  char *unescaped;

  download = ephy_download_get_webkit_download (ephy_download);
#ifdef HAVE_WEBKIT2
  dest = webkit_download_get_destination (download);
#else
  dest = webkit_download_get_destination_uri (download);
#endif
  if (!dest)
    return NULL;

  basename = g_filename_display_basename (dest);
  unescaped = g_uri_unescape_string (basename, NULL);
  g_free (basename);

  return unescaped;
}

static char *
format_interval (gdouble interval)
{
   int hours, mins, secs;

   hours = (int) (interval / 3600);
   interval -= hours * 3600;
   mins = (int) (interval / 60);
   interval -= mins * 60;
   secs = (int) interval;

   if (hours > 0) {
     if (mins > 0)
       return g_strdup_printf (ngettext ("%u:%02u hour left", "%u:%02u hours left", hours), hours, mins);
     else
       return g_strdup_printf (ngettext ("%u hour left", "%u hours left", hours), hours);
   } else {
     if (mins > 0)
       return g_strdup_printf (ngettext ("%u:%02u minute left", "%u:%02u minutes left", mins), mins, secs);
     else
       return g_strdup_printf (ngettext ("%u second left", "%u seconds left", secs), secs);
   }
}

static gdouble
get_remaining_time (WebKitDownload *download)
{
  gint64 total, cur;
  gdouble elapsed_time;
  gdouble remaining_time;
  gdouble per_byte_time;
#ifdef HAVE_WEBKIT2
  WebKitURIResponse *response;

  response = webkit_download_get_response (download);
  total = webkit_uri_response_get_content_length (response);
  cur = webkit_download_get_received_data_length (download);
#else

  total = webkit_download_get_total_size (download);
  cur = webkit_download_get_current_size (download);
#endif
  elapsed_time = webkit_download_get_elapsed_time (download);

  if (cur <= 0)
    return -1.0;

  per_byte_time = elapsed_time / cur;
  remaining_time = per_byte_time * (total - cur);

  return remaining_time;
}

static void
download_clicked_cb (GtkButton *button,
                     EphyDownloadWidget *widget)
{
  EphyDownload *download;

  if (!widget->priv->finished)
    return;

  download = widget->priv->download;
  if (ephy_download_do_download_action (download, EPHY_DOWNLOAD_ACTION_AUTO))
    gtk_widget_destroy (GTK_WIDGET (widget));
}

static void
update_download_icon (EphyDownloadWidget *widget)
{
  GIcon *new_icon, *old_icon;

  new_icon = get_gicon_from_download (widget->priv->download);
  gtk_image_get_gicon (GTK_IMAGE (widget->priv->icon), &old_icon, NULL);
  if (!g_icon_equal (new_icon, old_icon)) {
    gtk_image_set_from_gicon (GTK_IMAGE (widget->priv->icon), new_icon,
                              GTK_ICON_SIZE_LARGE_TOOLBAR);
  }
  g_object_unref (new_icon);
}

static void
update_download_label_and_tooltip (EphyDownloadWidget *widget,
                                   const char *download_label)
{
  WebKitDownload *download;
  char *remaining_tooltip;
  char *destination;

  download = ephy_download_get_webkit_download (widget->priv->download);
#ifdef HAVE_WEBKIT2
  destination = g_filename_display_basename (webkit_download_get_destination (download));
#else
  destination = g_filename_display_basename (webkit_download_get_destination_uri (download));
#endif

  remaining_tooltip = g_markup_printf_escaped ("%s\n%s", destination, download_label);
  g_free (destination);

  gtk_label_set_text (GTK_LABEL (widget->priv->remaining), download_label);
  gtk_widget_set_tooltip_text (GTK_WIDGET (widget), remaining_tooltip);
  g_free (remaining_tooltip);
}

static gboolean
download_content_length_is_known (WebKitDownload *download)
{
#ifdef HAVE_WEBKIT2
  WebKitURIResponse *response;

  response = webkit_download_get_response (download);
  return webkit_uri_response_get_content_length (response);
#else
  WebKitNetworkResponse *response;
  SoupMessage* message;

  response = webkit_download_get_network_response (download);
  message = webkit_network_response_get_message (response);
  return soup_message_headers_get_content_length (message->response_headers) > 0;
#endif
}

static void
widget_progress_cb (WebKitDownload *download,
                    GParamSpec *pspec,
                    EphyDownloadWidget *widget)
{
  int progress;
  char *download_label = NULL;

#ifdef HAVE_WEBKIT2
  if (!webkit_download_get_destination (download))
    return;

  progress = webkit_download_get_estimated_progress (download) * 100;
#else
  progress = webkit_download_get_progress (download) * 100;
#endif

  if (progress % 10 == 0)
    update_download_icon (widget);

  if (download_content_length_is_known (download)) {
    gdouble time;

    time = get_remaining_time (download);
    if (time > 0) {
      char *remaining;

      remaining = format_interval (time);
      download_label = g_strdup_printf ("%d%% (%s)", progress, remaining);
      g_free (remaining);
    }
  } else {
    gint64 current_size;

    /* Unknown content length, show received bytes instead. */
#ifdef HAVE_WEBKIT2
    current_size = webkit_download_get_received_data_length (download);
#else
    current_size = webkit_download_get_current_size (download);
#endif
    if (current_size > 0)
      download_label = g_format_size (current_size);
  }

  if (download_label) {
    update_download_label_and_tooltip (widget, download_label);
    g_free (download_label);
  }
}

#ifdef HAVE_WEBKIT2
static void
widget_destination_changed_cb (WebKitDownload *download,
                               GParamSpec *pspec,
                               EphyDownloadWidget *widget)
{
  char *dest;

  dest = get_destination_basename_from_download (widget->priv->download);
  gtk_label_set_text (GTK_LABEL (widget->priv->text), dest);
  g_free (dest);
}

static void
widget_finished_cb (WebKitDownload *download,
                    EphyDownloadWidget *widget)
{
  widget->priv->finished = TRUE;
  update_download_label_and_tooltip (widget, _("Finished"));
  totem_glow_button_set_glow (TOTEM_GLOW_BUTTON (widget->priv->button), TRUE);
}
#else
static void
widget_status_cb (WebKitDownload *download,
                  GParamSpec *pspec,
                  EphyDownloadWidget *widget)
{
  WebKitDownloadStatus status;

  status = webkit_download_get_status (download);

  if (status != WEBKIT_DOWNLOAD_STATUS_FINISHED)
    return;

  widget->priv->finished = TRUE;
  update_download_label_and_tooltip (widget, _("Finished"));
  totem_glow_button_set_glow (TOTEM_GLOW_BUTTON (widget->priv->button), TRUE);
}
#endif

#ifdef HAVE_WEBKIT2
static void
widget_failed_cb (WebKitDownload *download,
                  GError *error,
                  EphyDownloadWidget *widget)
{
  char *error_msg;

  g_signal_handlers_disconnect_by_func (download, widget_progress_cb, widget);

  error_msg = g_strdup_printf (_("Error downloading: %s"), error->message);
  gtk_label_set_text (GTK_LABEL (widget->priv->remaining), error_msg);
  gtk_widget_set_tooltip_text (GTK_WIDGET (widget), error_msg);
  g_free (error_msg);
}
#else
static gboolean
widget_error_cb (WebKitDownload *download,
                 gint error_code,
                 gint error_detail,
                 char *reason,
                 EphyDownloadWidget *widget)
{
  char *error_msg;

  g_signal_handlers_disconnect_by_func (download, widget_status_cb, widget);
  g_signal_handlers_disconnect_by_func (download, widget_progress_cb, widget);

  error_msg = g_strdup_printf (_("Error downloading: %s"), reason);

  gtk_label_set_text (GTK_LABEL (widget->priv->remaining), error_msg);
  gtk_widget_set_tooltip_text (GTK_WIDGET (widget), error_msg);

  g_free (error_msg);

  totem_glow_button_set_glow (TOTEM_GLOW_BUTTON (widget->priv->button), TRUE);

  return FALSE;
}
#endif

static void
open_activate_cb (GtkMenuItem *item, EphyDownloadWidget *widget)
{
  if (ephy_download_do_download_action (widget->priv->download,
                                        EPHY_DOWNLOAD_ACTION_OPEN))
    gtk_widget_destroy (GTK_WIDGET (widget));
}
static void
folder_activate_cb (GtkMenuItem *item, EphyDownloadWidget *widget)
{
  if (ephy_download_do_download_action (widget->priv->download,
                                        EPHY_DOWNLOAD_ACTION_BROWSE_TO))
    gtk_widget_destroy (GTK_WIDGET (widget));
}
static void
cancel_activate_cb (GtkMenuItem *item, EphyDownloadWidget *widget)
{
  ephy_download_cancel (widget->priv->download);
  gtk_widget_destroy (GTK_WIDGET (widget));
}

static void
download_menu_clicked_cb (GtkWidget *button,
                          GdkEventButton *event,
                          EphyDownloadWidget *widget)
{
  GtkWidget *item;
  GtkWidget *menu;
  GtkWidget *box;
  GList *children = NULL;
  char *basename, *name;
  WebKitDownload *download;

  download = ephy_download_get_webkit_download (widget->priv->download);

#ifdef HAVE_WEBKIT2
  basename = g_filename_display_basename (webkit_download_get_destination (download));
#else
  basename = g_filename_display_basename (webkit_download_get_destination_uri (download));
#endif
  name = g_uri_unescape_string (basename, NULL);

  box = gtk_widget_get_parent (button);
  children = gtk_container_get_children (GTK_CONTAINER (box));
  totem_glow_button_set_glow (TOTEM_GLOW_BUTTON (children->data), FALSE);
  g_list_free (children);

  menu = gtk_menu_new ();

  item = gtk_menu_item_new_with_label (name);
  gtk_widget_set_sensitive (item, FALSE);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  g_free (basename);
  g_free (name);

  item = gtk_menu_item_new_with_label (_("Cancel"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_set_sensitive (item, !widget->priv->finished);
  g_signal_connect (item, "activate",
                    G_CALLBACK (cancel_activate_cb), widget);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  item = gtk_menu_item_new_with_label (_("Open"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_set_sensitive (item, widget->priv->finished);
  g_signal_connect (item, "activate",
                    G_CALLBACK (open_activate_cb), widget);

  item = gtk_menu_item_new_with_label (_("Show in folder"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_set_sensitive (item, widget->priv->finished);
  g_signal_connect (item, "activate",
                    G_CALLBACK (folder_activate_cb), widget);

  gtk_widget_show_all (menu);

  gtk_menu_attach_to_widget (GTK_MENU (menu), button, NULL);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                  event->button, event->time);
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
      widget->priv->download = g_object_ref (g_value_get_object (value));
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
  WebKitDownload *download;

  LOG ("EphyDownloadWidget %p dispose", object);

  widget = EPHY_DOWNLOAD_WIDGET (object);

  if (widget->priv->download != NULL) {
    download = ephy_download_get_webkit_download (widget->priv->download);

#ifdef HAVE_WEBKIT2
    g_signal_handlers_disconnect_by_func (download, widget_progress_cb, widget);
    g_signal_handlers_disconnect_by_func (download, widget_destination_changed_cb, widget);
    g_signal_handlers_disconnect_by_func (download, widget_finished_cb, widget);
    g_signal_handlers_disconnect_by_func (download, widget_failed_cb, widget);
#else
    g_signal_handlers_disconnect_by_func (download, widget_progress_cb, widget);
    g_signal_handlers_disconnect_by_func (download, widget_status_cb, widget);
    g_signal_handlers_disconnect_by_func (download, widget_error_cb, widget);
#endif

    g_object_unref (widget->priv->download);
    widget->priv->download = NULL;
  }

  G_OBJECT_CLASS (ephy_download_widget_parent_class)->dispose (object);
}

static void
ephy_download_widget_class_init (EphyDownloadWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (EphyDownloadWidgetPrivate));

  object_class->get_property = ephy_download_widget_get_property;
  object_class->set_property = ephy_download_widget_set_property;
  object_class->dispose = ephy_download_widget_dispose;

  /**
   * EphyDownloadWidget::download:
   *
   * The EphyDownload that this widget is showing.
   */
  g_object_class_install_property (object_class, PROP_DOWNLOAD,
                                   g_param_spec_object ("download",
                                                        "An EphyDownload object",
                                                        "The EphyDownload shown by this widget",
                                                        G_TYPE_OBJECT,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_NICK |
                                                        G_PARAM_STATIC_BLURB));
}

static void
ephy_download_widget_init (EphyDownloadWidget *self)
{
  GtkStyleContext *context;

  self->priv = DOWNLOAD_WIDGET_PRIVATE (self);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_HORIZONTAL);
  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_LINKED);
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
  g_return_val_if_fail (EPHY_IS_DOWNLOAD_WIDGET (widget), NULL);
  return widget->priv->download;
}

/**
 * ephy_download_widget_download_is_finished:
 * @widget: an #EphyDownloadWidget
 *
 * Whether the download finished
 *
 * Returns: %TRUE if download operation finished or %FALSE otherwise
 **/
gboolean
ephy_download_widget_download_is_finished (EphyDownloadWidget *widget)
{
  g_return_val_if_fail (EPHY_IS_DOWNLOAD_WIDGET (widget), FALSE);
  return widget->priv->finished;
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

  GtkWidget *grid;
  GtkWidget *icon;
  GtkWidget *text;
  GtkWidget *button;
  GtkWidget *menu;
  GtkWidget *remain;

  char *dest;
  WebKitDownload *download;
  GIcon *gicon;

  g_return_val_if_fail (EPHY_IS_DOWNLOAD (ephy_download), NULL);

  widget = g_object_new (EPHY_TYPE_DOWNLOAD_WIDGET,
                         "download", ephy_download, NULL);
  download = ephy_download_get_webkit_download (ephy_download);

  grid = gtk_grid_new ();

  button = totem_glow_button_new ();
  menu = gtk_button_new ();

  gicon = get_gicon_from_download (ephy_download);
  icon = gtk_image_new_from_gicon (gicon, GTK_ICON_SIZE_LARGE_TOOLBAR);
  g_object_unref (gicon);

  dest = get_destination_basename_from_download (ephy_download);
  text = gtk_label_new (dest);
  gtk_misc_set_alignment (GTK_MISC (text), 0, 0.5);
  gtk_label_set_ellipsize (GTK_LABEL (text), PANGO_ELLIPSIZE_END);

  remain = gtk_label_new (_("Starting…"));
  gtk_misc_set_alignment (GTK_MISC (remain), 0, 0.5);
  gtk_label_set_ellipsize (GTK_LABEL (remain), PANGO_ELLIPSIZE_END);

  gtk_grid_attach (GTK_GRID (grid), icon, 0, 0, 1, 2);
  gtk_grid_attach (GTK_GRID (grid), text, 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), remain, 1, 1, 1, 1);

  gtk_widget_set_tooltip_text (GTK_WIDGET (widget), dest);
  g_free (dest);

  widget->priv->text = text;
  widget->priv->icon = icon;
  widget->priv->button = button;
  widget->priv->remaining = remain;
  widget->priv->menu = menu;

#ifdef HAVE_WEBKIT2
  g_signal_connect (download, "notify::estimated-progress",
                    G_CALLBACK (widget_progress_cb), widget);
  g_signal_connect (download, "notify::destination",
                    G_CALLBACK (widget_destination_changed_cb), widget);
  g_signal_connect (download, "finished",
                    G_CALLBACK (widget_finished_cb), widget);
  g_signal_connect (download, "failed",
                    G_CALLBACK (widget_failed_cb), widget);
#else
  g_signal_connect (download, "notify::progress",
                    G_CALLBACK (widget_progress_cb), widget);
  g_signal_connect (download, "notify::status",
                    G_CALLBACK (widget_status_cb), widget);
  g_signal_connect (download, "error",
                    G_CALLBACK (widget_error_cb), widget);
#endif

  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_HALF);
  gtk_button_set_relief (GTK_BUTTON (menu), GTK_RELIEF_NORMAL);

  gtk_container_add (GTK_CONTAINER (button), grid);
  gtk_container_add (GTK_CONTAINER (menu),
                     gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE));

  gtk_box_pack_start (GTK_BOX (widget), button, FALSE, FALSE, 0);
  gtk_box_pack_end (GTK_BOX (widget), menu, FALSE, FALSE, 0);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (download_clicked_cb), widget);
  g_signal_connect (menu, "button-press-event",
                    G_CALLBACK (download_menu_clicked_cb), widget);

  gtk_widget_show_all (button);
  gtk_widget_show_all (menu);

  ephy_download_set_widget (ephy_download, GTK_WIDGET (widget));

  return GTK_WIDGET (widget);
}
