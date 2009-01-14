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
#include "ephy-command-manager.h"
#include "ephy-debug.h"
#include "ephy-file-chooser.h"
#include "ephy-file-helpers.h"
#include "ephy-history.h"
#include "ephy-embed-factory.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-embed-persist.h"
#include "ephy-stock-icons.h"
#include "ephy-string.h"
#include "ephy-embed-event.h"
#include "ephy-embed-utils.h"
#include "ephy-prefs.h"
#include "ephy-web-view.h"

#include <webkit/webkit.h>
#include <errno.h>
#include <string.h>
#include <glib/gi18n.h>

#include "webkit-embed.h"
#include "webkit-embed-prefs.h"
#include "ephy-embed.h"
#include "ephy-base-embed.h"

static void     webkit_embed_class_init (WebKitEmbedClass *klass);
static void     webkit_embed_init       (WebKitEmbed *gs);
static void     ephy_embed_iface_init   (EphyEmbedIface *iface);

#define WEBKIT_EMBED_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), WEBKIT_TYPE_EMBED, WebKitEmbedPrivate))

typedef enum
{
    WEBKIT_EMBED_LOAD_STARTED,
    WEBKIT_EMBED_LOAD_REDIRECTING,
    WEBKIT_EMBED_LOAD_LOADING,
    WEBKIT_EMBED_LOAD_STOPPED
} WebKitEmbedLoadState;

struct WebKitEmbedPrivate
{
  WebKitWebView *web_view;
  GtkScrolledWindow *scrolled_window;
  WebKitEmbedLoadState load_state;
  char *loading_uri;
  EphyHistory *history;
  GtkWidget *inspector_window;
  guint is_setting_zoom : 1;
};

static void
impl_manager_do_command (EphyCommandManager *manager,
                         const char *command)
{
  WebKitWebView *web_view = WEBKIT_EMBED (manager)->priv->web_view;

  if (! strcmp (command, "cmd_copy"))
    return webkit_web_view_copy_clipboard (web_view);
  else if (! strcmp (command, "cmd_cut"))
    return webkit_web_view_cut_clipboard (web_view);
  else if (! strcmp (command, "cmd_paste"))
    return webkit_web_view_paste_clipboard (web_view);
  else if (! strcmp (command, "cmd_selectAll"))
    return webkit_web_view_select_all (web_view);
}

static gboolean
impl_manager_can_do_command (EphyCommandManager *manager,
                             const char *command)
{
  WebKitWebView *web_view = WEBKIT_EMBED (manager)->priv->web_view;

  if (! strcmp (command, "cmd_copy"))
    return webkit_web_view_can_copy_clipboard (web_view);
  else if (! strcmp (command, "cmd_cut"))
    return webkit_web_view_can_cut_clipboard (web_view);
  else if (! strcmp (command, "cmd_paste"))
    return webkit_web_view_can_paste_clipboard (web_view);

  return FALSE;
}

static void
ephy_command_manager_iface_init (EphyCommandManagerIface *iface)
{
  iface->do_command = impl_manager_do_command;
  iface->can_do_command = impl_manager_can_do_command;
}

G_DEFINE_TYPE_WITH_CODE (WebKitEmbed, webkit_embed, EPHY_TYPE_BASE_EMBED,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_EMBED,
                                                ephy_embed_iface_init)
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_COMMAND_MANAGER,
                                                ephy_command_manager_iface_init))

static void
title_changed_cb (WebKitWebView *web_view,
                  GParamSpec *spec,
                  EphyEmbed *embed)
{
  const char *uri;
  char *title;
  WebKitWebFrame *frame;

  g_object_get (web_view, "title", &title, NULL);

  ephy_base_embed_set_title (EPHY_BASE_EMBED (embed),
                             title);

  frame = webkit_web_view_get_main_frame (web_view);
  uri = webkit_web_frame_get_uri (frame);
  ephy_history_set_page_title (WEBKIT_EMBED (embed)->priv->history,
                               uri,
                               title);
  g_free (title);

}

static void
update_load_state (WebKitEmbed *embed, WebKitWebView *web_view)
{
  EphyEmbedNetState estate = EPHY_EMBED_STATE_UNKNOWN;

  if (embed->priv->load_state == WEBKIT_EMBED_LOAD_STARTED)
    {
      estate = (EphyEmbedNetState) (estate |
                                    EPHY_EMBED_STATE_START |
                                    EPHY_EMBED_STATE_NEGOTIATING |
                                    EPHY_EMBED_STATE_IS_REQUEST |
                                    EPHY_EMBED_STATE_IS_NETWORK);

      g_signal_emit_by_name (embed, "new-document-now", embed->priv->loading_uri);
    }

  if (embed->priv->load_state == WEBKIT_EMBED_LOAD_LOADING)
      estate = (EphyEmbedNetState) (estate |
                                    EPHY_EMBED_STATE_TRANSFERRING |
                                    EPHY_EMBED_STATE_IS_REQUEST |
                                    EPHY_EMBED_STATE_IS_NETWORK);

  if (embed->priv->load_state == WEBKIT_EMBED_LOAD_STOPPED)
      estate = (EphyEmbedNetState) (estate |
                                    EPHY_EMBED_STATE_STOP |
                                    EPHY_EMBED_STATE_IS_DOCUMENT |
                                    EPHY_EMBED_STATE_IS_NETWORK);

  ephy_base_embed_update_from_net_state (EPHY_BASE_EMBED (embed),
                                         embed->priv->loading_uri,
                                         (EphyEmbedNetState)estate);
}

static void
restore_zoom_level (WebKitEmbed *embed,
                    const char *address)
{
  WebKitEmbedPrivate *priv = embed->priv;

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
load_committed_cb (WebKitWebView *web_view,
                   WebKitWebFrame *web_frame,
                   EphyEmbed *embed)
{
  const gchar* uri;
  EphyEmbedSecurityLevel security_level;

  uri = webkit_web_frame_get_uri (web_frame);
  ephy_base_embed_location_changed (EPHY_BASE_EMBED (embed),
                                    uri);

  restore_zoom_level (WEBKIT_EMBED (embed), uri);
  ephy_history_add_page (WEBKIT_EMBED (embed)->priv->history,
                         uri,
                         FALSE,
                         FALSE);

  /*
   * FIXME: as a temporary workaround while soup lacks the needed
   * security API, determine security level based on the existence of
   * a 'https' prefix for the URI
   */
  if (uri && g_str_has_prefix (uri, "https"))
    security_level = EPHY_EMBED_STATE_IS_SECURE_HIGH;
  else
    security_level = EPHY_EMBED_STATE_IS_UNKNOWN;

  ephy_base_embed_set_security_level (EPHY_BASE_EMBED (embed), security_level);
}

static void
load_started_cb (WebKitWebView *web_view,
                 WebKitWebFrame *web_frame,
                 EphyEmbed *embed)
{
  WebKitEmbed *wembed = WEBKIT_EMBED (embed);
  wembed->priv->load_state = WEBKIT_EMBED_LOAD_STARTED;

  update_load_state (wembed, web_view);
}

static void
load_progress_changed_cb (WebKitWebView *web_view,
                          int progress,
                          EphyEmbed *embed)
{
  WebKitEmbed *wembed = WEBKIT_EMBED (embed);

  if (wembed->priv->load_state == WEBKIT_EMBED_LOAD_STARTED)
    wembed->priv->load_state = WEBKIT_EMBED_LOAD_LOADING;

  ephy_base_embed_set_load_percent (EPHY_BASE_EMBED (embed), progress);
}

static void
load_finished_cb (WebKitWebView *web_view,
                  WebKitWebFrame *web_frame,
                  EphyEmbed *embed)
{
  WebKitEmbed *wembed = WEBKIT_EMBED (embed);

  wembed->priv->load_state = WEBKIT_EMBED_LOAD_STOPPED;
  update_load_state (wembed, web_view);
}

static void
hovering_over_link_cb (WebKitWebView *web_view,
                       char *title,
                       char *location,
                       EphyEmbed *embed)
{
  ephy_base_embed_set_link_message (EPHY_BASE_EMBED (embed), location);
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

  if (WEBKIT_EMBED (embed)->priv->is_setting_zoom) {
    return;
  }

  address = ephy_embed_get_location (embed, TRUE);
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
webkit_embed_finalize (GObject *object)
{
  WebKitEmbed *wembed = WEBKIT_EMBED (object);

  g_free (wembed->priv->loading_uri);

  G_OBJECT_CLASS (webkit_embed_parent_class)->finalize (object);
}

static void
webkit_embed_class_init (WebKitEmbedClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = webkit_embed_finalize;

  g_type_class_add_private (object_class, sizeof(WebKitEmbedPrivate));
}

static WebKitWebView *
webkit_embed_inspect_web_view_cb (WebKitWebInspector *inspector,
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
webkit_embed_inspect_show_cb (WebKitWebInspector *inspector,
                              GtkWidget *widget)
{
  gtk_widget_show (widget);
  return TRUE;
}

static gboolean
webkit_embed_inspect_close_cb (WebKitWebInspector *inspector,
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
                                        WebKitEmbed *embed)
{
  EphyEmbedDocumentType type;

  g_return_val_if_fail (mime_type, FALSE);

  type = EPHY_EMBED_DOCUMENT_OTHER;

  if (!strcmp (mime_type, "text/html"))
    type = EPHY_EMBED_DOCUMENT_HTML;
  else if (!strcmp (mime_type, "application/xhtml+xml"))
    type = EPHY_EMBED_DOCUMENT_XML;
  else if (!strncmp (mime_type, "image/", 6))
    type = EPHY_EMBED_DOCUMENT_IMAGE;

  /* FIXME: maybe it makes more sense to have an API to query the mime
   * type when the load of a page starts than doing this here.
   */
  /* FIXME: rename ge-document-type (and all ge- signals...) to
   * something else
   */
  g_signal_emit_by_name (embed, "ge-document-type", type);

  /* If WebKit can't handle the mime type start the download
     process */
  /* FIXME: need to use ephy_file_check_mime if auto-downloading */
  if (!webkit_web_view_can_show_mime_type (web_view, mime_type)) {
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
  if (!GTK_WIDGET_TOPLEVEL (window))
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

static gboolean
define_destination_uri (WebKitDownload *download,
                        gboolean temporary)
{
  char *tmp_dir;
  char *tmp_filename;
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

  tmp_filename = g_build_filename (tmp_dir, suggested_filename, NULL);
  destination_uri = g_strconcat ("file://", tmp_filename, NULL);

  webkit_download_set_destination_uri (download, destination_uri);

  g_free (tmp_dir);
  g_free (tmp_filename);
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
  DownloaderView *dview = EPHY_DOWNLOADER_VIEW (ephy_embed_shell_get_downloader_view (embed_shell));

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

  parent_window = gtk_widget_get_toplevel (GTK_WIDGET(web_view));
  if (!GTK_WIDGET_TOPLEVEL (parent_window))
    parent_window = NULL;

  /* FIXME: we still have no way of getting the content type from
   * webkit yet; we need to have a proper WebKitNetworkRequest
   * implementation to do this here; see
   * https://bugs.webkit.org/show_bug.cgi?id=18608
   */
  helper_app = NULL;
  mime_description = NULL;
  mime_permission = EPHY_MIME_PERMISSION_SAFE;
  if (mime_description == NULL) {
    mime_description = g_strdup (C_("file type", "Unknown"));
    action = DOWNLOAD_ACTION_OPEN_LOCATION;
  }

  /* OPEN will never happen here, for now; see comment about
   * WebKitNetworkRequest above!
   */
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

  button = gtk_button_new_with_label (_("_Save As..."));
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

static gboolean
download_requested_cb (WebKitWebView *web_view,
                       WebKitDownload *download)
{
  /* Is download locked down? */
  if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_SAVE_TO_DISK))
    return FALSE;

  /* Is auto-download enabled? */
  if (eel_gconf_get_boolean (CONF_AUTO_DOWNLOADS)) {
    perform_auto_download (download);
    return TRUE;
  }

  /* If we are not performing an auto-download, we will ask the user
   * where they want the file to go to; we will start downloading to a
   * temporary location while the user decides.
   */
  if (!define_destination_uri (download, TRUE))
    return FALSE;

  /* FIXME: when we are able to obtain the MIME information from
   * WebKit, we will want to decide earlier whether we want to open or
   * open the location to where the file was downloaded. See
   * perform_auto_download, too.
   */
  g_object_ref (download); /* balanced in confirm_action_response_cb */
  confirm_action_from_mime (web_view, download, DOWNLOAD_ACTION_DOWNLOAD);

  return TRUE;
}
                                                  
static void
webkit_embed_init (WebKitEmbed *embed)
{
  WebKitWebView *web_view;
  WebKitWebInspector *inspector;
  GtkWidget *sw;
  GtkWidget *inspector_sw;

  embed->priv = WEBKIT_EMBED_GET_PRIVATE (embed);

  sw = gtk_scrolled_window_new (NULL, NULL);
  embed->priv->scrolled_window = GTK_SCROLLED_WINDOW (sw);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  web_view = WEBKIT_WEB_VIEW (ephy_web_view_new ());
  embed->priv->web_view = web_view;
  gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (web_view));
  gtk_widget_show (sw);
  gtk_widget_show (GTK_WIDGET (web_view));

  gtk_container_add (GTK_CONTAINER (embed), sw);

  g_object_connect (web_view,
                    "signal::load-committed", G_CALLBACK (load_committed_cb), embed,
                    "signal::load-started", G_CALLBACK (load_started_cb), embed,
                    "signal::load_finished", G_CALLBACK (load_finished_cb), embed,
                    "signal::load-progress-changed", G_CALLBACK (load_progress_changed_cb), embed,
                    "signal::hovering-over-link", G_CALLBACK (hovering_over_link_cb), embed,
                    "signal::mime-type-policy-decision-requested", G_CALLBACK (mime_type_policy_decision_requested_cb), embed,
                    "signal::download-requested", G_CALLBACK (download_requested_cb), embed,
                    "signal::notify::zoom-level", G_CALLBACK (zoom_changed_cb), embed,
                    "signal::notify::title", G_CALLBACK (title_changed_cb), embed,
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
                    "signal::inspect-web-view", G_CALLBACK (webkit_embed_inspect_web_view_cb),
                    inspector_sw,
                    "signal::show-window", G_CALLBACK (webkit_embed_inspect_show_cb),
                    embed->priv->inspector_window,
                    "signal::close-window", G_CALLBACK (webkit_embed_inspect_close_cb),
                    embed->priv->inspector_window,
                    NULL);

  webkit_embed_prefs_add_embed (embed);

  embed->priv->history = EPHY_HISTORY (ephy_embed_shell_get_global_history (ephy_embed_shell_get_default ()));
}

static void
impl_load_url (EphyEmbed *embed,
               const char *url)
{
  WebKitEmbed *wembed = WEBKIT_EMBED (embed);

  webkit_web_view_open (wembed->priv->web_view, url);
}

static void
impl_load (EphyEmbed *embed,
           const char *url,
           EphyEmbedLoadFlags flags,
           EphyEmbed *preview_embed)
{
  WebKitEmbed *wembed = WEBKIT_EMBED (embed);
  char *effective_url = NULL;

  /* WebKit cannot handle URLs without a protocol */
  if (ephy_embed_utils_address_has_web_scheme (url) == FALSE)
    effective_url = g_strconcat ("http://", url, NULL);
  else
    effective_url = g_strdup (url);

  g_free (wembed->priv->loading_uri);
  wembed->priv->loading_uri = g_strdup (effective_url);

  webkit_web_view_open (wembed->priv->web_view, effective_url);

  g_free (effective_url);
}

static gboolean
impl_can_go_up (EphyEmbed *embed)
{
  return FALSE;
}

static GSList *
impl_get_go_up_list (EphyEmbed *embed)
{
  return NULL;
}

static void
impl_go_up (EphyEmbed *embed)
{
}

static char *
impl_get_js_status (EphyEmbed *embed)
{
  return NULL;
}

static char *
impl_get_location (EphyEmbed *embed,
                   gboolean toplevel)
{
  WebKitWebFrame *web_frame = webkit_web_view_get_main_frame (WEBKIT_EMBED (embed)->priv->web_view);
  return g_strdup (webkit_web_frame_get_uri (web_frame));
}

static void
impl_shistory_copy (EphyEmbed *source,
                    EphyEmbed *dest,
                    gboolean copy_back,
                    gboolean copy_forward,
                    gboolean copy_current)
{
  WebKitWebView *source_view, *dest_view;
  WebKitWebBackForwardList* source_bflist, *dest_bflist;
  WebKitWebHistoryItem *item;
  GList *items;

  source_view = WEBKIT_EMBED (source)->priv->web_view;
  dest_view = WEBKIT_EMBED (dest)->priv->web_view;

  source_bflist = webkit_web_view_get_back_forward_list (source_view);
  dest_bflist = webkit_web_view_get_back_forward_list (dest_view);

  if (copy_back) {
    items = webkit_web_back_forward_list_get_back_list_with_limit (source_bflist, EPHY_WEBKIT_BACK_FORWARD_LIMIT);
    /* We want to add the items in the reverse order here, so the
       history ends up the same */
    items = g_list_reverse (items);
    for (; items; items = items->next) {
      item = (WebKitWebHistoryItem*)items->data;
      webkit_web_back_forward_list_add_item (dest_bflist, g_object_ref (item));
    }
    g_list_free (items);
  }

  /* The ephy/gecko behavior is to add the current item of the source
     embed at the end of the back history, so keep doing that */
  item = webkit_web_back_forward_list_get_current_item (source_bflist);
  webkit_web_back_forward_list_add_item (dest_bflist, g_object_ref (item));

  /* We ignore the 'copy_current' flag, it's unused in Epiphany */
  /* We ignore the 'copy_forward' flag, ephy/gecko did nothing with it
     either AFAICT*/
}

static void
impl_get_security_level (EphyEmbed *embed,
                         EphyEmbedSecurityLevel *level,
                         char **description)
{
  if (level) {
    const gchar *uri = ephy_embed_get_address (embed);

    /* FIXME: as a temporary workaround, determine security level
       based on the existence of a 'https' prefix for the URI */
    if (uri && g_str_has_prefix(uri, "https"))
      *level = EPHY_EMBED_STATE_IS_SECURE_HIGH;
    else
      *level = EPHY_EMBED_STATE_IS_UNKNOWN;
  }
}

static void
impl_show_page_certificate (EphyEmbed *embed)
{
}

static void
impl_set_print_preview_mode (EphyEmbed *embed, gboolean preview_mode)
{
}

static int
impl_print_preview_n_pages (EphyEmbed *embed)
{
  return 0;
}

static void
impl_print_preview_navigate (EphyEmbed *embed,
                             EphyEmbedPrintPreviewNavType type,
                             int page)
{
}

static gboolean
impl_has_modified_forms (EphyEmbed *embed)
{
  return FALSE;
}

static void
ephy_embed_iface_init (EphyEmbedIface *iface)
{
  iface->load_url = impl_load_url; 
  iface->load = impl_load; 
  iface->can_go_up = impl_can_go_up;
  iface->get_go_up_list = impl_get_go_up_list;
  iface->go_up = impl_go_up;
  iface->get_location = impl_get_location;
  iface->get_js_status = impl_get_js_status;
  iface->shistory_copy = impl_shistory_copy;
  iface->show_page_certificate = impl_show_page_certificate;
  iface->set_print_preview_mode = impl_set_print_preview_mode;
  iface->print_preview_n_pages = impl_print_preview_n_pages;
  iface->print_preview_navigate = impl_print_preview_navigate;
  iface->has_modified_forms = impl_has_modified_forms;
  iface->get_security_level = impl_get_security_level;
}
