/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2007 Xan Lopez
 *  Copyright © 2008 Jan Alonzo
 *  Copyright © 2009 Gustavo Noronha Silva
 *  Copyright © 2009 Igalia S.L.
 *  Copyright © 2009 Collabora Ltd.
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "downloader-view.h"
#include "eel-gconf-extensions.h"
#include "ephy-adblock-manager.h"
#include "ephy-debug.h"
#include "ephy-embed.h"
#include "ephy-embed-event.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-embed-persist.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-utils.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-history.h"
#include "ephy-prefs.h"
#include "ephy-stock-icons.h"
#include "ephy-string.h"
#include "ephy-web-view.h"

#include <errno.h>
#include <glib/gi18n.h>
#include <string.h>
#include <webkit/webkit.h>

static void     ephy_embed_class_init  (EphyEmbedClass *klass);
static void     ephy_embed_init        (EphyEmbed *gs);
static void     ephy_embed_constructed (GObject *object);

#define EPHY_EMBED_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED, EphyEmbedPrivate))

struct EphyEmbedPrivate
{
  WebKitWebView *web_view;
  EphyHistory *history;
  GtkWidget *inspector_window;
  char *load_failed_uri;
  guint is_setting_zoom : 1;
};

G_DEFINE_TYPE (EphyEmbed, ephy_embed, GTK_TYPE_SCROLLED_WINDOW)

static void
uri_changed_cb (WebKitWebView *web_view,
                GParamSpec *spec,
                EphyEmbed *embed)
{
  char *uri;
  const char *current_address;

  g_object_get (web_view, "uri", &uri, NULL);
  current_address = ephy_web_view_get_address (EPHY_WEB_VIEW (web_view));

  /* We need to check if we get URI notifications without going
     through the usual load process, as this can happen when changing
     location within a page */
  if (g_str_equal (uri, current_address) == FALSE)
    ephy_web_view_set_address (EPHY_WEB_VIEW (web_view), uri);

  g_free (uri);
}

static void
title_changed_cb (WebKitWebView *web_view,
                  GParamSpec *spec,
                  EphyEmbed *embed)
{
  const char *uri;
  char *title;
  WebKitWebFrame *frame;

  g_object_get (web_view, "title", &title, NULL);

  ephy_web_view_set_title (EPHY_WEB_VIEW (web_view),
                           title);

  frame = webkit_web_view_get_main_frame (web_view);
  uri = webkit_web_frame_get_uri (frame);
  ephy_history_set_page_title (EPHY_EMBED (embed)->priv->history,
                               uri,
                               title);
  g_free (title);

}

static void
restore_zoom_level (EphyEmbed *embed,
                    const char *address)
{
  EphyEmbedPrivate *priv = embed->priv;

  /* restore zoom level */
  if (ephy_embed_utils_address_has_web_scheme (address)) {
    EphyHistory *history;
    EphyNode *host;
    WebKitWebView *web_view;
    GValue value = { 0, };
    float zoom = 1.0, current_zoom;

    history = EPHY_HISTORY
              (ephy_embed_shell_get_global_history (embed_shell));
    host = ephy_history_get_host (history, address);

    if (host != NULL && ephy_node_get_property
        (host, EPHY_NODE_HOST_PROP_ZOOM, &value)) {
      zoom = g_value_get_float (&value);
      g_value_unset (&value);
    }

    web_view = priv->web_view;

    g_object_get (web_view, "zoom-level", &current_zoom,
                  NULL);

    if (zoom != current_zoom) {
      priv->is_setting_zoom = TRUE;
      g_object_set (web_view, "zoom-level", zoom, NULL);
      priv->is_setting_zoom = FALSE;
    }
  }
}

static void
resource_request_starting_cb (WebKitWebView *web_view,
                              WebKitWebFrame *web_frame,
                              WebKitWebResource *web_resource,
                              WebKitNetworkRequest *request,
                              WebKitNetworkResponse *response,
                              EphyEmbed *embed)
{
  EphyAdBlockManager *adblock_manager = EPHY_ADBLOCK_MANAGER (ephy_embed_shell_get_adblock_manager (embed_shell));
  const char *uri = webkit_network_request_get_uri (request);

  /* FIXME: How do we implement the other CHECK_TYPEs?  Perhaps we
   * should figure out a way of adding more information about what the
   * resource is for to WebResource? */
  if (!ephy_adblock_manager_should_load (adblock_manager, embed, uri,
                                         AD_URI_CHECK_TYPE_OTHER)) {
    g_signal_emit_by_name (EPHY_WEB_VIEW (web_view),
                           "content-blocked", uri);

    webkit_network_request_set_uri (request, "about:blank");
  }
}

static void
load_status_changed_cb (WebKitWebView *view,
                        GParamSpec *spec,
                        EphyEmbed *embed)
{
  WebKitLoadStatus status = webkit_web_view_get_load_status (view);

  if (status == WEBKIT_LOAD_COMMITTED) {
    const gchar* uri;
    EphyWebViewSecurityLevel security_level;

    uri = webkit_web_view_get_uri (view);

    /* If the load failed for this URI, do nothing */
    if (embed->priv->load_failed_uri &&
        g_str_equal (embed->priv->load_failed_uri, uri)) {
      g_free (embed->priv->load_failed_uri);
      embed->priv->load_failed_uri = NULL;

      ephy_web_view_set_security_level (EPHY_WEB_VIEW (view), EPHY_WEB_VIEW_STATE_IS_UNKNOWN);

      return;
    }

    ephy_web_view_location_changed (EPHY_WEB_VIEW (view),
                                    uri);
    restore_zoom_level (embed, uri);
    ephy_history_add_page (embed->priv->history,
                           uri,
                           FALSE,
                           FALSE);

    /*
     * FIXME: as a temporary workaround while soup lacks the needed
     * security API, determine security level based on the existence of
     * a 'https' prefix for the URI
     */
    if (uri && g_str_has_prefix (uri, "https"))
      security_level = EPHY_WEB_VIEW_STATE_IS_SECURE_HIGH;
    else
      security_level = EPHY_WEB_VIEW_STATE_IS_UNKNOWN;

    ephy_web_view_set_security_level (EPHY_WEB_VIEW (view), security_level);
  } else if (status == WEBKIT_LOAD_PROVISIONAL || status == WEBKIT_LOAD_FINISHED) {
    EphyWebViewNetState estate = EPHY_WEB_VIEW_STATE_UNKNOWN;
    const char *loading_uri = NULL;

    if (status == WEBKIT_LOAD_PROVISIONAL) {
      WebKitWebFrame *frame;
      WebKitWebDataSource *source;
      WebKitNetworkRequest *request;

      frame = webkit_web_view_get_main_frame (view);
      source = webkit_web_frame_get_provisional_data_source (frame);
      request = webkit_web_data_source_get_initial_request (source);
      loading_uri = webkit_network_request_get_uri (request);

      estate = (EphyWebViewNetState) (estate |
                                      EPHY_WEB_VIEW_STATE_START |
                                      EPHY_WEB_VIEW_STATE_NEGOTIATING |
                                      EPHY_WEB_VIEW_STATE_IS_REQUEST |
                                      EPHY_WEB_VIEW_STATE_IS_NETWORK);
      
      g_signal_emit_by_name (EPHY_WEB_VIEW (view), "new-document-now", loading_uri);
    } else if (status == WEBKIT_LOAD_FINISHED) {
      loading_uri = ephy_web_view_get_address (EPHY_WEB_VIEW (view));

      estate = (EphyWebViewNetState) (estate |
                                      EPHY_WEB_VIEW_STATE_STOP |
                                      EPHY_WEB_VIEW_STATE_IS_DOCUMENT |
                                      EPHY_WEB_VIEW_STATE_IS_NETWORK);
    }
    
    ephy_web_view_update_from_net_state (EPHY_WEB_VIEW (view),
                                         loading_uri,
                                         (EphyWebViewNetState)estate);
  }
}

static void
hovering_over_link_cb (WebKitWebView *web_view,
                       char *title,
                       char *location,
                       EphyEmbed *embed)
{
  ephy_web_view_set_link_message (EPHY_WEB_VIEW (web_view), location);
}

static void
zoom_changed_cb (WebKitWebView *web_view,
                 GParamSpec *pspec,
                 EphyEmbed  *embed)
{
  char *address;
  float zoom;

  g_object_get (web_view,
                "zoom-level", &zoom,
                NULL);

  if (EPHY_EMBED (embed)->priv->is_setting_zoom) {
    return;
  }

  address = ephy_web_view_get_location (EPHY_WEB_VIEW (web_view), TRUE);
  if (ephy_embed_utils_address_has_web_scheme (address)) {
    EphyHistory *history;
    EphyNode *host;
    history = EPHY_HISTORY
      (ephy_embed_shell_get_global_history (embed_shell));
    host = ephy_history_get_host (history, address);

    if (host != NULL) {
      ephy_node_set_property_float (host,
                                    EPHY_NODE_HOST_PROP_ZOOM,
                                    zoom);
    }
  }

  g_free (address);
}

static void
ephy_embed_grab_focus (GtkWidget *widget)
{
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child)
    gtk_widget_grab_focus (child);
}

static void
ephy_embed_finalize (GObject *object)
{
  EphyEmbed *embed = EPHY_EMBED (object);

  g_free (embed->priv->load_failed_uri);

  G_OBJECT_CLASS (ephy_embed_parent_class)->finalize (object);
}

static void
ephy_embed_class_init (EphyEmbedClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

  object_class->constructed = ephy_embed_constructed;
  object_class->finalize = ephy_embed_finalize;
  widget_class->grab_focus = ephy_embed_grab_focus;

  g_type_class_add_private (G_OBJECT_CLASS (klass), sizeof(EphyEmbedPrivate));
}

static WebKitWebView *
ephy_embed_inspect_web_view_cb (WebKitWebInspector *inspector,
                                  WebKitWebView *web_view,
                                  gpointer data)
{
  GtkWidget *inspector_sw = GTK_WIDGET (data);
  GtkWidget *inspector_web_view;

  inspector_web_view = ephy_web_view_new ();
  gtk_container_add (GTK_CONTAINER (inspector_sw), inspector_web_view);

  gtk_widget_show_all (gtk_widget_get_toplevel (inspector_sw));

  return WEBKIT_WEB_VIEW (inspector_web_view);
}

static gboolean
ephy_embed_inspect_show_cb (WebKitWebInspector *inspector,
                              GtkWidget *widget)
{
  gtk_widget_show (widget);
  return TRUE;
}

static gboolean
ephy_embed_inspect_close_cb (WebKitWebInspector *inspector,
                               GtkWidget *widget)
{
  gtk_widget_hide (widget);
  return TRUE;
}

static gboolean
mime_type_policy_decision_requested_cb (WebKitWebView *web_view,
                                        WebKitWebFrame *frame,
                                        WebKitNetworkRequest *request,
                                        const char *mime_type,
                                        WebKitWebPolicyDecision *decision,
                                        EphyEmbed *embed)
{
  EphyWebViewDocumentType type;

  g_return_val_if_fail (mime_type, FALSE);

  /* Get the mime type for the page only from the main frame */
  if (webkit_web_view_get_main_frame (web_view) == frame) {
    type = EPHY_WEB_VIEW_DOCUMENT_OTHER;

    if (!strcmp (mime_type, "text/html"))
      type = EPHY_WEB_VIEW_DOCUMENT_HTML;
    else if (!strcmp (mime_type, "application/xhtml+xml"))
      type = EPHY_WEB_VIEW_DOCUMENT_XML;
    else if (!strncmp (mime_type, "image/", 6))
      type = EPHY_WEB_VIEW_DOCUMENT_IMAGE;

    /* FIXME: mayb  e it makes more sense to have an API to query the mime
     * type when the load of a page starts than doing this here.
     */
    /* FIXME: rename ge-document-type (and all ge- signals...) to
     * something else
     */
    g_signal_emit_by_name (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed), "ge-document-type", type);
  }

  /* If WebKit can't handle the mime type start the download
     process */
  /* FIXME: need to use ephy_file_check_mime if auto-downloading */
  if (!webkit_web_view_can_show_mime_type (web_view, mime_type)) {
    GObject *single;
    const char *uri;
    gboolean handled = FALSE;

    single = ephy_embed_shell_get_embed_single (embed_shell);
    uri = webkit_network_request_get_uri (request);
    g_signal_emit_by_name (single, "handle-content", mime_type, uri, &handled);

    if (handled)
      webkit_web_policy_decision_ignore (decision);
    else
      webkit_web_policy_decision_download (decision);

    return TRUE;
  }

  return FALSE;
}

static void
download_requested_dialog_response_cb (GtkDialog *dialog,
                                       int response_id,
                                       WebKitDownload *download)
{
  if (response_id == GTK_RESPONSE_ACCEPT) {
    DownloaderView *dview;
    char *uri;

    uri = gtk_file_chooser_get_uri  (GTK_FILE_CHOOSER(dialog));
    webkit_download_set_destination_uri (download, uri);
    g_free (uri);

    dview = EPHY_DOWNLOADER_VIEW (ephy_embed_shell_get_downloader_view (embed_shell));
    downloader_view_add_download (dview, download);
  } else {
    webkit_download_cancel (download);
    ephy_file_delete_uri (webkit_download_get_destination_uri (download));
  }

  gtk_widget_destroy (GTK_WIDGET (dialog));
  g_object_unref (download);
}

static void
request_destination_uri (WebKitWebView *web_view,
                         WebKitDownload *download)
{
  EphyFileChooser *dialog;
  GtkWidget *window;
  const char *suggested_filename;

  suggested_filename = webkit_download_get_suggested_filename (download);

  /*
   * Try to get the toplevel window related to the WebView that caused
   * the download, and use NULL otherwise; we don't want to pass the
   * WebView or other widget as a parent window.
   */
  window = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
  if (!gtk_widget_is_toplevel (window))
    window = NULL;

  dialog = ephy_file_chooser_new (_("Save"),
                                  window,
                                  GTK_FILE_CHOOSER_ACTION_SAVE,
                                  CONF_STATE_SAVE_DIR,
                                  EPHY_FILE_FILTER_ALL_SUPPORTED);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), suggested_filename);

  g_signal_connect (dialog, "response",
                    G_CALLBACK (download_requested_dialog_response_cb), download);

  gtk_widget_show_all (GTK_WIDGET (dialog));
}

/* From the old embed/mozilla/MozDownload.cpp */
static const char*
file_is_compressed (const char *filename)
{
  int i;
  static const char * const compression[] = {".gz", ".bz2", ".Z", ".lz", NULL};

  for (i = 0; compression[i] != NULL; i++) {
    if (g_str_has_suffix (filename, compression[i]))
      return compression[i];
  }

  return NULL;
}

static const char*
parse_extension (const char *filename)
{
  const char *compression;

  compression = file_is_compressed (filename);

  /* if the file is compressed we might have a double extension */
  if (compression != NULL) {
    int i;
    static const char * const extensions[] = {"tar", "ps", "xcf", "dvi", "txt", "text", NULL};

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
  return g_strrstr (filename, ".");
}

static gboolean
define_destination_uri (WebKitDownload *download,
                        gboolean temporary)
{
  char *tmp_dir;
  char *destination_filename;
  char *destination_uri;
  const char *suggested_filename;

  suggested_filename = webkit_download_get_suggested_filename (download);

  /* If we are not doing an automatic download, use a temporary file
   * to start the download while we ask the user for the location to
   * where the file must go.
   */
  if (temporary)
    tmp_dir = g_build_filename (ephy_dot_dir (), "downloads", NULL);
  else
    tmp_dir = ephy_file_get_downloads_dir ();

  /* Make sure the download directory exists */
  if (g_mkdir_with_parents (tmp_dir, 0700) == -1) {
    g_critical ("Could not create downloads directory \"%s\": %s",
                tmp_dir, strerror (errno));
    g_free (tmp_dir);
    return FALSE;
  }

  destination_filename = g_build_filename (tmp_dir, suggested_filename, NULL);

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

  destination_uri = g_strconcat ("file://", destination_filename, NULL);

  webkit_download_set_destination_uri (download, destination_uri);

  g_free (tmp_dir);
  g_free (destination_filename);
  g_free (destination_uri);

  return TRUE;
}

static void
perform_auto_download (WebKitDownload *download)
{
  DownloaderView *dview;

  if (!define_destination_uri (download, FALSE)) {
    webkit_download_cancel (download);
    ephy_file_delete_uri (webkit_download_get_destination_uri (download));
    return;
  }

  dview = EPHY_DOWNLOADER_VIEW (ephy_embed_shell_get_downloader_view (embed_shell));

  g_object_set_data (G_OBJECT(download), "download-action", GINT_TO_POINTER(DOWNLOAD_ACTION_OPEN));
  downloader_view_add_download (dview, download);
}

static void
confirm_action_response_cb (GtkWidget *dialog,
                            int response,
                            WebKitDownload *download)
{
  WebKitWebView *web_view = g_object_get_data (G_OBJECT(dialog), "webkit-view");
  DownloaderView *dview;

  gtk_widget_destroy (dialog);

  if (response > 0) {
    switch (response) {
    case DOWNLOAD_ACTION_OPEN:
      g_object_set_data (G_OBJECT (download), "download-action",
                         GINT_TO_POINTER (DOWNLOAD_ACTION_OPEN));
      break;
    case DOWNLOAD_ACTION_DOWNLOAD:
    case DOWNLOAD_ACTION_OPEN_LOCATION:
      g_object_set_data (G_OBJECT (download), "download-action",
                         GINT_TO_POINTER (DOWNLOAD_ACTION_OPEN_LOCATION));
      break;
    }

    if (response == DOWNLOAD_ACTION_DOWNLOAD) {
      /* balanced in download_requested_dialog_response_cb */
      g_object_ref (download);
      request_destination_uri (web_view, download);
    } else {
      if (!define_destination_uri (download, FALSE)) {
        goto cleanup;
      }
      dview = EPHY_DOWNLOADER_VIEW (ephy_embed_shell_get_downloader_view (embed_shell));
      downloader_view_add_download (dview, download);
    }
    g_object_unref (download);
    return;
  }

cleanup:
  webkit_download_cancel (download);
  ephy_file_delete_uri (webkit_download_get_destination_uri (download));
  g_object_unref (download);
}

static void
confirm_action_from_mime (WebKitWebView *web_view,
                          WebKitDownload *download,
                          DownloadAction action)
{
  GtkWidget *parent_window;
  GtkWidget *dialog, *button, *image;
  const char *action_label;
  char *mime_description;
  EphyMimePermission mime_permission;
  GAppInfo *helper_app;
  const char *suggested_filename;
  int default_response;
  WebKitNetworkResponse *response;
  SoupMessage *message;

  parent_window = gtk_widget_get_toplevel (GTK_WIDGET(web_view));
  if (!gtk_widget_is_toplevel (parent_window))
    parent_window = NULL;

  helper_app = NULL;
  mime_description = NULL;
  mime_permission = EPHY_MIME_PERMISSION_SAFE;

  response = webkit_download_get_network_response (download);
  message = webkit_network_response_get_message (response);

  if (message) {
    const char *content_type = soup_message_headers_get_content_type (message->response_headers, NULL);

    if (content_type) {
      mime_description = g_content_type_get_description (content_type);
      helper_app = g_app_info_get_default_for_type (content_type, FALSE);

      if (helper_app) {
        action = DOWNLOAD_ACTION_OPEN;
      }
    }
  }

  if (mime_description == NULL) {
    mime_description = g_strdup (C_("file type", "Unknown"));
    action = DOWNLOAD_ACTION_OPEN_LOCATION;
  }

  action_label = (action == DOWNLOAD_ACTION_OPEN) ? GTK_STOCK_OPEN : STOCK_DOWNLOAD;
  suggested_filename = webkit_download_get_suggested_filename (download);

  if (mime_permission != EPHY_MIME_PERMISSION_SAFE && helper_app) {
    dialog = gtk_message_dialog_new (GTK_WINDOW (parent_window),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
                                     _("Download this potentially unsafe file?"));

    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              /* translators: First %s is the file type description,
                                                 Second %s is the file name */
                                              _("File Type: “%s”.\n\nIt is unsafe to open “%s” as "
                                                "it could potentially damage your documents or "
                                                "invade your privacy. You can download it instead."),
                                              mime_description, suggested_filename);
  } else if (action == DOWNLOAD_ACTION_OPEN && helper_app) {
    dialog = gtk_message_dialog_new (GTK_WINDOW (parent_window),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                     _("Open this file?"));

    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              /* translators: First %s is the file type description,
                                                 Second %s is the file name,
                                                 Third %s is the application used to open the file */
                                              _("File Type: “%s”.\n\nYou can open “%s” using “%s” or save it."),
                                              mime_description, suggested_filename,
                                              g_app_info_get_name (helper_app));
  } else  {
    dialog = gtk_message_dialog_new (GTK_WINDOW (parent_window),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                     _("Download this file?"));

    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                              /* translators: First %s is the file type description,
                                                 Second %s is the file name */
                                              _("File Type: “%s”.\n\nYou have no application able to open “%s”. "
                                                "You can download it instead."),
                                              mime_description, suggested_filename);
  }

  g_free (mime_description);

  button = gtk_button_new_with_mnemonic (_("_Save As..."));
  image = gtk_image_new_from_stock (GTK_STOCK_SAVE_AS, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  /* don't show the image! see bug #307818 */
  gtk_widget_show (button);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, DOWNLOAD_ACTION_DOWNLOAD);

  gtk_dialog_add_button (GTK_DIALOG (dialog),
                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (dialog),
                         action_label, action);

  gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);

  default_response = action == DOWNLOAD_ACTION_NONE
        ? (int) GTK_RESPONSE_CANCEL
        : (int) action;
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), default_response);

  g_object_set_data (G_OBJECT (dialog), "webkit-view", web_view);
  g_signal_connect (dialog, "response",
                    G_CALLBACK (confirm_action_response_cb),
                    download);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
download_status_changed_cb (GObject *object,
                            GParamSpec *pspec,
                            WebKitWebView *web_view)
{
  WebKitDownload *download = WEBKIT_DOWNLOAD (object);

  g_return_if_fail (webkit_download_get_status (download) == WEBKIT_DOWNLOAD_STATUS_STARTED);

  g_signal_handlers_disconnect_by_func (download,
                                        download_status_changed_cb,
                                        web_view);

  /* Is auto-download enabled? */
  if (eel_gconf_get_boolean (CONF_AUTO_DOWNLOADS)) {
    perform_auto_download (download);
    return;
  }

  g_object_ref (download); /* balanced in confirm_action_response_cb */
  confirm_action_from_mime (web_view, download, DOWNLOAD_ACTION_DOWNLOAD);
}

static gboolean
download_requested_cb (WebKitWebView *web_view,
                       WebKitDownload *download)
{
  /* Is download locked down? */
  if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_SAVE_TO_DISK))
    return FALSE;

  /* Wait for the request to be sent in all cases, so that we have a
   * response which may contain a suggested filename */
  g_signal_connect (download, "notify::status",
                    G_CALLBACK (download_status_changed_cb),
                    web_view);

  /* If we are not performing an auto-download, we will ask the user
   * where they want the file to go to; we will start downloading to a
   * temporary location while the user decides.
   */
  if (!define_destination_uri (download, TRUE))
    return FALSE;

  return TRUE;
}

static gboolean
load_error_cb (WebKitWebView *web_view,
               WebKitWebFrame *frame,
               const char *uri,
               GError *error,
               EphyEmbed *embed)
{
  /* Flag the page as error. We need the flag to check it when
     receiving COMMITTED status, since for some reason we are getting
     that when the load fails too */
  embed->priv->load_failed_uri = g_strdup (uri);

  return FALSE;
}

static void
ephy_embed_constructed (GObject *object)
{
  EphyEmbed *embed = (EphyEmbed*)object;
  WebKitWebView *web_view;
  WebKitWebInspector *inspector;
  GtkWidget *inspector_sw;

  embed->priv = EPHY_EMBED_GET_PRIVATE (embed);
  web_view = WEBKIT_WEB_VIEW (ephy_web_view_new ());
  embed->priv->web_view = web_view;
  gtk_container_add (GTK_CONTAINER (embed), GTK_WIDGET (web_view));
  gtk_widget_show (GTK_WIDGET (web_view));

  g_object_connect (web_view,
                    "signal::notify::load-status", G_CALLBACK (load_status_changed_cb), embed,
                    "signal::resource-request-starting", G_CALLBACK (resource_request_starting_cb), embed,
                    "signal::hovering-over-link", G_CALLBACK (hovering_over_link_cb), embed,
                    "signal::mime-type-policy-decision-requested", G_CALLBACK (mime_type_policy_decision_requested_cb), embed,
                    "signal::download-requested", G_CALLBACK (download_requested_cb), embed,
                    "signal::notify::zoom-level", G_CALLBACK (zoom_changed_cb), embed,
                    "signal::notify::title", G_CALLBACK (title_changed_cb), embed,
                    "signal::notify::uri", G_CALLBACK (uri_changed_cb), embed,
                    "signal::load-error", G_CALLBACK (load_error_cb), embed,
                    NULL);

  embed->priv->inspector_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  inspector = webkit_web_view_get_inspector (web_view);

  inspector_sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (inspector_sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (embed->priv->inspector_window),
                     inspector_sw);

  gtk_window_set_title (GTK_WINDOW (embed->priv->inspector_window),
                        _("Web Inspector"));
  gtk_window_set_default_size (GTK_WINDOW (embed->priv->inspector_window),
                               600, 400);

  g_signal_connect (embed->priv->inspector_window,
                    "delete-event", G_CALLBACK (gtk_widget_hide_on_delete),
                    NULL);

  g_object_connect (inspector,
                    "signal::inspect-web-view", G_CALLBACK (ephy_embed_inspect_web_view_cb),
                    inspector_sw,
                    "signal::show-window", G_CALLBACK (ephy_embed_inspect_show_cb),
                    embed->priv->inspector_window,
                    "signal::close-window", G_CALLBACK (ephy_embed_inspect_close_cb),
                    embed->priv->inspector_window,
                    NULL);

  ephy_embed_prefs_add_embed (embed);

  embed->priv->history = EPHY_HISTORY (ephy_embed_shell_get_global_history (ephy_embed_shell_get_default ()));
}

static void
ephy_embed_init (EphyEmbed *embed)
{
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (embed),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
}

/**
 * ephy_embed_get_web_view:
 * @embed: and #EphyEmbed
 * 
 * Returns the #EphyWebView wrapped by @embed.
 * 
 * Returns: (transfer none): an #EphyWebView
 **/
EphyWebView*
ephy_embed_get_web_view (EphyEmbed *embed)
{
  g_return_val_if_fail (EPHY_IS_EMBED (embed), NULL);

  return EPHY_WEB_VIEW (embed->priv->web_view);
}
