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
#include "ephy-embed.h"

#include "ephy-adblock-manager.h"
#include "ephy-debug.h"
#include "ephy-download.h"
#include "ephy-embed-event.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-history.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-stock-icons.h"
#include "ephy-string.h"
#include "ephy-web-view.h"

#include <errno.h>
#include <glib/gi18n.h>
#include <string.h>
#include <webkit/webkit.h>

static void     ephy_embed_constructed      (GObject *object);
static gboolean ephy_embed_inspect_show_cb  (WebKitWebInspector *inspector,
                                             EphyEmbed *embed);
static gboolean ephy_embed_inspect_close_cb (WebKitWebInspector *inspector,
                                             EphyEmbed *embed);

#define EPHY_EMBED_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED, EphyEmbedPrivate))

typedef struct {
  gchar *text;
  guint context_id;
  guint message_id;
} EphyEmbedStatusbarMsg;

struct _EphyEmbedPrivate
{
  GtkBox *top_widgets_vbox;
  GtkScrolledWindow *scrolled_window;
  GtkPaned *paned;
  WebKitWebView *web_view;
  EphyHistory *history;
  GtkWidget *inspector_window;
  GtkWidget *inspector_web_view;
  GtkWidget *inspector_scrolled_window;
  gboolean inspector_attached;
  guint is_setting_zoom : 1;
  GSList *destroy_on_transition_list;
  GtkWidget *statusbar_label;
  GtkWidget *progress;

  GSList *messages;
  GSList *keys;

  guint seq_context_id;
  guint seq_message_id;

  guint tab_message_id;
  guint pop_statusbar_later_source_id;
};

G_DEFINE_TYPE (EphyEmbed, ephy_embed, GTK_TYPE_BOX)

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
  EphyEmbedPrivate *priv = embed->priv;

  if (embed->priv->inspector_window) {
    WebKitWebInspector *inspector;

    inspector = webkit_web_view_get_inspector (priv->web_view);

    g_signal_handlers_disconnect_by_func (inspector,
                                          ephy_embed_inspect_show_cb,
                                          priv->inspector_window);

    g_signal_handlers_disconnect_by_func (inspector,
                                          ephy_embed_inspect_close_cb,
                                          priv->inspector_window);

    gtk_widget_destroy (GTK_WIDGET (priv->inspector_window));
    priv->inspector_window = NULL;
  }

  if (priv->pop_statusbar_later_source_id) {
    g_source_remove (priv->pop_statusbar_later_source_id);
    priv->pop_statusbar_later_source_id = 0;
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

  for (list = embed->priv->messages; list; list = list->next) {
    EphyEmbedStatusbarMsg *msg;

    msg = list->data;
    g_free (msg->text);
    g_slice_free (EphyEmbedStatusbarMsg, msg);
  }

  g_slist_free (embed->priv->messages);
  embed->priv->messages = NULL;

  for (list = embed->priv->keys; list; list = list->next)
    g_free (list->data);

  g_slist_free (embed->priv->keys);
  embed->priv->keys = NULL;

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
                                EphyEmbed *embed)
{
  return WEBKIT_WEB_VIEW (embed->priv->inspector_web_view);
}

static gboolean
ephy_embed_attach_inspector_cb (WebKitWebInspector *inspector,
                                EphyEmbed *embed)
{
  GtkAllocation allocation;
  gtk_widget_get_allocation (GTK_WIDGET (embed->priv->scrolled_window), &allocation);

  embed->priv->inspector_attached = TRUE;

  /* Set a sane position for the mover */
  gtk_paned_set_position (embed->priv->paned, allocation.height * 0.5);

  gtk_widget_hide (embed->priv->inspector_window);
  gtk_widget_reparent (GTK_WIDGET (embed->priv->inspector_scrolled_window),
                       GTK_WIDGET (embed->priv->paned));

  return TRUE;
}

static gboolean
ephy_embed_detach_inspector_cb (WebKitWebInspector *inspector,
                                EphyEmbed *embed)
{
  embed->priv->inspector_attached = FALSE;

  gtk_widget_reparent (GTK_WIDGET (embed->priv->inspector_scrolled_window),
                       GTK_WIDGET (embed->priv->inspector_window));

  gtk_widget_show_all (embed->priv->inspector_window);

  return TRUE;
}

static gboolean
ephy_embed_inspect_show_cb (WebKitWebInspector *inspector,
                            EphyEmbed *embed)
{
  if (!embed->priv->inspector_attached) {
    gtk_widget_show_all (embed->priv->inspector_window);
    gtk_window_present (GTK_WINDOW (embed->priv->inspector_window));
  } else
    gtk_widget_show (embed->priv->inspector_scrolled_window);

  return TRUE;
}

static gboolean
ephy_embed_inspect_close_cb (WebKitWebInspector *inspector,
                             EphyEmbed *embed)
{
  if (!embed->priv->inspector_attached)
    gtk_widget_hide (embed->priv->inspector_window);
  else
    gtk_widget_hide (embed->priv->inspector_scrolled_window);

  return TRUE;
}

void
ephy_embed_auto_download_url (EphyEmbed *embed, const char *url)
{
  EphyDownload *download;

  download = ephy_download_new_for_uri (url);
  ephy_download_set_auto_destination (download);
  ephy_download_set_action (download, EPHY_DOWNLOAD_ACTION_OPEN);
}

static gboolean
download_requested_cb (WebKitWebView *web_view,
                       WebKitDownload *download,
                       EphyEmbed *embed)
{
  EphyDownload *ed;
  GtkWidget *window;

  /* Is download locked down? */
  if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                              EPHY_PREFS_LOCKDOWN_SAVE_TO_DISK))
    return FALSE;

  window = gtk_widget_get_toplevel (GTK_WIDGET (embed));

  ed = ephy_download_new_for_download (download);
  ephy_download_set_window (ed, window);
  ephy_download_set_auto_destination (ed);

  return TRUE;
}

void
_ephy_embed_set_statusbar_label (EphyEmbed *embed, const char *label)
{
  EphyEmbedPrivate *priv = embed->priv;
  GtkWidget *parent;

  gtk_label_set_label (GTK_LABEL (priv->statusbar_label), label);

  parent = gtk_widget_get_parent (priv->statusbar_label);
  if (parent == NULL)
    return;

  if (label == NULL || label[0] == '\0')
    gtk_widget_hide (parent);
  else
    gtk_widget_show (parent);
}

static gboolean
pop_statusbar_later_cb (gpointer data)
{
  EphyEmbed *embed = EPHY_EMBED (data);
  EphyEmbedPrivate *priv = embed->priv;

  ephy_embed_statusbar_pop (embed, priv->tab_message_id);
  priv->pop_statusbar_later_source_id = 0;
  return FALSE;
}

static void
status_message_notify_cb (EphyWebView *view, GParamSpec *pspec, EphyEmbed *embed)
{
  const char *message;
  EphyEmbedPrivate *priv;

  message = ephy_web_view_get_status_message (view);

  priv = embed->priv;

  if (message) {
    if (priv->pop_statusbar_later_source_id) {
      g_source_remove (priv->pop_statusbar_later_source_id);
      priv->pop_statusbar_later_source_id = 0;
    }

    ephy_embed_statusbar_pop (embed, priv->tab_message_id);
    ephy_embed_statusbar_push (embed, priv->tab_message_id, message);
  } else {
    /* A short timeout before hiding the statusbar ensures that while moving
      over a series of links, the overlay widget doesn't flicker on and off. */
    priv->pop_statusbar_later_source_id = g_timeout_add (250, pop_statusbar_later_cb, embed);
  }
}

static void
window_resize_requested (WebKitWebWindowFeatures *features, GParamSpec *pspec, EphyEmbed *embed)
{
  GtkWidget *window;
  gboolean is_popup;
  const char *property_name;
  int width, height;

  window = gtk_widget_get_toplevel (GTK_WIDGET (embed));
  if (!window || !gtk_widget_is_toplevel (window))
    return;

  g_object_get (window, "is-popup", &is_popup, NULL);
  if (!is_popup)
    return;

  property_name = g_param_spec_get_name (pspec);

  if (g_str_equal (property_name, "x") || g_str_equal (property_name, "y")) {
    int x, y;
    g_object_get (features, "x", &x, "y", &y, NULL);
    gtk_window_move (GTK_WINDOW (window), x, y);
    return;
  }

  g_object_get (features, "width", &width, "height", &height, NULL);
  gtk_window_resize (GTK_WINDOW (window), width, height);
}

static gboolean
frame_enter_notify_cb (GtkWidget *widget,
                       GdkEventCrossing *event,
                       gpointer user_data)
{
	if (gtk_widget_get_halign (widget) == GTK_ALIGN_START)
		gtk_widget_set_halign (widget, GTK_ALIGN_END);
  else
		gtk_widget_set_halign (widget, GTK_ALIGN_START);

	gtk_widget_queue_resize (widget);

  return FALSE;
}

static void
progress_update (EphyWebView *view, GParamSpec *pspec, EphyEmbed *embed)
{
  gdouble progress;
  gboolean loading;

  EphyEmbedPrivate *priv = embed->priv;

  progress = webkit_web_view_get_progress (priv->web_view);
  loading = ephy_web_view_is_loading (EPHY_WEB_VIEW (priv->web_view));

  if (progress == 1.0 || !loading)
    gtk_widget_hide (priv->progress);
  else
    gtk_widget_show (priv->progress);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->progress),
                                 (loading || progress == 1.0) ? progress : 0.0);
}

static void
ephy_embed_constructed (GObject *object)
{
  EphyEmbed *embed = (EphyEmbed*)object;
  EphyEmbedPrivate *priv = embed->priv;
  GtkWidget *scrolled_window;
  GtkWidget *paned;
  WebKitWebView *web_view;
  WebKitWebWindowFeatures *window_features;
  WebKitWebInspector *inspector;
  GtkWidget *overlay;
  GtkWidget *frame;
  GtkWidget *eventbox;

  /* Skeleton */
  web_view = WEBKIT_WEB_VIEW (ephy_web_view_new ());
  scrolled_window = GTK_WIDGET (embed->priv->scrolled_window);
  overlay = gtk_overlay_new ();
  gtk_widget_set_name (overlay, "ephy-overlay");
  gtk_container_add (GTK_CONTAINER (overlay), scrolled_window);

  /* statusbar is hidden by default */
  priv->statusbar_label = gtk_label_new (NULL);
  eventbox = gtk_event_box_new ();
  frame = gtk_frame_new (NULL);
  gtk_widget_set_name (frame, "ephy-status-frame");
  gtk_widget_set_halign (eventbox, GTK_ALIGN_START);
  gtk_widget_set_valign (eventbox, GTK_ALIGN_END);
  gtk_widget_show (eventbox);

  gtk_container_add (GTK_CONTAINER (eventbox), frame);
  gtk_container_add (GTK_CONTAINER (frame), priv->statusbar_label);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), eventbox);
  g_signal_connect (eventbox, "enter-notify-event",
                    G_CALLBACK (frame_enter_notify_cb), object);

  embed->priv->progress = gtk_progress_bar_new ();
  gtk_widget_set_name (embed->priv->progress, "ephy-progress-bar");
  gtk_widget_set_halign (embed->priv->progress, GTK_ALIGN_FILL);
  gtk_widget_set_valign (embed->priv->progress, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), embed->priv->progress);

  paned = GTK_WIDGET (embed->priv->paned);

  embed->priv->web_view = web_view;
  g_signal_connect (web_view, "notify::progress",
                    G_CALLBACK (progress_update), object);

  gtk_container_add (GTK_CONTAINER (scrolled_window),
                     GTK_WIDGET (web_view));
  gtk_paned_pack1 (GTK_PANED (paned), GTK_WIDGET (overlay),
                   TRUE, FALSE);

  gtk_box_pack_start (GTK_BOX (embed),
                      GTK_WIDGET (embed->priv->top_widgets_vbox),
                      FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (embed), paned, TRUE, TRUE, 0);

  gtk_widget_show (GTK_WIDGET (embed->priv->top_widgets_vbox));
  gtk_widget_show (GTK_WIDGET (web_view));
  gtk_widget_show_all (paned);

  g_object_connect (web_view,
                    "signal::notify::load-status", G_CALLBACK (load_status_changed_cb), embed,
                    "signal::resource-request-starting", G_CALLBACK (resource_request_starting_cb), embed,
                    "signal::download-requested", G_CALLBACK (download_requested_cb), embed,
                    "signal::notify::zoom-level", G_CALLBACK (zoom_changed_cb), embed,
                    "signal::notify::status-message", G_CALLBACK (status_message_notify_cb), embed,
                    NULL);

  /* Window features */
  window_features = webkit_web_view_get_window_features (web_view);
  g_object_connect (window_features,
                    "signal::notify::x", G_CALLBACK (window_resize_requested), embed,
                    "signal::notify::y", G_CALLBACK (window_resize_requested), embed,
                    "signal::notify::width", G_CALLBACK (window_resize_requested), embed,
                    "signal::notify::height", G_CALLBACK (window_resize_requested), embed,
                    NULL);

  /* The inspector */
  embed->priv->inspector_web_view  = ephy_web_view_new ();
  embed->priv->inspector_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  inspector = webkit_web_view_get_inspector (web_view);

  embed->priv->inspector_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (embed->priv->inspector_scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (embed->priv->inspector_window),
                     embed->priv->inspector_scrolled_window);
  gtk_container_add (GTK_CONTAINER (embed->priv->inspector_scrolled_window),
                     embed->priv->inspector_web_view);

  gtk_window_set_title (GTK_WINDOW (embed->priv->inspector_window),
                        _("Web Inspector"));
  gtk_window_set_default_size (GTK_WINDOW (embed->priv->inspector_window),
                               800, 600);

  g_signal_connect (embed->priv->inspector_window,
                    "delete-event", G_CALLBACK (gtk_widget_hide_on_delete),
                    NULL);

  g_object_connect (inspector,
                    "signal::inspect-web-view", G_CALLBACK (ephy_embed_inspect_web_view_cb),
                    embed,
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

  gtk_orientable_set_orientation (GTK_ORIENTABLE (embed),
                                  GTK_ORIENTATION_VERTICAL);

  embed->priv->scrolled_window = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
  embed->priv->paned = GTK_PANED (gtk_paned_new (GTK_ORIENTATION_VERTICAL));
  embed->priv->top_widgets_vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  embed->priv->seq_context_id = 1;
  embed->priv->seq_message_id = 1;
  embed->priv->tab_message_id = ephy_embed_statusbar_get_context_id (embed, EPHY_EMBED_STATUSBAR_TAB_MESSAGE_CONTEXT_DESCRIPTION);

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

static void
ephy_embed_statusbar_update (EphyEmbed *embed, const char *text)
{
  g_return_if_fail (EPHY_IS_EMBED (embed));

  _ephy_embed_set_statusbar_label (embed, text);
}

/* Portions of the following code based on GTK+.
 * License block as follows:
 *
 * GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkStatusbar Copyright (C) 1998 Shawn T. Amundson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 *
 */

guint
ephy_embed_statusbar_get_context_id (EphyEmbed *embed, const char  *context_description)
{
  char *string;
  guint id;

  g_return_val_if_fail (EPHY_IS_EMBED (embed), 0);
  g_return_val_if_fail (context_description != NULL, 0);

  /* we need to preserve namespaces on object datas */
  string = g_strconcat ("ephy-embed-status-bar-context:", context_description, NULL);

  id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (embed), string));
  if (id == 0) {
    EphyEmbedPrivate *priv = embed->priv;

    id = priv->seq_context_id++;
    g_object_set_data_full (G_OBJECT (embed), string, GUINT_TO_POINTER (id), NULL);
    priv->keys = g_slist_prepend (priv->keys, string);
  } else
    g_free (string);

  return id;
}

guint
ephy_embed_statusbar_push (EphyEmbed *embed, guint context_id, const char *text)
{
  EphyEmbedPrivate *priv;
  EphyEmbedStatusbarMsg *msg;

  g_return_val_if_fail (EPHY_IS_EMBED (embed), 0);
  g_return_val_if_fail (context_id != 0, 0);
  g_return_val_if_fail (text != NULL, 0);

  priv = embed->priv;

  msg = g_slice_new (EphyEmbedStatusbarMsg);
  msg->text = g_strdup (text);
  msg->context_id = context_id;
  msg->message_id = priv->seq_message_id++;

  priv->messages = g_slist_prepend (priv->messages, msg);

  ephy_embed_statusbar_update (embed, text);

  return msg->message_id;
}

void
ephy_embed_statusbar_pop (EphyEmbed *embed, guint context_id)
{
  EphyEmbedPrivate *priv;
  EphyEmbedStatusbarMsg *msg;
  GSList *list;

  g_return_if_fail (EPHY_IS_EMBED (embed));
  g_return_if_fail (context_id != 0);

  priv = embed->priv;

  for (list = priv->messages; list; list = list->next) {
    EphyEmbedStatusbarMsg *msg = list->data;

    if (msg->context_id == context_id) {
      priv->messages = g_slist_remove_link (priv->messages, list);
      g_free (msg->text);
      g_slice_free (EphyEmbedStatusbarMsg, msg);
      g_slist_free_1 (list);
      break;
    }
  }

  msg = priv->messages ? priv->messages->data : NULL;
  ephy_embed_statusbar_update (embed, msg ? msg->text : NULL);
}
