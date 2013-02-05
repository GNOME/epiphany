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
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-web-view.h"
#include "nautilus-floating-bar.h"

#include <glib/gi18n.h>
#ifdef HAVE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif

static void     ephy_embed_constructed      (GObject *object);
#ifndef HAVE_WEBKIT2
static gboolean ephy_embed_inspect_show_cb  (WebKitWebInspector *inspector,
                                             EphyEmbed *embed);
static gboolean ephy_embed_inspect_close_cb (WebKitWebInspector *inspector,
                                             EphyEmbed *embed);
#endif

#define EPHY_EMBED_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED, EphyEmbedPrivate))

#define EPHY_EMBED_STATUSBAR_TAB_MESSAGE_CONTEXT_DESCRIPTION "tab_message"

typedef struct {
  gchar *text;
  guint context_id;
  guint message_id;
} EphyEmbedStatusbarMsg;

struct _EphyEmbedPrivate
{
  GtkBox *top_widgets_vbox;
#ifndef HAVE_WEBKIT2
  GtkScrolledWindow *scrolled_window;
  GtkWidget *inspector_window;
  GtkWidget *inspector_web_view;
  GtkWidget *inspector_scrolled_window;
  gboolean inspector_attached;
#endif
  GtkPaned *paned;
  WebKitWebView *web_view;
  GSList *destroy_on_transition_list;
  GtkWidget *floating_bar;
  GtkWidget *progress;
  GtkWidget *fullscreen_message_label;
  char *fullscreen_string;

  GtkWidget *overview;
  guint overview_mode : 1;
  GSList *messages;
  GSList *keys;

  guint seq_context_id;
  guint seq_message_id;

  guint tab_message_id;
  guint pop_statusbar_later_source_id;

  guint fullscreen_message_id;

  guint clear_progress_source_id;

  gulong status_handler_id;
  gulong progress_update_handler_id;

  gulong adblock_handler_id;
};

enum
{
  PROP_0,
  PROP_OVERVIEW_MODE,
};

G_DEFINE_TYPE (EphyEmbed, ephy_embed, GTK_TYPE_BOX)

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

static guint
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

static void
ephy_embed_set_statusbar_label (EphyEmbed *embed, const char *label)
{
  EphyEmbedPrivate *priv = embed->priv;

  nautilus_floating_bar_set_label (NAUTILUS_FLOATING_BAR (priv->floating_bar), label);

  if (label == NULL || label[0] == '\0') {
    gtk_widget_hide (priv->floating_bar);
    gtk_widget_set_halign (priv->floating_bar, GTK_ALIGN_START);
  } else
    gtk_widget_show (priv->floating_bar);
}

static void
ephy_embed_statusbar_update (EphyEmbed *embed, const char *text)
{
  g_return_if_fail (EPHY_IS_EMBED (embed));

  ephy_embed_set_statusbar_label (embed, text);
}

static guint
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

/* End of code based on GTK+ GtkStatusbar. */

static void
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

#ifdef HAVE_WEBKIT2
/* TODO: WebKitWebResource::send-request */
#else
static void
resource_request_starting_cb (WebKitWebView *web_view,
                              WebKitWebFrame *web_frame,
                              WebKitWebResource *web_resource,
                              WebKitNetworkRequest *request,
                              WebKitNetworkResponse *response,
                              EphyEmbed *embed)
{
  EphyAdBlockManager *adblock_manager = EPHY_ADBLOCK_MANAGER (ephy_embed_shell_get_adblock_manager (ephy_embed_shell_get_default ()));
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
#endif

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

#ifdef HAVE_WEBKIT2
static void
load_changed_cb (WebKitWebView *web_view,
                 WebKitLoadEvent load_event,
                 EphyEmbed *embed)
{
  const char *address;

  if (load_event == WEBKIT_LOAD_COMMITTED) {
    ephy_embed_destroy_top_widgets (embed);
    address = ephy_web_view_get_address (EPHY_WEB_VIEW (web_view));
    ephy_embed_set_overview_mode (embed, strcmp (address, "ephy-about:overview") == 0);
  }
}
#else
static void
load_status_changed_cb (WebKitWebView *web_view,
                        GParamSpec *spec,
                        EphyEmbed *embed)
{
  WebKitLoadStatus status = webkit_web_view_get_load_status (web_view);
  const char *address;

  if (status == WEBKIT_LOAD_COMMITTED) {
    ephy_embed_destroy_top_widgets (embed);
    address = ephy_web_view_get_address (EPHY_WEB_VIEW (web_view));
    ephy_embed_set_overview_mode (embed, strcmp (address, "ephy-about:overview") == 0);
  }
}
#endif

static void
ephy_embed_grab_focus (GtkWidget *widget)
{
  GtkWidget *child;

  child = GTK_WIDGET (ephy_embed_get_web_view (EPHY_EMBED (widget)));

  if (child)
    gtk_widget_grab_focus (child);
}


static gboolean
fullscreen_message_label_hide (EphyEmbed *embed)
{
  if (embed->priv->fullscreen_message_id) {
    gtk_widget_hide (embed->priv->fullscreen_message_label);
    g_source_remove (embed->priv->fullscreen_message_id);
    embed->priv->fullscreen_message_id = 0;
  }

  return FALSE;
}

void
ephy_embed_entering_fullscreen (EphyEmbed *embed)
{
  gtk_widget_show (embed->priv->fullscreen_message_label);

  if (embed->priv->fullscreen_message_id)
    g_source_remove (embed->priv->fullscreen_message_id);

  embed->priv->fullscreen_message_id = g_timeout_add_seconds (5,
                                                              (GSourceFunc)fullscreen_message_label_hide,
                                                              embed);
}

void
ephy_embed_leaving_fullscreen (EphyEmbed *embed)
{
  fullscreen_message_label_hide (embed);
}

static void
ephy_embed_dispose (GObject *object)
{
  EphyEmbed *embed = EPHY_EMBED (object);
  EphyEmbedPrivate *priv = embed->priv;

#ifndef HAVE_WEBKIT2
  if (priv->inspector_window) {
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
#endif

  if (priv->pop_statusbar_later_source_id) {
    g_source_remove (priv->pop_statusbar_later_source_id);
    priv->pop_statusbar_later_source_id = 0;
  }

  if (priv->clear_progress_source_id) {
    g_source_remove (priv->clear_progress_source_id);
    priv->clear_progress_source_id = 0;
  }

  /* Do not listen to status message notifications anymore, if we try
   * to update the statusbar after dispose we might crash. */
  if (priv->status_handler_id) {
    g_signal_handler_disconnect (priv->web_view, priv->status_handler_id);
    priv->status_handler_id = 0;
  }

  if (priv->progress_update_handler_id) {
    g_signal_handler_disconnect (priv->web_view, priv->progress_update_handler_id);
    priv->progress_update_handler_id = 0;
  }

  if (priv->fullscreen_message_id) {
    g_source_remove (priv->fullscreen_message_id);
    priv->fullscreen_message_id = 0;
  }

  if (priv->adblock_handler_id) {
    g_signal_handler_disconnect (priv->web_view, priv->adblock_handler_id);
    priv->adblock_handler_id = 0;
  }

  G_OBJECT_CLASS (ephy_embed_parent_class)->dispose (object);
}

static void
ephy_embed_finalize (GObject *object)
{
  EphyEmbed *embed = EPHY_EMBED (object);
  EphyEmbedPrivate *priv = embed->priv;
  GSList *list;

  list = priv->destroy_on_transition_list;
  for (; list; list = list->next) {
    GtkWidget *widget = GTK_WIDGET (list->data);
    g_signal_handlers_disconnect_by_func (widget, remove_from_destroy_list_cb, embed);
  }
  g_slist_free (priv->destroy_on_transition_list);

  for (list = priv->messages; list; list = list->next) {
    EphyEmbedStatusbarMsg *msg;

    msg = list->data;
    g_free (msg->text);
    g_slice_free (EphyEmbedStatusbarMsg, msg);
  }

  g_slist_free (priv->messages);
  priv->messages = NULL;

  for (list = priv->keys; list; list = list->next)
    g_free (list->data);

  g_slist_free (priv->keys);
  priv->keys = NULL;

  g_free (embed->priv->fullscreen_string);

  G_OBJECT_CLASS (ephy_embed_parent_class)->finalize (object);
}

static void
ephy_embed_set_property (GObject *object,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
  EphyEmbed *embed = EPHY_EMBED (object);

  switch (prop_id)
  {
  case PROP_OVERVIEW_MODE:
    ephy_embed_set_overview_mode (embed, g_value_get_boolean (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
ephy_embed_get_property (GObject *object,
                         guint prop_id,
                         GValue *value,
                         GParamSpec *pspec)
{
  EphyEmbed *embed = EPHY_EMBED (object);

  switch (prop_id)
  {
  case PROP_OVERVIEW_MODE:
    g_value_set_boolean (value, ephy_embed_get_overview_mode (embed));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
ephy_embed_class_init (EphyEmbedClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;

  object_class->constructed = ephy_embed_constructed;
  object_class->finalize = ephy_embed_finalize;
  object_class->dispose = ephy_embed_dispose;
  object_class->set_property = ephy_embed_set_property;
  object_class->get_property = ephy_embed_get_property;
  widget_class->grab_focus = ephy_embed_grab_focus;

/**
 * EphyEmbed:overview-mode:
 *
 * If %TRUE activates the overview mode in this #EphyEmbed.
 **/
  g_object_class_install_property (object_class,
                                   PROP_OVERVIEW_MODE,
                                   g_param_spec_boolean ("overview-mode",
                                                         "Overview mode",
                                                         "Whether the embed is showing the overview",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_type_class_add_private (G_OBJECT_CLASS (klass), sizeof(EphyEmbedPrivate));
}

#ifdef HAVE_WEBKIT2
static gboolean
ephy_embed_attach_inspector_cb (WebKitWebInspector *inspector,
                                EphyEmbed *embed)
{
  GtkWidget *inspector_view = GTK_WIDGET (webkit_web_inspector_get_web_view (inspector));
  int inspected_view_height;
  guint attached_height;

  inspected_view_height = gtk_widget_get_allocated_height (GTK_WIDGET (embed->priv->web_view));
  attached_height = webkit_web_inspector_get_attached_height (inspector);
  gtk_paned_set_position (embed->priv->paned, inspected_view_height - attached_height);

  gtk_paned_add2 (embed->priv->paned, inspector_view);
  gtk_widget_show (inspector_view);

  return TRUE;
}
#else
static WebKitWebView *
ephy_embed_inspect_web_view_cb (WebKitWebInspector *inspector,
                                WebKitWebView *web_view,
                                EphyEmbed *embed)
{
  EphyEmbedPrivate *priv = embed->priv;

  priv->inspector_web_view = ephy_web_view_new();

  gtk_container_add (GTK_CONTAINER (priv->inspector_scrolled_window),
                     priv->inspector_web_view);

  return WEBKIT_WEB_VIEW (priv->inspector_web_view);
}

static gboolean
ephy_embed_attach_inspector_cb (WebKitWebInspector *inspector,
                                EphyEmbed *embed)
{
  embed->priv->inspector_attached = TRUE;

  gtk_widget_hide (embed->priv->inspector_window);
  gtk_widget_reparent (GTK_WIDGET (embed->priv->inspector_scrolled_window),
                       GTK_WIDGET (embed->priv->paned));

  return TRUE;
}

static void
ephy_embed_detach_inspector (EphyEmbed *embed, WebKitWebInspector *inspector)
{
  embed->priv->inspector_attached = FALSE;

  gtk_widget_reparent (GTK_WIDGET (embed->priv->inspector_scrolled_window),
                       GTK_WIDGET (embed->priv->inspector_window));
}

static gboolean
ephy_embed_detach_inspector_cb (WebKitWebInspector *inspector,
                                EphyEmbed *embed)
{
  ephy_embed_detach_inspector (embed, inspector);

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
  } else {
    GtkAllocation allocation;
    gtk_widget_get_allocation (GTK_WIDGET (embed->priv->scrolled_window), &allocation);

    /* Set a sane position for the mover */
    gtk_paned_set_position (embed->priv->paned, allocation.height * 0.5);
    gtk_widget_show (embed->priv->inspector_scrolled_window);
  }

  return TRUE;
}

static gboolean
ephy_embed_inspect_close_cb (WebKitWebInspector *inspector,
                             EphyEmbed *embed)
{
  EphyEmbedPrivate *priv = embed->priv;

  if (!priv->inspector_window)
    return TRUE;

  gtk_widget_destroy (priv->inspector_web_view);
  priv->inspector_web_view = NULL;

  if (!priv->inspector_attached)
    gtk_widget_hide (priv->inspector_window);
  else
    gtk_widget_hide (priv->inspector_scrolled_window);

  ephy_embed_detach_inspector (embed, inspector);

  return TRUE;
}
#endif

void
ephy_embed_auto_download_url (EphyEmbed *embed, const char *url)
{
  EphyDownload *download;

  download = ephy_download_new_for_uri (url, NULL);
  ephy_download_set_auto_destination (download);
  ephy_download_set_action (download, EPHY_DOWNLOAD_ACTION_OPEN);
}

#ifndef HAVE_WEBKIT2
static gboolean
download_requested_cb (WebKitWebView *web_view,
                       WebKitDownload *download,
                       EphyEmbed *embed)
{
  EphyDownload *ed;
  GtkWidget *toplevel;
  GtkWindow *window = NULL;

  /* Is download locked down? */
  if (g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN,
                              EPHY_PREFS_LOCKDOWN_SAVE_TO_DISK))
    return FALSE;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (embed));
  if (GTK_IS_WINDOW (toplevel))
    window = GTK_WINDOW (toplevel);

  ed = ephy_download_new_for_download (download, window);
  ephy_download_set_auto_destination (ed);

  return TRUE;
}
#endif

static void
ephy_embed_set_fullscreen_message (EphyEmbed *embed,
                                   gboolean is_html5_fullscreen)
{
  char *message;

  if (G_UNLIKELY (embed->priv->fullscreen_string == NULL))
    embed->priv->fullscreen_string = g_strdup (_("Press %s to exit fullscreen"));

  /* Translators: 'ESC' and 'F11' are keyboard keys. */
  message = g_strdup_printf (embed->priv->fullscreen_string, is_html5_fullscreen ? _("ESC") : _("F11"));
  gtk_label_set_text (GTK_LABEL (embed->priv->fullscreen_message_label),
                      message);
  g_free (message);
}

static gboolean
entering_fullscreen_cb (WebKitWebView *web_view,
#ifndef HAVE_WEBKIT2
                        GObject *element,
#endif
                        EphyEmbed *embed)
{
  ephy_embed_set_fullscreen_message (embed, TRUE);
  return FALSE;
}

static gboolean
leaving_fullscreen_cb (WebKitWebView *web_view,
#ifndef HAVE_WEBKIT2
                       GObject *element,
#endif
                       EphyEmbed *embed)
{
  ephy_embed_set_fullscreen_message (embed, FALSE);
  return FALSE;
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
    if (priv->pop_statusbar_later_source_id == 0) {
      priv->pop_statusbar_later_source_id = g_timeout_add (250, pop_statusbar_later_cb, embed);
    }
  }
}

#ifdef HAVE_WEBKIT2
static void
window_geometry_changed (WebKitWindowProperties *properties, GParamSpec *pspec, EphyEmbed *embed)
{
  GtkWidget *window;
  gboolean is_popup;
  GdkRectangle geometry;

  window = gtk_widget_get_toplevel (GTK_WIDGET (embed));
  if (!window || !gtk_widget_is_toplevel (window))
    return;

  g_object_get (window, "is-popup", &is_popup, NULL);
  if (!is_popup)
    return;

  webkit_window_properties_get_geometry (properties, &geometry);
  if (geometry.x >= 0 && geometry.y >= 0)
    gtk_window_move (GTK_WINDOW (window), geometry.x, geometry.y);
  if (geometry.width > 0 && geometry.height > 0)
    gtk_window_resize (GTK_WINDOW (window), geometry.width, geometry.height);
}
#else
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
#endif

static gboolean
clear_progress_cb (EphyEmbed *embed)
{
  gtk_widget_hide (embed->priv->progress);
  embed->priv->clear_progress_source_id = 0;

  return FALSE;
}

static void
progress_update (EphyWebView *view, GParamSpec *pspec, EphyEmbed *embed)
{
  gdouble progress;
  gboolean loading;
  const char *uri;

  EphyEmbedPrivate *priv = embed->priv;

  if (priv->clear_progress_source_id) {
    g_source_remove (priv->clear_progress_source_id);
    priv->clear_progress_source_id = 0;
  }

  uri = webkit_web_view_get_uri (priv->web_view);
  if (!uri || g_str_has_prefix (uri, "ephy-about:") ||
      g_str_has_prefix (uri, "about:"))
    return;

#ifdef HAVE_WEBKIT2
  progress = webkit_web_view_get_estimated_load_progress (priv->web_view);
#else
  progress = webkit_web_view_get_progress (priv->web_view);
#endif
  loading = ephy_web_view_is_loading (EPHY_WEB_VIEW (priv->web_view));

  if (progress == 1.0 || !loading)
    priv->clear_progress_source_id = g_timeout_add (500,
                                                    (GSourceFunc)clear_progress_cb,
                                                    embed);
  else
    gtk_widget_show (priv->progress);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->progress),
                                 (loading || progress == 1.0) ? progress : 0.0);
}

#ifndef HAVE_WEBKIT2
static void
setup_adblock (GSettings *settings,
               char *key,
               EphyEmbed *embed)
{
  EphyEmbedPrivate *priv = embed->priv;
  EphyWebView *web_view = ephy_embed_get_web_view (embed);

  if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_ADBLOCK) &&
      priv->adblock_handler_id == 0) {
    priv->adblock_handler_id = g_signal_connect (web_view, "resource_request_starting",
                                                 G_CALLBACK (resource_request_starting_cb), embed);
  } else if (priv->adblock_handler_id) {
    g_signal_handler_disconnect (web_view, priv->adblock_handler_id);
    priv->adblock_handler_id = 0;
  }
}
#endif

static void
ephy_embed_constructed (GObject *object)
{
  EphyEmbed *embed = (EphyEmbed*)object;
  EphyEmbedPrivate *priv = embed->priv;
#ifndef HAVE_WEBKIT2
  GtkWidget *scrolled_window;
#endif
  GtkWidget *paned;
  WebKitWebView *web_view;
#ifdef HAVE_WEBKIT2
  WebKitWindowProperties *window_properties;
#else
  WebKitWebWindowFeatures *window_features;
#endif
  WebKitWebInspector *inspector;
  GtkWidget *overlay;

  /* Skeleton */
  web_view = WEBKIT_WEB_VIEW (ephy_web_view_new ());
#ifndef HAVE_WEBKIT2
  scrolled_window = GTK_WIDGET (priv->scrolled_window);
#endif
  overlay = gtk_overlay_new ();

  gtk_widget_add_events (overlay, 
                         GDK_ENTER_NOTIFY_MASK |
                         GDK_LEAVE_NOTIFY_MASK);
#ifdef HAVE_WEBKIT2
  gtk_container_add (GTK_CONTAINER (overlay), GTK_WIDGET (web_view));
#else
  gtk_container_add (GTK_CONTAINER (overlay), scrolled_window);
#endif

  /* The overview */
  priv->overview = ephy_overview_new ();
  gtk_widget_set_halign (priv->overview, GTK_ALIGN_FILL);
  gtk_widget_set_valign (priv->overview, GTK_ALIGN_FILL);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), priv->overview);

  g_object_bind_property (embed, "overview-mode",
                          priv->overview, "visible",
                          G_BINDING_SYNC_CREATE);

  /* Floating message popup for fullscreen mode. */
  priv->fullscreen_message_label = gtk_label_new (NULL);
  gtk_widget_set_name (priv->fullscreen_message_label, "fullscreen-popup");
  gtk_widget_set_halign (priv->fullscreen_message_label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (priv->fullscreen_message_label, GTK_ALIGN_CENTER);
  gtk_widget_set_no_show_all (priv->fullscreen_message_label, TRUE);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), priv->fullscreen_message_label);
  ephy_embed_set_fullscreen_message (embed, FALSE);

  /* statusbar is hidden by default */
  priv->floating_bar = nautilus_floating_bar_new (NULL, FALSE);
  gtk_widget_set_halign (priv->floating_bar, GTK_ALIGN_START);
  gtk_widget_set_valign (priv->floating_bar, GTK_ALIGN_END);
  gtk_widget_set_no_show_all (priv->floating_bar, TRUE);

  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), priv->floating_bar);

  priv->progress = gtk_progress_bar_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (priv->progress),
                               GTK_STYLE_CLASS_OSD);
  gtk_widget_set_halign (priv->progress, GTK_ALIGN_FILL);
  gtk_widget_set_valign (priv->progress, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), priv->progress);

  paned = GTK_WIDGET (priv->paned);

  priv->web_view = web_view;
#ifdef HAVE_WEBKIT2
  priv->progress_update_handler_id = g_signal_connect (web_view, "notify::estimated-load-progress",
                                                       G_CALLBACK (progress_update), object);
#else
  priv->progress_update_handler_id =  g_signal_connect (web_view, "notify::progress",
                                                        G_CALLBACK (progress_update), object);
#endif

#ifndef HAVE_WEBKIT2
  gtk_container_add (GTK_CONTAINER (scrolled_window),
                     GTK_WIDGET (web_view));
#endif
  gtk_paned_pack1 (GTK_PANED (paned), GTK_WIDGET (overlay),
                   TRUE, FALSE);

  gtk_box_pack_start (GTK_BOX (embed),
                      GTK_WIDGET (priv->top_widgets_vbox),
                      FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (embed), paned, TRUE, TRUE, 0);

  gtk_widget_show (GTK_WIDGET (priv->top_widgets_vbox));
  gtk_widget_show (GTK_WIDGET (web_view));
  gtk_widget_show_all (paned);

#ifdef HAVE_WEBKIT2
  /* TODO: WebKitWebResource::send-request, Downloads */
  g_object_connect (web_view,
                    "signal::load-changed", G_CALLBACK (load_changed_cb), embed,
                    "signal::enter-fullscreen", G_CALLBACK (entering_fullscreen_cb), embed,
                    "signal::leave-fullscreen", G_CALLBACK (leaving_fullscreen_cb), embed,
                    NULL);
#else
  g_object_connect (web_view,
                    "signal::notify::load-status", G_CALLBACK (load_status_changed_cb), embed,
                    "signal::download-requested", G_CALLBACK (download_requested_cb), embed,
                    "signal::entering-fullscreen", G_CALLBACK (entering_fullscreen_cb), embed,
                    "signal::leaving-fullscreen", G_CALLBACK (leaving_fullscreen_cb), embed,
                    NULL);

  g_signal_connect (EPHY_SETTINGS_WEB,
                    "changed::" EPHY_PREFS_WEB_ENABLE_ADBLOCK,
                    G_CALLBACK (setup_adblock), embed);

  setup_adblock (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ENABLE_ADBLOCK, embed);
#endif

  priv->status_handler_id = g_signal_connect (web_view, "notify::status-message",
                                              G_CALLBACK (status_message_notify_cb),
                                              embed);
#ifdef HAVE_WEBKIT2
  /* Window properties */
  window_properties = webkit_web_view_get_window_properties (web_view);
  g_signal_connect (window_properties, "notify::geometry",
                    G_CALLBACK (window_geometry_changed),
                    embed);
#else
  /* Window features */
  window_features = webkit_web_view_get_window_features (web_view);
  g_object_connect (window_features,
                    "signal::notify::x", G_CALLBACK (window_resize_requested), embed,
                    "signal::notify::y", G_CALLBACK (window_resize_requested), embed,
                    "signal::notify::width", G_CALLBACK (window_resize_requested), embed,
                    "signal::notify::height", G_CALLBACK (window_resize_requested), embed,
                    NULL);
#endif

  /* The inspector */
  inspector = webkit_web_view_get_inspector (web_view);

#ifdef HAVE_WEBKIT2
  g_signal_connect (inspector, "attach",
                    G_CALLBACK (ephy_embed_attach_inspector_cb),
                    embed);
#else
  priv->inspector_attached = TRUE;
  priv->inspector_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->inspector_scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (priv->paned),
                     priv->inspector_scrolled_window);

  priv->inspector_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (priv->inspector_window),
                        _("Web Inspector"));
  gtk_window_set_default_size (GTK_WINDOW (priv->inspector_window),
                               800, 600);

  g_signal_connect (priv->inspector_window,
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
#endif

  ephy_embed_prefs_add_embed (embed);
}

static void
ephy_embed_init (EphyEmbed *embed)
{
  EphyEmbedPrivate *priv;

  priv = embed->priv = EPHY_EMBED_GET_PRIVATE (embed);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (embed),
                                  GTK_ORIENTATION_VERTICAL);

#ifndef HAVE_WEBKIT2
  priv->scrolled_window = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
#endif
  priv->paned = GTK_PANED (gtk_paned_new (GTK_ORIENTATION_VERTICAL));
  priv->top_widgets_vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  priv->seq_context_id = 1;
  priv->seq_message_id = 1;
  priv->tab_message_id = ephy_embed_statusbar_get_context_id (embed, EPHY_EMBED_STATUSBAR_TAB_MESSAGE_CONTEXT_DESCRIPTION);

#ifndef HAVE_WEBKIT2
  gtk_scrolled_window_set_policy (priv->scrolled_window,
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (priv->scrolled_window,
                                       GTK_SHADOW_IN);
#endif
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
 * must have been added using ephy_embed_add_top_widget(), and not
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

void
ephy_embed_set_overview_mode (EphyEmbed *embed, gboolean overview_mode)
{
  EphyEmbedPrivate *priv;

  g_return_if_fail (EPHY_IS_EMBED (embed));

  priv = embed->priv;

  if (priv->overview_mode == overview_mode)
    return;

  priv->overview_mode = overview_mode;

  g_object_notify (G_OBJECT (embed), "overview-mode");
}

gboolean
ephy_embed_get_overview_mode (EphyEmbed *embed)
{
  g_return_val_if_fail (EPHY_IS_EMBED (embed), FALSE);

  return embed->priv->overview_mode;
}

/**
 * ephy_embed_get_overview:
 * @embed: a #EphyEmbed
 *
 * Gets the #EphyOverview in this @embed
 *
 * Returns: (transfer none): the overview widget
 **/
EphyOverview *
ephy_embed_get_overview (EphyEmbed *embed)
{
  g_return_val_if_fail (EPHY_IS_EMBED (embed), NULL);

  return EPHY_OVERVIEW (embed->priv->overview);
}
