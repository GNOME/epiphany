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

#include "ephy-debug.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-find-toolbar.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-web-view.h"
#include "nautilus-floating-bar.h"

#include <glib/gi18n.h>
#include <webkit2/webkit2.h>

static void     ephy_embed_constructed         (GObject *object);
static void     ephy_embed_restored_window_cb  (EphyEmbedShell *shell,
                                                EphyEmbed *embed);

#define EPHY_EMBED_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED, EphyEmbedPrivate))

#define EPHY_EMBED_STATUSBAR_TAB_MESSAGE_CONTEXT_DESCRIPTION "tab_message"

typedef struct {
  gchar *text;
  guint context_id;
  guint message_id;
} EphyEmbedStatusbarMsg;

struct _EphyEmbedPrivate
{
  EphyFindToolbar *find_toolbar;
  GtkBox *top_widgets_vbox;
  GtkPaned *paned;
  WebKitWebView *web_view;
  GSList *destroy_on_transition_list;
  GtkWidget *floating_bar;
  GtkWidget *progress;
  GtkWidget *fullscreen_message_label;
  char *fullscreen_string;

  WebKitURIRequest *delayed_request;

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
};

enum
{
  PROP_0,
  PROP_WEB_VIEW,
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

  nautilus_floating_bar_set_primary_label (NAUTILUS_FLOATING_BAR (priv->floating_bar), label);

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
load_changed_cb (WebKitWebView *web_view,
                 WebKitLoadEvent load_event,
                 EphyEmbed *embed)
{
  switch (load_event) {
  case WEBKIT_LOAD_STARTED: {
    const char *uri;

    uri = webkit_web_view_get_uri (web_view);
    ephy_embed_set_overview_mode (embed, g_strcmp0 (uri, "ephy-about:overview") == 0);
    break;
  }
  case WEBKIT_LOAD_COMMITTED:
    ephy_embed_destroy_top_widgets (embed);
    break;
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
  if (!g_settings_get_boolean (EPHY_SETTINGS_LOCKDOWN, EPHY_PREFS_LOCKDOWN_FULLSCREEN)) {
    gtk_widget_show (embed->priv->fullscreen_message_label);

    if (embed->priv->fullscreen_message_id)
      g_source_remove (embed->priv->fullscreen_message_id);

    embed->priv->fullscreen_message_id = g_timeout_add_seconds (5,
                                                                (GSourceFunc)fullscreen_message_label_hide,
                                                                embed);
    g_source_set_name_by_id (embed->priv->fullscreen_message_id, "[epiphany] fullscreen_message_label_hide");
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
  EphyEmbedPrivate *priv = embed->priv;

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

  g_clear_object (&priv->delayed_request);

  G_OBJECT_CLASS (ephy_embed_parent_class)->dispose (object);
}

static void
ephy_embed_finalize (GObject *object)
{
  EphyEmbed *embed = EPHY_EMBED (object);
  EphyEmbedPrivate *priv = embed->priv;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  GSList *list;

  g_signal_handlers_disconnect_by_func(shell, ephy_embed_restored_window_cb, embed);

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
  case PROP_WEB_VIEW:
    embed->priv->web_view = g_value_get_object (value);
    break;
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
  case PROP_WEB_VIEW:
    g_value_set_object (value, ephy_embed_get_web_view (embed));
    break;
  case PROP_OVERVIEW_MODE:
    g_value_set_boolean (value, ephy_embed_get_overview_mode (embed));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
ephy_embed_find_toolbar_close_cb (EphyFindToolbar *toolbar,
                                  EphyEmbed *embed)
{
  EphyEmbedPrivate *priv = embed->priv;

  ephy_find_toolbar_close (priv->find_toolbar);

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

  g_object_class_install_property (object_class,
                                   PROP_WEB_VIEW,
                                   g_param_spec_object ("web-view",
                                                        "Web View",
                                                        "The WebView contained in the embed",
                                                        EPHY_TYPE_WEB_VIEW,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
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
                        EphyEmbed *embed)
{
  ephy_embed_set_fullscreen_message (embed, TRUE);
  return FALSE;
}

static gboolean
leaving_fullscreen_cb (WebKitWebView *web_view,
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
      g_source_set_name_by_id (priv->pop_statusbar_later_source_id, "[epiphany] pop_statusbar_later_cb");
    }
  }
}

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

  progress = webkit_web_view_get_estimated_load_progress (priv->web_view);
  loading = ephy_web_view_is_loading (EPHY_WEB_VIEW (priv->web_view));

  if (progress == 1.0 || !loading) {
    priv->clear_progress_source_id = g_timeout_add (500,
                                                    (GSourceFunc)clear_progress_cb,
                                                    embed);
    g_source_set_name_by_id (priv->clear_progress_source_id, "[epiphany] clear_progress_cb");
  } else
    gtk_widget_show (priv->progress);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->progress),
                                 (loading || progress == 1.0) ? progress : 0.0);
}

static void
ephy_embed_maybe_load_delayed_request (EphyEmbed *embed)
{
  EphyEmbedPrivate *priv = embed->priv;
  EphyWebView *web_view;

  if (!priv->delayed_request)
    return;

  web_view = ephy_embed_get_web_view (embed);

  ephy_web_view_load_request (web_view, priv->delayed_request);
  g_clear_object (&priv->delayed_request);

  /* This is to allow UI elements watching load status to show that the page is
   * loading as soon as possible.
   */
  g_signal_emit_by_name (web_view, "load-changed", WEBKIT_LOAD_STARTED);
}

static void
ephy_embed_restored_window_cb (EphyEmbedShell *shell, EphyEmbed *embed)
{
  if (!gtk_widget_get_mapped (GTK_WIDGET (embed)))
    return;

  ephy_embed_maybe_load_delayed_request (embed);
}

static void
ephy_embed_mapped_cb (GtkWidget *widget, gpointer data)
{
  ephy_embed_maybe_load_delayed_request ((EphyEmbed*)widget);
}

static void
ephy_embed_constructed (GObject *object)
{
  EphyEmbed *embed = (EphyEmbed*)object;
  EphyEmbedPrivate *priv = embed->priv;
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();
  GtkWidget *paned;
  WebKitWebInspector *inspector;
  GtkWidget *overlay;

  g_signal_connect (shell, "window-restored",
                    G_CALLBACK (ephy_embed_restored_window_cb), embed);

  g_signal_connect (embed, "map",
                    G_CALLBACK (ephy_embed_mapped_cb), NULL);

  /* Skeleton */
  overlay = gtk_overlay_new ();

  gtk_widget_add_events (overlay, 
                         GDK_ENTER_NOTIFY_MASK |
                         GDK_LEAVE_NOTIFY_MASK);
  gtk_container_add (GTK_CONTAINER (overlay), GTK_WIDGET (priv->web_view));

  /* The overview. In incognito mode we don't use it. */
  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ()) !=
      EPHY_EMBED_SHELL_MODE_INCOGNITO) {
    priv->overview = ephy_overview_new ();
    gtk_widget_set_halign (priv->overview, GTK_ALIGN_FILL);
    gtk_widget_set_valign (priv->overview, GTK_ALIGN_FILL);
    gtk_overlay_add_overlay (GTK_OVERLAY (overlay), priv->overview);

    g_object_bind_property (embed, "overview-mode",
                            priv->overview, "visible",
                            G_BINDING_SYNC_CREATE);
  }

  /* Floating message popup for fullscreen mode. */
  priv->fullscreen_message_label = gtk_label_new (NULL);
  gtk_widget_set_name (priv->fullscreen_message_label, "fullscreen-popup");
  gtk_widget_set_halign (priv->fullscreen_message_label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (priv->fullscreen_message_label, GTK_ALIGN_CENTER);
  gtk_widget_set_no_show_all (priv->fullscreen_message_label, TRUE);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), priv->fullscreen_message_label);
  ephy_embed_set_fullscreen_message (embed, FALSE);

  /* statusbar is hidden by default */
  priv->floating_bar = nautilus_floating_bar_new (NULL, NULL, FALSE);
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

  priv->find_toolbar = ephy_find_toolbar_new (priv->web_view);
  g_signal_connect (priv->find_toolbar, "close",
                    G_CALLBACK (ephy_embed_find_toolbar_close_cb),
                    embed);

  gtk_box_pack_start (GTK_BOX (embed),
                      GTK_WIDGET (priv->find_toolbar),
                      FALSE, FALSE, 0);

  paned = GTK_WIDGET (priv->paned);

  priv->progress_update_handler_id = g_signal_connect (priv->web_view, "notify::estimated-load-progress",
                                                       G_CALLBACK (progress_update), object);
  gtk_paned_pack1 (GTK_PANED (paned), GTK_WIDGET (overlay),
                   TRUE, FALSE);

  gtk_box_pack_start (GTK_BOX (embed),
                      GTK_WIDGET (priv->top_widgets_vbox),
                      FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (embed), paned, TRUE, TRUE, 0);

  gtk_widget_show (GTK_WIDGET (priv->top_widgets_vbox));
  gtk_widget_show (GTK_WIDGET (priv->web_view));
  gtk_widget_show_all (paned);

  g_object_connect (priv->web_view,
                    "signal::load-changed", G_CALLBACK (load_changed_cb), embed,
                    "signal::enter-fullscreen", G_CALLBACK (entering_fullscreen_cb), embed,
                    "signal::leave-fullscreen", G_CALLBACK (leaving_fullscreen_cb), embed,
                    NULL);

  priv->status_handler_id = g_signal_connect (priv->web_view, "notify::status-message",
                                              G_CALLBACK (status_message_notify_cb),
                                              embed);

  /* The inspector */
  inspector = webkit_web_view_get_inspector (priv->web_view);

  g_signal_connect (inspector, "attach",
                    G_CALLBACK (ephy_embed_attach_inspector_cb),
                    embed);
}

static void
ephy_embed_init (EphyEmbed *embed)
{
  EphyEmbedPrivate *priv;

  priv = embed->priv = EPHY_EMBED_GET_PRIVATE (embed);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (embed),
                                  GTK_ORIENTATION_VERTICAL);

  priv->paned = GTK_PANED (gtk_paned_new (GTK_ORIENTATION_VERTICAL));
  priv->top_widgets_vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  priv->seq_context_id = 1;
  priv->seq_message_id = 1;
  priv->tab_message_id = ephy_embed_statusbar_get_context_id (embed, EPHY_EMBED_STATUSBAR_TAB_MESSAGE_CONTEXT_DESCRIPTION);
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
 * ephy_embed_get_find_toolbar:
 * @embed: and #EphyEmbed
 * 
 * Returns the #EphyFindToolbar wrapped by @embed.
 * 
 * Returns: (transfer none): an #EphyFindToolbar
 **/
EphyFindToolbar*
ephy_embed_get_find_toolbar (EphyEmbed *embed)
{
  g_return_val_if_fail (EPHY_IS_EMBED (embed), NULL);

  return EPHY_FIND_TOOLBAR (embed->priv->find_toolbar);
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
                    GTK_WIDGET (widget), FALSE, FALSE, 0);
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
 * ephy_embed_set_delayed_load_request:
 * @embed: a #EphyEmbed
 * @request: a #WebKitNetworkRequest
 *
 * Sets the #WebKitNetworkRequest that should be loaded when the tab this embed
 * is on is switched to.
 */
void
ephy_embed_set_delayed_load_request (EphyEmbed *embed, WebKitURIRequest *request)
{
  g_return_if_fail (EPHY_IS_EMBED (embed));
  g_return_if_fail (WEBKIT_IS_URI_REQUEST (request));

  g_clear_object (&embed->priv->delayed_request);

  g_object_ref (request);
  embed->priv->delayed_request = request;
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
  g_return_val_if_fail (EPHY_IS_EMBED (embed), FALSE);

  return !!embed->priv->delayed_request;
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
