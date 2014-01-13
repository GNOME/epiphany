/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2008, 2009 Gustavo Noronha Silva
 *  Copyright © 2009, 2010 Igalia S.L.
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
#include "ephy-web-view.h"

#include "ephy-about-handler.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-private.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-embed-utils.h"
#include "ephy-embed.h"
#include "ephy-favicon-helpers.h"
#include "ephy-file-helpers.h"
#include "ephy-file-monitor.h"
#include "ephy-form-auth-data.h"
#include "ephy-history-service.h"
#include "ephy-overview.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-string.h"
#include "ephy-web-app-utils.h"
#include "ephy-web-dom-utils.h"
#include "ephy-zoom.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>

/**
 * SECTION:ephy-web-view
 * @short_description: Epiphany custom #WebkitWebView
 *
 * #EphyWebView wraps #WebkitWebView implementing custom functionality on top of
 * it.
 */

#define MAX_HIDDEN_POPUPS       5
#define MAX_TITLE_LENGTH        512 /* characters */
#define EMPTY_PAGE              _("Blank page") /* Title for the empty page */

#define EPHY_PAGE_TEMPLATE_ERROR         "/org/gnome/epiphany/page-templates/error.html"
#define EPHY_PAGE_TEMPLATE_RECOVERY      "/org/gnome/epiphany/page-templates/recovery.html"
#define EPHY_PAGE_TEMPLATE_PROCESS_CRASH "/org/gnome/epiphany/page-templates/process-crash.html"

struct _EphyWebViewPrivate {
  EphyWebViewSecurityLevel security_level;
  EphyWebViewDocumentType document_type;
  EphyWebViewNavigationFlags nav_flags;

  /* Flags */
  guint is_blank : 1;
  guint is_setting_zoom : 1;
  guint load_failed : 1;
  guint history_frozen : 1;
  guint ever_committed : 1;

  char *address;
  char *typed_address;
  char *title;
  char *loading_title;
  char *status_message;
  char *link_message;
  GdkPixbuf *icon;

  /* Local file watch. */
  EphyFileMonitor *file_monitor;

  /* Regex to figure out if we're dealing with a wanna-be URI */
  GRegex *non_search_regex;
  GRegex *domain_regex;

  GSList *hidden_popups;
  GSList *shown_popups;

  GtkWidget *password_info_bar;

  EphyHistoryService *history_service;
  GCancellable *history_service_cancellable;

  guint snapshot_idle_id;

  EphyHistoryPageVisitType visit_type;

  gulong do_not_track_handler;

  /* TLS information. */
  GTlsCertificate *certificate;
  GTlsCertificateFlags tls_errors;
};

typedef struct {
  char *url;
  char *name;
  char *features;
} PopupInfo;

enum {
  PROP_0,
  PROP_ADDRESS,
  PROP_DOCUMENT_TYPE,
  PROP_HIDDEN_POPUP_COUNT,
  PROP_ICON,
  PROP_LINK_MESSAGE,
  PROP_NAVIGATION,
  PROP_POPUPS_ALLOWED,
  PROP_SECURITY,
  PROP_STATUS_MESSAGE,
  PROP_EMBED_TITLE,
  PROP_TYPED_ADDRESS,
  PROP_IS_BLANK,
};

#define EPHY_WEB_VIEW_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_WEB_VIEW, EphyWebViewPrivate))

G_DEFINE_TYPE (EphyWebView, ephy_web_view, WEBKIT_TYPE_WEB_VIEW)

static guint
popup_blocker_n_hidden (EphyWebView *view)
{
  return g_slist_length (view->priv->hidden_popups);
}

static void
popups_manager_free_info (PopupInfo *popup)
{
  g_free (popup->url);
  g_free (popup->name);
  g_free (popup->features);
  g_slice_free (PopupInfo, popup);
}

static void
popups_manager_show (PopupInfo *popup,
                     EphyWebView *view)
{
  /* Only show popup with non NULL url */
  if (popup->url != NULL) {
    /* FIXME: we need a way of opening windows in here. This used to
     * be implemented in EphyEmbedSingle open_window method, but it's
     * been a no-op for a while. */
  }
  popups_manager_free_info (popup);
}

static void
popups_manager_show_all (EphyWebView *view)
{
  LOG ("popup_blocker_show_all: view %p", view);

  g_slist_foreach (view->priv->hidden_popups,
                   (GFunc)popups_manager_show, view);
  g_slist_free (view->priv->hidden_popups);
  view->priv->hidden_popups = NULL;

  g_object_notify (G_OBJECT (view), "hidden-popup-count");
}

static char *
popups_manager_new_window_info (EphyEmbedContainer *container)
{
  EphyEmbed *embed;
  EphyWebViewChrome chrome;
  GtkAllocation allocation;
  gboolean is_popup;
  char *features;

  g_object_get (container, "chrome", &chrome, "is-popup", &is_popup, NULL);
  g_return_val_if_fail (is_popup, g_strdup (""));

  embed = ephy_embed_container_get_active_child (container);
  g_return_val_if_fail (embed != NULL, g_strdup (""));

  gtk_widget_get_allocation (GTK_WIDGET (embed), &allocation);

  features = g_strdup_printf
             ("width=%d,height=%d,toolbar=%d",
              allocation.width,
              allocation.height,
              (chrome & EPHY_WEB_VIEW_CHROME_TOOLBAR) > 0);

  return features;
}

static void
popups_manager_add (EphyWebView *view,
                    const char *url,
                    const char *name,
                    const char *features)
{
  EphyWebViewPrivate *priv = view->priv;
  PopupInfo *popup;

  LOG ("popups_manager_add: view %p, url %s, features %s",
       view, url, features);

  popup = g_slice_new (PopupInfo);

  popup->url = g_strdup (url);
  popup->name = g_strdup (name);
  popup->features = g_strdup (features);

  priv->hidden_popups = g_slist_prepend (priv->hidden_popups, popup);

  if (popup_blocker_n_hidden (view) > MAX_HIDDEN_POPUPS) {/* bug #160863 */
    /* Remove the oldest popup */
    GSList *l = view->priv->hidden_popups;

    while (l->next->next != NULL) {
      l = l->next;
    }

    popup = (PopupInfo *)l->next->data;
    popups_manager_free_info (popup);

    l->next = NULL;
  } else {
    g_object_notify (G_OBJECT (view), "hidden-popup-count");
  }
}

static void
popups_manager_hide (EphyEmbedContainer *container,
                     EphyWebView *parent_view)
{
  EphyEmbed *embed;
  const char *location;
  char *features;

  embed = ephy_embed_container_get_active_child (container);
  g_return_if_fail (EPHY_IS_EMBED (embed));

  location = ephy_web_view_get_address (ephy_embed_get_web_view (embed));
  if (location == NULL) return;

  features = popups_manager_new_window_info (container);

  popups_manager_add (parent_view, location, "" /* FIXME? maybe _blank? */, features);

  gtk_widget_destroy (GTK_WIDGET (container));

  g_free (features);
}

static void
popups_manager_hide_all (EphyWebView *view)
{
  LOG ("popup_blocker_hide_all: view %p", view);

  g_slist_foreach (view->priv->shown_popups,
                   (GFunc)popups_manager_hide, view);
  g_slist_free (view->priv->shown_popups);
  view->priv->shown_popups = NULL;
}

static void
ephy_web_view_set_popups_allowed (EphyWebView *view,
                                  gboolean allowed)
{
  if (allowed) {
    popups_manager_show_all (view);
  } else {
    popups_manager_hide_all (view);
  }
}

static gboolean
ephy_web_view_get_popups_allowed (EphyWebView *view)
{
  const char *location;
  gboolean allow;

  location = ephy_web_view_get_address (view);
  if (location == NULL) return FALSE;/* FALSE, TRUE… same thing */

  allow = g_settings_get_boolean (EPHY_SETTINGS_WEB,
                                  EPHY_PREFS_WEB_ENABLE_POPUPS);
  return allow;
}

static gboolean
popups_manager_remove_window (EphyWebView *view,
                              EphyEmbedContainer *container)
{
  view->priv->shown_popups = g_slist_remove (view->priv->shown_popups,
                                              container);

  return FALSE;
}

static void
popups_manager_add_window (EphyWebView *view,
                           EphyEmbedContainer *container)
{
  LOG ("popups_manager_add_window: view %p, container %p", view, container);

  view->priv->shown_popups = g_slist_prepend (view->priv->shown_popups, container);

  g_signal_connect_swapped (container, "destroy",
                            G_CALLBACK (popups_manager_remove_window),
                            view);
}

static void
disconnect_popup (EphyEmbedContainer *container,
                  EphyWebView *view)
{
  g_signal_handlers_disconnect_by_func
  (container, G_CALLBACK (popups_manager_remove_window), view);
}

/**
 * ephy_web_view_popups_manager_reset:
 * @view: an #EphyWebView
 *
 * Resets the state of the popups manager in @view.
 **/
void
ephy_web_view_popups_manager_reset (EphyWebView *view)
{
  g_slist_foreach (view->priv->hidden_popups,
                   (GFunc)popups_manager_free_info, NULL);
  g_slist_free (view->priv->hidden_popups);
  view->priv->hidden_popups = NULL;

  g_slist_foreach (view->priv->shown_popups,
                   (GFunc)disconnect_popup, view);
  g_slist_free (view->priv->shown_popups);
  view->priv->shown_popups = NULL;

  g_object_notify (G_OBJECT (view), "hidden-popup-count");
  g_object_notify (G_OBJECT (view), "popups-allowed");
}

static void
ephy_web_view_get_property (GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  EphyWebViewPrivate *priv = EPHY_WEB_VIEW (object)->priv;

  switch (prop_id) {
    case PROP_ADDRESS:
      g_value_set_string (value, priv->address);
      break;
    case PROP_EMBED_TITLE:
      g_value_set_string (value, priv->title);
      break;
    case PROP_TYPED_ADDRESS:
      g_value_set_string (value, priv->typed_address);
      break;
    case PROP_DOCUMENT_TYPE:
      g_value_set_enum (value, priv->document_type);
      break;
    case PROP_HIDDEN_POPUP_COUNT:
      g_value_set_int (value, popup_blocker_n_hidden
                       (EPHY_WEB_VIEW (object)));
      break;
    case PROP_ICON:
      g_value_set_object (value, priv->icon);
      break;
    case PROP_LINK_MESSAGE:
      g_value_set_string (value, priv->link_message);
      break;
    case PROP_NAVIGATION:
      g_value_set_flags (value, priv->nav_flags);
      break;
    case PROP_POPUPS_ALLOWED:
      g_value_set_boolean (value, ephy_web_view_get_popups_allowed
                           (EPHY_WEB_VIEW (object)));
      break;
    case PROP_SECURITY:
      g_value_set_enum (value, priv->security_level);
      break;
    case PROP_STATUS_MESSAGE:
      g_value_set_string (value, priv->status_message);
      break;
    case PROP_IS_BLANK:
      g_value_set_boolean (value, priv->is_blank);
      break;
    default:
      break;
  }
}

static void
ephy_web_view_set_property (GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  switch (prop_id) {
    case PROP_POPUPS_ALLOWED:
      ephy_web_view_set_popups_allowed (EPHY_WEB_VIEW (object), g_value_get_boolean (value));
      break;
    case PROP_TYPED_ADDRESS:
      ephy_web_view_set_typed_address (EPHY_WEB_VIEW (object), g_value_get_string (value));
      break;
      break;
    case PROP_ADDRESS:
    case PROP_DOCUMENT_TYPE:
    case PROP_HIDDEN_POPUP_COUNT:
    case PROP_ICON:
    case PROP_LINK_MESSAGE:
    case PROP_NAVIGATION:
    case PROP_SECURITY:
    case PROP_STATUS_MESSAGE:
    case PROP_EMBED_TITLE:
    case PROP_IS_BLANK:
      /* read only */
      break;
    default:
      break;
  }
}

static gboolean
ephy_web_view_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
  EphyWebView *web_view = EPHY_WEB_VIEW (widget);
  gboolean key_handled = FALSE;

  key_handled = GTK_WIDGET_CLASS (ephy_web_view_parent_class)->key_press_event (widget, event);

  if (key_handled)
    return TRUE;

  g_signal_emit_by_name (web_view, "search-key-press", event, &key_handled);

  return key_handled;
}

static gboolean
ephy_web_view_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
  /* This are the special cases WebkitWebView doesn't handle but we have an
   * interest in handling. */

  /* We always show the browser context menu on control-rightclick. */
  if (event->button == 3 && event->state == GDK_CONTROL_MASK)
    return FALSE;

  /* Handle typical back/forward mouse buttons. */
  if (event->button == 8) {
    webkit_web_view_go_back (WEBKIT_WEB_VIEW (widget));
    return TRUE;
  }

  if (event->button == 9) {
    webkit_web_view_go_forward (WEBKIT_WEB_VIEW (widget));
    return TRUE;
  }

  /* Let WebKitWebView handle this. */
  return GTK_WIDGET_CLASS (ephy_web_view_parent_class)->button_press_event (widget, event);
}

static GtkWidget *
ephy_web_view_create_form_auth_save_confirmation_info_bar (EphyWebView *web_view,
                                                           const char *hostname,
                                                           const char *username)
{
  GtkWidget *info_bar;
  GtkWidget *action_area;
  GtkWidget *content_area;
  GtkWidget *label;
  char *message;

  LOG ("Going to show infobar about %s", webkit_web_view_get_uri (WEBKIT_WEB_VIEW (web_view)));

  info_bar = gtk_info_bar_new_with_buttons (_("Save"), GTK_RESPONSE_YES,
                                            NULL);
  gtk_info_bar_set_show_close_button (GTK_INFO_BAR (info_bar), TRUE);

  action_area = gtk_info_bar_get_action_area (GTK_INFO_BAR (info_bar));
  gtk_orientable_set_orientation (GTK_ORIENTABLE (action_area),
                                  GTK_ORIENTATION_HORIZONTAL);

  label = gtk_label_new (NULL);
  /* Translators: The %s the hostname where this is happening. 
   * Example: mail.google.com.
   */
  message = g_markup_printf_escaped (_("Do you want to save your password for “%s”?"),
                                     hostname);
  gtk_label_set_markup (GTK_LABEL (label), message);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  g_free (message);

  content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar));
  gtk_container_add (GTK_CONTAINER (content_area), label);
  gtk_widget_show (label);

  ephy_embed_add_top_widget (EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view),
                             info_bar, FALSE);

  /* We track the info_bar, so we only ever show one */
  if (web_view->priv->password_info_bar)
    gtk_widget_destroy (web_view->priv->password_info_bar);

  web_view->priv->password_info_bar = info_bar;
  g_object_add_weak_pointer (G_OBJECT (info_bar),
                             (gpointer *)&web_view->priv->password_info_bar);

  return info_bar;
}

static void
update_navigation_flags (EphyWebView *view)
{
  EphyWebViewPrivate *priv = view->priv;
  guint flags = 0;
  WebKitWebView *web_view = WEBKIT_WEB_VIEW (view);

  if (webkit_web_view_can_go_back (web_view))
    flags |= EPHY_WEB_VIEW_NAV_BACK;

  if (webkit_web_view_can_go_forward (web_view))
    flags |= EPHY_WEB_VIEW_NAV_FORWARD;

  if (priv->nav_flags != (EphyWebViewNavigationFlags)flags) {
    priv->nav_flags = (EphyWebViewNavigationFlags)flags;

    g_object_notify (G_OBJECT (view), "navigation");
  }
}

static void
ephy_web_view_freeze_history (EphyWebView *view)
{
  view->priv->history_frozen = TRUE;
}

static void
ephy_web_view_thaw_history (EphyWebView *view)
{
  view->priv->history_frozen = FALSE;
}

static gboolean
ephy_web_view_is_history_frozen (EphyWebView *view)
{
  return view->priv->history_frozen;
}


static void
ephy_web_view_clear_history (EphyWebView *view)
{
  /* TODO: WebKitBackForwardList is read-only in WebKit2 */
}

static void
ephy_web_view_history_cleared_cb (EphyHistoryService *history_service,
                                  EphyWebView *view)
{
  ephy_web_view_clear_history (view);
}

static void
_ephy_web_view_update_icon (EphyWebView *view)
{
  EphyWebViewPrivate *priv = view->priv;

  if (priv->icon != NULL) {
    g_object_unref (priv->icon);
    priv->icon = NULL;
  }

  if (priv->address) {
    cairo_surface_t *icon_surface = webkit_web_view_get_favicon (WEBKIT_WEB_VIEW (view));
    if (icon_surface)
      priv->icon = ephy_pixbuf_get_from_surface_scaled (icon_surface, FAVICON_SIZE, FAVICON_SIZE);
  }

  g_object_notify (G_OBJECT (view), "icon");
}

static void
icon_changed_cb (EphyWebView *view,
                 GParamSpec *pspec,
                 gpointer user_data)
{
  _ephy_web_view_update_icon (view);
}

static void
form_auth_data_save_confirmation_response (GtkInfoBar *info_bar,
                                           gint response_id,
                                           gpointer user_data)
{
  GDBusProxy *web_extension;
  guint request_id = GPOINTER_TO_INT (user_data);

  gtk_widget_destroy (GTK_WIDGET (info_bar));

  web_extension = ephy_embed_shell_get_web_extension_proxy (ephy_embed_shell_get_default ());
  if (!web_extension)
    return;

  g_dbus_proxy_call (web_extension,
                     "FormAuthDataSaveConfirmationResponse",
                     g_variant_new ("(ub)", request_id, response_id == GTK_RESPONSE_YES),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, NULL, NULL);
}

static void
form_auth_data_save_requested (EphyEmbedShell *shell,
                               guint request_id,
                               guint64 page_id,
                               const char *hostname,
                               const char *username,
                               WebKitWebView *web_view)
{
  GtkWidget *info_bar;

  if (webkit_web_view_get_page_id (web_view) != page_id)
    return;

  info_bar = ephy_web_view_create_form_auth_save_confirmation_info_bar (EPHY_WEB_VIEW (web_view),
                                                                        hostname, username);
  g_signal_connect (info_bar, "response",
                    G_CALLBACK (form_auth_data_save_confirmation_response),
                    GINT_TO_POINTER (request_id));

  gtk_widget_show (info_bar);
}

static void
ephy_web_view_dispose (GObject *object)
{
  EphyWebViewPrivate *priv = EPHY_WEB_VIEW (object)->priv;

  g_signal_handlers_disconnect_by_func (priv->history_service,
                                        ephy_web_view_history_cleared_cb,
                                        EPHY_WEB_VIEW (object));

  g_signal_handlers_disconnect_by_func (object, icon_changed_cb, NULL);

  g_signal_handlers_disconnect_by_func (ephy_embed_shell_get_default (), form_auth_data_save_requested, object);

  g_clear_object (&priv->file_monitor);

  g_clear_object (&priv->icon);

  if (priv->history_service_cancellable) {
    g_cancellable_cancel (priv->history_service_cancellable);
    g_clear_object (&priv->history_service_cancellable);
  }

  if (priv->snapshot_idle_id) {
    g_source_remove (priv->snapshot_idle_id);
    priv->snapshot_idle_id = 0;
  }

  g_clear_object(&priv->certificate);

  G_OBJECT_CLASS (ephy_web_view_parent_class)->dispose (object);
}

static void
ephy_web_view_finalize (GObject *object)
{
  EphyWebViewPrivate *priv = EPHY_WEB_VIEW (object)->priv;

  if (priv->non_search_regex) {
    g_regex_unref (priv->non_search_regex);
    priv->non_search_regex = NULL;
  }

  if (priv->domain_regex) {
    g_regex_unref (priv->domain_regex);
    priv->domain_regex = NULL;
  }

  ephy_web_view_popups_manager_reset (EPHY_WEB_VIEW (object));

  g_free (priv->address);
  g_free (priv->typed_address);
  g_free (priv->title);
  g_free (priv->status_message);
  g_free (priv->link_message);
  g_free (priv->loading_title);

  G_OBJECT_CLASS (ephy_web_view_parent_class)->finalize (object);
}

static char*
get_title_from_address (const char *address)
{
  if (g_str_has_prefix (address, "file://"))
    return g_strdup (address + 7);
  else if (!strcmp (address, EPHY_ABOUT_SCHEME":overview") ||
           !strcmp (address, "about:overview"))
    return g_strdup (EPHY_OVERVIEW_TITLE);
  else
    return ephy_string_get_host_name (address);
}

static void
_ephy_web_view_set_is_blank (EphyWebView *view,
                             gboolean is_blank)
{
  EphyWebViewPrivate *priv = view->priv;

  if (priv->is_blank != is_blank) {
    priv->is_blank = is_blank;
    g_object_notify (G_OBJECT (view), "is-blank");
  }
}

static void
ephy_web_view_set_title (EphyWebView *view,
                         const char *view_title)
{
  EphyWebViewPrivate *priv = view->priv;
  char *title = g_strdup (view_title);

  if (title == NULL || g_strstrip (title)[0] == '\0') {
    g_free (title);
    title = priv->address ? get_title_from_address (priv->address) : NULL;

    if (title == NULL || title[0] == '\0') {
      g_free (title);
      title = g_strdup (EMPTY_PAGE);
    }
  }

  g_free (priv->title);
  priv->title = ephy_string_shorten (title, MAX_TITLE_LENGTH);

  g_object_notify (G_OBJECT (view), "embed-title");
}

static void
title_changed_cb (WebKitWebView *web_view,
                  GParamSpec *spec,
                  gpointer data)
{
  const char *uri;
  char *title;
  EphyWebView *webview  = EPHY_WEB_VIEW (web_view);
  EphyHistoryService *history = webview->priv->history_service;

  uri = webkit_web_view_get_uri (web_view);

  g_object_get (web_view, "title", &title, NULL);

  ephy_web_view_set_title (webview, title);
  
  if (!title && uri)
    title = get_title_from_address (uri);

  if (uri && title && !ephy_web_view_is_history_frozen (webview))
    ephy_history_service_set_url_title (history, uri, title, NULL, NULL, NULL);

  g_free (title);

}

/*
 * Sets the view location to be address. Note that this function might
 * also set the typed-address property to NULL.
 */
static void
ephy_web_view_set_address (EphyWebView *view,
                           const char *address)
{
  EphyWebViewPrivate *priv = view->priv;
  GObject *object = G_OBJECT (view);
  gboolean is_blank;

  g_free (priv->address);
  priv->address = g_strdup (address);

  is_blank = address == NULL ||
             strcmp (address, "about:blank") == 0;
  _ephy_web_view_set_is_blank (view, is_blank);

  if (ephy_web_view_is_loading (view) && priv->typed_address != NULL)
    ephy_web_view_set_typed_address (view, NULL);

  g_object_notify (object, "address");
}

static void
uri_changed_cb (WebKitWebView *web_view,
                GParamSpec *spec,
                gpointer data)
{
  char *uri;
  const char *current_address;

  /* We already update the URI manually while loading, so only
   * update the URI when it changes after the page has been loaded
   * which is usually the result of navigation within the same page action.
   */
  if (webkit_web_view_is_loading (web_view))
    return;

  /* We need to check if we get URI notifications without going
     through the usual load process, as this can happen when changing
     location within a page */
  current_address = ephy_web_view_get_address (EPHY_WEB_VIEW (web_view));
  g_object_get (web_view, "uri", &uri, NULL);

  if (g_str_equal (uri, current_address) == FALSE)
    ephy_web_view_set_address (EPHY_WEB_VIEW (web_view), uri);

  g_free (uri);
}

static void
mouse_target_changed_cb (EphyWebView *web_view,
                         WebKitHitTestResult *hit_test_result,
                         guint modifiers,
                         gpointer data)
{
  const char *message = NULL;

  if (webkit_hit_test_result_context_is_link (hit_test_result))
    message = webkit_hit_test_result_get_link_uri (hit_test_result);

  ephy_web_view_set_link_message (web_view, message);
}

static void
process_crashed_cb (EphyWebView *web_view, gpointer user_data)
{
  if (ephy_embed_has_load_pending (EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view)))
    return;

  ephy_web_view_load_error_page (web_view, ephy_web_view_get_address (web_view),
                                 EPHY_WEB_VIEW_ERROR_PROCESS_CRASH, NULL);
}

static void
ephy_web_view_constructed (GObject *object)
{
  if (G_OBJECT_CLASS (ephy_web_view_parent_class)->constructed)
    G_OBJECT_CLASS (ephy_web_view_parent_class)->constructed (object);

  /* Use full content zooming by default */
  /* FIXME: we could make this configurable through GSettings, or have
   * different keys for text and full content zooming. AFAIK you can
   * have both enabled at the same time in WebKit now (although our
   * API does not reflect this atm). See r67274 in WebKit. */
  g_signal_emit_by_name (ephy_embed_shell_get_default (), "web-view-created", object);

  g_signal_connect (object, "web-process-crashed",
                    G_CALLBACK (process_crashed_cb), object);
}

static void
impl_loading_homepage (EphyWebView *view)
{
  ephy_web_view_freeze_history (view);
}

static void
ephy_web_view_class_init (EphyWebViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->dispose = ephy_web_view_dispose;
  gobject_class->finalize = ephy_web_view_finalize;
  gobject_class->get_property = ephy_web_view_get_property;
  gobject_class->set_property = ephy_web_view_set_property;
  gobject_class->constructed = ephy_web_view_constructed;

  widget_class->button_press_event = ephy_web_view_button_press_event;
  widget_class->key_press_event = ephy_web_view_key_press_event;

  klass->loading_homepage = impl_loading_homepage;

/**
 * EphyWebView:address:
 *
 * View's current address.
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_ADDRESS,
                                   g_param_spec_string ("address",
                                                        "Address",
                                                        "The view's address",
                                                        "",
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

/**
 * EphyWebView:typed-address:
 *
 * User typed address for the current view.
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_TYPED_ADDRESS,
                                   g_param_spec_string ("typed-address",
                                                        "Typed Address",
                                                        "The typed address",
                                                        "",
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

/**
 * EphyWebView:embed-title:
 *
 * Title for this embed.
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_EMBED_TITLE,
                                   g_param_spec_string ("embed-title",
                                                        "Title",
                                                        "The view's title",
                                                        EMPTY_PAGE,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

/**
 * EphyWebView:security-level:
 *
 * One of #EphyWebViewSecurityLevel, determining view's current security level.
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_SECURITY,
                                   g_param_spec_enum ("security-level",
                                                      "Security Level",
                                                      "The view's security level",
                                                      EPHY_TYPE_WEB_VIEW_SECURITY_LEVEL,
                                                      EPHY_WEB_VIEW_STATE_IS_UNKNOWN,
                                                      G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

/**
 * EphyWebView:document-type:
 *
 * Document type determined for the view.
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_DOCUMENT_TYPE,
                                   g_param_spec_enum ("document-type",
                                                      "Document Type",
                                                      "The view's document type",
                                                      EPHY_TYPE_WEB_VIEW_DOCUMENT_TYPE,
                                                      EPHY_WEB_VIEW_DOCUMENT_HTML,
                                                      G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

/**
 * EphyWebView:navigation:
 *
 * View's navigation flags as #EphyWebViewNavigationFlags.
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_NAVIGATION,
                                   g_param_spec_flags ("navigation",
                                                       "Navigation flags",
                                                       "The view's navigation flags",
                                                       EPHY_TYPE_WEB_VIEW_NAVIGATION_FLAGS,
                                                       0,
                                                       G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

/**
 * EphyWebView:status-message:
 *
 * Statusbar message corresponding to this view.
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_STATUS_MESSAGE,
                                   g_param_spec_string ("status-message",
                                                        "Status Message",
                                                        "The view's statusbar message",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

/**
 * EphyWebView:link-message:
 *
 * ???
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_LINK_MESSAGE,
                                   g_param_spec_string ("link-message",
                                                        "Link Message",
                                                        "The view's link message",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

/**
 * EphyWebView:icon:
 *
 * View's favicon set by the loaded site.
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON,
                                   g_param_spec_object ("icon",
                                                        "Icon",
                                                        "The view icon's",
                                                        GDK_TYPE_PIXBUF,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

/**
 * EphyWebView:hidden-popup-count:
 *
 * Number of hidden (blocked) popup windows.
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_HIDDEN_POPUP_COUNT,
                                   g_param_spec_int ("hidden-popup-count",
                                                     "Number of Blocked Popups",
                                                     "The view's number of blocked popup windows",
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

/**
 * EphyWebView:popups-allowed:
 *
 * If popup windows from this view are to be displayed.
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_POPUPS_ALLOWED,
                                   g_param_spec_boolean ("popups-allowed",
                                                         "Popups Allowed",
                                                         "Whether popup windows are to be displayed",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

/**
 * EphyWebView:is-blank:
 *
 * Whether the view is showing the blank address.
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_IS_BLANK,
                                   g_param_spec_boolean ("is-blank",
                                                         "Is blank",
                                                         "If the EphyWebView is blank",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

/**
 * EphyWebView::new-window:
 * @view: the #EphyWebView that received the signal
 * @new_view: the newly opened #EphyWebView
 *
 * The ::new-window signal is emitted after a new window has been opened by
 * the view. For example, when a JavaScript popup window is opened.
 **/
    g_signal_new ("new-window",
            EPHY_TYPE_WEB_VIEW,
            G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (EphyWebViewClass, new_window),
            NULL, NULL,
            g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE,
            1,
            GTK_TYPE_WIDGET);
/**
 * EphyWebView::ge-popup-blocked:
 * @view: the #EphyWebView that received the signal
 * @address: The requested URL
 * @target: The requested window name, e.g. "_blank"
 * @features: The requested features: for example, "height=400,width=200"
 *
 * The ::ge_popup_blocked signal is emitted when the viewed web page requests
 * a popup window (with javascript:open()) but popup windows are not allowed.
 **/
    g_signal_new ("ge_popup_blocked",
            EPHY_TYPE_WEB_VIEW,
            G_SIGNAL_RUN_FIRST,
            G_STRUCT_OFFSET (EphyWebViewClass, popup_blocked),
            NULL, NULL,
            g_cclosure_marshal_generic,
            G_TYPE_NONE,
            3,
            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

/**
 * EphyWebView::ge-search-link:
 * @view: the #EphyWebView that received the signal
 * @type: the mime-type of the search description
 * @title: the title of the news feed
 * @address: the URL to @embed's web site's search description
 *
 * The ::ge-search-link signal is emitted when @embed discovers that a
 * search description is available for the site it is visiting.
 **/
    g_signal_new ("ge_search_link",
            EPHY_TYPE_WEB_VIEW,
            G_SIGNAL_RUN_FIRST,
            G_STRUCT_OFFSET (EphyWebViewClass, search_link),
            NULL, NULL,
            g_cclosure_marshal_generic,
            G_TYPE_NONE,
            3,
            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

/**
 * EphyWebView::ge-feed-link:
 * @view: the #EphyWebView that received the signal
 * @type: the mime-type of the news feed
 * @title: the title of the news feed
 * @address: the URL to @embed's web site's news feed
 *
 * The ::ge-feed-link signal is emitted when @embed discovers that a
 * news feed is available for the site it is visiting.
 **/
    g_signal_new ("ge_feed_link",
            EPHY_TYPE_WEB_VIEW,
            G_SIGNAL_RUN_FIRST,
            G_STRUCT_OFFSET (EphyWebViewClass, feed_link),
            NULL, NULL,
            g_cclosure_marshal_generic,
            G_TYPE_NONE,
            3,
            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
/**
 * EphyWebView::ge-modal-alert:
 * @view: the #EphyWebView that received the signal
 *
 * The ::ge-modal-alert signal is emitted when a DOM event will open a
 * modal alert.
 *
 * Return %TRUE to prevent the dialog from being opened.
 **/
    g_signal_new ("ge_modal_alert",
            EPHY_TYPE_WEB_VIEW,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (EphyWebViewClass, modal_alert),
            g_signal_accumulator_true_handled, NULL,
            g_cclosure_marshal_generic,
            G_TYPE_BOOLEAN,
            0);
/**
 * EphyWebView::ge-modal-alert-closed:
 * @view: the #EphyWebView that received the signal
 *
 * The ::ge-modal-alert-closed signal is emitted when a modal alert put up by a
 * DOM event was closed.
 **/
    g_signal_new ("ge_modal_alert_closed",
            EPHY_TYPE_WEB_VIEW,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (EphyWebViewClass, modal_alert_closed),
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

/**
 * EphyWebView::search-key-press:
 * @view: the #EphyWebView that received the signal
 * @event: the #GdkEventKey which triggered this signal
 *
 * The ::search-key-press signal is emitted for keypresses which
 * should be used for find implementations.
 **/
    g_signal_new ("search-key-press",
            EPHY_TYPE_WEB_VIEW,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (EphyWebViewClass, search_key_press),
            g_signal_accumulator_true_handled, NULL,
            g_cclosure_marshal_generic,
            G_TYPE_BOOLEAN,
            1,
            GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

/**
 * EphyWebView::content-blocked:
 * @view: the #EphyWebView that received the signal
 * @uri: blocked URI
 *
 * The ::content-blocked signal is emitted when an url has been blocked.
 **/
    g_signal_new ("content-blocked",
            EPHY_TYPE_WEB_VIEW,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (EphyWebViewClass, content_blocked),
            NULL, NULL,
            g_cclosure_marshal_VOID__STRING,
            G_TYPE_NONE,
            1,
            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
/**
 * EphyWebView::new-document-now:
 * @view: the #EphyWebView that received the signal
 * @uri: URI of the new content
 *
 * The ::new-document-now signal is emitted when a new page content
 * is being loaded into the browser. It's a good place to do view
 * related changes, for example to restore the zoom level of a page
 * or to set an user style sheet.
 **/
    g_signal_new ("new-document-now",
            EPHY_TYPE_WEB_VIEW,
            G_SIGNAL_RUN_FIRST,
            G_STRUCT_OFFSET (EphyWebViewClass, new_document_now),
            NULL, NULL,
            g_cclosure_marshal_VOID__STRING,
            G_TYPE_NONE,
            1,
            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

/**
 * EphyWebView::loading-homepage:
 * @view: the #EphyWebView that received the signal
 *
 * The ::loading-homepage signal is emitted when the @view is about to
 * load the homepage set by the user.
 **/
    g_signal_new ("loading-homepage",
                  EPHY_TYPE_WEB_VIEW,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (EphyWebViewClass, loading_homepage),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

/**
 * EphyWebView::donload-only-load:
 * @view: the #EphyWebView that received the signal
 *
 * The ::download-only-load signal is emitted when the @view has its main load
 * replaced by a download, and that is the only reason why the @view has been created.
 **/
    g_signal_new ("download-only-load",
                  EPHY_TYPE_WEB_VIEW,
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  g_type_class_add_private (gobject_class, sizeof (EphyWebViewPrivate));
}

static void
new_window_cb (EphyWebView *view,
               EphyWebView *new_view,
               gpointer user_data)
{
  EphyEmbedContainer *container;

  g_return_if_fail (new_view != NULL);

  container = EPHY_EMBED_CONTAINER (gtk_widget_get_toplevel (GTK_WIDGET (new_view)));
  g_return_if_fail (container != NULL || !gtk_widget_is_toplevel (GTK_WIDGET (container)));

  popups_manager_add_window (view, container);
}

static void
ge_popup_blocked_cb (EphyWebView *view,
                     const char *url,
                     const char *name,
                     const char *features,
                     gpointer user_data)
{
  popups_manager_add (view, url, name, features);
}

static gboolean
decide_policy_cb (WebKitWebView *web_view,
                  WebKitPolicyDecision *decision,
                  WebKitPolicyDecisionType decision_type,
                  gpointer user_data)
{
  WebKitResponsePolicyDecision *response_decision;
  WebKitURIResponse *response;
  WebKitURIRequest *request;
  WebKitWebResource *main_resource;
  EphyWebViewDocumentType type;
  const char *mime_type;
  const char *request_uri;

  if (decision_type != WEBKIT_POLICY_DECISION_TYPE_RESPONSE)
    return FALSE;

  response_decision = WEBKIT_RESPONSE_POLICY_DECISION (decision);
  response = webkit_response_policy_decision_get_response (response_decision);
  mime_type = webkit_uri_response_get_mime_type (response);

  type = EPHY_WEB_VIEW_DOCUMENT_OTHER;
  if (!strcmp (mime_type, "text/html") || !strcmp (mime_type, "text/plain"))
    type = EPHY_WEB_VIEW_DOCUMENT_HTML;
  else if (!strcmp (mime_type, "application/xhtml+xml"))
    type = EPHY_WEB_VIEW_DOCUMENT_XML;
  else if (!strncmp (mime_type, "image/", 6))
    type = EPHY_WEB_VIEW_DOCUMENT_IMAGE;

  /* FIXME: maybe it makes more sense to have an API to query the mime
   * type when the load of a page starts than doing this here.
   */
  if (EPHY_WEB_VIEW (web_view)->priv->document_type != type) {
    EPHY_WEB_VIEW (web_view)->priv->document_type = type;

    g_object_notify (G_OBJECT (web_view), "document-type");
  }

  /* If WebKit can't handle the mime type start the download
     process */
  if (webkit_response_policy_decision_is_mime_type_supported (response_decision))
    return FALSE;

  request = webkit_response_policy_decision_get_request (response_decision);
  request_uri = webkit_uri_request_get_uri (request);
  main_resource = webkit_web_view_get_main_resource (web_view);
  if (g_strcmp0 (webkit_web_resource_get_uri (main_resource), request_uri) != 0)
    return FALSE;

  webkit_policy_decision_download (decision);

  return TRUE;
}

static void
decide_on_geolocation_policy_request (GtkWidget *info_bar,
                                      int response,
                                      WebKitPermissionRequest *request)
{
  gtk_widget_destroy (info_bar);

  switch (response) {
  case GTK_RESPONSE_YES:
    webkit_permission_request_allow (request);
    break;
  default:
    webkit_permission_request_deny (request);
    break;
  }

  gtk_widget_destroy (info_bar);
  g_object_unref (request);
}

static gboolean
permission_request_cb (WebKitWebView           *web_view,
                       WebKitPermissionRequest *decision)
{
  GtkWidget *info_bar;
  GtkWidget *action_area;
  GtkWidget *content_area;
  GtkWidget *label;
  char *message;
  char *host;

  if (!WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST (decision))
    return FALSE;

  info_bar = gtk_info_bar_new_with_buttons (_("Deny"), GTK_RESPONSE_NO,
                                            _("Allow"), GTK_RESPONSE_YES,
                                            NULL);

  action_area = gtk_info_bar_get_action_area (GTK_INFO_BAR (info_bar));
  /* Translators: Geolocation policy for a specific site. */
  gtk_orientable_set_orientation (GTK_ORIENTABLE (action_area),
                                  GTK_ORIENTATION_HORIZONTAL);

  /* Label */
  host = ephy_string_get_host_name (webkit_web_view_get_uri (web_view));
  message = g_markup_printf_escaped (_("The page at <b>%s</b> wants to know your location."),
                                     host);
  g_free (host);

  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), message);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

  g_free (message);

  content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar));
  gtk_container_add (GTK_CONTAINER (content_area), label);

  gtk_widget_show_all (info_bar);

  /* Ref the decision, to keep it alive while we decide */
  g_signal_connect (info_bar, "response",
                    G_CALLBACK (decide_on_geolocation_policy_request),
                    g_object_ref (decision));

  ephy_embed_add_top_widget (EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view),
                             info_bar, TRUE);

  return TRUE;
}

static void
get_host_for_url_cb (gpointer service,
                     gboolean success,
                     gpointer result_data,
                     gpointer user_data)
{
  EphyHistoryHost *host;
  EphyWebView *view;
  double current_zoom;

  if (success == FALSE)
    return;

  view = EPHY_WEB_VIEW (user_data);
  host = (EphyHistoryHost *)result_data;

  current_zoom = webkit_web_view_get_zoom_level (WEBKIT_WEB_VIEW (view));

  if (host->zoom_level != current_zoom) {
    view->priv->is_setting_zoom = TRUE;
    webkit_web_view_set_zoom_level (WEBKIT_WEB_VIEW (view), host->zoom_level);
    view->priv->is_setting_zoom = FALSE;
  }

  ephy_history_host_free (host);
}

static void
restore_zoom_level (EphyWebView *view,
                    const char *address)
{
  if (ephy_embed_utils_address_has_web_scheme (address))
    ephy_history_service_get_host_for_url (view->priv->history_service,
                                           address, view->priv->history_service_cancellable,
                                           (EphyHistoryJobCallback)get_host_for_url_cb, view);
}

static void
ephy_web_view_location_changed (EphyWebView *view,
                                const char *location)
{
  GObject *object = G_OBJECT (view);
  EphyWebViewPrivate *priv = view->priv;

  g_object_freeze_notify (object);

  /* Do this up here so we still have the old address around. */
  ephy_file_monitor_update_location (priv->file_monitor, location);

  if (location == NULL || location[0] == '\0') {
    ephy_web_view_set_address (view, NULL);
    ephy_web_view_set_title (view, EMPTY_PAGE);
  } else if (g_str_has_prefix (location, EPHY_ABOUT_SCHEME":applications")) {
    SoupURI *uri = soup_uri_new (location);
    char *new_address;

    /* Strip the query from the URL for about:applications. */
    soup_uri_set_query (uri, NULL);
    new_address = soup_uri_to_string (uri, FALSE);
    soup_uri_free (uri);

    ephy_web_view_set_address (view, new_address);
    g_free (new_address);
  } else {
    /* We do this to get rid of an eventual password in the URL. */
    ephy_web_view_set_address (view, location);
    ephy_web_view_set_loading_title (view, location, TRUE);
  }

  ephy_web_view_set_link_message (view, NULL);

  _ephy_web_view_update_icon (view);

  update_navigation_flags (view);

  g_object_thaw_notify (object);
}

static void
on_snapshot_ready (WebKitWebView *webview,
                   GAsyncResult *res,
                   GtkTreeRowReference *ref)
{
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  cairo_surface_t *surface;
  GError *error = NULL;

  surface = webkit_web_view_get_snapshot_finish (webview, res, &error);
  if (error) {
    g_warning ("%s(): %s", G_STRFUNC, error->message);
    g_error_free (error);
    return;
  }

  model = gtk_tree_row_reference_get_model (ref);
  path = gtk_tree_row_reference_get_path (ref);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);

  ephy_overview_store_set_snapshot (EPHY_OVERVIEW_STORE (model), &iter, surface,
                                    webkit_web_view_get_favicon (webview));
  cairo_surface_destroy (surface);
}

/* FIXME: We should be using the snapshot service for this instead of
   using the WK API directly. */
static gboolean
web_view_check_snapshot (WebKitWebView *web_view)
{
  EphyOverviewStore *store;
  GtkTreeIter iter;
  GtkTreeRowReference *ref;
  GtkTreePath *path;
  EphyWebView* view = EPHY_WEB_VIEW (web_view);

  EphyEmbedShell *embed_shell = ephy_embed_shell_get_default ();

  store = EPHY_OVERVIEW_STORE (ephy_embed_shell_get_frecent_store (embed_shell));
  if (ephy_overview_store_find_url (store, webkit_web_view_get_uri (web_view), &iter) &&
      ephy_overview_store_needs_snapshot (store, &iter)) {
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
    ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (store), path);
    gtk_tree_path_free (path);
    webkit_web_view_get_snapshot (web_view, WEBKIT_SNAPSHOT_REGION_VISIBLE,
                                  WEBKIT_SNAPSHOT_OPTIONS_NONE,
                                  NULL, (GAsyncReadyCallback)on_snapshot_ready,
                                  ref);
  }

  view->priv->snapshot_idle_id = 0;

  return FALSE;
}

static void
load_changed_cb (WebKitWebView *web_view,
                 WebKitLoadEvent load_event,
                 gpointer user_data)
{
  EphyWebView *view = EPHY_WEB_VIEW (web_view);
  EphyWebViewPrivate *priv = view->priv;
  GObject *object = G_OBJECT (web_view);

  g_object_freeze_notify (object);

  switch (load_event) {
  case WEBKIT_LOAD_STARTED: {
    const char *loading_uri = NULL;

    priv->load_failed = FALSE;

    loading_uri = webkit_web_view_get_uri (web_view);
    g_signal_emit_by_name (view, "new-document-now", loading_uri);

    if (ephy_embed_utils_is_no_show_address (loading_uri))
      ephy_web_view_freeze_history (view);

    if (priv->address == NULL || priv->address[0] == '\0')
      ephy_web_view_set_address (view, loading_uri);

    ephy_web_view_set_loading_title (view, loading_uri, TRUE);

    g_free (priv->status_message);
    priv->status_message = g_strdup (priv->loading_title);
    g_object_notify (object, "status-message");

    /* Zoom level. */
    restore_zoom_level (view, loading_uri);

    break;
  }
  case WEBKIT_LOAD_REDIRECTED:
    /* TODO: Update the loading uri */
    break;
  case WEBKIT_LOAD_COMMITTED: {
    const char* uri;
    EphyWebViewSecurityLevel security_level = EPHY_WEB_VIEW_STATE_IS_UNKNOWN;

    priv->ever_committed = TRUE;

    /* Title and location. */
    uri = webkit_web_view_get_uri (web_view);
    ephy_web_view_location_changed (view, uri);

    /* Security status. */
    g_clear_object (&priv->certificate);
    if (webkit_web_view_get_tls_info (web_view, &priv->certificate, &priv->tls_errors)) {
      g_object_ref (priv->certificate);
      security_level = priv->tls_errors == 0 ?
        EPHY_WEB_VIEW_STATE_IS_SECURE_HIGH : EPHY_WEB_VIEW_STATE_IS_BROKEN;
    }

    ephy_web_view_set_security_level (EPHY_WEB_VIEW (web_view), security_level);

    /* History. */
    if (!ephy_web_view_is_history_frozen (view)) {
      char *history_uri = NULL;

      /* TODO: move the normalization down to the history service? */
      if (g_str_has_prefix (uri, EPHY_ABOUT_SCHEME))
          history_uri = g_strdup_printf ("about:%s", uri + EPHY_ABOUT_SCHEME_LEN + 1);
      else
        history_uri = g_strdup (uri);

      ephy_history_service_visit_url (priv->history_service,
                                      history_uri,
                                      priv->visit_type);

      g_free (history_uri);
    }

    break;
  }
  case WEBKIT_LOAD_FINISHED:
    g_free (priv->status_message);
    priv->status_message = NULL;
    g_object_notify (object, "status-message");
    ephy_web_view_set_loading_title (view, NULL, FALSE);

    if (priv->is_blank || !webkit_web_view_get_title (web_view))
      ephy_web_view_set_title (view, NULL);

    /* Ensure we load the icon for this web view, if available. */
    _ephy_web_view_update_icon (view);

    /* Reset visit type. */
    priv->visit_type = EPHY_PAGE_VISIT_NONE;

    if (!ephy_web_view_is_history_frozen (view)) {
      if (priv->snapshot_idle_id)
        g_source_remove (priv->snapshot_idle_id);
      priv->snapshot_idle_id = g_idle_add_full (G_PRIORITY_LOW, (GSourceFunc)web_view_check_snapshot, web_view, NULL);
    }

    ephy_web_view_thaw_history (view);

    break;
  }

  g_object_thaw_notify (object);
}

/**
 * ephy_web_view_set_placeholder:
 * @view: an #EphyWebView
 * @uri: uri that will eventually be loaded
 * @title: last-known title of the page that will eventually be loaded
 *
 * Makes the #EphyWebView pretend a page that will eventually be loaded is
 * already there.
 *
 **/
void
ephy_web_view_set_placeholder (EphyWebView *view,
                               const char *uri,
                               const char *title)
{
  char *html;

  g_return_if_fail (EPHY_IS_WEB_VIEW (view));

  /* We want only the actual load to be the one recorded in history, but
   * doing a load here is the simplest way to replace the loading
   * spinner with the favicon. */
  ephy_web_view_freeze_history (view);

  html = g_markup_printf_escaped ("<head><title>%s</title></head>", title);

  webkit_web_view_load_alternate_html (WEBKIT_WEB_VIEW (view), html, uri, NULL);

  g_free (html);

  ephy_web_view_set_address (view, uri);
}


static char *
get_style_sheet (void)
{
  const gchar *file;
  GError *error = NULL;
  char *sheet;

  file = ephy_file ("error.css");
  if (file && !g_file_get_contents (file, &sheet, NULL, &error)) {
    g_debug ("Unable to load error style sheet: %s", error->message);
    g_error_free (error);
  }

  return sheet;
}

/**
 * ephy_web_view_load_error_page:
 * @view: an #EphyWebView
 * @uri: uri that caused the failure
 * @page: one of #EphyWebViewErrorPage
 * @error: a GError to inspect, or %NULL
 *
 * Loads an error page appropiate for @page in @view.
 *
 **/
void
ephy_web_view_load_error_page (EphyWebView *view,
                               const char *uri,
                               EphyWebViewErrorPage page,
                               GError *error)
{
  GString *html = g_string_new ("");
  const char *reason;
  char *hostname;
  char *lang;
  char *page_title;
  char *msg_title;
  char *msg;
  char *button_label;
  char *stylesheet;
  GBytes *html_file;

  if (error)
    reason = error->message;
  else
    reason = _("None specified");

  hostname = ephy_string_get_host_name (uri);
  if (hostname == NULL)
    hostname = g_strdup (uri);

  lang = g_strdup (pango_language_to_string (gtk_get_default_language ()));
  g_strdelimit (lang, "_-@", '\0');

  switch (page) {
    case EPHY_WEB_VIEW_ERROR_PAGE_NETWORK_ERROR:
      page_title = g_strdup_printf (_("Problem loading “%s”"), hostname);

      msg_title = g_strdup (_("Oops! Unable to display this website."));
      msg = g_strdup_printf (_("<p>The site at “%s” seems "
                               "to be unavailable. The precise error was:</p>"
                               "<p><code>%s</code></p>"
                               "<p>It may be "
                               "temporarily unavailable or moved to a new "
                               "address. You may wish to verify that your "
                               "internet connection is working correctly.</p>"),
                             uri, reason);

      button_label = g_strdup (_("Try Again"));

      html_file = g_resources_lookup_data (EPHY_PAGE_TEMPLATE_ERROR, 0, NULL);
      break;
    case EPHY_WEB_VIEW_ERROR_PAGE_CRASH:
      page_title = g_strdup_printf (_("Problem loading “%s”"), hostname);

      msg_title = g_strdup (_("Oops! There may be a problem."));
      msg = g_strdup_printf (_("<p>This site may have caused Web to close unexpectedly.</p>"
                               "<p>If this happens again, "
                               "please report the problem to the "
                               "<strong>%s</strong> developers.</p>"),
                             LSB_DISTRIBUTOR);

      button_label = g_strdup (_("Reload Anyway"));

      html_file = g_resources_lookup_data (EPHY_PAGE_TEMPLATE_RECOVERY, 0, NULL);
      break;
    case EPHY_WEB_VIEW_ERROR_PROCESS_CRASH:
      page_title = g_strdup_printf (_("Problem displaying “%s”"), hostname);
      msg_title = g_strdup (_("Oops!"));
      msg = g_strdup (_("Something went wrong while displaying this page. Please reload or visit a different page to continue."));
      button_label = g_strdup (_("Reload Anyway"));

      html_file = g_resources_lookup_data (EPHY_PAGE_TEMPLATE_PROCESS_CRASH, 0, NULL);

      break;
    default:
      return;
      break;
  }
  g_free (hostname);

  ephy_web_view_set_title (view, page_title);

  _ephy_web_view_update_icon (view);

  stylesheet = get_style_sheet ();
  g_string_printf (html,
                   g_bytes_get_data (html_file, NULL),
                   lang, lang,
                   ((gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL) ? "rtl" : "ltr"),
                   page_title,
                   stylesheet,
                   uri,
                   msg_title, msg, button_label);

  g_bytes_unref (html_file);
  g_free (stylesheet);
  g_free (lang);
  g_free (page_title);
  g_free (msg_title);
  g_free (msg);
  g_free (button_label);

  /* Make our history backend ignore the next page load, since it will be an error page. */
  ephy_web_view_freeze_history (view);
  webkit_web_view_load_alternate_html (WEBKIT_WEB_VIEW (view), html->str, uri, 0);
  g_string_free (html, TRUE);
}

static gboolean
load_failed_cb (WebKitWebView *web_view,
                WebKitLoadEvent load_event,
                const char *uri,
                GError *error,
                gpointer user_data)
{
  EphyWebView *view = EPHY_WEB_VIEW (web_view);
  EphyWebViewPrivate *priv = view->priv;

  priv->load_failed = TRUE;
  ephy_web_view_set_link_message (view, NULL);
  update_navigation_flags (view);

  if (error->domain == SOUP_HTTP_ERROR) {
    ephy_web_view_load_error_page (view, uri, EPHY_WEB_VIEW_ERROR_PAGE_NETWORK_ERROR, error);
    return TRUE;
  }

  g_return_val_if_fail ((error->domain == WEBKIT_NETWORK_ERROR) ||
                        (error->domain == WEBKIT_POLICY_ERROR) ||
                        (error->domain == WEBKIT_PLUGIN_ERROR), FALSE);

  switch (error->code) {
  case WEBKIT_NETWORK_ERROR_FAILED:
  case WEBKIT_NETWORK_ERROR_TRANSPORT:
  case WEBKIT_NETWORK_ERROR_UNKNOWN_PROTOCOL:
  case WEBKIT_NETWORK_ERROR_FILE_DOES_NOT_EXIST:
  case WEBKIT_POLICY_ERROR_FAILED:
  case WEBKIT_POLICY_ERROR_CANNOT_SHOW_MIME_TYPE:
  case WEBKIT_POLICY_ERROR_CANNOT_SHOW_URI:
  case WEBKIT_POLICY_ERROR_CANNOT_USE_RESTRICTED_PORT:
  case WEBKIT_PLUGIN_ERROR_FAILED:
  case WEBKIT_PLUGIN_ERROR_CANNOT_FIND_PLUGIN:
  case WEBKIT_PLUGIN_ERROR_CANNOT_LOAD_PLUGIN:
  case WEBKIT_PLUGIN_ERROR_JAVA_UNAVAILABLE:
  case WEBKIT_PLUGIN_ERROR_CONNECTION_CANCELLED:
    ephy_web_view_load_error_page (view, uri, EPHY_WEB_VIEW_ERROR_PAGE_NETWORK_ERROR, error);
    return TRUE;
  case WEBKIT_NETWORK_ERROR_CANCELLED:
    {
      if (!priv->typed_address) {
        const char* prev_uri;

        prev_uri = webkit_web_view_get_uri (web_view);
        ephy_web_view_set_address (view, prev_uri);
      }
    }
    break;
  case WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE:
    /* If we are going to download something, and this is the first
     * page to load in this tab, we may want to close it down. */
    if (!priv->ever_committed)
      g_signal_emit_by_name (view, "download-only-load", NULL);
    break;
  /* In case the resource is going to be showed with a plugin just let
   * WebKit do it */
  case WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD:
  default:
    break;
  }

  return FALSE;
}

static void
close_web_view_cb (WebKitWebView *web_view,
                   gpointer user_data)

{
  GtkWidget *widget = gtk_widget_get_toplevel (GTK_WIDGET (web_view));

  LOG ("close web view");

  if (EPHY_IS_EMBED_CONTAINER (widget))
    ephy_embed_container_remove_child (EPHY_EMBED_CONTAINER (widget),
                                       EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view));
  else
    gtk_widget_destroy (widget);
}


static void
zoom_changed_cb (WebKitWebView *web_view,
                 GParamSpec *pspec,
                 gpointer user_data)
{
  const char *address;
  double zoom;
  EphyWebViewPrivate *priv = EPHY_WEB_VIEW (web_view)->priv;

  zoom = webkit_web_view_get_zoom_level (web_view);

  if (priv->is_setting_zoom)
    return;

  address = ephy_web_view_get_address (EPHY_WEB_VIEW (web_view));
  if (ephy_embed_utils_address_has_web_scheme (address)) {
    ephy_history_service_set_url_zoom_level (priv->history_service,
                                             address, zoom,
                                             NULL, NULL, NULL);
  }
}

static void
ephy_web_view_init (EphyWebView *web_view)
{
  EphyWebViewPrivate *priv;

  priv = web_view->priv = EPHY_WEB_VIEW_GET_PRIVATE (web_view);

  priv->is_blank = TRUE;
  priv->ever_committed = FALSE;
  priv->title = g_strdup (EMPTY_PAGE);
  priv->document_type = EPHY_WEB_VIEW_DOCUMENT_HTML;
  priv->security_level = EPHY_WEB_VIEW_STATE_IS_UNKNOWN;

  priv->file_monitor = ephy_file_monitor_new (web_view);

  priv->non_search_regex = g_regex_new (EPHY_WEB_VIEW_NON_SEARCH_REGEX,
                                        G_REGEX_OPTIMIZE, G_REGEX_MATCH_NOTEMPTY, NULL);

  priv->domain_regex = g_regex_new (EPHY_WEB_VIEW_DOMAIN_REGEX,
                                    G_REGEX_OPTIMIZE, G_REGEX_MATCH_NOTEMPTY, NULL);

  priv->history_service = EPHY_HISTORY_SERVICE (ephy_embed_shell_get_global_history_service (ephy_embed_shell_get_default ()));
  priv->history_service_cancellable = g_cancellable_new ();

  g_signal_connect (priv->history_service,
                    "cleared", G_CALLBACK (ephy_web_view_history_cleared_cb),
                    web_view);

  g_signal_connect (web_view, "decide-policy",
                    G_CALLBACK (decide_policy_cb),
                    NULL);

  g_signal_connect (web_view, "permission-request",
                    G_CALLBACK (permission_request_cb),
                    NULL);

  g_signal_connect (web_view, "load-changed",
                    G_CALLBACK (load_changed_cb),
                    NULL);

  g_signal_connect (web_view, "close",
                    G_CALLBACK (close_web_view_cb),
                    NULL);
  g_signal_connect (web_view, "load-failed",
                    G_CALLBACK (load_failed_cb),
                    NULL);

  g_signal_connect (web_view, "notify::zoom-level",
                    G_CALLBACK (zoom_changed_cb),
                    NULL);

  g_signal_connect (web_view, "notify::title",
                    G_CALLBACK (title_changed_cb),
                    NULL);

  g_signal_connect (web_view, "notify::uri",
                    G_CALLBACK (uri_changed_cb),
                    NULL);

  g_signal_connect (web_view, "mouse-target-changed",
                    G_CALLBACK (mouse_target_changed_cb),
                    NULL);

  g_signal_connect (web_view, "notify::favicon",
                    G_CALLBACK (icon_changed_cb),
                    NULL);

  g_signal_connect (web_view, "new-window",
                    G_CALLBACK (new_window_cb),
                    NULL);

  g_signal_connect (web_view, "ge_popup_blocked",
                    G_CALLBACK (ge_popup_blocked_cb),
                    NULL);

  g_signal_connect (ephy_embed_shell_get_default (), "form-auth-data-save-requested",
                    G_CALLBACK (form_auth_data_save_requested),
                    web_view);
}

/**
 * ephy_web_view_new:
 *
 * Equivalent to g_object_new() but returns an #GtkWidget so you don't have
 * to cast it when dealing with most code.
 *
 * Return value: the newly created #EphyWebView widget
 **/
GtkWidget *
ephy_web_view_new (void)
{
  return g_object_new (EPHY_TYPE_WEB_VIEW,
                       "group", ephy_embed_prefs_get_web_view_group (),
                       NULL);
}

static gboolean
is_public_domain (EphyWebView *view, const char *url)
{
  gboolean retval = FALSE;
  char *host = NULL;
  EphyWebViewPrivate *priv = view->priv;

  host = ephy_string_get_host_name (url);
  g_return_val_if_fail (host, FALSE);

  if (g_regex_match (priv->domain_regex, host, 0, NULL)) {
    if (g_str_equal (host, "localhost"))
      retval = TRUE;
    else {
      const char *end;

      end = g_strrstr (host, ".");

      if (end && *end != '\0')
        retval = soup_tld_domain_is_public_suffix (end);
    }
  }

  g_free (host);

  return retval;
}

/**
 * ephy_web_view_normalize_or_autosearch_url:
 * @view: an %EphyWebView
 * @url: a URI
 * 
 * Returns a normalized representation of @url, or an autosearch
 * string for it when necessary.
 * 
 * Returns: the normalized @url or autosearch string.
 **/
char*
ephy_web_view_normalize_or_autosearch_url (EphyWebView *view, const char *url)
{
  char *effective_url;
  char *scheme;
  EphyWebViewPrivate *priv = view->priv;

  g_return_val_if_fail (EPHY_IS_WEB_VIEW (view), NULL);
  g_return_val_if_fail (url, NULL);

  scheme = g_uri_parse_scheme (url);

  /* If the string doesn't look like an URI, let's search it; */
  if (!ephy_embed_utils_address_has_web_scheme (url) &&
      scheme == NULL &&
      !ephy_embed_utils_address_is_existing_absolute_filename (url) &&
      priv->non_search_regex &&
      !g_regex_match (priv->non_search_regex, url, 0, NULL) &&
      !is_public_domain (view, url)) {
    char *query_param, *url_search;

    url_search = g_settings_get_string (EPHY_SETTINGS_MAIN,
                                        EPHY_PREFS_KEYWORD_SEARCH_URL);

    if (url_search == NULL || url_search[0] == '\0') {
      g_free (url_search);

      url_search = g_strdup (_("http://duckduckgo.com/?q=%s&amp;t=epiphany"));
    }

    query_param = soup_form_encode ("q", url, NULL);
    /* + 2 here is getting rid of 'q=' */
    effective_url = g_strdup_printf (url_search, query_param + 2);
    g_free (query_param);
    g_free (url_search);
  } else
    effective_url = ephy_embed_utils_normalize_address (url);

  if (scheme)
    g_free (scheme);

  return effective_url;
}

/**
 * ephy_web_view_load_request:
 * @view: the #EphyWebView in which to load the request
 * @request: the #WebKitNetworkRequest to be loaded
 *
 * Loads the given #WebKitNetworkRequest in the given #EphyWebView.
 **/
void
ephy_web_view_load_request (EphyWebView *view,
                            WebKitURIRequest *request)
{
  const char *url;
  char *effective_url;

  g_return_if_fail (EPHY_IS_WEB_VIEW (view));
  g_return_if_fail (WEBKIT_IS_URI_REQUEST (request));

  url = webkit_uri_request_get_uri (request);
  effective_url = ephy_web_view_normalize_or_autosearch_url (view, url);

  webkit_uri_request_set_uri (request, effective_url);
  g_free (effective_url);

  webkit_web_view_load_request (WEBKIT_WEB_VIEW (view), request);
}

#if 0
typedef struct {
  EphyWebView *view;
  char *original_uri;
} HEADAttemptData;

static void
effective_url_head_cb (SoupSession *session,
                       SoupMessage *message,
                       gpointer user_data)
{
  HEADAttemptData *data = (HEADAttemptData*)user_data;

  EphyWebView *view = data->view;

  if (message->status_code == SOUP_STATUS_OK) {
    char *uri = soup_uri_to_string (soup_message_get_uri (message), FALSE);

    webkit_web_view_load_uri (WEBKIT_WEB_VIEW (view), uri);

    g_free (uri);
  } else {
    GError *error = NULL;
    GdkScreen *screen;

    screen = gtk_widget_get_screen (GTK_WIDGET (view));
    gtk_show_uri (screen, data->original_uri, GDK_CURRENT_TIME, &error);

    if (error) {
      LOG ("failed to handle non web scheme: %s", error->message);
      g_error_free (error);

      /* Load the original URI to trigger an error in the view. */
      webkit_web_view_load_uri (WEBKIT_WEB_VIEW (view), data->original_uri);
    }
  }

  g_free (data->original_uri);
  g_slice_free (HEADAttemptData, data);
}
#endif

/**
 * ephy_web_view_load_url:
 * @view: an #EphyWebView
 * @url: a URL
 *
 * Loads @url in @view.
 **/
void
ephy_web_view_load_url (EphyWebView *view,
                        const char *url)
{
  char *effective_url;

  g_return_if_fail (EPHY_IS_WEB_VIEW (view));
  g_return_if_fail (url);

  effective_url = ephy_web_view_normalize_or_autosearch_url (view, url);

  /* After normalization there are still some cases that are
   * impossible to tell apart. One example is <URI>:<PORT> and <NON
   * WEB SCHEME>:<DATA>. To fix this, let's do a HEAD request to the
   * effective URI prefxed with http://; if we get OK Status the URI
   * exists, and we'll go ahead, otherwise we'll try to launch a
   * proper handler through gtk_show_uri. We only do this in
   * ephy_web_view_load_url, since this case is only relevant for URIs
   * typed in the location entry, which uses this method to do the
   * load. */
  if (!ephy_embed_utils_address_has_web_scheme (effective_url)) {
#if 0
    /* TODO: WebKit2, Network features */
    SoupMessage *message;
    SoupSession *session;
    char *temp_url;
    HEADAttemptData *data;

    temp_url = g_strconcat ("http://", effective_url, NULL);

    session = webkit_get_default_session ();
    message = soup_message_new (SOUP_METHOD_HEAD,
                                temp_url);

    if (message) {
      data = g_slice_new (HEADAttemptData);
      data->view = view;
      data->original_uri = g_strdup (effective_url);
      soup_session_queue_message (session, message,
                                  effective_url_head_cb, data);
    } else {
      /* If we cannot even create a message fallback to the effective
       * url, the gtk_show_uri code will make another attempt in
       * EphyWindow's policy code. */
      webkit_web_view_load_uri (WEBKIT_WEB_VIEW (view), effective_url);
    }

    g_free (temp_url);
#endif
  } else if (g_str_has_prefix (effective_url, "javascript:")) {
    char *decoded_url;

    decoded_url = soup_uri_decode (effective_url);
    webkit_web_view_run_javascript (WEBKIT_WEB_VIEW (view), decoded_url, NULL, NULL, NULL);
    g_free (decoded_url);
  } else
    webkit_web_view_load_uri (WEBKIT_WEB_VIEW (view), effective_url);

  g_free (effective_url);
}

/**
 * ephy_web_view_copy_back_history:
 * @source: the #EphyWebView from which to get the back history
 * @dest: the #EphyWebView to copy the history to
 *
 * Sets the back history (up to the current item) of @source as the
 * back history of @dest.
 *
 * Useful to keep the history when opening links in new tabs or
 * windows.
 **/
void
ephy_web_view_copy_back_history (EphyWebView *source,
                                 EphyWebView *dest)
{
  /* TODO: WebKit2, BackForwardList */
}

/**
 * ephy_web_view_get_is_blank:
 * @view: an #EphyWebView
 *
 * Returns whether the  @view's address is "blank".
 *
 * Return value: %TRUE if the @view's address is "blank"
 **/
gboolean
ephy_web_view_get_is_blank (EphyWebView *view)
{
  return view->priv->is_blank;
}

/**
 * ephy_web_view_get_address:
 * @view: an #EphyWebView
 *
 * Returns the address of the currently loaded page.
 *
 * Return value: @view's address. Will never be %NULL.
 **/
const char *
ephy_web_view_get_address (EphyWebView *view)
{
  EphyWebViewPrivate *priv = view->priv;
  return priv->address ? priv->address : "about:blank";
}

/**
 * ephy_web_view_get_title:
 * @view: an #EphyWebView
 *
 * Return value: the title of the web page displayed in @view
 **/
const char *
ephy_web_view_get_title (EphyWebView *view)
{
  return view->priv->title;
}

/**
 * ephy_web_view_set_loading_title:
 * @view: an #EphyWebView
 * @title: new loading title for @view
 * @is_address: %TRUE if @title is an address
 *
 * Update @view's loading title to @title, if @is_address is %TRUE it will
 * retrieve the title of the page at @title.
 **/
void
ephy_web_view_set_loading_title (EphyWebView *view,
                                 const char *title,
                                 gboolean is_address)
{
  EphyWebViewPrivate *priv = view->priv;
  char *freeme = NULL;

  g_free (priv->loading_title);
  priv->loading_title = NULL;

  if (is_address && title) {
    title = freeme = get_title_from_address (title);
  }

  if (title != NULL && title[0] != '\0') {
    /* translators: %s here is the address of the web page */
    priv->loading_title = g_strdup_printf (_ ("Loading “%s”…"), title);
  } else {
    priv->loading_title = g_strdup (_ ("Loading…"));
  }

  g_free (freeme);
}

/**
 * ephy_web_view_is_loading:
 * @view: an #EphyWebView
 *
 * Returns whether the web page in @view has finished loading. A web
 * page is only finished loading after all images, styles, and other
 * dependencies have been downloaded and rendered, or when the load
 * has failed for some reason.
 *
 * Return value: %TRUE if the page is still loading, %FALSE if complete
 **/
gboolean
ephy_web_view_is_loading (EphyWebView *view)
{
  return webkit_web_view_is_loading (WEBKIT_WEB_VIEW (view));
}

/**
 * ephy_web_view_load_failed:
 * @view: an #EphyWebView
 *
 * Returns whether the web page in @view has failed to load.
 *
 * Return value: %TRUE if the page failed to load, %FALSE if it's loading
 * or load finished successfully
 **/
gboolean
ephy_web_view_load_failed (EphyWebView *view)
{
  return view->priv->load_failed;
}

/**
 * ephy_web_view_get_loading_title:
 * @view: an #EphyWebView
 *
 * Returns the loading title for @view.
 *
 * Return value: the provisional title of @view while loading
 **/
const char *
ephy_web_view_get_loading_title (EphyWebView *view)
{
  return view->priv->loading_title;
}

/**
 * ephy_web_view_get_icon:
 * @view: an #EphyWebView
 *
 * Returns the view's site icon as a #GdkPixbuf,
 * or %NULL if it is not available.
 *
 * Return value: (transfer none): a the view's site icon
 **/
GdkPixbuf *
ephy_web_view_get_icon (EphyWebView *view)
{
  return view->priv->icon;
}

/**
 * ephy_web_view_get_document_type:
 * @view: an #EphyWebView
 *
 * Returns the type of document loaded in the @view
 *
 * Return value: the #EphyWebViewDocumentType
 **/
EphyWebViewDocumentType
ephy_web_view_get_document_type (EphyWebView *view)
{
  return view->priv->document_type;
}

/**
 * ephy_web_view_get_navigation_flags:
 * @view: an #EphyWebView
 *
 * Returns @view's navigation flags.
 *
 * Return value: @view's navigation flags
 **/
EphyWebViewNavigationFlags
ephy_web_view_get_navigation_flags (EphyWebView *view)
{
  return view->priv->nav_flags;
}

/**
 * ephy_web_view_get_status_message:
 * @view: an #EphyWebView
 *
 * Returns the message displayed in @view's #EphyWindow's
 * #EphyStatusbar. If the user is hovering the mouse over a hyperlink,
 * this function will return the same value as
 * ephy_web_view_get_link_message(). Otherwise, it will return a network
 * status message, or NULL.
 *
 * The message returned has a limited lifetime, and so should be copied with
 * g_strdup() if it must be stored.
 *
 * Return value: The current statusbar message
 **/
const char *
ephy_web_view_get_status_message (EphyWebView *view)
{
  EphyWebViewPrivate *priv;

  g_return_val_if_fail (EPHY_IS_WEB_VIEW (view), NULL);

  priv = view->priv;

  if (priv->link_message && priv->link_message[0] != '\0') {
    return priv->link_message;
  } else if (priv->status_message) {
    return priv->status_message;
  } else {
    return NULL;
  }
}

/**
 * ephy_web_view_get_link_message:
 * @view: an #EphyWebView
 *
 * When the user is hovering the mouse over a hyperlink, returns the URL of the
 * hyperlink.
 *
 * Return value: the URL of the link over which the mouse is hovering
 **/
const char *
ephy_web_view_get_link_message (EphyWebView *view)
{
  g_return_val_if_fail (EPHY_IS_WEB_VIEW (view), NULL);

  return view->priv->link_message;
}

/**
 * ephy_web_view_set_link_message:
 * @view: an #EphyWebView
 * @link_message: new value for link-message in @view
 *
 * Sets the value of link-message property which tells the URL of the hovered
 * link.
 **/
void
ephy_web_view_set_link_message (EphyWebView *view,
                                const char *link_message)
{
  EphyWebViewPrivate *priv;

  g_return_if_fail (EPHY_IS_WEB_VIEW (view));

  priv = view->priv;

  g_free (priv->link_message);

  priv->link_message = ephy_embed_utils_link_message_parse (link_message);

  g_object_notify (G_OBJECT (view), "status-message");
  g_object_notify (G_OBJECT (view), "link-message");
}

/**
 * ephy_web_view_set_security_level:
 * @view: an #EphyWebView
 * @level: the new #EphyWebViewSecurityLevel for @view
 *
 * Sets @view's security-level property to @level.
 **/
void
ephy_web_view_set_security_level (EphyWebView *view,
                                  EphyWebViewSecurityLevel level)
{
  EphyWebViewPrivate *priv = view->priv;

  g_return_if_fail (EPHY_IS_WEB_VIEW (view));

  if (priv->security_level != level) {
    priv->security_level = level;

    g_object_notify (G_OBJECT (view), "security-level");
  }
}

/**
 * ephy_web_view_get_typed_address:
 * @view: an #EphyWebView
 *
 * Returns the text that the user introduced in the @view's
 * #EphyWindow location entry, if any.
 *
 * This is not guaranteed to be the same as @view's location,
 * available through ephy_web_view_get_address(). As the user types a
 * new address into the location entry,
 * ephy_web_view_get_typed_address()'s returned string will
 * change. When the load starts, ephy_web_view_get_typed_address()
 * will return %NULL, and ephy_web_view_get_address() will return the
 * new page being loaded. Note that the typed_address can be changed
 * again while a load is in progress (in case the user starts to type
 * again in the location entry); in that case
 * ephy_web_view_get_typed_address() will be again non-%NULL, and the
 * contents of the entry will not be overwritten.
 *
 * Return value: @view's #EphyWindow's location entry text when @view
 * is selected.
 **/
const char *
ephy_web_view_get_typed_address (EphyWebView *view)
{
  g_return_val_if_fail (EPHY_IS_WEB_VIEW (view), NULL);

  return view->priv->typed_address;
}

/**
 * ephy_web_view_set_typed_address:
 * @view: an #EphyWebView
 * @address: the new typed address, or %NULL to clear it
 *
 * Sets the text that @view's #EphyWindow will display in its location toolbar
 * entry when @view is selected.
 **/
void
ephy_web_view_set_typed_address (EphyWebView *view,
                                 const char *address)
{
  EphyWebViewPrivate *priv;

  g_return_if_fail (EPHY_IS_WEB_VIEW (view));

  priv = EPHY_WEB_VIEW (view)->priv;

  g_free (priv->typed_address);
  priv->typed_address = g_strdup (address);

  g_object_notify (G_OBJECT (view), "typed-address");
}

static void
has_modified_forms_cb (GDBusProxy *web_extension,
                       GAsyncResult *result,
                       GTask *task)
{
  GVariant *return_value;
  gboolean retval = FALSE;

  return_value = g_dbus_proxy_call_finish (web_extension, result, NULL);
  if (return_value) {
    g_variant_get (return_value, "(b)", &retval);
    g_variant_unref (return_value);
  }

  g_task_return_boolean (task, retval);
  g_object_unref (task);
}

/**
 * ephy_web_view_has_modified_forms:
 * @view: an #EphyWebView
 *
 * A small heuristic is used here. If there's only one input element modified
 * and it does not have a lot of text the user is likely not very interested in
 * saving this work, so it returns %FALSE in this case (eg, google search
 * input).
 *
 * Returns %TRUE if the user has modified &lt;input&gt; or &lt;textarea&gt;
 * values in @view's loaded document.
 *
 * Return value: %TRUE if @view has user-modified forms
 **/
void
ephy_web_view_has_modified_forms (EphyWebView *view,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
  GTask *task = g_task_new (view, cancellable, callback, user_data);
  GDBusProxy *web_extension;

  web_extension = ephy_embed_shell_get_web_extension_proxy (ephy_embed_shell_get_default ());
  if (web_extension) {
    g_dbus_proxy_call (web_extension,
                       "HasModifiedForms",
                       g_variant_new ("(t)", webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view))),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       cancellable,
                       (GAsyncReadyCallback)has_modified_forms_cb,
                       g_object_ref (task));
  } else {
    g_task_return_boolean (task, FALSE);
  }

  g_object_unref (task);
}

gboolean
ephy_web_view_has_modified_forms_finish (EphyWebView *view,
                                         GAsyncResult *result,
                                         GError **error)
{
  g_return_val_if_fail (g_task_is_valid (result, view), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * ephy_web_view_get_security_level:
 * @view: an #EphyWebView
 * @level: (out): return value of security level
 * @certificate: (out) (transfer none): return value of TLS certificate
 * @errors: (out): return value of TLS errors
 *
 * Fetches the #EphyWebViewSecurityLevel and a #GTlsCertificate associated
 * with @view and a #GTlsCertificateFlags showing what problems, if any,
 * have been found with that certificate.
 **/
void
ephy_web_view_get_security_level (EphyWebView *view,
                                  EphyWebViewSecurityLevel *level,
                                  GTlsCertificate **certificate,
                                  GTlsCertificateFlags *errors)
{
  g_return_if_fail (EPHY_IS_WEB_VIEW (view));

  if (level)
    *level = view->priv->security_level;

  if (certificate)
    *certificate = view->priv->certificate;

  if (errors)
    *errors = view->priv->tls_errors;
}

static void
ephy_web_view_print_failed (EphyWebView *view, GError *error)
{
  GtkWidget *info_bar;
  GtkWidget *label;
  GtkContainer *content_area;
  EphyEmbed *embed = EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view);

  info_bar = gtk_info_bar_new_with_buttons (_("_OK"), GTK_RESPONSE_OK, NULL);
  label = gtk_label_new (error->message);
  content_area = GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar)));

  gtk_info_bar_set_message_type (GTK_INFO_BAR (info_bar), GTK_MESSAGE_ERROR);
  gtk_container_add (content_area, label);
  g_signal_connect (info_bar, "response",
                    G_CALLBACK (gtk_widget_destroy), NULL);

  ephy_embed_add_top_widget (embed, info_bar, FALSE);
  gtk_widget_show_all (info_bar);
}

static void
print_operation_finished_cb (WebKitPrintOperation *operation,
                             EphyWebView *view)
{
  ephy_embed_shell_set_page_setup (ephy_embed_shell_get_default (),
                                   webkit_print_operation_get_page_setup (operation));
}

static void
print_operation_failed_cb (WebKitPrintOperation *operation,
                           GError *error,
                           EphyWebView *view)
{
  g_signal_handlers_disconnect_by_func (operation, print_operation_finished_cb, view);
  ephy_web_view_print_failed (view, error);
}

/**
 * ephy_web_view_print:
 * @view: an #EphyWebView
 *
 * Opens a dialog to print the specified view.
 *
 * Since: 2.30
 **/
void
ephy_web_view_print (EphyWebView *view)
{
  WebKitPrintOperation *operation;
  EphyEmbedShell *shell;
  GtkPrintSettings *settings;

  g_return_if_fail (EPHY_IS_WEB_VIEW (view));

  shell = ephy_embed_shell_get_default ();

  operation = webkit_print_operation_new (WEBKIT_WEB_VIEW (view));
  g_signal_connect (operation, "finished",
                    G_CALLBACK (print_operation_finished_cb),
                    view);
  g_signal_connect (operation, "failed",
                    G_CALLBACK (print_operation_failed_cb),
                    view);
  webkit_print_operation_set_page_setup (operation, ephy_embed_shell_get_page_setup (shell));
  settings = gtk_print_settings_new ();
  gtk_print_settings_set (settings,
                          GTK_PRINT_SETTINGS_OUTPUT_BASENAME,
                          ephy_web_view_get_title (view));
  webkit_print_operation_set_print_settings (operation, settings);
  webkit_print_operation_run_dialog (operation, NULL);
  g_object_unref (operation);
  g_object_unref (settings);
}

/**
 * ephy_web_view_get_title_composite:
 * @view: an #EphyView
 *
 * Returns the title of the web page loaded in @view.
 *
 * This differs from #ephy_web_view_get_title in that this function
 * will return a special title while the page is still loading.
 *
 * Return value: @view's web page's title. Will never be %NULL.
 **/
const char *
ephy_web_view_get_title_composite (EphyWebView *view)
{
  const char *title = "";
  const char *loading_title;
  gboolean is_loading;

  g_return_val_if_fail (EPHY_IS_WEB_VIEW (view), NULL);

  is_loading = ephy_web_view_is_loading (view);
  loading_title = ephy_web_view_get_loading_title (view);
  title = ephy_web_view_get_title (view);

  if (view->priv->is_blank)
  {
    if (is_loading)
      title = loading_title;
    else
      title = _("Blank page");
  }

  return title != NULL ? title : "";
}

static void
web_resource_get_data_cb (WebKitWebResource *resource,
                          GAsyncResult *result,
                          GOutputStream *output_stream)
{
  guchar *data;
  gsize data_length;
  GInputStream *input_stream;
  GError *error = NULL;

  data = webkit_web_resource_get_data_finish (resource, result, &data_length, &error);
  if (!data) {
    g_printerr ("Failed to save page: %s", error->message);
    g_error_free (error);
    g_object_unref (output_stream);

    return;
  }

  input_stream = g_memory_input_stream_new_from_data (data, data_length, g_free);
  g_output_stream_splice_async (output_stream, input_stream,
                                G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT,
                                NULL, NULL, NULL);
  g_object_unref (input_stream);
  g_object_unref (output_stream);
}

static void
ephy_web_view_save_main_resource_cb (GFile *file,
                                     GAsyncResult *result,
                                     WebKitWebView *view)
{
  GFileOutputStream *output_stream;
  WebKitWebResource *resource;
  GError *error = NULL;

  output_stream = g_file_replace_finish (file, result, &error);
  if (!output_stream) {
    g_printerr ("Failed to save page: %s", error->message);
    g_error_free (error);

    return;
  }

  resource = webkit_web_view_get_main_resource (view);
  webkit_web_resource_get_data (resource, NULL,
                                (GAsyncReadyCallback)web_resource_get_data_cb,
                                output_stream);
}
/**
 * ephy_web_view_save:
 * @view: an #EphyWebView
 * @uri: location to store the saved page
 *
 * Saves the currently loaded page of @view to @uri.
 **/
void
ephy_web_view_save (EphyWebView *view, const char *uri)
{
  GFile *file;

  g_return_if_fail (EPHY_IS_WEB_VIEW (view));
  g_return_if_fail (uri);

  file = g_file_new_for_uri (uri);

  if (g_str_has_suffix (uri, ".mhtml"))
    webkit_web_view_save_to_file (WEBKIT_WEB_VIEW (view), file, WEBKIT_SAVE_MODE_MHTML,
                                  NULL, NULL, NULL);
  else
    g_file_replace_async (file, NULL, FALSE,
                          G_FILE_CREATE_REPLACE_DESTINATION | G_FILE_CREATE_PRIVATE,
                          G_PRIORITY_DEFAULT, NULL,
                          (GAsyncReadyCallback)ephy_web_view_save_main_resource_cb,
                          view);
  g_object_unref (file);
}

/**
 * ephy_web_view_load_homepage:
 * @view: an #EphyWebView
 *
 * Loads the homepage, which is hardcoded to be "about:overview"
 *
 **/
void
ephy_web_view_load_homepage (EphyWebView *view)
{
  g_return_if_fail (EPHY_IS_WEB_VIEW (view));

  g_signal_emit_by_name (view, "loading-homepage");

  ephy_web_view_set_visit_type (view,
                                EPHY_PAGE_VISIT_HOMEPAGE);
  if (ephy_embed_shell_get_mode (ephy_embed_shell_get_default ())
      == EPHY_EMBED_SHELL_MODE_INCOGNITO)
    ephy_web_view_load_url (view, "about:incognito");
  else
    ephy_web_view_load_url (view, "about:overview");
}

/**
 * ephy_web_view_get_visit_type:
 * @view: an #EphyWebView
 *
 * Returns: the @view #EphyWebViewVisitType
 **/
EphyHistoryPageVisitType
ephy_web_view_get_visit_type (EphyWebView *view)
{
  g_return_val_if_fail (EPHY_IS_WEB_VIEW (view), EPHY_PAGE_VISIT_NONE);

  return view->priv->visit_type;
}

/**
 * ephy_web_view_set_visit_type:
 * @view: an #EphyWebView
 * @visit_type: an #EphyHistoryPageVisitType
 *
 * Sets the @visit_type for @view, so that the URI can be
 * properly weighted in the history backend.
 **/
void
ephy_web_view_set_visit_type (EphyWebView *view, EphyHistoryPageVisitType visit_type)
{
  g_return_if_fail (EPHY_IS_WEB_VIEW (view));

  view->priv->visit_type = visit_type;
}
