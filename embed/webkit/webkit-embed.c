/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2007 Xan Lopez
 *  Copyright © 2008 Jan Alonzo
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
#include "ephy-command-manager.h"
#include "ephy-debug.h"
#include "ephy-file-chooser.h"
#include "ephy-history.h"
#include "ephy-embed-factory.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-embed-persist.h"
#include "ephy-string.h"
#include "ephy-embed-event.h"
#include "ephy-embed-utils.h"
#include "ephy-prefs.h"

#include <webkit/webkit.h>
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
                  WebKitWebFrame *web_frame,
                  const gchar *title,
                  EphyEmbed *embed)
{
  const gchar* uri;

  ephy_base_embed_set_title (EPHY_BASE_EMBED (embed),
                             title);

  uri = webkit_web_frame_get_uri (web_frame);
  ephy_history_set_page_title (WEBKIT_EMBED (embed)->priv->history,
                               uri,
                               title);

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

  uri = webkit_web_frame_get_uri (web_frame);
  ephy_base_embed_location_changed (EPHY_BASE_EMBED (embed),
                                    uri);

  restore_zoom_level (WEBKIT_EMBED (embed), uri);
  ephy_history_add_page (WEBKIT_EMBED (embed)->priv->history,
                         uri,
                         FALSE,
                         FALSE);
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
webkit_web_view_zoom_change_cb (WebKitWebView *web_view,
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

  inspector_web_view = webkit_web_view_new ();
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

  return FALSE;
}

static gboolean
download_requested_cb (WebKitWebView *web_view,
                       WebKitDownload *download,
                       WebKitEmbed *embed)
{
  EphyFileChooser *dialog;

  GtkWindow *window;

  gint dialog_result;
  gboolean handled = FALSE;

  /* Try to get the toplevel window related to the WebView that caused the
   * download, and use NULL otherwise; we don't want to pass the WebView
   * or other widget as a parent window.
   */
  window = gtk_widget_get_toplevel (GTK_WIDGET(web_view));
  if (!GTK_WIDGET_TOPLEVEL (window))
    window = NULL;

  dialog = ephy_file_chooser_new (_("Save"),
                                  window ? GTK_WIDGET (window) : NULL,
                                  GTK_FILE_CHOOSER_ACTION_SAVE,
                                  CONF_STATE_SAVE_DIR,
                                  EPHY_FILE_FILTER_ALL_SUPPORTED);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog),
                                     webkit_download_get_suggested_filename (download));

  dialog_result = gtk_dialog_run (GTK_DIALOG (dialog));

  if (dialog_result == GTK_RESPONSE_ACCEPT)
  {
    DownloaderView *dview;
    char *uri;
    
    uri = gtk_file_chooser_get_uri  (GTK_FILE_CHOOSER(dialog));
    webkit_download_set_destination_uri (download, uri);
    
    dview = EPHY_DOWNLOADER_VIEW (ephy_embed_shell_get_downloader_view (embed_shell));
    downloader_view_add_download (dview, download);

    handled = TRUE;
  }

  gtk_widget_destroy (GTK_WIDGET (dialog));

  return handled;
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

  web_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());
  embed->priv->web_view = web_view;
  gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (web_view));
  gtk_widget_show (sw);
  gtk_widget_show (GTK_WIDGET (web_view));

  gtk_container_add (GTK_CONTAINER (embed), sw);

  g_object_connect (web_view,
                    "signal::load-committed", G_CALLBACK (load_committed_cb), embed,
                    "signal::load-started", G_CALLBACK (load_started_cb), embed,
                    "signal::load_finished", G_CALLBACK (load_finished_cb), embed,
                    "signal::title-changed", G_CALLBACK (title_changed_cb), embed,
                    "signal::load-progress-changed", G_CALLBACK (load_progress_changed_cb), embed,
                    "signal::hovering-over-link", G_CALLBACK (hovering_over_link_cb), embed,
                    "signal::mime-type-policy-decision-requested", G_CALLBACK (mime_type_policy_decision_requested_cb), embed,
                    "signal::download-requested", G_CALLBACK (download_requested_cb), embed,
                    NULL);

  g_signal_connect (web_view, "notify::zoom-level",
                    G_CALLBACK (webkit_web_view_zoom_change_cb), embed);

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

  /* FIXME: WebKit has some strange bug for which there must be
   * protocol prefix into the parsed URL, or it will not show images
   * and lock badly.  I copied this function from WebKit's
   * GdkLauncher.
   */
  if (strncmp ("about:", url, 6)   != 0 &&
      strncmp ("http://", url, 7)  != 0 &&
      strncmp ("https://", url, 8) != 0 &&
      strncmp ("file://", url, 7)  != 0 &&
      strncmp ("ftp://", url, 6)   != 0)
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
}

static void
impl_get_security_level (EphyEmbed *embed,
                         EphyEmbedSecurityLevel *level,
                         char **description)
{
  if (level) *level = EPHY_EMBED_STATE_IS_UNKNOWN;
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
