/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/*
 *  Copyright © 2007 Xan Lopez
 *  Copyright © 2008 Jan Alonzo
 *  Copyright © 2009 Gustavo Noronha Silva
 *  Copyright © 2009 Igalia S.L.
 *  Copyright © 2009 Collabora Ltd.
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-embed.h"

#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-find-toolbar.h"
#include "ephy-notification-container.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-string.h"
#include "ephy-web-view.h"
#include "nautilus-floating-bar.h"

#include <glib/gi18n.h>
#include <webkit2/webkit2.h>

static void     ephy_embed_constructed (GObject *object);
static void     ephy_embed_restored_window_cb (EphyEmbedShell *shell,
                                               EphyEmbed      *embed);

#define EPHY_EMBED_STATUSBAR_TAB_MESSAGE_CONTEXT_DESCRIPTION "tab_message"
#define MAX_TITLE_LENGTH 512 /* characters */

typedef struct {
  gchar *text;
  guint context_id;
  guint message_id;
} EphyEmbedStatusbarMsg;

struct _EphyEmbed {
  GtkBox parent_instance;

  EphyFindToolbar *find_toolbar;
  GtkBox *top_widgets_vbox;
  WebKitWebView *web_view;
  GSList *destroy_on_transition_list;
  GtkWidget *overlay;
  GtkWidget *floating_bar;
  GtkWidget *progress;
  GtkWidget *fullscreen_message_label;

  char *title;
  WebKitURIRequest *delayed_request;
  WebKitWebViewSessionState *delayed_state;
  guint delayed_request_source_id;

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
  gboolean inspector_loaded;
  gboolean progress_bar_enabled;
};

G_DEFINE_TYPE (EphyEmbed, ephy_embed, GTK_TYPE_BOX)

enum {
  PROP_0,
  PROP_WEB_VIEW,
  PROP_TITLE,
  PROP_PROGRESS_BAR_ENABLED,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

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
ephy_embed_statusbar_get_context_id (EphyEmbed  *embed,
                                     const char *context_description)
{
  char *string;
  guint id;

  g_assert (EPHY_IS_EMBED (embed));
  g_assert (context_description != NULL);

  /* we need to preserve namespaces on object datas */
  string = g_strconcat ("ephy-embed-status-bar-context:", context_description, NULL);

  id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (embed), string));
  if (id == 0) {
    id = embed->seq_context_id++;
    g_object_set_data_full (G_OBJECT (embed), string, GUINT_TO_POINTER (id), NULL);
    embed->keys = g_slist_prepend (embed->keys, string);
  } else
    g_free (string);

  return id;
}

static void
ephy_embed_set_statusbar_label (EphyEmbed  *embed,
                                const char *label)
{
  nautilus_floating_bar_set_primary_label (NAUTILUS_FLOATING_BAR (embed->floating_bar), label);

  if (label == NULL || label[0] == '\0') {
    gtk_widget_hide (embed->floating_bar);
    gtk_widget_set_halign (embed->floating_bar, GTK_ALIGN_START);
  } else
    gtk_widget_show (embed->floating_bar);
}

static void
ephy_embed_statusbar_update (EphyEmbed  *embed,
                             const char *text)
{
  g_assert (EPHY_IS_EMBED (embed));

  ephy_embed_set_statusbar_label (embed, text);
}

static guint
ephy_embed_statusbar_push (EphyEmbed  *embed,
                           guint       context_id,
                           const char *text)
{
  EphyEmbedStatusbarMsg *msg;

  g_assert (EPHY_IS_EMBED (embed));
  g_assert (context_id != 0);
  g_assert (text != NULL);

  msg = g_new (EphyEmbedStatusbarMsg, 1);
  msg->text = g_strdup (text);
  msg->context_id = context_id;
  msg->message_id = embed->seq_message_id++;

  embed->messages = g_slist_prepend (embed->messages, msg);

  ephy_embed_statusbar_update (embed, text);

  return msg->message_id;
}

/* End of code based on GTK+ GtkStatusbar. */

static void
ephy_embed_statusbar_pop (EphyEmbed *embed,
                          guint      context_id)
{
  EphyEmbedStatusbarMsg *msg;
  GSList *list;

  g_assert (EPHY_IS_EMBED (embed));
  g_assert (context_id != 0);

  for (list = embed->messages; list; list = list->next) {
    msg = list->data;

    if (msg->context_id == context_id) {
      embed->messages = g_slist_remove_link (embed->messages, list);
      g_free (msg->text);
      g_free (msg);
      g_slist_free_1 (list);
      break;
    }
  }

  msg = embed->messages ? embed->messages->data : NULL;
  ephy_embed_statusbar_update (embed, msg ? msg->text : NULL);
}

static void
remove_from_destroy_list_cb (GtkWidget *widget,
                             EphyEmbed *embed)
{
  GSList *list;

  list = embed->destroy_on_transition_list;
  list = g_slist_remove (list, widget);
  embed->destroy_on_transition_list = list;
}

static void
ephy_embed_destroy_top_widgets (EphyEmbed *embed)
{
  GSList *iter;

  for (iter = embed->destroy_on_transition_list; iter; iter = iter->next) {
    g_signal_handlers_disconnect_by_func (iter->data, remove_from_destroy_list_cb, embed);
    gtk_widget_destroy (GTK_WIDGET (iter->data));
  }

  embed->destroy_on_transition_list = NULL;
}

static void
ephy_embed_set_title (EphyEmbed  *embed,
                      const char *title)
{
  char *new_title;

  new_title = g_strdup (title);
  if (new_title == NULL || g_strstrip (new_title)[0] == '\0') {
    const char *address;

    g_free (new_title);
    new_title = NULL;

    address = ephy_web_view_get_address (EPHY_WEB_VIEW (embed->web_view));
    if (address && strcmp (address, "about:blank") != 0)
      new_title = ephy_embed_utils_get_title_from_address (address);

    if (new_title == NULL || new_title[0] == '\0') {
      g_free (new_title);
      new_title = g_strdup (_(BLANK_PAGE_TITLE));
    }
  }

  g_free (embed->title);
  embed->title = ephy_string_shorten (new_title, MAX_TITLE_LENGTH);

  g_object_notify_by_pspec (G_OBJECT (embed), obj_properties[PROP_TITLE]);
}

static void
web_view_title_changed_cb (WebKitWebView *web_view,
                           GParamSpec    *spec,
                           EphyEmbed     *embed)
{
  ephy_embed_set_title (embed, webkit_web_view_get_title (web_view));
}

static void
load_changed_cb (WebKitWebView   *web_view,
                 WebKitLoadEvent  load_event,
                 EphyEmbed       *embed)
{
  switch (load_event) {
    case WEBKIT_LOAD_COMMITTED:
      ephy_embed_destroy_top_widgets (embed);
      break;
    case WEBKIT_LOAD_FINISHED: {
      const char *title = webkit_web_view_get_title (web_view);
      if (ephy_web_view_get_is_blank (EPHY_WEB_VIEW (web_view)) || !title || !*title)
        ephy_embed_set_title (embed, NULL);
      break;
    }
    case WEBKIT_LOAD_STARTED:
    case WEBKIT_LOAD_REDIRECTED:
    default:
      break;
  }
}

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
  if (embed->fullscreen_message_id) {
    gtk_widget_hide (embed->fullscreen_message_label);
    g_source_remove (embed->fullscreen_message_id);
    embed->fullscreen_message_id = 0;
  }

  return FALSE;
}

void
ephy_embed_entering_fullscreen (EphyEmbed *embed)
{
  if (!g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN, EPHY_PREFS_LOCKDOWN_FULLSCREEN)) {
    gtk_widget_show (embed->fullscreen_message_label);

    g_clear_handle_id (&embed->fullscreen_message_id, g_source_remove);
    embed->fullscreen_message_id = g_timeout_add_seconds (5,
                                                          (GSourceFunc)fullscreen_message_label_hide,
                                                          embed);
    g_source_set_name_by_id (embed->fullscreen_message_id, "[epiphany] fullscreen_message_label_hide");
  }
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

  g_clear_handle_id (&embed->pop_statusbar_later_source_id, g_source_remove);
  g_clear_handle_id (&embed->clear_progress_source_id, g_source_remove);
  g_clear_handle_id (&embed->delayed_request_source_id, g_source_remove);
  g_clear_handle_id (&embed->fullscreen_message_id, g_source_remove);

  g_clear_signal_handler (&embed->status_handler_id, embed->web_view);
  g_clear_signal_handler (&embed->progress_update_handler_id, embed->web_view);

  g_clear_object (&embed->delayed_request);
  g_clear_pointer (&embed->delayed_state, webkit_web_view_session_state_unref);

  G_OBJECT_CLASS (ephy_embed_parent_class)->dispose (object);
}

static void
ephy_embed_finalize (GObject *object)
{
  EphyEmbed *embed = EPHY_EMBED (object);
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  GSList *list;

  g_signal_handlers_disconnect_by_func (shell, ephy_embed_restored_window_cb, embed);

  list = embed->destroy_on_transition_list;
  for (; list; list = list->next) {
    GtkWidget *widget = GTK_WIDGET (list->data);
    g_signal_handlers_disconnect_by_func (widget, remove_from_destroy_list_cb, embed);
  }
  g_slist_free (embed->destroy_on_transition_list);

  for (list = embed->messages; list; list = list->next) {
    EphyEmbedStatusbarMsg *msg;

    msg = list->data;
    g_free (msg->text);
    g_free (msg);
  }

  g_slist_free (embed->messages);
  embed->messages = NULL;

  for (list = embed->keys; list; list = list->next)
    g_free (list->data);

  g_slist_free (embed->keys);
  embed->keys = NULL;

  g_free (embed->title);

  G_OBJECT_CLASS (ephy_embed_parent_class)->finalize (object);
}

static void
ephy_embed_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  EphyEmbed *embed = EPHY_EMBED (object);

  switch (prop_id) {
    case PROP_WEB_VIEW:
      embed->web_view = g_value_get_object (value);
      break;
    case PROP_TITLE:
      ephy_embed_set_title (embed, g_value_get_string (value));
      break;
    case PROP_PROGRESS_BAR_ENABLED:
      embed->progress_bar_enabled = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_embed_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  EphyEmbed *embed = EPHY_EMBED (object);

  switch (prop_id) {
    case PROP_WEB_VIEW:
      g_value_set_object (value, ephy_embed_get_web_view (embed));
      break;
    case PROP_TITLE:
      g_value_set_string (value, ephy_embed_get_title (embed));
      break;
    case PROP_PROGRESS_BAR_ENABLED:
      g_value_set_boolean (value, embed->progress_bar_enabled);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_embed_find_toolbar_close_cb (EphyFindToolbar *toolbar,
                                  EphyEmbed       *embed)
{
  ephy_find_toolbar_close (embed->find_toolbar);

  gtk_widget_grab_focus (GTK_WIDGET (embed));
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

  obj_properties[PROP_WEB_VIEW] =
    g_param_spec_object ("web-view",
                         "Web View",
                         "The WebView contained in the embed",
                         EPHY_TYPE_WEB_VIEW,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The embed's title",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_PROGRESS_BAR_ENABLED] =
    g_param_spec_boolean ("progress-bar-enabled",
                          "Progress bar",
                          "Whether to show progress bar within embed",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

static gboolean
ephy_embed_attach_inspector_cb (WebKitWebInspector *inspector,
                                EphyEmbed          *embed)
{
  embed->inspector_loaded = TRUE;

  return FALSE;
}

static gboolean
ephy_embed_close_inspector_cb (WebKitWebInspector *inspector,
                               EphyEmbed          *embed)
{
  embed->inspector_loaded = FALSE;

  return TRUE;
}

static void
ephy_embed_set_fullscreen_message (EphyEmbed *embed,
                                   gboolean   is_html5_fullscreen)
{
  char *message;

  /* Translators: 'ESC' and 'F11' are keyboard keys. */
  message = g_strdup_printf (_("Press %s to exit fullscreen"), is_html5_fullscreen ? _("ESC") : _("F11"));
  gtk_label_set_text (GTK_LABEL (embed->fullscreen_message_label),
                      message);
  g_free (message);
}

static gboolean
entering_fullscreen_cb (WebKitWebView *web_view,
                        EphyEmbed     *embed)
{
  ephy_embed_set_fullscreen_message (embed, TRUE);
  return FALSE;
}

static gboolean
leaving_fullscreen_cb (WebKitWebView *web_view,
                       EphyEmbed     *embed)
{
  ephy_embed_set_fullscreen_message (embed, FALSE);
  return FALSE;
}

static gboolean
pop_statusbar_later_cb (gpointer data)
{
  EphyEmbed *embed = EPHY_EMBED (data);

  ephy_embed_statusbar_pop (embed, embed->tab_message_id);
  embed->pop_statusbar_later_source_id = 0;
  return FALSE;
}

static void
status_message_notify_cb (EphyWebView *view,
                          GParamSpec  *pspec,
                          EphyEmbed   *embed)
{
  const char *message;

  message = ephy_web_view_get_status_message (view);

  if (message) {
    g_clear_handle_id (&embed->pop_statusbar_later_source_id, g_source_remove);
    ephy_embed_statusbar_pop (embed, embed->tab_message_id);
    ephy_embed_statusbar_push (embed, embed->tab_message_id, message);
  } else {
    /* A short timeout before hiding the statusbar ensures that while moving
     *  over a series of links, the overlay widget doesn't flicker on and off. */
    if (embed->pop_statusbar_later_source_id == 0) {
      embed->pop_statusbar_later_source_id = g_timeout_add (250, pop_statusbar_later_cb, embed);
      g_source_set_name_by_id (embed->pop_statusbar_later_source_id, "[epiphany] pop_statusbar_later_cb");
    }
  }
}

static gboolean
clear_progress_cb (EphyEmbed *embed)
{
  gtk_widget_hide (embed->progress);
  embed->clear_progress_source_id = 0;

  return FALSE;
}

static void
progress_update (EphyWebView *view,
                 GParamSpec  *pspec,
                 EphyEmbed   *embed)
{
  gdouble progress;
  gboolean loading;
  const char *uri;

  g_clear_handle_id (&embed->clear_progress_source_id, g_source_remove);

  uri = webkit_web_view_get_uri (embed->web_view);
  if (!uri || g_str_has_prefix (uri, "ephy-about:") ||
      g_str_has_prefix (uri, "about:")) {
    gtk_widget_hide (embed->progress);
    return;
  }

  progress = webkit_web_view_get_estimated_load_progress (embed->web_view);
  loading = ephy_web_view_is_loading (EPHY_WEB_VIEW (embed->web_view));

  if (progress == 1.0 || !loading) {
    embed->clear_progress_source_id = g_timeout_add (500,
                                                     (GSourceFunc)clear_progress_cb,
                                                     embed);
    g_source_set_name_by_id (embed->clear_progress_source_id, "[epiphany] clear_progress_cb");
  } else
    gtk_widget_show (embed->progress);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (embed->progress),
                                 (loading || progress == 1.0) ? progress : 0.0);
}

static gboolean
load_delayed_request_if_mapped (gpointer user_data)
{
  EphyEmbed *embed = EPHY_EMBED (user_data);
  EphyWebView *web_view;
  WebKitBackForwardListItem *item;

  embed->delayed_request_source_id = 0;

  if (!gtk_widget_get_mapped (GTK_WIDGET (embed)))
    return G_SOURCE_REMOVE;

  web_view = ephy_embed_get_web_view (embed);
  if (embed->delayed_state)
    webkit_web_view_restore_session_state (WEBKIT_WEB_VIEW (web_view), embed->delayed_state);

  item = webkit_back_forward_list_get_current_item (webkit_web_view_get_back_forward_list (WEBKIT_WEB_VIEW (web_view)));
  if (item)
    webkit_web_view_go_to_back_forward_list_item (WEBKIT_WEB_VIEW (web_view), item);
  else
    ephy_web_view_load_request (web_view, embed->delayed_request);

  g_clear_object (&embed->delayed_request);
  g_clear_pointer (&embed->delayed_state, webkit_web_view_session_state_unref);

  return G_SOURCE_REMOVE;
}

static void
ephy_embed_maybe_load_delayed_request (EphyEmbed *embed)
{
  if (!embed->delayed_request || embed->delayed_request_source_id != 0)
    return;

  /* Add a very small delay before loading the request, so that if the user
   * is scrolling rapidly through a bunch of delayed tabs, we don't start
   * loading them all.
   */
  embed->delayed_request_source_id = g_timeout_add (300, load_delayed_request_if_mapped, embed);
  g_source_set_name_by_id (embed->delayed_request_source_id, "[epiphany] load_delayed_request_if_mapped");
}

static void
ephy_embed_restored_window_cb (EphyEmbedShell *shell,
                               EphyEmbed      *embed)
{
  if (!gtk_widget_get_mapped (GTK_WIDGET (embed)))
    return;

  ephy_embed_maybe_load_delayed_request (embed);
}

static void
ephy_embed_mapped_cb (GtkWidget *widget,
                      gpointer   data)
{
  ephy_embed_maybe_load_delayed_request ((EphyEmbed *)widget);
}

static gboolean
on_enter_notify_event (GtkWidget        *widget,
                       GdkEventCrossing *event,
                       gpointer          user_data)
{
  EphyEmbed *embed = EPHY_EMBED (user_data);

  if (event->window != gtk_widget_get_window (embed->floating_bar))
    return GDK_EVENT_PROPAGATE;

  if (gtk_widget_get_halign (embed->floating_bar) == GTK_ALIGN_START)
    gtk_widget_set_halign (embed->floating_bar, GTK_ALIGN_END);
  else
    gtk_widget_set_halign (embed->floating_bar, GTK_ALIGN_START);

  gtk_widget_queue_allocate (embed->overlay);

  return GDK_EVENT_PROPAGATE;
}

static void
ephy_embed_constructed (GObject *object)
{
  EphyEmbed *embed = (EphyEmbed *)object;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  WebKitWebInspector *inspector;

  g_signal_connect (shell, "window-restored",
                    G_CALLBACK (ephy_embed_restored_window_cb), embed);

  g_signal_connect (embed, "map",
                    G_CALLBACK (ephy_embed_mapped_cb), NULL);

  /* Skeleton */
  embed->overlay = gtk_overlay_new ();

  gtk_widget_add_events (embed->overlay,
                         GDK_ENTER_NOTIFY_MASK |
                         GDK_LEAVE_NOTIFY_MASK);
  gtk_widget_set_vexpand (embed->overlay, TRUE);
  gtk_container_add (GTK_CONTAINER (embed->overlay), GTK_WIDGET (embed->web_view));

  /* Floating message popup for fullscreen mode. */
  embed->fullscreen_message_label = gtk_label_new (NULL);
  gtk_widget_set_name (embed->fullscreen_message_label, "fullscreen-popup");
  gtk_widget_set_halign (embed->fullscreen_message_label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (embed->fullscreen_message_label, GTK_ALIGN_CENTER);
  gtk_widget_set_no_show_all (embed->fullscreen_message_label, TRUE);
  gtk_overlay_add_overlay (GTK_OVERLAY (embed->overlay), embed->fullscreen_message_label);
  ephy_embed_set_fullscreen_message (embed, FALSE);

  /* statusbar is hidden by default */
  embed->floating_bar = nautilus_floating_bar_new ();
  gtk_widget_set_halign (embed->floating_bar, GTK_ALIGN_START);
  gtk_widget_set_valign (embed->floating_bar, GTK_ALIGN_END);
  gtk_widget_set_no_show_all (embed->floating_bar, TRUE);
  g_signal_connect_object (embed->overlay, "enter-notify-event", G_CALLBACK (on_enter_notify_event), embed, 0);

  gtk_overlay_add_overlay (GTK_OVERLAY (embed->overlay), embed->floating_bar);

  if (embed->progress_bar_enabled) {
    embed->progress = gtk_progress_bar_new ();
    gtk_style_context_add_class (gtk_widget_get_style_context (embed->progress), "osd");
    gtk_widget_set_halign (embed->progress, GTK_ALIGN_FILL);
    gtk_widget_set_valign (embed->progress, GTK_ALIGN_START);
    gtk_overlay_add_overlay (GTK_OVERLAY (embed->overlay), embed->progress);
  }

  embed->find_toolbar = ephy_find_toolbar_new (embed->web_view);
  g_signal_connect (embed->find_toolbar, "close",
                    G_CALLBACK (ephy_embed_find_toolbar_close_cb),
                    embed);

  gtk_box_pack_start (GTK_BOX (embed),
                      GTK_WIDGET (embed->find_toolbar),
                      FALSE, TRUE, 0);

  if (embed->progress_bar_enabled)
    embed->progress_update_handler_id = g_signal_connect (embed->web_view, "notify::estimated-load-progress",
                                                          G_CALLBACK (progress_update), object);

  gtk_box_pack_start (GTK_BOX (embed),
                      GTK_WIDGET (embed->top_widgets_vbox),
                      FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (embed), embed->overlay, FALSE, TRUE, 0);

  gtk_widget_show (GTK_WIDGET (embed->top_widgets_vbox));
  gtk_widget_show (GTK_WIDGET (embed->web_view));
  gtk_widget_show_all (embed->overlay);

  g_object_connect (embed->web_view,
                    "signal::notify::title", G_CALLBACK (web_view_title_changed_cb), embed,
                    "signal::load-changed", G_CALLBACK (load_changed_cb), embed,
                    "signal::enter-fullscreen", G_CALLBACK (entering_fullscreen_cb), embed,
                    "signal::leave-fullscreen", G_CALLBACK (leaving_fullscreen_cb), embed,
                    NULL);

  embed->status_handler_id = g_signal_connect (embed->web_view, "notify::status-message",
                                               G_CALLBACK (status_message_notify_cb),
                                               embed);

  /* The inspector */
  inspector = webkit_web_view_get_inspector (embed->web_view);

  g_signal_connect (inspector, "attach",
                    G_CALLBACK (ephy_embed_attach_inspector_cb),
                    embed);
  g_signal_connect (inspector, "closed",
                    G_CALLBACK (ephy_embed_close_inspector_cb),
                    embed);

  if (webkit_web_view_is_controlled_by_automation (embed->web_view)) {
    GtkWidget *info_bar;
    GtkWidget *label;

    info_bar = gtk_info_bar_new ();
    gtk_info_bar_set_message_type (GTK_INFO_BAR (info_bar), GTK_MESSAGE_INFO);
    /* Translators: this means WebDriver control. */
    label = gtk_label_new (_("Web is being controlled by automation."));
    gtk_box_pack_start (GTK_BOX (gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar))), label, FALSE, TRUE, 0);
    gtk_widget_show (label);

    ephy_embed_add_top_widget (embed, info_bar, EPHY_EMBED_TOP_WIDGET_POLICY_RETAIN_ON_TRANSITION);
    gtk_widget_show (info_bar);
  }
}

static void
ephy_embed_init (EphyEmbed *embed)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (embed),
                                  GTK_ORIENTATION_VERTICAL);

  embed->top_widgets_vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  embed->seq_context_id = 1;
  embed->seq_message_id = 1;
  embed->tab_message_id = ephy_embed_statusbar_get_context_id (embed, EPHY_EMBED_STATUSBAR_TAB_MESSAGE_CONTEXT_DESCRIPTION);
  embed->inspector_loaded = FALSE;
}

/**
 * ephy_embed_get_web_view:
 * @embed: and #EphyEmbed
 *
 * Returns the #EphyWebView wrapped by @embed.
 *
 * Returns: (transfer none): an #EphyWebView
 **/
EphyWebView *
ephy_embed_get_web_view (EphyEmbed *embed)
{
  g_assert (EPHY_IS_EMBED (embed));

  return EPHY_WEB_VIEW (embed->web_view);
}

/**
 * ephy_embed_get_find_toolbar:
 * @embed: and #EphyEmbed
 *
 * Returns the #EphyFindToolbar wrapped by @embed.
 *
 * Returns: (transfer none): an #EphyFindToolbar
 **/
EphyFindToolbar *
ephy_embed_get_find_toolbar (EphyEmbed *embed)
{
  g_assert (EPHY_IS_EMBED (embed));

  return EPHY_FIND_TOOLBAR (embed->find_toolbar);
}


/**
 * ephy_embed_add_top_widget:
 * @embed: an #EphyEmbed
 * @widget: a #GtkWidget
 * @policy: whether the widget be automatically
 * destroyed on page transitions
 *
 * Adds a #GtkWidget to the top of the embed.
 */
void
ephy_embed_add_top_widget (EphyEmbed                *embed,
                           GtkWidget                *widget,
                           EphyEmbedTopWidgetPolicy  policy)
{
  GSList *list;

  if (policy == EPHY_EMBED_TOP_WIDGET_POLICY_DESTROY_ON_TRANSITION) {
    list = embed->destroy_on_transition_list;
    list = g_slist_prepend (list, widget);
    embed->destroy_on_transition_list = list;

    g_signal_connect (widget, "destroy", G_CALLBACK (remove_from_destroy_list_cb), embed);
  }

  gtk_box_pack_end (embed->top_widgets_vbox,
                    GTK_WIDGET (widget), FALSE, TRUE, 0);
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
ephy_embed_remove_top_widget (EphyEmbed *embed,
                              GtkWidget *widget)
{
  if (g_slist_find (embed->destroy_on_transition_list, widget)) {
    GSList *list;
    g_signal_handlers_disconnect_by_func (widget, remove_from_destroy_list_cb, embed);

    list = embed->destroy_on_transition_list;
    list = g_slist_remove (list, widget);
    embed->destroy_on_transition_list = list;
  }

  gtk_container_remove (GTK_CONTAINER (embed->top_widgets_vbox),
                        GTK_WIDGET (widget));
}

/**
 * ephy_embed_set_delayed_load_request:
 * @embed: a #EphyEmbed
 * @request: a #WebKitNetworkRequest
 * @state: (nullable): a #WebKitWebViewSessionState
 *
 * Sets the #WebKitNetworkRequest that should be loaded when the tab this embed
 * is on is switched to.
 */
void
ephy_embed_set_delayed_load_request (EphyEmbed                 *embed,
                                     WebKitURIRequest          *request,
                                     WebKitWebViewSessionState *state)
{
  g_assert (EPHY_IS_EMBED (embed));
  g_assert (WEBKIT_IS_URI_REQUEST (request));

  g_clear_pointer (&embed->delayed_state, webkit_web_view_session_state_unref);
  g_clear_object (&embed->delayed_request);

  embed->delayed_request = g_object_ref (request);
  if (state)
    embed->delayed_state = webkit_web_view_session_state_ref (state);
}

/**
 * ephy_embed_has_load_pending:
 * @embed: a #EphyEmbed
 *
 * Checks whether a load has been delayed for this #EphyEmbed.
 *
 * Returns: %TRUE or %FALSE
 */
gboolean
ephy_embed_has_load_pending (EphyEmbed *embed)
{
  g_assert (EPHY_IS_EMBED (embed));

  return !!embed->delayed_request;
}

const char *
ephy_embed_get_title (EphyEmbed *embed)
{
  g_assert (EPHY_IS_EMBED (embed));

  return embed->title;
}


/**
 * ephy_embed_inspector_is_loaded:
 * @embed: a #EphyEmbed
 *
 * Checks if the Web Inspector is loaded in this #EphyEmbed.
 *
 * Returns: %TRUE or %FALSE
 */
gboolean
ephy_embed_inspector_is_loaded (EphyEmbed *embed)
{
  g_assert (EPHY_IS_EMBED (embed));

  return embed->inspector_loaded;
}

void
ephy_embed_attach_notification_container (EphyEmbed *embed)
{
  EphyNotificationContainer *container;

  g_assert (EPHY_IS_EMBED (embed));

  container = ephy_notification_container_get_default ();
  if (gtk_widget_get_parent (GTK_WIDGET (container)) == NULL)
    gtk_overlay_add_overlay (GTK_OVERLAY (embed->overlay), GTK_WIDGET (container));
}

void
ephy_embed_detach_notification_container (EphyEmbed *embed)
{
  EphyNotificationContainer *container;

  g_assert (EPHY_IS_EMBED (embed));

  container = ephy_notification_container_get_default ();
  if (gtk_widget_get_parent (GTK_WIDGET (container)) == embed->overlay) {
    /* Since the overlay container will own the one and only reference to the
     * notification widget, removing it from the container will destroy the
     * singleton. To prevent this, add a reference to it before removing it
     * from the container. */
    gtk_container_remove (GTK_CONTAINER (embed->overlay), g_object_ref (GTK_WIDGET (container)));
  }
}
