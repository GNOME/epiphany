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

static void     ephy_embed_class_init       (EphyEmbedClass *klass);
static void     ephy_embed_init             (EphyEmbed *gs);
static void     ephy_embed_constructed      (GObject *object);
static gboolean ephy_embed_inspect_show_cb  (WebKitWebInspector *inspector,
                                             EphyEmbed *embed);
static gboolean ephy_embed_inspect_close_cb (WebKitWebInspector *inspector,
                                             EphyEmbed *embed);

#define EPHY_EMBED_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED, EphyEmbedPrivate))

struct EphyEmbedPrivate
{
  GtkBox *top_widgets_vbox;
  GtkScrolledWindow *scrolled_window;
  GtkPaned *paned;
  WebKitWebView *web_view;
  EphyHistory *history;
  GtkWidget *inspector_window;
  GtkWidget *inspector_scrolled_window;
  gboolean inspector_attached;
  guint is_setting_zoom : 1;
  GSList *destroy_on_transition_list;
};

G_DEFINE_TYPE (EphyEmbed, ephy_embed, GTK_TYPE_VBOX)

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
ephy_embed_destroy_top_widgets (EphyEmbed *embed)
{
  GSList *iter;

  for (iter = embed->priv->destroy_on_transition_list; iter; iter = iter->next)
    gtk_widget_destroy (GTK_WIDGET (iter->data));
}

static void
remove_from_destroy_list_cb (GtkWidget *widget, EphyEmbed *embed)
{
  GSList *list;

  list = embed->priv->destroy_on_transition_list;
  list = g_slist_remove (list, widget);
  embed->priv->destroy_on_transition_list = list;
}

static void
load_status_changed_cb (WebKitWebView *view,
                        GParamSpec *spec,
                        EphyEmbed *embed)
{
  WebKitLoadStatus status = webkit_web_view_get_load_status (view);

  if (status == WEBKIT_LOAD_COMMITTED) {
    const gchar* uri;

    uri = webkit_web_view_get_uri (view);

    ephy_embed_destroy_top_widgets (embed);

    restore_zoom_level (embed, uri);

    /* FIXME: we are not identifying redirects at the moment */
    ephy_history_add_page (embed->priv->history,
                           uri,
                           FALSE,
                           FALSE);
  }
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
ephy_embed_history_cleared_cb (EphyHistory *history,
                               EphyEmbed *embed)
{
  ephy_web_view_clear_history (EPHY_WEB_VIEW (embed->priv->web_view));
}

static void
ephy_embed_grab_focus (GtkWidget *widget)
{
  GtkWidget *child;

  child = GTK_WIDGET (ephy_embed_get_web_view (EPHY_EMBED (widget)));

  if (child)
    gtk_widget_grab_focus (child);
}

static void
ephy_embed_dispose (GObject *object)
{
  EphyEmbed *embed = EPHY_EMBED (object);

  if (embed->priv->inspector_window)
  {
    WebKitWebInspector *inspector;

    inspector = webkit_web_view_get_inspector (embed->priv->web_view);

    g_signal_handlers_disconnect_by_func (inspector,
                                          ephy_embed_inspect_show_cb,
                                          embed->priv->inspector_window);

    g_signal_handlers_disconnect_by_func (inspector,
                                          ephy_embed_inspect_close_cb,
                                          embed->priv->inspector_window);

    gtk_widget_destroy (GTK_WIDGET (embed->priv->inspector_window));
    embed->priv->inspector_window = NULL;
  }

  G_OBJECT_CLASS (ephy_embed_parent_class)->dispose (object);
}

static void
ephy_embed_finalize (GObject *object)
{
  EphyEmbed *embed = EPHY_EMBED (object);
  GSList *list;

  list = embed->priv->destroy_on_transition_list;
  for (; list; list = list->next) {
    GtkWidget *widget = GTK_WIDGET (list->data);
    g_signal_handlers_disconnect_by_func (widget, remove_from_destroy_list_cb, embed);
  }
  g_slist_free (embed->priv->destroy_on_transition_list);

  g_signal_handlers_disconnect_by_func (embed->priv->history,
                                        ephy_embed_history_cleared_cb,
                                        embed);

  G_OBJECT_CLASS (ephy_embed_parent_class)->finalize (object);
}

static void
ephy_embed_class_init (EphyEmbedClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

  object_class->constructed = ephy_embed_constructed;
  object_class->finalize = ephy_embed_finalize;
  object_class->dispose = ephy_embed_dispose;
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
ephy_embed_attach_inspector_cb (WebKitWebInspector *inspector,
                                EphyEmbed *embed)
{
  GtkAllocation allocation;
  gtk_widget_get_allocation (GTK_WIDGET (embed->priv->scrolled_window), &allocation);

  embed->priv->inspector_attached = TRUE;

  if (embed->priv->paned == NULL)
  {
    embed->priv->paned = GTK_PANED (gtk_vpaned_new ());
    g_object_ref_sink (embed->priv->paned);
  }

  /* Main view */
  g_object_ref (embed->priv->scrolled_window);
  gtk_container_remove (GTK_CONTAINER (embed),
                        GTK_WIDGET (embed->priv->scrolled_window));
  gtk_paned_pack1 (embed->priv->paned,
                   GTK_WIDGET (embed->priv->scrolled_window),
                   TRUE, FALSE);
  g_object_unref (embed->priv->scrolled_window);

  /* Set a sane position for the mover */
  gtk_paned_set_position (embed->priv->paned, allocation.height * 0.5);

  /* The inspector */
  g_object_ref (embed->priv->inspector_scrolled_window);
  gtk_container_remove (GTK_CONTAINER (embed->priv->inspector_window),
                        GTK_WIDGET (embed->priv->inspector_scrolled_window));
  gtk_paned_pack2 (embed->priv->paned,
                   GTK_WIDGET (embed->priv->inspector_scrolled_window),
                   FALSE, TRUE);
  g_object_unref (embed->priv->inspector_scrolled_window);

  /* Add the paned to the embed, show it, and hide the inspector window */
  gtk_container_add (GTK_CONTAINER (embed), GTK_WIDGET (embed->priv->paned));
  gtk_widget_show_all (GTK_WIDGET (embed->priv->paned));

  gtk_widget_hide (embed->priv->inspector_window);

  return TRUE;
}

static gboolean
ephy_embed_detach_inspector_cb (WebKitWebInspector *inspector,
                                EphyEmbed *embed)
{
  embed->priv->inspector_attached = FALSE;

  gtk_container_remove (GTK_CONTAINER (embed),
                        GTK_WIDGET (embed->priv->paned));

  /* Main view */
  gtk_widget_reparent (GTK_WIDGET (embed->priv->scrolled_window),
                       GTK_WIDGET (embed));

  /* The inspector */
  gtk_widget_reparent (GTK_WIDGET (embed->priv->inspector_scrolled_window),
                       GTK_WIDGET (embed->priv->inspector_window));

  /* Get the view and the inspector back to their places */
  gtk_widget_destroy (GTK_WIDGET (embed->priv->paned));
  embed->priv->paned = NULL;

  gtk_widget_show_all (embed->priv->inspector_window);
  gtk_widget_show_all (embed->priv->inspector_scrolled_window);

  return TRUE;
}

static gboolean
ephy_embed_inspect_show_cb (WebKitWebInspector *inspector,
                            EphyEmbed *embed)
{
  if (!embed->priv->inspector_attached)
    gtk_window_present (GTK_WINDOW (embed->priv->inspector_window));

  return TRUE;
}

static gboolean
ephy_embed_inspect_close_cb (WebKitWebInspector *inspector,
                             EphyEmbed *embed)
{
  if (!embed->priv->inspector_attached)
    gtk_widget_hide (embed->priv->inspector_window);

  return TRUE;
}

static void
download_requested_dialog_response_cb (GtkDialog *dialog,
                                       int response_id,
                                       WebKitDownload *download)
{
  if (response_id == GTK_RESPONSE_ACCEPT) {
    DownloaderView *dview;
    char *uri;

    uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
    g_object_set_data (G_OBJECT (download), "user-destination-uri", uri);

    dview = EPHY_DOWNLOADER_VIEW (ephy_embed_shell_get_downloader_view (embed_shell));
    downloader_view_add_download (dview, download);
  } else {
    webkit_download_cancel (download);
    ephy_file_delete_uri (webkit_download_get_destination_uri (download));
  }

  gtk_widget_destroy (GTK_WIDGET (dialog));
  /* User provided us with a destination or cancelled, unfreeze. */
  g_object_thaw_notify (G_OBJECT (download));
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
  const char *last_separator;

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
  last_separator = strrchr (filename, G_DIR_SEPARATOR);
  return strrchr ((last_separator) ? last_separator : filename, '.');
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

  LOG ("define_destination_uri: Downloading to %s", destination_filename);

  webkit_download_set_destination_uri (download, destination_uri);

  g_free (tmp_dir);
  g_free (destination_filename);
  g_free (destination_uri);

  return TRUE;
}

static gboolean
perform_auto_download (WebKitDownload *download)
{
  DownloaderView *dview;

  if (!define_destination_uri (download, FALSE)) {
    webkit_download_cancel (download);
    ephy_file_delete_uri (webkit_download_get_destination_uri (download));
    return FALSE;
  }

  dview = EPHY_DOWNLOADER_VIEW (ephy_embed_shell_get_downloader_view (embed_shell));

  g_object_set_data (G_OBJECT(download), "download-action", GINT_TO_POINTER(DOWNLOAD_ACTION_OPEN));
  downloader_view_add_download (dview, download);

  return TRUE;
}

void
ephy_embed_auto_download_url (EphyEmbed *embed, const char *url)
{
  WebKitNetworkRequest *request;
  WebKitDownload *download;

  request = webkit_network_request_new (url);
  download = webkit_download_new (request);
  g_object_unref (request);

  if (perform_auto_download (download))
    webkit_download_start (download);
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
      /* User selected "Open", he won't be providing a destination, unfreeze. */
      g_object_thaw_notify (G_OBJECT (download));
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
  GtkWidget *dialog;
  const char *action_label;
  char *mime_description;
  EphyMimePermission mime_permission;
  GAppInfo *helper_app;
  const char *suggested_filename;
  int default_response;
  WebKitNetworkResponse *response;
  SoupMessage *message;
  GtkMessageType mtype;
  char *title;
  char *secondary;

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
      mime_permission = ephy_file_check_mime (content_type);

      if (helper_app)
        action = DOWNLOAD_ACTION_OPEN;
    }
  }

  if (mime_description == NULL) {
    mime_description = g_strdup (C_("file type", "Unknown"));
    action = DOWNLOAD_ACTION_OPEN_LOCATION;
  }

  /* Sometimes downloads can have a mime_description but a NULL helper_app
   * in that case action is never changed so DOWNLOAD_ACTION_DOWNLOAD remains
   * as action value. This is the same response value as Save as...
   * button, which is wrong for the Download button.
   */
  if (helper_app == NULL)
    action = DOWNLOAD_ACTION_OPEN_LOCATION;

  action_label = (action == DOWNLOAD_ACTION_OPEN) ? GTK_STOCK_OPEN : STOCK_DOWNLOAD;
  suggested_filename = webkit_download_get_suggested_filename (download);

  if (mime_permission != EPHY_MIME_PERMISSION_SAFE && helper_app) {
    title = _("Download this potentially unsafe file?");
    mtype = GTK_MESSAGE_WARNING;
    /* translators: First %s is the file type description, second %s is the
     * file name */
    secondary = g_strdup_printf (_("File Type: “%s”.\n\nIt is unsafe to open "
                                   "“%s” as it could potentially damage your "
                                   "documents or invade your privacy. "
                                   "You can download it instead."),
                                 mime_description, suggested_filename);

    action_label = STOCK_DOWNLOAD;
  } else if (action == DOWNLOAD_ACTION_OPEN && helper_app) {
    title = _("Open this file?");
    mtype = GTK_MESSAGE_QUESTION;
    /* translators: First %s is the file type description, second %s is the
     * file name, third %s is the application used to open the file */
    secondary = g_strdup_printf (_("File Type: “%s”.\n\nYou can open “%s” "
                                   "using “%s” or save it."),
                                 mime_description, suggested_filename,
                                 g_app_info_get_name (helper_app));
  } else  {
    title = _("Download this file?");
    mtype = GTK_MESSAGE_QUESTION;
    /* translators: First %s is the file type description, second %s is the
     * file name */
    secondary = g_strdup_printf (_("File Type: “%s”.\n\nYou have no "
                                   "application able to open “%s”. "
                                   "You can download it instead."),
                                 mime_description, suggested_filename);
  }

  dialog = gtk_message_dialog_new (GTK_WINDOW (parent_window),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   mtype, GTK_BUTTONS_NONE,
                                   title);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            secondary, NULL);

  g_free (mime_description);

  gtk_dialog_add_button (GTK_DIALOG (dialog),
                         GTK_STOCK_SAVE_AS, DOWNLOAD_ACTION_DOWNLOAD);
  gtk_dialog_add_button (GTK_DIALOG (dialog),
                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (dialog),
                         action_label, action);

  gtk_window_set_icon_name (GTK_WINDOW (dialog), EPHY_STOCK_EPHY);

  default_response = (action == DOWNLOAD_ACTION_NONE) ?
                     (int) GTK_RESPONSE_CANCEL : (int) action;

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
                            EphyEmbed *embed)
{
  WebKitDownload *download = WEBKIT_DOWNLOAD (object);

  if (webkit_download_get_status (download) == WEBKIT_DOWNLOAD_STATUS_FINISHED)
  {
    GFile *destination;
    GFile *temp;
    char *destination_uri;
    const char *temp_uri;

    temp_uri = webkit_download_get_destination_uri (download);
    destination_uri = g_object_get_data (G_OBJECT (download),
                                         "user-destination-uri");

    LOG ("download_status_changed_cb: finished, moving temp file %s to %s",
         temp_uri, destination_uri);

    /* No user-destination-uri is set, hence this is an auto download and we
     * have nothing else to do. */
    if (destination_uri == NULL) return;

    temp = g_file_new_for_uri (temp_uri);
    destination = g_file_new_for_uri (destination_uri);

    ephy_file_switch_temp_file (destination, temp);

    g_object_unref (destination);
    g_object_unref (temp);
  }
  else if (webkit_download_get_status (download) == WEBKIT_DOWNLOAD_STATUS_STARTED)
  {
    /* Prevent this callback from being called before the user has selected a
     * destination. It is freed either here or in
     * download_requested_dialog_response_cb(). Both situations are mutually
     * exclusive.
     *
     * This freeze is removed either here below, in
     * download_requested_dialog_response_cb() or confirm_action_response_cb().
     */
    g_object_freeze_notify (G_OBJECT (download));

    if (eel_gconf_get_boolean (CONF_AUTO_DOWNLOADS)) {
      perform_auto_download (download);
      /* User won't select a destination, unfreeze. */
      g_object_thaw_notify (G_OBJECT (download));
      return;
    }

    /* Balanced in confirm_action_response_cb. */
    g_object_ref (download);

    confirm_action_from_mime (EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (embed),
                              download, DOWNLOAD_ACTION_DOWNLOAD);
  }
}

static gboolean
download_error_cb (WebKitDownload *download,
                   gint error_code,
                   gint error_detail,
                   const gchar *reason,
                   EphyEmbed *embed)
{
  /* FIXME: handle download errors and notify the user. */
  LOG ("download_error_cb: Error (%d:%d): %s", error_code, error_detail, reason);

  return FALSE;
}

static gboolean
download_requested_cb (WebKitWebView *web_view,
                       WebKitDownload *download,
                       EphyEmbed *embed)
{
  /* Is download locked down? */
  if (eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_SAVE_TO_DISK))
    return FALSE;

  /* Wait for the request to be sent in all cases, so that we have a
   * response which may contain a suggested filename */
  g_signal_connect (download, "notify::status",
                    G_CALLBACK (download_status_changed_cb),
                    embed);
  g_signal_connect (download, "error",
                    G_CALLBACK (download_error_cb),
                    embed);

  /* If we are not performing an auto-download, we will ask the user
   * where they want the file to go to; we will start downloading to a
   * temporary location while the user decides.
   */
  if (!define_destination_uri (download, TRUE))
    return FALSE;

  return TRUE;
}

static void
ephy_embed_constructed (GObject *object)
{
  EphyEmbed *embed = (EphyEmbed*)object;
  GtkWidget *scrolled_window;
  WebKitWebView *web_view;
  WebKitWebInspector *inspector;

  embed->priv->top_widgets_vbox = GTK_BOX (gtk_vbox_new (FALSE, 0));
  gtk_box_pack_start (GTK_BOX (embed), GTK_WIDGET (embed->priv->top_widgets_vbox),
                      FALSE, FALSE, 0);
  gtk_widget_show (GTK_WIDGET (embed->priv->top_widgets_vbox));

  scrolled_window = GTK_WIDGET (embed->priv->scrolled_window);
  gtk_container_add (GTK_CONTAINER (embed), scrolled_window);
  gtk_widget_show (scrolled_window);

  web_view = WEBKIT_WEB_VIEW (ephy_web_view_new ());
  embed->priv->web_view = web_view;
  gtk_container_add (GTK_CONTAINER (embed->priv->scrolled_window), GTK_WIDGET (web_view));
  gtk_widget_show (GTK_WIDGET (web_view));

  g_object_connect (web_view,
                    "signal::notify::load-status", G_CALLBACK (load_status_changed_cb), embed,
                    "signal::resource-request-starting", G_CALLBACK (resource_request_starting_cb), embed,
                    "signal::download-requested", G_CALLBACK (download_requested_cb), embed,
                    "signal::notify::zoom-level", G_CALLBACK (zoom_changed_cb), embed,
                    NULL);

  embed->priv->inspector_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  inspector = webkit_web_view_get_inspector (web_view);

  embed->priv->inspector_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (embed->priv->inspector_scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (embed->priv->inspector_window),
                     embed->priv->inspector_scrolled_window);

  gtk_window_set_title (GTK_WINDOW (embed->priv->inspector_window),
                        _("Web Inspector"));
  gtk_window_set_default_size (GTK_WINDOW (embed->priv->inspector_window),
                               800, 600);

  g_signal_connect (embed->priv->inspector_window,
                    "delete-event", G_CALLBACK (gtk_widget_hide_on_delete),
                    NULL);

  g_object_connect (inspector,
                    "signal::inspect-web-view", G_CALLBACK (ephy_embed_inspect_web_view_cb),
                    embed->priv->inspector_scrolled_window,
                    "signal::show-window", G_CALLBACK (ephy_embed_inspect_show_cb),
                    embed,
                    "signal::close-window", G_CALLBACK (ephy_embed_inspect_close_cb),
                    embed,
                    "signal::attach-window", G_CALLBACK (ephy_embed_attach_inspector_cb),
                    embed,
                    "signal::detach-window", G_CALLBACK (ephy_embed_detach_inspector_cb),
                    embed,
                    NULL);

  ephy_embed_prefs_add_embed (embed);

  embed->priv->history = EPHY_HISTORY (ephy_embed_shell_get_global_history (ephy_embed_shell_get_default ()));

  g_signal_connect (embed->priv->history,
                    "cleared", G_CALLBACK (ephy_embed_history_cleared_cb),
                    embed);
}

static void
ephy_embed_init (EphyEmbed *embed)
{
  embed->priv = EPHY_EMBED_GET_PRIVATE (embed);

  embed->priv->scrolled_window = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
  embed->priv->paned = NULL;

  gtk_scrolled_window_set_policy (embed->priv->scrolled_window,
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

/**
 * ephy_embed_add_top_widget:
 * @embed: an #EphyEmbed
 * @widget: a #GtkWidget
 * @destroy_on_transition: whether the widget be automatically
 * destroyed on page transitions
 *
 * Adds a #GtkWidget to the top of the embed.
 */
void
ephy_embed_add_top_widget (EphyEmbed *embed, GtkWidget *widget, gboolean destroy_on_transition)
{
  GSList *list;

  if (destroy_on_transition) {
    list = embed->priv->destroy_on_transition_list;
    list = g_slist_prepend (list, widget);
    embed->priv->destroy_on_transition_list = list;

    g_signal_connect (widget, "destroy", G_CALLBACK (remove_from_destroy_list_cb), embed);
  }

  gtk_box_pack_end (embed->priv->top_widgets_vbox,
                    GTK_WIDGET (widget), TRUE, TRUE, 0);
}

/**
 * ephy_embed_remove_top_widget:
 * @embed: an #EphyEmbed
 * @widget: a #GtkWidget
 *
 * Removes an #GtkWidget from the top of the embed. The #GtkWidget
 * must be have been added using ephy_embed_add_widget(), and not
 * have been removed by other means. See gtk_container_remove() for
 * details.
 */
void
ephy_embed_remove_top_widget (EphyEmbed *embed, GtkWidget *widget)
{
  if (g_slist_find (embed->priv->destroy_on_transition_list, widget)) {
    GSList *list;
    g_signal_handlers_disconnect_by_func (widget, remove_from_destroy_list_cb, embed);

    list = embed->priv->destroy_on_transition_list;
    list = g_slist_remove (list, widget);
    embed->priv->destroy_on_transition_list = list;
  }

  gtk_container_remove (GTK_CONTAINER (embed->priv->top_widgets_vbox),
                        GTK_WIDGET (widget));
}
