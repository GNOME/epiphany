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
#include "ephy-uri-helpers.h"

#include <glib/gi18n.h>
#include <webkit2/webkit2.h>

G_DEFINE_TYPE (EphyDownloadWidget, ephy_download_widget, GTK_TYPE_BOX)

#define DOWNLOAD_WIDGET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), EPHY_TYPE_DOWNLOAD_WIDGET, EphyDownloadWidgetPrivate))

struct _EphyDownloadWidgetPrivate
{
  EphyDownload *download;

  GtkWidget *text;
  GtkWidget *remaining;
  GtkWidget *button;
  GtkWidget *menu_button;
  GtkWidget *icon;
  GtkWidget *open_menuitem;
  GtkWidget *cancel_menuitem;
  GtkWidget *show_folder_menuitem;

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
  dest = webkit_download_get_destination (download);
  if (!dest)
    return NULL;

  basename = g_filename_display_basename (dest);
  unescaped = ephy_uri_safe_unescape (basename);
  g_free (basename);

  return unescaped;
}

/* modified from telepathy-account-widgets/tpaw-time.c */
static gchar *
duration_to_string (guint seconds)
{
  if (seconds < 60)
    {
      return g_strdup_printf (ngettext ("%d second left",
        "%d seconds left", seconds), seconds);
    }
  else if (seconds < (60 * 60))
    {
      seconds /= 60;
      return g_strdup_printf (ngettext ("%d minute left",
        "%d minutes left", seconds), seconds);
    }
  else if (seconds < (60 * 60 * 24))
    {
      seconds /= 60 * 60;
      return g_strdup_printf (ngettext ("%d hour left",
        "%d hours left", seconds), seconds);
    }
  else if (seconds < (60 * 60 * 24 * 7))
    {
      seconds /= 60 * 60 * 24;
      return g_strdup_printf (ngettext ("%d day left",
        "%d days left", seconds), seconds);
    }
  else if (seconds < (60 * 60 * 24 * 30))
    {
      seconds /= 60 * 60 * 24 * 7;
      return g_strdup_printf (ngettext ("%d week left",
        "%d weeks left", seconds), seconds);
    }
  else
    {
      seconds /= 60 * 60 * 24 * 30;
      return g_strdup_printf (ngettext ("%d month left",
        "%d months left", seconds), seconds);
    }
}

static gdouble
get_remaining_time (WebKitDownload *download)
{
  gint64 total, cur;
  gdouble elapsed_time;
  gdouble remaining_time;
  gdouble per_byte_time;
  WebKitURIResponse *response;

  response = webkit_download_get_response (download);
  total = webkit_uri_response_get_content_length (response);
  cur = webkit_download_get_received_data_length (download);
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
update_download_destination (EphyDownloadWidget *widget)
{
  char *dest;

  dest = get_destination_basename_from_download (widget->priv->download);
  gtk_label_set_text (GTK_LABEL (widget->priv->text), dest);
  g_free (dest);
}

static void
update_download_label_and_tooltip (EphyDownloadWidget *widget,
                                   const char *download_label)
{
  WebKitDownload *download;
  char *remaining_tooltip;
  char *destination;
  const char *dest;

  download = ephy_download_get_webkit_download (widget->priv->download);
  dest = webkit_download_get_destination (download);
  if (dest == NULL)
    return;

  destination = g_filename_display_basename (dest);

  remaining_tooltip = g_markup_printf_escaped ("%s\n%s", destination, download_label);
  g_free (destination);

  gtk_label_set_text (GTK_LABEL (widget->priv->remaining), download_label);
  gtk_widget_set_tooltip_text (GTK_WIDGET (widget), remaining_tooltip);
  g_free (remaining_tooltip);
}

static gboolean
download_content_length_is_known (WebKitDownload *download)
{
  WebKitURIResponse *response;

  response = webkit_download_get_response (download);
  return webkit_uri_response_get_content_length (response);
}

static void
widget_progress_cb (WebKitDownload *download,
                    GParamSpec *pspec,
                    EphyDownloadWidget *widget)
{
  int progress;
  char *download_label = NULL;

  if (!webkit_download_get_destination (download))
    return;

  progress = webkit_download_get_estimated_progress (download) * 100;

  if (progress % 10 == 0)
    update_download_icon (widget);

  if (download_content_length_is_known (download)) {
    gdouble time;

    time = get_remaining_time (download);
    if (time > 0) {
      char *remaining;

      remaining = duration_to_string ((guint)time);
      download_label = g_strdup_printf ("%d%% (%s)", progress, remaining);
      g_free (remaining);
    }
  } else {
    gint64 current_size;

    /* Unknown content length, show received bytes instead. */
    current_size = webkit_download_get_received_data_length (download);
    if (current_size > 0)
      download_label = g_format_size (current_size);
  }

  if (download_label) {
    update_download_label_and_tooltip (widget, download_label);
    g_free (download_label);
  }
}

static void
update_popup_menu (EphyDownloadWidget *widget)
{
  gtk_widget_set_sensitive (widget->priv->cancel_menuitem, !widget->priv->finished);
  gtk_widget_set_visible (widget->priv->cancel_menuitem, !widget->priv->finished);
  gtk_widget_set_sensitive (widget->priv->open_menuitem, widget->priv->finished);
  gtk_widget_set_sensitive (widget->priv->show_folder_menuitem, widget->priv->finished);
}

static void
widget_attention_needed (EphyDownloadWidget *widget)
{
  gtk_style_context_add_class (gtk_widget_get_style_context (widget->priv->button), "needs-attention");
}

static void
widget_attention_unneeded (EphyDownloadWidget *widget)
{
  gtk_style_context_remove_class (gtk_widget_get_style_context (widget->priv->button), "needs-attention");
}

static void
widget_finished_cb (WebKitDownload *download,
                    EphyDownloadWidget *widget)
{
  widget->priv->finished = TRUE;
  update_popup_menu (widget);
  update_download_label_and_tooltip (widget, _("Finished"));
  widget_attention_needed (widget);
}

static void
widget_failed_cb (WebKitDownload *download,
                  GError *error,
                  EphyDownloadWidget *widget)
{
  char *error_msg;

  g_signal_handlers_disconnect_by_func (download, widget_finished_cb, widget);
  g_signal_handlers_disconnect_by_func (download, widget_progress_cb, widget);

  widget->priv->finished = TRUE;
  update_popup_menu (widget);
  error_msg = g_strdup_printf (_("Error downloading: %s"), error->message);
  gtk_label_set_text (GTK_LABEL (widget->priv->remaining), error_msg);
  gtk_widget_set_tooltip_text (GTK_WIDGET (widget), error_msg);
  g_free (error_msg);

  widget_attention_needed (widget);
}

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
add_popup_menu (EphyDownloadWidget *widget)
{
  GtkWidget *item;
  GtkWidget *menu;
  char *basename, *name;
  WebKitDownload *download;
  const char *dest;

  download = ephy_download_get_webkit_download (widget->priv->download);
  dest = webkit_download_get_destination (download);
  if (dest == NULL)
    return;

  basename = g_filename_display_basename (dest);
  name = ephy_uri_safe_unescape (basename);

  menu = gtk_menu_new ();
  gtk_widget_set_halign (menu, GTK_ALIGN_END);

  item = gtk_menu_item_new_with_label (name);
  gtk_widget_set_sensitive (item, FALSE);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  g_free (basename);
  g_free (name);

  widget->priv->cancel_menuitem = item = gtk_menu_item_new_with_label (_("Cancel"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  g_signal_connect (item, "activate",
                    G_CALLBACK (cancel_activate_cb), widget);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  widget->priv->open_menuitem = item = gtk_menu_item_new_with_label (_("Open"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  g_signal_connect (item, "activate",
                    G_CALLBACK (open_activate_cb), widget);

  widget->priv->show_folder_menuitem = item = gtk_menu_item_new_with_label (_("Show in folder"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  g_signal_connect (item, "activate",
                    G_CALLBACK (folder_activate_cb), widget);

  update_popup_menu (widget);

  gtk_widget_show_all (menu);

  gtk_menu_button_set_popup (GTK_MENU_BUTTON (widget->priv->menu_button), menu);
}

static void
widget_destination_changed_cb (WebKitDownload *download,
                               GParamSpec *pspec,
                               EphyDownloadWidget *widget)
{
  update_download_destination (widget);
  add_popup_menu (widget);
}

static void
disconnect_download (EphyDownloadWidget *widget)
{
  WebKitDownload *download;

  download = ephy_download_get_webkit_download (widget->priv->download);

  g_signal_handlers_disconnect_by_func (download, widget_progress_cb, widget);
  g_signal_handlers_disconnect_by_func (download, widget_destination_changed_cb, widget);
  g_signal_handlers_disconnect_by_func (download, widget_finished_cb, widget);
  g_signal_handlers_disconnect_by_func (download, widget_failed_cb, widget);

  ephy_download_set_widget (widget->priv->download, NULL);
}

static void
connect_download (EphyDownloadWidget *widget)
{
  WebKitDownload *download;

  download = ephy_download_get_webkit_download (widget->priv->download);

  g_signal_connect (download, "notify::estimated-progress",
                    G_CALLBACK (widget_progress_cb), widget);
  g_signal_connect (download, "notify::destination",
                    G_CALLBACK (widget_destination_changed_cb), widget);
  g_signal_connect (download, "finished",
                    G_CALLBACK (widget_finished_cb), widget);
  g_signal_connect (download, "failed",
                    G_CALLBACK (widget_failed_cb), widget);

  ephy_download_set_widget (widget->priv->download, GTK_WIDGET (widget));
}

static void
ephy_download_widget_set_download (EphyDownloadWidget *widget,
                                   EphyDownload       *download)
{
  g_return_if_fail (EPHY_IS_DOWNLOAD_WIDGET (widget));

  if (widget->priv->download == download)
    return;

  if (widget->priv->download != NULL) {
    disconnect_download (widget);
    g_object_unref (widget->priv->download);
  }

  widget->priv->download = NULL;

  if (download != NULL) {
    widget->priv->download = g_object_ref (download);
    connect_download (widget);
  }

  update_download_icon (widget);
  update_download_destination (widget);
  add_popup_menu (widget);

  g_object_notify (G_OBJECT (widget), "download");
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
      ephy_download_widget_set_download (widget, g_value_get_object (value));
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

  if (widget->priv->download != NULL) {
    disconnect_download (widget);
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
smallify_label (GtkLabel *label)
{
        PangoAttrList *attrs;
        attrs = pango_attr_list_new ();
        pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_SMALL));
        gtk_label_set_attributes (label, attrs);
        pango_attr_list_unref (attrs);
}

static void
create_widget (EphyDownloadWidget *widget)
{

  GtkWidget *grid;
  GtkWidget *icon;
  GtkWidget *text;
  GtkWidget *button;
  GtkWidget *menu_button;
  GtkWidget *remain;

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 6);

  button = gtk_button_new ();
  menu_button = gtk_menu_button_new ();
  gtk_menu_button_set_direction (GTK_MENU_BUTTON (menu_button), GTK_ARROW_UP);

  icon = gtk_image_new ();

  text = gtk_label_new (NULL);
  smallify_label (GTK_LABEL (text));
  gtk_misc_set_alignment (GTK_MISC (text), 0, 0.5);
  gtk_label_set_ellipsize (GTK_LABEL (text), PANGO_ELLIPSIZE_END);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_LABEL (text)), "filename");

  remain = gtk_label_new (_("Starting…"));
  smallify_label (GTK_LABEL (remain));
  gtk_misc_set_alignment (GTK_MISC (remain), 0, 0.5);
  gtk_label_set_ellipsize (GTK_LABEL (remain), PANGO_ELLIPSIZE_END);

  gtk_grid_attach (GTK_GRID (grid), icon, 0, 0, 1, 2);
  gtk_grid_attach (GTK_GRID (grid), text, 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), remain, 1, 1, 1, 1);

  widget->priv->text = text;
  widget->priv->icon = icon;
  widget->priv->button = button;
  widget->priv->remaining = remain;
  widget->priv->menu_button = menu_button;

  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_HALF);

  gtk_container_add (GTK_CONTAINER (button), grid);

  gtk_box_pack_start (GTK_BOX (widget), button, FALSE, FALSE, 0);
  gtk_box_pack_end (GTK_BOX (widget), menu_button, FALSE, FALSE, 0);

  g_signal_connect (button, "clicked",
                    G_CALLBACK (download_clicked_cb), widget);
  g_signal_connect_swapped (menu_button, "clicked",
                            G_CALLBACK (widget_attention_unneeded), widget);

  gtk_widget_show_all (button);
  gtk_widget_show_all (menu_button);

}

static void
ephy_download_widget_init (EphyDownloadWidget *self)
{
  GtkStyleContext *context;

  self->priv = DOWNLOAD_WIDGET_PRIVATE (self);

  create_widget (self);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_HORIZONTAL);
  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_LINKED);

  g_object_set (self,
                "margin", 2,
                NULL);
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

  g_return_val_if_fail (EPHY_IS_DOWNLOAD (ephy_download), NULL);

  widget = g_object_new (EPHY_TYPE_DOWNLOAD_WIDGET,
                         "download", ephy_download, NULL);

  return GTK_WIDGET (widget);
}
