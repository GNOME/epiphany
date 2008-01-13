/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2007 Xan Lopez
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

#include "ephy-command-manager.h"
#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-string.h"
#include "ephy-embed-event.h"

#include <webkit.h>
#include <string.h>

#include "webkit-embed.h"
#include "ephy-embed.h"
#include "ephy-base-embed.h"

static void     webkit_embed_class_init (WebKitEmbedClass *klass);
static void     webkit_embed_init               (WebKitEmbed *gs);
static void     webkit_embed_destroy            (GtkObject *object);
static void     webkit_embed_finalize           (GObject *object);
static void     ephy_embed_iface_init           (EphyEmbedIface *iface);

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
  WebKitEmbedLoadState load_state;
  char *loading_uri;
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
impl_close (EphyEmbed *embed)
{
  gtk_widget_destroy (GTK_WIDGET (embed));
}

static void
webkit_embed_title_changed_cb (WebKitWebView *web_view,
                               WebKitWebFrame *web_frame,
                               const gchar *title,
                               EphyEmbed *embed)
{
  ephy_base_embed_set_title (EPHY_BASE_EMBED (embed),
                             title);
}

static void
update_load_state (WebKitEmbed *embed, WebKitWebView *web_view)
{
  EphyEmbedNetState estate = EPHY_EMBED_STATE_UNKNOWN;

  if (embed->priv->load_state == WEBKIT_EMBED_LOAD_STARTED)
      estate = (EphyEmbedNetState) (estate |
                                    EPHY_EMBED_STATE_START |
                                    EPHY_EMBED_STATE_NEGOTIATING |
                                    EPHY_EMBED_STATE_IS_REQUEST |
                                    EPHY_EMBED_STATE_IS_NETWORK);

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
webkit_embed_load_committed_cb (WebKitWebView *web_view,
                                WebKitWebFrame *web_frame,
                                EphyEmbed *embed)
{
  const gchar* uri = webkit_web_frame_get_uri(web_frame);
  ephy_base_embed_location_changed (EPHY_BASE_EMBED (embed),
                                    uri);
}

static void
webkit_embed_load_started_cb (WebKitWebView *web_view,
                              WebKitWebFrame *web_frame,
                              EphyEmbed *embed)
{
  WebKitEmbed *wembed = WEBKIT_EMBED (embed);
  wembed->priv->load_state = WEBKIT_EMBED_LOAD_STARTED;

  update_load_state (wembed, web_view);
}

static void
webkit_embed_load_progress_changed_cb (WebKitWebView *web_view,
                                       int progress,
                                       EphyEmbed *embed)
{
  WebKitEmbed *wembed = WEBKIT_EMBED (embed);

  if (wembed->priv->load_state == WEBKIT_EMBED_LOAD_STARTED)
    wembed->priv->load_state = WEBKIT_EMBED_LOAD_LOADING;

  ephy_base_embed_set_load_percent (EPHY_BASE_EMBED (embed), progress);
}

static void
webkit_embed_load_finished_cb (WebKitWebView *web_view,
                               WebKitWebFrame *web_frame,
                               EphyEmbed *embed)
{
  WebKitEmbed *wembed = WEBKIT_EMBED (embed);
  wembed->priv->load_state = WEBKIT_EMBED_LOAD_STOPPED;

  update_load_state (wembed, web_view);
}

static void
webkit_embed_hovering_over_link_cb (WebKitWebView *web_view,
                                    char *title,
                                    char *location,
                                    EphyEmbed *embed)
{
  ephy_base_embed_set_link_message (EPHY_BASE_EMBED (embed), location);
}

static void
webkit_embed_class_init (WebKitEmbedClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass);

  object_class->finalize = webkit_embed_finalize;

  gtk_object_class->destroy = webkit_embed_destroy;

  g_type_class_add_private (object_class, sizeof(WebKitEmbedPrivate));
}

static void
webkit_embed_init (WebKitEmbed *embed)
{
  WebKitWebView *web_view;
  GtkWidget *sw;

  embed->priv = WEBKIT_EMBED_GET_PRIVATE (embed);
  embed->priv->loading_uri = NULL;

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  web_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());
  embed->priv->web_view = web_view;
  gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (web_view));
  gtk_widget_show (sw);
  gtk_widget_show (GTK_WIDGET (web_view));

  gtk_container_add (GTK_CONTAINER (embed), sw);

  g_signal_connect (G_OBJECT (web_view), "load-committed",
                    G_CALLBACK (webkit_embed_load_committed_cb), embed);
  g_signal_connect (G_OBJECT (web_view), "load-started",
                    G_CALLBACK (webkit_embed_load_started_cb), embed);
  g_signal_connect (G_OBJECT (web_view), "load_finished",
                    G_CALLBACK (webkit_embed_load_finished_cb), embed);
  g_signal_connect (G_OBJECT (web_view), "title-changed",
                    G_CALLBACK (webkit_embed_title_changed_cb), embed);
  g_signal_connect (G_OBJECT (web_view), "load-progress-changed",
                    G_CALLBACK (webkit_embed_load_progress_changed_cb), embed);
  g_signal_connect (G_OBJECT (web_view), "hovering-over-link",
                    G_CALLBACK (webkit_embed_hovering_over_link_cb), embed);
}

static void
webkit_embed_destroy (GtkObject *object)
{
  GTK_OBJECT_CLASS (webkit_embed_parent_class)->destroy (object);
}

static void
webkit_embed_finalize (GObject *object)
{
  WebKitEmbed *wembed = WEBKIT_EMBED (object);

  g_free (wembed->priv->loading_uri);

  G_OBJECT_CLASS (webkit_embed_parent_class)->finalize (object);
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

static void
impl_stop_load (EphyEmbed *embed)
{
  webkit_web_view_stop_loading (WEBKIT_EMBED (embed)->priv->web_view);
}

static gboolean
impl_can_go_back (EphyEmbed *embed)
{
  return webkit_web_view_can_go_backward (WEBKIT_EMBED (embed)->priv->web_view);
}

static gboolean
impl_can_go_forward (EphyEmbed *embed)
{
  return webkit_web_view_can_go_forward (WEBKIT_EMBED (embed)->priv->web_view);
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
impl_go_back (EphyEmbed *embed)
{
  webkit_web_view_go_backward (WEBKIT_EMBED (embed)->priv->web_view);
}

static void
impl_go_forward (EphyEmbed *embed)
{
  webkit_web_view_go_forward (WEBKIT_EMBED (embed)->priv->web_view);
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
impl_reload (EphyEmbed *embed,
             gboolean force)
{
  webkit_web_view_reload (WEBKIT_EMBED (embed)->priv->web_view);
}

static float
impl_get_zoom (EphyEmbed *embed)
{
  return 1.0;
}

static void
impl_set_zoom (EphyEmbed *embed,
               float zoom)
{
}

static void
impl_scroll_lines (EphyEmbed *embed,
                   int num_lines)
{
}

static void
impl_scroll_pages (EphyEmbed *embed,
                   int num_pages)
{
}

static void
impl_scroll_pixels (EphyEmbed *embed,
                    int dx,
                    int dy)
{
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
impl_print (EphyEmbed *embed)
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

static void
impl_set_encoding (EphyEmbed *embed,
                   const char *encoding)
{
}

static char *
impl_get_encoding (EphyEmbed *embed)
{
  return NULL;
}

static gboolean
impl_has_automatic_encoding (EphyEmbed *embed)
{
  return FALSE;
}

static gboolean
impl_has_modified_forms (EphyEmbed *embed)
{
  return FALSE;
}

static GList*
impl_get_backward_history (EphyEmbed *embed)
{
  return NULL;
}

static GList*
impl_get_forward_history    (EphyEmbed *embed)
{
  return NULL;
}

static EphyHistoryItem*
impl_get_next_history_item  (EphyEmbed *embed)
{
  return NULL;
}

static EphyHistoryItem*
impl_get_previous_history_item  (EphyEmbed *embed)
{
  return NULL;
}

static void
impl_go_to_history_item   (EphyEmbed *embed, EphyHistoryItem *history_item)
{
}

static void
ephy_embed_iface_init (EphyEmbedIface *iface)
{
  iface->load_url = impl_load_url; 
  iface->load = impl_load; 
  iface->stop_load = impl_stop_load;
  iface->can_go_back = impl_can_go_back;
  iface->can_go_forward =impl_can_go_forward;
  iface->can_go_up = impl_can_go_up;
  iface->get_go_up_list = impl_get_go_up_list;
  iface->go_back = impl_go_back;
  iface->go_forward = impl_go_forward;
  iface->go_up = impl_go_up;
  iface->get_location = impl_get_location;
  iface->get_js_status = impl_get_js_status;
  iface->reload = impl_reload;
  iface->set_zoom = impl_set_zoom;
  iface->get_zoom = impl_get_zoom;
  iface->scroll_lines = impl_scroll_lines;
  iface->scroll_pages = impl_scroll_pages;
  iface->scroll_pixels = impl_scroll_pixels;
  iface->shistory_copy = impl_shistory_copy;
  iface->show_page_certificate = impl_show_page_certificate;
  iface->close = impl_close;
  iface->set_encoding = impl_set_encoding;
  iface->get_encoding = impl_get_encoding;
  iface->has_automatic_encoding = impl_has_automatic_encoding;
  iface->print = impl_print;
  iface->set_print_preview_mode = impl_set_print_preview_mode;
  iface->print_preview_n_pages = impl_print_preview_n_pages;
  iface->print_preview_navigate = impl_print_preview_navigate;
  iface->has_modified_forms = impl_has_modified_forms;
  iface->get_security_level = impl_get_security_level;
  iface->get_backward_history = impl_get_backward_history;
  iface->get_forward_history = impl_get_forward_history;
  iface->get_next_history_item = impl_get_next_history_item;
  iface->get_previous_history_item = impl_get_previous_history_item;
  iface->go_to_history_item = impl_go_to_history_item;
}
