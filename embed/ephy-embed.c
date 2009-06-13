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

#include "ephy-embed-prefs.h"
#include "ephy-embed.h"

static void     ephy_embed_class_init (EphyEmbedClass *klass);
static void     ephy_embed_init       (EphyEmbed *gs);

#define EPHY_EMBED_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED, EphyEmbedPrivate))

typedef enum
{
    EPHY_EMBED_LOAD_STARTED,
    EPHY_EMBED_LOAD_REDIRECTING,
    EPHY_EMBED_LOAD_LOADING,
    EPHY_EMBED_LOAD_STOPPED
} EphyEmbedLoadState;

struct EphyEmbedPrivate
{
  WebKitWebView *web_view;
  GtkScrolledWindow *scrolled_window;
  EphyEmbedLoadState load_state;
  EphyHistory *history;
  GtkWidget *inspector_window;
  guint is_setting_zoom : 1;
};

static void
impl_manager_do_command (EphyCommandManager *manager,
                         const char *command)
{
  WebKitWebView *web_view = EPHY_EMBED (manager)->priv->web_view;

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
  WebKitWebView *web_view = EPHY_EMBED (manager)->priv->web_view;

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

G_DEFINE_TYPE_WITH_CODE (EphyEmbed, ephy_embed, GTK_TYPE_BIN,
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
update_load_state (EphyEmbed *embed, WebKitWebView *web_view)
{
  EphyWebViewNetState estate = EPHY_WEB_VIEW_STATE_UNKNOWN;
  const char *loading_uri = ephy_web_view_get_typed_address (EPHY_WEB_VIEW (web_view));

  if (embed->priv->load_state == EPHY_EMBED_LOAD_STARTED)
    {
      estate = (EphyWebViewNetState) (estate |
                                    EPHY_WEB_VIEW_STATE_START |
                                    EPHY_WEB_VIEW_STATE_NEGOTIATING |
                                    EPHY_WEB_VIEW_STATE_IS_REQUEST |
                                    EPHY_WEB_VIEW_STATE_IS_NETWORK);

      g_signal_emit_by_name (EPHY_WEB_VIEW (web_view), "new-document-now", loading_uri);
    }

  if (embed->priv->load_state == EPHY_EMBED_LOAD_LOADING)
      estate = (EphyWebViewNetState) (estate |
                                    EPHY_WEB_VIEW_STATE_TRANSFERRING |
                                    EPHY_WEB_VIEW_STATE_IS_REQUEST |
                                    EPHY_WEB_VIEW_STATE_IS_NETWORK);

  if (embed->priv->load_state == EPHY_EMBED_LOAD_STOPPED)
      estate = (EphyWebViewNetState) (estate |
                                    EPHY_WEB_VIEW_STATE_STOP |
                                    EPHY_WEB_VIEW_STATE_IS_DOCUMENT |
                                    EPHY_WEB_VIEW_STATE_IS_NETWORK);

  ephy_web_view_update_from_net_state (EPHY_WEB_VIEW (web_view),
                                       loading_uri,
                                       (EphyWebViewNetState)estate);
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
load_committed_cb (WebKitWebView *web_view,
                   WebKitWebFrame *web_frame,
                   EphyEmbed *embed)
{
  const gchar* uri;
  EphyWebViewSecurityLevel security_level;

  uri = webkit_web_frame_get_uri (web_frame);
  ephy_web_view_location_changed (EPHY_WEB_VIEW (web_view),
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

  ephy_web_view_set_security_level (EPHY_WEB_VIEW (web_view), security_level);
}

static void
load_started_cb (WebKitWebView *web_view,
                 WebKitWebFrame *web_frame,
                 EphyEmbed *embed)
{
  EphyEmbed *wembed = EPHY_EMBED (embed);
  wembed->priv->load_state = EPHY_EMBED_LOAD_STARTED;

  update_load_state (wembed, web_view);
}

static void
load_progress_changed_cb (WebKitWebView *web_view,
                          GParamSpec *spec,
                          EphyEmbed *embed)
{
  EphyEmbed *wembed = EPHY_EMBED (embed);

  if (wembed->priv->load_state == EPHY_EMBED_LOAD_STARTED)
    wembed->priv->load_state = EPHY_EMBED_LOAD_LOADING;
}

static void
load_finished_cb (WebKitWebView *web_view,
                  WebKitWebFrame *web_frame,
                  EphyEmbed *embed)
{
  EphyEmbed *wembed = EPHY_EMBED (embed);

  wembed->priv->load_state = EPHY_EMBED_LOAD_STOPPED;
  update_load_state (wembed, web_view);
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
ephy_embed_size_request (GtkWidget *widget,
                         GtkRequisition *requisition)
{
  GtkWidget *child;

  GTK_WIDGET_CLASS (ephy_embed_parent_class)->size_request (widget, requisition);

  child = GTK_BIN (widget)->child;

  if (child && GTK_WIDGET_VISIBLE (child)) {
    GtkRequisition child_requisition;
    gtk_widget_size_request (GTK_WIDGET (child), &child_requisition);
  }
}

static void
ephy_embed_size_allocate (GtkWidget *widget,
                               GtkAllocation *allocation)
{
  GtkWidget *child;

  widget->allocation = *allocation;

  child = GTK_BIN (widget)->child;
  g_return_if_fail (child != NULL);

  gtk_widget_size_allocate (child, allocation);
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
ephy_embed_class_init (EphyEmbedClass *klass)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

  widget_class->size_request = ephy_embed_size_request;
  widget_class->size_allocate = ephy_embed_size_allocate;
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

  type = EPHY_WEB_VIEW_DOCUMENT_OTHER;

  if (!strcmp (mime_type, "text/html"))
    type = EPHY_WEB_VIEW_DOCUMENT_HTML;
  else if (!strcmp (mime_type, "application/xhtml+xml"))
    type = EPHY_WEB_VIEW_DOCUMENT_XML;
  else if (!strncmp (mime_type, "image/", 6))
    type = EPHY_WEB_VIEW_DOCUMENT_IMAGE;

  /* FIXME: maybe it makes more sense to have an API to query the mime
   * type when the load of a page starts than doing this here.
   */
  /* FIXME: rename ge-document-type (and all ge- signals...) to
   * something else
   */
  g_signal_emit_by_name (EPHY_GET_EPHY_WEB_VIEW_FROM_EMBED (embed), "ge-document-type", type);

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
ephy_embed_init (EphyEmbed *embed)
{
  WebKitWebView *web_view;
  WebKitWebInspector *inspector;
  GtkWidget *sw;
  GtkWidget *inspector_sw;

  embed->priv = EPHY_EMBED_GET_PRIVATE (embed);

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
                    "signal::notify::progress", G_CALLBACK (load_progress_changed_cb), embed,
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

