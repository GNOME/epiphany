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
#include "ephy-embed-single.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-embed-utils.h"
#include "ephy-embed.h"
#include "ephy-file-helpers.h"
#include "ephy-file-monitor.h"
#include "ephy-history-service.h"
#include "ephy-permission-manager.h"
#include "ephy-prefs.h"
#include "ephy-profile-utils.h"
#include "ephy-settings.h"
#include "ephy-string.h"
#include "ephy-web-app-utils.h"
#include "ephy-zoom.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gnome-keyring.h>
#include <gtk/gtk.h>
#include <libsoup/soup-gnome.h>

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

struct _EphyWebViewPrivate {
  EphyWebViewSecurityLevel security_level;
  EphyWebViewDocumentType document_type;
  EphyWebViewNavigationFlags nav_flags;
#ifndef HAVE_WEBKIT2
  WebKitLoadStatus load_status;
#endif

  /* Flags */
  guint is_blank : 1;
  guint visibility : 1;
  guint loading_homepage : 1;
  guint is_setting_zoom : 1;
#ifdef HAVE_WEBKIT2
  guint is_loading : 1;
#endif
  guint load_failed : 1;

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

  GSList *hidden_popups;
  GSList *shown_popups;

  GtkWidget *password_info_bar;

  EphyHistoryService *history_service;
  GCancellable *history_service_cancellable;

  EphyHistoryPageVisitType visit_type;

  gulong do_not_track_handler;
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
  PROP_VISIBLE,
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
  EphyEmbedSingle *single;

  /* Only show popup with non NULL url */
  if (popup->url != NULL) {
    single = EPHY_EMBED_SINGLE
             (ephy_embed_shell_get_embed_single (embed_shell));

    ephy_embed_single_open_window (single, EPHY_EMBED (view), popup->url,
                                   popup->name, popup->features);
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
  const char *location;
  EphyPermissionManager *manager;
  EphyPermission permission;

  manager = EPHY_PERMISSION_MANAGER
            (ephy_embed_shell_get_embed_single (embed_shell));
  g_return_if_fail (EPHY_IS_PERMISSION_MANAGER (manager));

  permission = allowed ? EPHY_PERMISSION_ALLOWED
               : EPHY_PERMISSION_DENIED;

  location = ephy_web_view_get_address (view);
  g_return_if_fail (location != NULL);

  ephy_permission_manager_add_permission (manager, location,
                                          EPHY_PERMISSION_TYPE_POPUP,
                                          permission);

  if (allowed) {
    popups_manager_show_all (view);
  } else {
    popups_manager_hide_all (view);
  }
}

static gboolean
ephy_web_view_get_popups_allowed (EphyWebView *view)
{
  EphyPermissionManager *permission_manager;
  EphyPermission response;
  const char *location;
  gboolean allow;

  permission_manager = EPHY_PERMISSION_MANAGER
                       (ephy_embed_shell_get_embed_single (embed_shell));
  g_return_val_if_fail (EPHY_IS_PERMISSION_MANAGER (permission_manager),
                        FALSE);

  location = ephy_web_view_get_address (view);
  if (location == NULL) return FALSE;/* FALSE, TRUE… same thing */

  response = ephy_permission_manager_test_permission
             (permission_manager, location, EPHY_PERMISSION_TYPE_POPUP);

  switch (response) {
    case EPHY_PERMISSION_ALLOWED:
      allow = TRUE;
      break;
    case EPHY_PERMISSION_DENIED:
      allow = FALSE;
      break;
    case EPHY_PERMISSION_DEFAULT:
    default:
      allow = g_settings_get_boolean (EPHY_SETTINGS_WEB,
                                      EPHY_PREFS_WEB_ENABLE_POPUPS);
      break;
  }

  LOG ("ephy_web_view_get_popups_allowed: view %p, allowed: %d", view, allow);

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
    case PROP_VISIBLE:
      g_value_set_boolean (value, priv->visibility);
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
    case PROP_VISIBLE:
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

  /* Let WebKitWebView handle this. */
  return GTK_WIDGET_CLASS (ephy_web_view_parent_class)->button_press_event (widget, event);
}

static void
ephy_web_view_dispose (GObject *object)
{
  EphyWebViewPrivate *priv = EPHY_WEB_VIEW (object)->priv;

  g_clear_object (&priv->file_monitor);

  g_clear_object (&priv->icon);

  if (priv->history_service_cancellable) {
    g_cancellable_cancel (priv->history_service_cancellable);
    g_clear_object (&priv->history_service_cancellable);
  }

  G_OBJECT_CLASS (ephy_web_view_parent_class)->dispose (object);
}

#ifdef HAVE_WEBKIT2
/* TODO: DOM bindings */
#else
typedef struct {
  WebKitDOMNode *username_node;
  WebKitDOMNode *password_node;
} FillData;

static void
fill_data_free (gpointer data)
{
  FillData *fill_data = (FillData*)data;

  g_object_unref (fill_data->username_node);
  g_object_unref (fill_data->password_node);

  g_slice_free (FillData, fill_data);
}

static void
fill_form_cb (GnomeKeyringResult retval,
              GList *results,
              gpointer user_data)
{
  FillData *fill_data = (FillData*)user_data;
  GnomeKeyringNetworkPasswordData* keyring_data;

  if (!results) {
    LOG ("No result");
    return;
  }

  /* FIXME: We use only the first result, for now; We need to do
   * something smarter here */
  keyring_data = (GnomeKeyringNetworkPasswordData*)results->data;

  if (retval != GNOME_KEYRING_RESULT_OK) {
    LOG ("Query failed.");
    return;
  }

  LOG ("Found: user %s pass (hidden)", keyring_data->user);

  g_object_set (fill_data->username_node,
                "value", keyring_data->user, NULL);
  g_object_set (fill_data->password_node,
                "value", keyring_data->password, NULL);
}

static void
find_username_and_password_elements (WebKitDOMNode *form_node,
                                     WebKitDOMNode **username_node,
                                     WebKitDOMNode **password_node)
{
  WebKitDOMHTMLCollection *elements;
  WebKitDOMHTMLFormElement *form = WEBKIT_DOM_HTML_FORM_ELEMENT (form_node);
  gulong elements_n;
  int j;

  elements = webkit_dom_html_form_element_get_elements (form);
  elements_n = webkit_dom_html_collection_get_length (elements);

  if (elements_n == 0) {
    LOG ("No elements found for this form.");
    return;
  }

  for (j = 0; j < elements_n; j++) {
    WebKitDOMNode *element;

    element = webkit_dom_html_collection_item (elements, j);

    if (WEBKIT_DOM_IS_HTML_INPUT_ELEMENT (element)) {
      char *element_type;

      g_object_get (element, "type", &element_type, NULL);

      if (g_str_equal ("text", element_type) || g_str_equal ("email", element_type)) {
        /* We found more than one inputs of type text; we won't be
         * saving here */
        if (*username_node) {
          g_object_unref (*username_node);
          *username_node = NULL;
          g_free (element_type);

          break;
        }

        *username_node = g_object_ref (element);
      }
      else if (g_str_equal ("password", element_type)) {
        if (*password_node) {
          g_object_unref (*password_node);
          *password_node = NULL;
          g_free (element_type);

          break;
        }

        *password_node = g_object_ref (element);
      }

      g_free (element_type);
    }
  }

  g_object_unref(elements);
}
#endif

typedef struct {
  EphyEmbed *embed;
  char *uri;
  char *name_field;
  char *password_field;
  char *name_value;
  char *password_value;
} StorePasswordData;

static void
store_password_data_free (gpointer data)
{
  StorePasswordData *store_data = (StorePasswordData*)data;

  g_free (store_data->uri);
  g_free (store_data->name_field);
  g_free (store_data->name_value);
  g_free (store_data->password_field);
  g_free (store_data->password_value);

  g_slice_free (StorePasswordData, store_data);
}

static void
store_password (GtkInfoBar *info_bar, gint response_id, gpointer data)
{
  StorePasswordData *store_data = (StorePasswordData*)data;
  EphyWebView *web_view = ephy_embed_get_web_view (store_data->embed);
  char *uri = store_data->uri;
  char *name_field_name = store_data->name_field;
  char *name_field_value = store_data->name_value;
  char *password_field_name = store_data->password_field;
  char *password_field_value = store_data->password_value;
  char *host;

  /* We are no longer showing a store password infobar */
  web_view->priv->password_info_bar = NULL;

  if (response_id != GTK_RESPONSE_YES) {
    LOG ("Response is %d - not saving.", response_id);
    store_password_data_free (store_data);
    gtk_widget_destroy (GTK_WIDGET (info_bar));
    return;
  }

  LOG ("Response is GTK_RESPONSE_YES - saving!");
  _ephy_profile_utils_store_form_auth_data (uri,
                                            name_field_name,
                                            password_field_name,
                                            name_field_value,
                                            password_field_value);

  /* Update internal caching */
  host = ephy_string_get_host_name (uri);

  ephy_embed_single_add_form_auth (EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell)),
                                   host,
                                   name_field_name,
                                   password_field_name,
                                   name_field_value);
  g_free (host);

  store_password_data_free (store_data);
  gtk_widget_destroy (GTK_WIDGET (info_bar));
}

static void
request_decision_on_storing (StorePasswordData *store_data)
{
  EphyEmbed *embed = store_data->embed;
  EphyWebView *web_view = ephy_embed_get_web_view (embed);
  GtkWidget *info_bar;
  GtkWidget *action_area;
  GtkWidget *content_area;
  GtkWidget *label;
  char *message;
  char *hostname;

  LOG ("Going to show infobar about %s", store_data->uri);

  info_bar = gtk_info_bar_new_with_buttons (_("Not now"), GTK_RESPONSE_NO,
                                            _("Store password"), GTK_RESPONSE_YES,
                                            NULL);

  action_area = gtk_info_bar_get_action_area (GTK_INFO_BAR (info_bar));
  gtk_orientable_set_orientation (GTK_ORIENTABLE (action_area),
                                  GTK_ORIENTATION_HORIZONTAL);

  label = gtk_label_new (NULL);
  hostname = ephy_string_get_host_name (store_data->uri);
  /* Translators: The first %s is the username and the second one is the
   * hostname where this is happening. Example: gnome@gmail.com and
   * mail.google.com.
   */
  message = g_markup_printf_escaped (_("<big>Would you like to store the password for <b>%s</b> in <b>%s</b>?</big>"),
                                     store_data->name_value,
                                     hostname);
  gtk_label_set_markup (GTK_LABEL (label), message);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  g_free (hostname);
  g_free (message);

  content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar));
  gtk_container_add (GTK_CONTAINER (content_area), label);

  gtk_widget_show_all (info_bar);

  g_signal_connect (info_bar, "response", G_CALLBACK (store_password), store_data);

  ephy_embed_add_top_widget (embed, info_bar, FALSE);

  /* We track the info_bar, so we only ever show one */
  if (web_view->priv->password_info_bar)
    gtk_widget_destroy (web_view->priv->password_info_bar);

  web_view->priv->password_info_bar = info_bar;
}

static void
should_store_cb (GnomeKeyringResult retval,
                 GList *results,
                 gpointer user_data)
{
  StorePasswordData *store_data = (StorePasswordData*)user_data;
  GnomeKeyringNetworkPasswordData* keyring_data;

  if (!results) {
    LOG ("No result on query; asking whether we should store.");
    request_decision_on_storing (store_data);
    return;
  }

  /* FIXME: We use only the first result, for now; We need to do
   * something smarter here */
  keyring_data = (GnomeKeyringNetworkPasswordData*)results->data;

  if (g_str_equal (keyring_data->user, store_data->name_value) &&
      g_str_equal (keyring_data->password, store_data->password_value)) {
    LOG ("User/password already stored. Not asking about storing.");
    store_password_data_free (store_data);
    return;
  }

  LOG ("User/password not yet stored. Asking about storing.");
  request_decision_on_storing (store_data);
}

#ifdef HAVE_WEBKIT2
/* TODO: DOM bindings */
#else
static gboolean
form_submitted_cb (WebKitDOMHTMLFormElement *dom_form,
                   WebKitDOMEvent *dom_event,
                   EphyWebView *web_view)
{
  SoupURI *uri;
  StorePasswordData *store_data;

  WebKitDOMNode *username_node = NULL;
  WebKitDOMNode *password_node = NULL;

  uri = soup_uri_new (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (web_view)));
  if (!uri)
    return TRUE;

  soup_uri_set_query (uri, NULL);

  find_username_and_password_elements (WEBKIT_DOM_NODE (dom_form),
                                       &username_node, &password_node);

  store_data = g_slice_new (StorePasswordData);

  store_data->uri = soup_uri_to_string (uri, FALSE);
  store_data->embed = EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view);

  g_object_get (username_node,
                "name", &store_data->name_field,
                "value", &store_data->name_value, NULL);

  g_object_get (password_node,
                "name", &store_data->password_field,
                "value", &store_data->password_value, NULL);

  LOG ("Form submitted! %s %s",
       store_data->name_value,
       store_data->password_value);

  _ephy_profile_utils_query_form_auth_data (store_data->uri,
                                            store_data->name_field,
                                            store_data->password_field,
                                            should_store_cb,
                                            store_data,
                                            NULL);

  soup_uri_free (uri);

  g_object_unref (username_node);
  g_object_unref (password_node);

  return TRUE;
}

static void
pre_fill_form (WebKitDOMNode *username_node,
               WebKitDOMNode *password_node,
               EphyWebView *view)
{
  GSList *p = NULL;
  GSList *l = NULL;
  SoupURI *uri = NULL;

  uri = soup_uri_new (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (view)));
  if (uri)
    l = ephy_embed_single_get_form_auth (EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell)), uri->host);

  for (p = l; p; p = p->next) {
    char *username_field_name;
    char *password_field_name;
    EphyEmbedSingleFormAuthData *data = (EphyEmbedSingleFormAuthData*)p->data;

    g_object_get (username_node,
                  "name", &username_field_name, NULL);
    g_object_get (password_node,
                  "name", &password_field_name, NULL);

    if (g_strcmp0 (username_field_name, data->form_username) == 0 &&
        g_strcmp0 (password_field_name, data->form_password) == 0) {
      FillData *fill_data = g_slice_new (FillData);
      char *uri_str = soup_uri_to_string (uri, FALSE);

      fill_data->username_node = g_object_ref (username_node);
      fill_data->password_node = g_object_ref (password_node);

      _ephy_profile_utils_query_form_auth_data (uri_str,
                                                data->form_username,
                                                data->form_password,
                                                fill_form_cb,
                                                fill_data,
                                                fill_data_free);
      g_free (uri_str);
    }
    g_free (username_field_name);
    g_free (password_field_name);
  }

  soup_uri_free (uri);
}

static void
_ephy_web_view_hook_into_forms (EphyWebView *web_view)
{
  WebKitDOMHTMLCollection *forms = NULL;
  WebKitDOMDocument *document = NULL;
  gulong forms_n;
  int i;

  document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (web_view));
  forms = webkit_dom_document_get_forms (document);
  forms_n = webkit_dom_html_collection_get_length (forms);

  if (forms_n == 0) {
    LOG ("No forms found.");
    g_object_unref(forms);
    return;
  }

  for (i = 0; i < forms_n; i++) {
    WebKitDOMNode *form;
    WebKitDOMNode *username_node = NULL;
    WebKitDOMNode *password_node = NULL;

    form = webkit_dom_html_collection_item (forms, i);
    find_username_and_password_elements (form, &username_node, &password_node);

    /* We have a field that may be the user, and one for a password. */
    if (username_node && password_node) {
      LOG ("Hooking and pre-filling a form");
      webkit_dom_event_target_add_event_listener (WEBKIT_DOM_EVENT_TARGET (form), "submit",
                                                  G_CALLBACK (form_submitted_cb), false,
                                                  web_view);

      pre_fill_form (username_node, password_node, web_view);

      g_object_unref (username_node);
      g_object_unref (password_node);
    } else
      LOG ("No pre-fillable/hookable form found");
  }

  g_object_unref(forms);
}

static void
_ephy_web_view_hook_into_links (EphyWebView *web_view)
{
  WebKitDOMNodeList *links = NULL;
  WebKitDOMDocument *document = NULL;
  gulong links_n;
  int i;

  document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (web_view));
  links = webkit_dom_document_get_elements_by_tag_name (document, "link");
  links_n = webkit_dom_node_list_get_length (links);

  if (links_n == 0) {
    LOG ("No links found.");
    g_object_unref(links);
    return;
  }

  for (i = 0; i < links_n; i++) {
    WebKitDOMNode *link;
    char *rel = NULL;

    link = webkit_dom_node_list_item (links, i);
    rel = webkit_dom_html_link_element_get_rel (WEBKIT_DOM_HTML_LINK_ELEMENT (link));

    if (g_strcmp0 (rel, "alternate") == 0) {
      char *type = NULL;
      char *title = NULL;
      char *address = NULL;
      SoupURI *feed_uri;
      SoupURI *current_uri;

      g_object_get (link,
                    "type", &type,
                    "title", &title,
                    "href", &address,
                    NULL);

      feed_uri = soup_uri_new (address);
      if (!feed_uri) {
        current_uri = soup_uri_new (ephy_web_view_get_address (web_view));
        feed_uri = soup_uri_new_with_base (current_uri, address);
        soup_uri_free (current_uri);
      }

      if (feed_uri) {
        g_free (address);
        address = soup_uri_to_string (feed_uri, FALSE);
        LOG ("Emitting ge-feed-link: %s, %s, %s", type, title, address);
        g_signal_emit_by_name (web_view, "ge-feed-link", type, title, address);
        soup_uri_free (feed_uri);
      }

      g_free (type);
      g_free (title);
      g_free (address);
    }

    g_free (rel);
  }

  g_object_unref(links);
}
#endif

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
ephy_web_view_clear_history (EphyWebView *view)
{
#ifdef HAVE_WEBKIT2
  /* TODO: WebKitBackForwardList is read-only in WebKit2 */
#else
  WebKitWebBackForwardList *history_list;

  g_return_if_fail (EPHY_IS_WEB_VIEW (view));

  history_list = webkit_web_view_get_back_forward_list (WEBKIT_WEB_VIEW (view));
  if (history_list != NULL) {
    WebKitWebHistoryItem *current_item;

    /* Save a ref to the first element to add it later */
    current_item = webkit_web_back_forward_list_get_current_item (history_list);
    g_object_ref (current_item);

    /* Clear the history and add the first element once again */
    webkit_web_back_forward_list_clear (history_list);
    webkit_web_back_forward_list_add_item (history_list, current_item);
    g_object_unref (current_item);

    update_navigation_flags (view);
  }
#endif
}

static void
ephy_web_view_history_cleared_cb (EphyHistoryService *history_service,
                                  EphyWebView *view)
{
  ephy_web_view_clear_history (view);
}

static void
ephy_web_view_finalize (GObject *object)
{
  EphyWebViewPrivate *priv = EPHY_WEB_VIEW (object)->priv;

  g_signal_handlers_disconnect_by_func (priv->history_service,
                                        ephy_web_view_history_cleared_cb,
                                        EPHY_WEB_VIEW (object));

  if (priv->non_search_regex != NULL) {
    g_regex_unref (priv->non_search_regex);
    priv->non_search_regex = NULL;
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
  else if (!strcmp (address, EPHY_ABOUT_SCHEME":plugins"))
    return g_strdup (_("Plugins"));
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

  if (!priv->is_blank && (title == NULL || g_strstrip (title)[0] == '\0')) {
    g_free (title);
    title = get_title_from_address (priv->address);

    /* Fallback */
    if (title == NULL || title[0] == '\0') {
      g_free (title);
      title = g_strdup (EMPTY_PAGE);
      _ephy_web_view_set_is_blank (view, TRUE);
    }
  } else if (priv->is_blank) {
    g_free (title);
    title = g_strdup (EMPTY_PAGE);
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
  EphyHistoryService *history = EPHY_WEB_VIEW (web_view)->priv->history_service;
#ifndef HAVE_WEBKIT2
  WebKitWebFrame *frame;

  frame = webkit_web_view_get_main_frame (web_view);
  uri = webkit_web_frame_get_uri (frame);
#else
  uri = webkit_web_view_get_uri (web_view);
#endif

  g_object_get (web_view, "title", &title, NULL);

  ephy_web_view_set_title (EPHY_WEB_VIEW (web_view), title);
  
  if (!title && uri)
    title = get_title_from_address (uri);

  if (uri && title)
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

  g_object_get (web_view, "uri", &uri, NULL);
  current_address = ephy_web_view_get_address (EPHY_WEB_VIEW (web_view));

#ifdef HAVE_WEBKIT2
  if (!EPHY_WEB_VIEW (web_view)->priv->is_loading)
    return;
#endif

  /* We need to check if we get URI notifications without going
     through the usual load process, as this can happen when changing
     location within a page */
  if (g_str_equal (uri, current_address) == FALSE)
    ephy_web_view_set_address (EPHY_WEB_VIEW (web_view), uri);

  g_free (uri);
}

#ifdef HAVE_WEBKIT2
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
#else
static void
hovering_over_link_cb (EphyWebView *web_view,
                       char *title,
                       char *location,
                       gpointer data)
{
  ephy_web_view_set_link_message (web_view, location);
}
#endif

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
#ifndef HAVE_WEBKIT2
  /* This is the default behaviour in WebKit2 */
  webkit_web_view_set_full_content_zoom (WEBKIT_WEB_VIEW (object), TRUE);
#endif
}

static void
impl_loading_homepage (EphyWebView *view)
{
  view->priv->loading_homepage = TRUE;
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
 * EphyWebView:visibility:
 *
 *
 **/
  g_object_class_install_property (gobject_class,
                                   PROP_VISIBLE,
                                   g_param_spec_boolean ("visibility",
                                                         "Visibility",
                                                         "The view's visibility",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

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
 * The ::ge_rss signal is emitted when @embed discovers that a search
 * description is available for the site it is visiting.
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
 * The ::ge_rss signal is emitted when @embed discovers that a news feed
 * is available for the site it is visiting.
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

  g_type_class_add_private (gobject_class, sizeof (EphyWebViewPrivate));
}

#ifdef HAVE_WEBKIT2
/* TODO: Favicons */
#else
static void
_ephy_web_view_load_icon (EphyWebView *view)
{
  EphyWebViewPrivate *priv = view->priv;
  const char* uri;

  if (priv->icon != NULL)
    return;

  uri = webkit_web_view_get_uri (WEBKIT_WEB_VIEW (view));
  priv->icon = webkit_favicon_database_try_get_favicon_pixbuf (webkit_get_favicon_database (), uri,
                                                               FAVICON_SIZE, FAVICON_SIZE);

  g_object_notify (G_OBJECT (view), "icon");
}

static void
_ephy_web_view_set_icon_address (EphyWebView *view,
                                 const char *icon_address)
{
  GObject *object = G_OBJECT (view);
  EphyWebViewPrivate *priv = view->priv;

  if (priv->icon != NULL) {
    g_object_unref (priv->icon);
    priv->icon = NULL;

    g_object_notify (object, "icon");
  }

  if (icon_address && priv->address)
    _ephy_web_view_load_icon (view);
}

static void
favicon_cb (EphyWebView *view,
            const char *address,
            gpointer user_data)
{
  _ephy_web_view_set_icon_address (view, address);
}
#endif

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

#ifdef HAVE_WEBKIT2
static gboolean
decide_policy_cb (WebKitWebView *web_view,
                  WebKitPolicyDecision *decision,
                  WebKitPolicyDecisionType decision_type,
                  gpointer user_data)
{
  WebKitResponsePolicyDecision *response_decision;
  WebKitURIResponse *response;
  WebKitURIRequest *request;
  EphyWebViewDocumentType type;
  GObject *single;
  const char *mime_type;
  const char *uri;
  gboolean handled = FALSE;

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
  if (webkit_web_view_can_show_mime_type (web_view, mime_type))
    return FALSE;

  /* TODO: Check also Content-Disposition header before emitting
   * handle-content signal. We need API for that in WebKit2.
   */
  single = ephy_embed_shell_get_embed_single (embed_shell);
  request = webkit_response_policy_decision_get_request (response_decision);
  uri = webkit_uri_request_get_uri (request);
  g_signal_emit_by_name (single, "handle-content", mime_type, uri, &handled);

  if (handled)
    webkit_policy_decision_ignore (decision);
  else
    webkit_policy_decision_download (decision);

  return TRUE;
}
#else
static gboolean
mime_type_policy_decision_requested_cb (WebKitWebView *web_view,
                                        WebKitWebFrame *frame,
                                        WebKitNetworkRequest *request,
                                        const char *mime_type,
                                        WebKitWebPolicyDecision *decision,
                                        gpointer user_data)
{
  EphyWebViewDocumentType type;
  gboolean should_download;

  g_return_val_if_fail (mime_type, FALSE);

  /* Get the mime type for the page only from the main frame */
  if (webkit_web_view_get_main_frame (web_view) == frame) {
    type = EPHY_WEB_VIEW_DOCUMENT_OTHER;

    if (!strcmp (mime_type, "text/html") ||
        !strcmp (mime_type, "text/plain"))
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
  }

  /* If WebKit can't handle the mime type start the download
     process */
  should_download = !webkit_web_view_can_show_mime_type (web_view, mime_type);

  /* Make sure we respect the Content-Disposition header */
  if (!should_download) {
    WebKitNetworkResponse *response = webkit_web_frame_get_network_response (frame);
    SoupMessage *message = NULL;

    if (response) {
      message = webkit_network_response_get_message (response);
    }

    if (message) {
      char *disposition = NULL;

      soup_message_headers_get_content_disposition (message->response_headers,
                                                    &disposition,
                                                    NULL);

      if (disposition) {
        should_download = g_str_equal (disposition, "attachment");
        g_free (disposition);
      }
    }

    g_object_unref (response);
  }

  /* FIXME: need to use ephy_file_check_mime if auto-downloading */
  if (should_download) {
    GObject *single;
    const char *uri;
    gboolean handled = FALSE;

    single = ephy_embed_shell_get_embed_single (embed_shell);
    uri = webkit_network_request_get_uri (request);
    g_signal_emit_by_name (single, "handle-content", mime_type, uri, &handled);

    if (handled)
      webkit_web_policy_decision_ignore (decision);
    else
      webkit_web_policy_decision_download (decision);

    return TRUE;
  }

  return FALSE;
}
#endif

#ifdef HAVE_WEBKIT2
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
#else
static void
decide_on_geolocation_policy_request (GtkWidget *info_bar,
                                      int response,
                                      WebKitGeolocationPolicyDecision *decision)
{
  gtk_widget_destroy (info_bar);

  /* Decide, and drop our ref on the decision object. */
  if (response == GTK_RESPONSE_YES) {
    webkit_geolocation_policy_allow (decision);
    g_object_unref (decision);
    return;
  }

  webkit_geolocation_policy_deny (decision);
  g_object_unref (decision);
}
#endif

#ifdef HAVE_WEBKIT2
static gboolean
permission_request_cb (WebKitWebView           *web_view,
                       WebKitPermissionRequest *decision)
#else
static gboolean
geolocation_policy_decision_requested_cb (WebKitWebView *web_view,
                                          WebKitWebFrame *web_frame,
                                          WebKitGeolocationPolicyDecision *decision,
                                          gpointer data)
#endif
{
  GtkWidget *info_bar;
  GtkWidget *action_area;
  GtkWidget *content_area;
  GtkWidget *label;
  char *message;
  char *host;

#ifdef HAVE_WEBKIT2
  if (!WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST (decision))
    return FALSE;
#endif

  info_bar = gtk_info_bar_new_with_buttons (_("Deny"), GTK_RESPONSE_NO,
                                            _("Allow"), GTK_RESPONSE_YES,
                                            NULL);

  action_area = gtk_info_bar_get_action_area (GTK_INFO_BAR (info_bar));
  /* Translators: Geolocation policy for a specific site. */
  gtk_orientable_set_orientation (GTK_ORIENTABLE (action_area),
                                  GTK_ORIENTATION_HORIZONTAL);

  /* Label */
#ifdef HAVE_WEBKIT2
  host = ephy_string_get_host_name (webkit_web_view_get_uri (web_view));
#else
  host = ephy_string_get_host_name (webkit_web_frame_get_uri (web_frame));
#endif
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

#ifdef HAVE_WEBKIT2
/* TODO: DOM Bindings */
#else
static gboolean
delete_web_app_cb (WebKitDOMHTMLElement *button,
                   WebKitDOMEvent *dom_event,
                   EphyWebView *web_view)
{
  char *id = NULL;

  id = webkit_dom_html_element_get_id (button);
  if (id)
    ephy_web_application_delete (id);

  g_free (id);

  return FALSE;
}
#endif

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

  /* Do not expose about:blank to the user, an empty address
     bar will do better */
  if (location == NULL || location[0] == '\0' ||
      strcmp (location, "about:blank") == 0) {
    ephy_web_view_set_address (view, NULL);
    ephy_web_view_set_title (view, EMPTY_PAGE);
  } else if (g_str_has_prefix (location, EPHY_ABOUT_SCHEME)) {
    char *new_address = g_strdup_printf ("about:%s", location + EPHY_ABOUT_SCHEME_LEN + 1);
    ephy_web_view_set_address (view, new_address);
    g_free (new_address);
  } else {
    /* We do this to get rid of an eventual password in the URL. */
    ephy_web_view_set_address (view, location);
    ephy_web_view_set_loading_title (view, location, TRUE);
  }

  ephy_web_view_set_link_message (view, NULL);
#ifdef HAVE_WEBKIT2
  /* TODO: Favicons */
#else
  _ephy_web_view_set_icon_address (view, NULL);
#endif

  update_navigation_flags (view);

  g_object_notify (object, "embed-title");

  g_object_thaw_notify (object);
}

#ifdef HAVE_WEBKIT2
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

    priv->is_loading = TRUE;
    priv->load_failed = FALSE;

    loading_uri = webkit_web_view_get_uri (web_view);
    g_signal_emit_by_name (view, "new-document-now", loading_uri);

    if (priv->address == NULL || priv->address[0] == '\0')
      ephy_web_view_set_address (view, loading_uri);

    ephy_web_view_set_loading_title (view, loading_uri, TRUE);

    g_free (priv->status_message);
    priv->status_message = g_strdup (priv->loading_title);
    g_object_notify (object, "status-message");

    break;
  }
  case WEBKIT_LOAD_REDIRECTED:
    /* TODO: Update the loading uri */
    break;
  case WEBKIT_LOAD_COMMITTED: {
    const char* uri;
    EphyWebViewSecurityLevel security_level = EPHY_WEB_VIEW_STATE_IS_UNKNOWN;

    /* Title and location. */
    uri = webkit_web_view_get_uri (web_view);
    ephy_web_view_location_changed (view, uri);

    /* Security status. */
    if (uri && g_str_has_prefix (uri, "https")) {
      WebKitWebResource *resource;
      WebKitURIResponse *response;
      GTlsCertificateFlags tls_errors = 0;
      gboolean has_certificate;

      resource = webkit_web_view_get_main_resource (web_view);
      response = webkit_web_resource_get_response (resource);
      has_certificate = webkit_uri_response_get_https_status (response, NULL, &tls_errors);
      if (has_certificate && tls_errors == 0)
        security_level = EPHY_WEB_VIEW_STATE_IS_SECURE_HIGH;
      else
        security_level = EPHY_WEB_VIEW_STATE_IS_BROKEN;
    }

    ephy_web_view_set_security_level (EPHY_WEB_VIEW (web_view), security_level);

    /* Zoom level. */
    restore_zoom_level (view, uri);

    /* History. */
    if (!ephy_web_view_is_loading_homepage (view)) {
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
  case WEBKIT_LOAD_FINISHED: {
    SoupURI *uri;

    priv->is_loading = FALSE;
    priv->loading_homepage = FALSE;

    g_free (priv->status_message);
    priv->status_message = NULL;
    g_object_notify (object, "status-message");
    ephy_web_view_set_loading_title (view, NULL, FALSE);

    if (priv->is_blank)
      g_object_notify (object, "embed-title");

#if 0
    /* TODO: DOM bindings */
    if (ephy_embed_shell_get_mode (embed_shell) != EPHY_EMBED_SHELL_MODE_PRIVATE &&
        g_settings_get_boolean (EPHY_SETTINGS_MAIN,
                                EPHY_PREFS_REMEMBER_PASSWORDS))
      _ephy_web_view_hook_into_forms (view);

    _ephy_web_view_hook_into_links (view);
#endif

    /* FIXME: It sucks to do this here, but it's not really possible
     * to hook the DOM actions nicely in the about: generator. */
    uri = soup_uri_new (webkit_web_view_get_uri (web_view));
    if (uri &&
        !g_strcmp0 (uri->scheme, "ephy-about") &&
        !g_strcmp0 (uri->path, "applications")) {
#if 0
      /* TODO: DOM bindings */
      WebKitDOMDocument *document;
      WebKitDOMNodeList *buttons;
      gulong buttons_n;
      int i;

      document = webkit_web_view_get_dom_document (web_view);
      buttons = webkit_dom_document_get_elements_by_tag_name (document, "input");
      buttons_n = webkit_dom_node_list_get_length (buttons);

      for (i = 0; i < buttons_n; i++) {
        WebKitDOMNode *button;

        button = webkit_dom_node_list_item (buttons, i);
        webkit_dom_event_target_add_event_listener (WEBKIT_DOM_EVENT_TARGET (button), "click",
                                                    G_CALLBACK (delete_web_app_cb), false,
                                                    NULL);
      }
#endif
    }

    if (uri)
      soup_uri_free (uri);

    /* Reset visit type. */
    priv->visit_type = EPHY_PAGE_VISIT_NONE;

    break;
  }
  }

  g_object_thaw_notify (object);
}
#else
static void
load_status_cb (WebKitWebView *web_view,
                GParamSpec *pspec,
                gpointer user_data)
{
  WebKitLoadStatus status = webkit_web_view_get_load_status (web_view);
  EphyWebView *view = EPHY_WEB_VIEW (web_view);
  EphyWebViewPrivate *priv = view->priv;
  GObject *object = G_OBJECT (web_view);

  g_object_freeze_notify (object);

  switch (status) {
  /* FIXME: add REDIRECTING and NEGOTIATING states to WebKitGTK */
  case WEBKIT_LOAD_PROVISIONAL: {
    const char *loading_uri = NULL;
    WebKitWebFrame *frame;

    WebKitWebDataSource *source;
    WebKitNetworkRequest *request;

    priv->load_failed = FALSE;
    frame = webkit_web_view_get_main_frame (web_view);

    source = webkit_web_frame_get_provisional_data_source (frame);
    request = webkit_web_data_source_get_initial_request (source);
    loading_uri = webkit_network_request_get_uri (request);

    g_signal_emit_by_name (view, "new-document-now", loading_uri);

    if (priv->address == NULL || priv->address[0] == '\0')
      ephy_web_view_set_address (view, loading_uri);

    ephy_web_view_set_loading_title (view, loading_uri, TRUE);
    
    g_free (priv->status_message);
    priv->status_message = g_strdup (priv->loading_title);
    g_object_notify (object, "status-message");
    
    break;
  }
  case WEBKIT_LOAD_COMMITTED: {
    const char* uri;
    EphyWebViewSecurityLevel security_level;

    /* Title and location. */
    uri = webkit_web_view_get_uri (web_view);
    ephy_web_view_location_changed (view,
                                    uri);

    /* Security status. */
    if (uri && g_str_has_prefix (uri, "https")) {
      WebKitWebFrame *frame;
      WebKitWebDataSource *source;
      WebKitNetworkRequest *request;
      SoupMessage *message;

      frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW(view));
      source = webkit_web_frame_get_data_source (frame);
      request = webkit_web_data_source_get_request (source);
      message = webkit_network_request_get_message (request);

      if (message &&
          (soup_message_get_flags (message) & SOUP_MESSAGE_CERTIFICATE_TRUSTED))
        security_level = EPHY_WEB_VIEW_STATE_IS_SECURE_HIGH;
      else
        security_level = EPHY_WEB_VIEW_STATE_IS_BROKEN;
    } else
      security_level = EPHY_WEB_VIEW_STATE_IS_UNKNOWN;

    ephy_web_view_set_security_level (EPHY_WEB_VIEW (web_view), security_level);

    /* Zoom level. */
    restore_zoom_level (view, uri);

    /* History. */
    if (!ephy_web_view_is_loading_homepage (view)) {
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
  case WEBKIT_LOAD_FINISHED: {
    SoupURI *uri;

    priv->loading_homepage = FALSE;

    g_free (priv->status_message);
    priv->status_message = NULL;
    g_object_notify (object, "status-message");
    ephy_web_view_set_loading_title (view, NULL, FALSE);

    if (priv->is_blank)
      g_object_notify (object, "embed-title");

    if (ephy_embed_shell_get_mode (embed_shell) != EPHY_EMBED_SHELL_MODE_PRIVATE &&
        g_settings_get_boolean (EPHY_SETTINGS_MAIN,
                                EPHY_PREFS_REMEMBER_PASSWORDS))
      _ephy_web_view_hook_into_forms (view);

    _ephy_web_view_hook_into_links (view);

    /* FIXME: It sucks to do this here, but it's not really possible
     * to hook the DOM actions nicely in the about: generator. */
    uri = soup_uri_new (webkit_web_view_get_uri (web_view));
    if (uri &&
        !g_strcmp0 (uri->scheme, "ephy-about") &&
        !g_strcmp0 (uri->path, "applications")) {
      WebKitDOMDocument *document;
      WebKitDOMNodeList *buttons;
      gulong buttons_n;
      int i;

      document = webkit_web_view_get_dom_document (web_view);
      buttons = webkit_dom_document_get_elements_by_tag_name (document, "input");
      buttons_n = webkit_dom_node_list_get_length (buttons);

      for (i = 0; i < buttons_n; i++) {
        WebKitDOMNode *button;

        button = webkit_dom_node_list_item (buttons, i);
        webkit_dom_event_target_add_event_listener (WEBKIT_DOM_EVENT_TARGET (button), "click",
                                                    G_CALLBACK (delete_web_app_cb), false,
                                                    NULL);
      }
    }

    if (uri)
      soup_uri_free (uri);

    /* Reset visit type. */
    priv->visit_type = EPHY_PAGE_VISIT_NONE;

    break;
  }
  case WEBKIT_LOAD_FAILED:
    priv->load_failed = TRUE;
    ephy_web_view_set_link_message (view, NULL);
    ephy_web_view_set_loading_title (view, NULL, FALSE);

    g_free (priv->status_message);
    priv->status_message = NULL;
    g_object_notify (object, "status-message");

    update_navigation_flags (view);

    /* Reset visit type. */
    priv->visit_type = EPHY_PAGE_VISIT_NONE;
    break;
  default:
    break;
  }

  g_object_thaw_notify (object);
}
#endif

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
  const char *html_file;
  const char *stock_icon;

  GtkIconInfo *icon_info;
  char *image_data;

  char *template;

  if (error)
    reason = error->message;
  else
    reason = _("None specified");

  hostname = ephy_string_get_host_name (uri);

  lang = g_strdup (pango_language_to_string (gtk_get_default_language ()));
  g_strdelimit (lang, "_-@", '\0');

  switch (page) {
    case EPHY_WEB_VIEW_ERROR_PAGE_NETWORK_ERROR:
      page_title = g_strdup_printf (_("Oops! Error loading %s"), hostname);

      msg_title = g_strdup (_("Oops! It was not possible to show this website"));
      msg = g_strdup_printf (_("<p>The website at <strong>%s</strong> seems "
                               "to be unavailable. The precise error was:</p>"
                               "<p><em>%s</em></p>"
                               "<p>It could be "
                               "temporarily switched off or moved to a new "
                               "address. Don't forget to check that your "
                               "internet connection is working correctly.</p>"),
                             uri, reason);

      button_label = g_strdup (_("Try again"));

      html_file = ephy_file ("error.html");
      stock_icon = "dialog-error";
      break;
    case EPHY_WEB_VIEW_ERROR_PAGE_CRASH:
      page_title = g_strdup_printf (_("Oops! Error loading %s"), hostname);

      msg_title = g_strdup (_("Oops! This site might have caused the web "
                              "browser to close unexpectedly"));
      msg = g_strdup_printf (_("<p>This page was loading when the web browser "
                               "closed unexpectedly.</p>"
                               "<p>This might happen again if you "
                               "reload the page. If it does, "
                               "please report the problem to the "
                               "<strong>%s</strong> developers.</p>"),
                             LSB_DISTRIBUTOR);

      button_label = g_strdup (_("Load again anyway"));

      html_file = ephy_file ("recovery.html");
      stock_icon = "dialog-information";
      break;
    default:
      return;
      break;
  }
  g_free (hostname);

  icon_info = gtk_icon_theme_lookup_icon (gtk_icon_theme_get_default (),
                                          stock_icon,
                                          48,
                                          GTK_ICON_LOOKUP_GENERIC_FALLBACK);

  image_data = icon_info ? ephy_file_create_data_uri_for_filename (gtk_icon_info_get_filename (icon_info), NULL) : NULL;

  g_file_get_contents (html_file, &template, NULL, NULL);

  ephy_web_view_set_title (view, page_title);
#ifdef HAVE_WEBKIT2
  /* TODO: Favicons */
#else
  _ephy_web_view_set_icon_address (view, NULL);
#endif

  g_string_printf (html, template,
                   lang, lang,
                   ((gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL) ? "rtl" : "ltr"),
                   page_title,
                   uri,
                   image_data ? image_data : "",
                   msg_title, msg, button_label);

  g_free (template);
  g_free (lang);
  g_free (page_title);
  g_free (msg_title);
  g_free (msg);
  g_free (button_label);
  g_free (image_data);

#ifdef HAVE_WEBKIT2
  webkit_web_view_replace_content (WEBKIT_WEB_VIEW (view), html->str, uri, 0);
#else
  webkit_web_view_load_string (WEBKIT_WEB_VIEW (view),
                               html->str, "text/html", "utf8", uri);
#endif
  g_string_free (html, TRUE);
}

#ifdef HAVE_WEBKIT2
static gboolean
load_failed_cb (WebKitWebView *web_view,
                WebKitLoadEvent load_event,
                const char *uri,
                GError *error,
                gpointer user_data)
#else
static gboolean
load_error_cb (WebKitWebView *web_view,
               WebKitWebFrame *frame,
               char *uri,
               GError *error,
               gpointer user_data)
#endif
{
  EphyWebView *view = EPHY_WEB_VIEW (web_view);

#ifdef HAVE_WEBKIT2
  view->priv->load_failed = TRUE;
  ephy_web_view_set_link_message (view, NULL);
  update_navigation_flags (view);
#else
  if (webkit_web_view_get_main_frame (web_view) != frame)
    return FALSE;
#endif

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
#ifdef HAVE_WEBKIT2
  case WEBKIT_POLICY_ERROR_CANNOT_SHOW_URI:
#else
  case WEBKIT_POLICY_ERROR_CANNOT_SHOW_URL:
#endif
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
      EphyWebViewPrivate *priv = view->priv;

      if (!priv->typed_address) {
        const char* prev_uri;

        prev_uri = webkit_web_view_get_uri (web_view);
        ephy_web_view_set_address (view, prev_uri);
      }
    }
    break;
  /* In case we are downloading something or the resource is going to
   * be showed with a plugin just let WebKit do it */
  case WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD:
  case WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE:
  default:
    break;
  }

  return FALSE;
}

#ifdef HAVE_WEBKIT2
static void
close_web_view_cb (WebKitWebView *web_view,
                   gpointer user_data)

#else
static gboolean
close_web_view_cb (WebKitWebView *web_view,
                   gpointer user_data)
#endif
{
  GtkWidget *widget = gtk_widget_get_toplevel (GTK_WIDGET (web_view));

  LOG ("close web view");

  if (EPHY_IS_EMBED_CONTAINER (widget))
    ephy_embed_container_remove_child (EPHY_EMBED_CONTAINER (widget),
                                       EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view));
  else
    gtk_widget_destroy (widget);

#ifndef HAVE_WEBKIT2
  return TRUE;
#endif
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

#ifdef HAVE_WEBKIT2
/* TODO: WebKitWebResource::send-request */
#else
static void
add_do_not_track_header_cb (WebKitWebView *view, WebKitWebFrame *frame,
                            WebKitWebResource *resource, WebKitNetworkRequest *request,
                            WebKitNetworkResponse *response, gpointer user_data)
{
  SoupMessage *message;

  message = webkit_network_request_get_message (request);

  if (!message)
    return;

  /* Do Not Track header. '1' means 'opt-out'. See:
   * http://tools.ietf.org/id/draft-mayer-do-not-track-00.txt */
  soup_message_headers_append (message->request_headers, "DNT", "1");
}

static void
do_not_track_setting_changed_cb (GSettings *settings,
                                 char *key,
                                 EphyWebView *view)
{
  gboolean do_not_track;
  EphyWebViewPrivate *priv = view->priv;

  do_not_track = g_settings_get_boolean (EPHY_SETTINGS_WEB,
                                         EPHY_PREFS_WEB_DO_NOT_TRACK);

  if (do_not_track && !priv->do_not_track_handler)
    priv->do_not_track_handler = g_signal_connect (view, "resource-request-starting",
                                                   G_CALLBACK (add_do_not_track_header_cb), NULL);
  else {
    g_signal_handlers_disconnect_by_func (view, add_do_not_track_header_cb, NULL);
    priv->do_not_track_handler = 0;
  }
}
#endif

static void
ephy_web_view_init (EphyWebView *web_view)
{
  EphyWebViewPrivate *priv;

  priv = web_view->priv = EPHY_WEB_VIEW_GET_PRIVATE (web_view);

  priv->is_blank = TRUE;
#ifndef HAVE_WEBKIT2
  priv->load_status = WEBKIT_LOAD_PROVISIONAL;
#endif
  priv->title = g_strdup (EMPTY_PAGE);
  priv->document_type = EPHY_WEB_VIEW_DOCUMENT_HTML;
  priv->security_level = EPHY_WEB_VIEW_STATE_IS_UNKNOWN;

  priv->file_monitor = ephy_file_monitor_new (web_view);

  priv->non_search_regex = g_regex_new (EPHY_WEB_VIEW_NON_SEARCH_REGEX,
                                        G_REGEX_OPTIMIZE, G_REGEX_MATCH_NOTEMPTY, NULL);

  priv->history_service = EPHY_HISTORY_SERVICE (ephy_embed_shell_get_global_history_service (embed_shell));
  priv->history_service_cancellable = g_cancellable_new ();

  g_signal_connect (priv->history_service,
                    "cleared", G_CALLBACK (ephy_web_view_history_cleared_cb),
                    web_view);

#ifdef HAVE_WEBKIT2
  g_signal_connect (web_view, "decide-policy",
                    G_CALLBACK (decide_policy_cb),
                    NULL);
#else
  g_signal_connect (web_view, "mime-type-policy-decision-requested",
                    G_CALLBACK (mime_type_policy_decision_requested_cb),
                    NULL);
#endif

#ifdef HAVE_WEBKIT2
  g_signal_connect (web_view, "permission-request",
                    G_CALLBACK (permission_request_cb),
                    NULL);
#else
  g_signal_connect (web_view, "geolocation-policy-decision-requested",
                    G_CALLBACK (geolocation_policy_decision_requested_cb),
                    NULL);
#endif

#ifdef HAVE_WEBKIT2
  g_signal_connect (web_view, "load-changed",
                    G_CALLBACK (load_changed_cb),
                    NULL);
#else
  g_signal_connect (web_view, "notify::load-status",
                    G_CALLBACK (load_status_cb),
                    NULL);
#endif

#ifdef HAVE_WEBKIT2
  g_signal_connect (web_view, "close",
                    G_CALLBACK (close_web_view_cb),
                    NULL);
#else
  g_signal_connect (web_view, "close-web-view",
                    G_CALLBACK (close_web_view_cb),
                    NULL);
#endif

#ifdef HAVE_WEBKIT2
  g_signal_connect (web_view, "load-failed",
                    G_CALLBACK (load_failed_cb),
                    NULL);
#else
  g_signal_connect (web_view, "load-error",
                    G_CALLBACK (load_error_cb),
                    NULL);
#endif

  g_signal_connect (web_view, "notify::zoom-level",
                    G_CALLBACK (zoom_changed_cb),
                    NULL);

  g_signal_connect (web_view, "notify::title",
                    G_CALLBACK (title_changed_cb),
                    NULL);

  g_signal_connect (web_view, "notify::uri",
                    G_CALLBACK (uri_changed_cb),
                    NULL);

#ifdef HAVE_WEBKIT2
  g_signal_connect (web_view, "mouse-target-changed",
                    G_CALLBACK (mouse_target_changed_cb),
                    NULL);
#else
  g_signal_connect (web_view, "hovering-over-link",
                    G_CALLBACK (hovering_over_link_cb),
                    NULL);
#endif

#ifdef HAVE_WEBKIT2
  /* TODO: Favicons */
#else
  g_signal_connect (web_view, "icon-loaded",
                    G_CALLBACK (favicon_cb),
                    NULL);
#endif

  g_signal_connect (web_view, "new-window",
                    G_CALLBACK (new_window_cb),
                    NULL);

  g_signal_connect (web_view, "ge_popup_blocked",
                    G_CALLBACK (ge_popup_blocked_cb),
                    NULL);
#ifdef HAVE_WEBKIT2
  /* TODO: WebKitWebResource::send-request */
#else
  if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_DO_NOT_TRACK))
    priv->do_not_track_handler = g_signal_connect (web_view, "resource-request-starting",
                                                   G_CALLBACK (add_do_not_track_header_cb), NULL);

  g_signal_connect (EPHY_SETTINGS_WEB,
                    "changed::" EPHY_PREFS_WEB_DO_NOT_TRACK,
                    G_CALLBACK (do_not_track_setting_changed_cb), web_view);
#endif
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
  return GTK_WIDGET (g_object_new (EPHY_TYPE_WEB_VIEW, NULL));
}

/*
 * Returns a normalized representation of @url, or an autosearch string
 * for it if it has no scheme.
 *
 * Returns: the normalized @url or autosearch string
 */
static char*
normalize_or_autosearch_url (EphyWebView *view, const char *url)
{
  char *effective_url;
  char *scheme;
  EphyWebViewPrivate *priv = view->priv;

  scheme = g_uri_parse_scheme (url);

  /* If the string doesn't look like an URI, let's search it; */
  if (!ephy_embed_utils_address_has_web_scheme (url) &&
      scheme == NULL &&
      !ephy_embed_utils_address_is_existing_absolute_filename (url) &&
      priv->non_search_regex &&
      !g_regex_match (priv->non_search_regex, url, 0, NULL)) {
    char *query_param, *url_search;

    url_search = g_settings_get_string (EPHY_SETTINGS_MAIN,
                                        EPHY_PREFS_KEYWORD_SEARCH_URL);

    if (url_search == NULL || url_search[0] == '\0') {
      g_free (url_search);

      url_search = g_strdup (_("http://www.google.com/search?q=%s&ie=UTF-8&oe=UTF-8"));
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
#ifdef HAVE_WEBKIT2
void
ephy_web_view_load_request (EphyWebView *view,
                            WebKitURIRequest *request)
#else
void
ephy_web_view_load_request (EphyWebView *view,
                            WebKitNetworkRequest *request)
#endif
{
#ifndef HAVE_WEBKIT2
  WebKitWebFrame *main_frame;
#endif
  const char *url;
  char *effective_url;

  g_return_if_fail (EPHY_IS_WEB_VIEW(view));
#ifdef HAVE_WEBKIT2
  g_return_if_fail (WEBKIT_IS_URI_REQUEST(request));

  url = webkit_uri_request_get_uri (request);
  effective_url = normalize_or_autosearch_url (view, url);

  // TODO: webkit_uri_request_set_uri?
  webkit_web_view_load_uri (WEBKIT_WEB_VIEW(view), effective_url);
  g_free (effective_url);
#else
  g_return_if_fail (WEBKIT_IS_NETWORK_REQUEST(request));

  url = webkit_network_request_get_uri (request);
  effective_url = normalize_or_autosearch_url (view, url);
  webkit_network_request_set_uri (request, effective_url);
  g_free (effective_url);

  main_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW(view));
  webkit_web_frame_load_request(main_frame, request);
#endif
}

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

  effective_url = normalize_or_autosearch_url (view, url);

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
#ifdef HAVE_WEBKIT2
    /* TODO: Network features */
#else
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
#ifdef HAVE_WEBKIT2
    webkit_web_view_run_javascript (WEBKIT_WEB_VIEW (view), decoded_url, NULL, NULL, NULL);
#else
    webkit_web_view_execute_script (WEBKIT_WEB_VIEW (view), decoded_url);
#endif
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
#ifdef HAVE_WEBKIT2
  /* TODO: BackForwardList */
#else
  WebKitWebView *source_view, *dest_view;
  WebKitWebBackForwardList* source_bflist, *dest_bflist;
  WebKitWebHistoryItem *item;
  GList *items, *i;

  g_return_if_fail(EPHY_IS_WEB_VIEW(source));
  g_return_if_fail(EPHY_IS_WEB_VIEW(dest));

  source_view = WEBKIT_WEB_VIEW (source);
  dest_view = WEBKIT_WEB_VIEW (dest);

  source_bflist = webkit_web_view_get_back_forward_list (source_view);
  dest_bflist = webkit_web_view_get_back_forward_list (dest_view);

  items = webkit_web_back_forward_list_get_back_list_with_limit (source_bflist, EPHY_WEBKIT_BACK_FORWARD_LIMIT);
  /* We want to add the items in the reverse order here, so the
     history ends up the same */
  items = g_list_reverse (items);
  for (i = items; i; i = i->next) {
    item = webkit_web_history_item_copy ((WebKitWebHistoryItem*)i->data);
    webkit_web_back_forward_list_add_item (dest_bflist, item);
    g_object_unref (item);
  }
  g_list_free (items);

  /* The ephy/gecko behavior is to add the current item of the source
     embed at the end of the back history, so keep doing that */
  item = webkit_web_back_forward_list_get_current_item (source_bflist);
  if (item)
    webkit_web_back_forward_list_add_item (dest_bflist, item);
#endif
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

  if (is_address) {
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
#ifdef HAVE_WEBKIT2
  return view->priv->is_loading;
#else
  WebKitLoadStatus status;

  status = webkit_web_view_get_load_status (WEBKIT_WEB_VIEW (view));

  /* FIRST_VISUALLY_NON_EMPTY_LAYOUT might be emitted after
   * LOAD_FINISHED or LOAD_FAILED. We just ignore any status other
   * than WEBKIT_LOAD_PROVISIONAL once LOAD_FINISHED or LOAD_FAILED
   * have been set, as WEBKIT_LOAD_PROVISIONAL probably means that
   * webview has started a new load.
   */
  if ((view->priv->load_status == WEBKIT_LOAD_FINISHED ||
       view->priv->load_status == WEBKIT_LOAD_FAILED) &&
      status != WEBKIT_LOAD_PROVISIONAL)
    return FALSE;

  view->priv->load_status = status;

  return status != WEBKIT_LOAD_FINISHED && status != WEBKIT_LOAD_FAILED;
#endif
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
  EphyWebViewPrivate *priv = view->priv;

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
 * ephy_web_view_get_visibility:
 * @view: an #EphyWebView
 *
 * Returns whether the @view's toplevel is visible or not. Used
 * mostly for popup visibility management.
 *
 * Return value: %TRUE if @view's "visibility" property is set
 **/
gboolean
ephy_web_view_get_visibility (EphyWebView *view)
{
  return view->priv->visibility;
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
  EphyWebViewPrivate *priv = view->priv;

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

  if (priv->security_level != level) {
    priv->security_level = level;

    g_object_notify (G_OBJECT (view), "security-level");
  }
}

/**
 * ephy_web_view_set_visibility:
 * @view: an #EphyWebView
 * @visibility: value for the visibility property of @view
 *
 * Sets whether the @view's toplevel is visible or not. See
 * ephy_web_view_get_visibility().
 **/
void
ephy_web_view_set_visibility (EphyWebView *view,
                              gboolean visibility)
{
  EphyWebViewPrivate *priv = view->priv;

  if (priv->visibility != visibility) {
    priv->visibility = visibility;

    g_object_notify (G_OBJECT (view), "visibility");
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

#define MIN_INPUT_LENGTH 50

/**
 * ephy_web_view_has_modified_forms:
 * @view: an #EphyWebView
 *
 * Returns %TRUE if the user has modified &lt;input&gt; or &lt;textarea&gt;
 * values in @view's loaded document.
 *
 * Return value: %TRUE if @view has user-modified forms
 **/
gboolean
ephy_web_view_has_modified_forms (EphyWebView *view)
{
#ifdef HAVE_WEBKIT2
  /* TODO: DOM Bindings */
#else
  WebKitDOMHTMLCollection *forms = NULL;
  WebKitDOMDocument *document = NULL;
  gulong forms_n;
  int i;

  document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (view));
  forms = webkit_dom_document_get_forms (document);
  forms_n = webkit_dom_html_collection_get_length (forms);

  for (i = 0; i < forms_n; i++) {
    WebKitDOMHTMLCollection *elements;
    WebKitDOMNode *form_element = webkit_dom_html_collection_item (forms, i);
    gulong elements_n;
    int j;
    gboolean modified_input_element = FALSE;

    elements = webkit_dom_html_form_element_get_elements (WEBKIT_DOM_HTML_FORM_ELEMENT (form_element));
    elements_n = webkit_dom_html_collection_get_length (elements);

    for (j = 0; j < elements_n; j++) {
      WebKitDOMNode *element;

      element = webkit_dom_html_collection_item (elements, j);

      if (WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT (element))
        if (webkit_dom_html_text_area_element_is_edited (WEBKIT_DOM_HTML_TEXT_AREA_ELEMENT (element)))
          return TRUE;

      if (WEBKIT_DOM_IS_HTML_INPUT_ELEMENT (element))
        if (webkit_dom_html_input_element_is_edited (WEBKIT_DOM_HTML_INPUT_ELEMENT (element))) {
          glong length;
          char *text;

          /* A small heuristic here. If there's only one input element
           * modified and it does not have a lot of text the user is
           * likely not very interested in saving this work, so do
           * nothing (eg, google search input). */
          if (modified_input_element)
            return TRUE;

          modified_input_element = TRUE;

          text = webkit_dom_html_input_element_get_value (WEBKIT_DOM_HTML_INPUT_ELEMENT (element));
          length = g_utf8_strlen (text, -1);
          g_free (text);

          if (length > MIN_INPUT_LENGTH)
            return TRUE;
        }
    }
  }
#endif
  return FALSE;
}

/**
 * ephy_web_view_get_security_level:
 * @view: an #EphyWebView
 * @level: (out): return value of security level
 * @description: (out): return value of the description of the security level
 *
 * Fetches the #EphyWebViewSecurityLevel and a string description of the
 * security state of @view.  The description will be a newly-allocated
 * string or %NULL.
 **/
void
ephy_web_view_get_security_level (EphyWebView *view,
                                  EphyWebViewSecurityLevel *level,
                                  char **description)
{
  g_return_if_fail (EPHY_IS_WEB_VIEW (view));

  if (level)
    *level = view->priv->security_level;

  if (description)
    *description = NULL;
}

static void
ephy_web_view_print_failed (EphyWebView *view, GError *error)
{
  GtkWidget *info_bar;
  GtkWidget *label;
  GtkContainer *content_area;
  EphyEmbed *embed = EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view);

  info_bar = gtk_info_bar_new_with_buttons (GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
  label = gtk_label_new (error->message);
  content_area = GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar)));

  gtk_info_bar_set_message_type (GTK_INFO_BAR (info_bar), GTK_MESSAGE_ERROR);
  gtk_container_add (content_area, label);
  g_signal_connect (info_bar, "response",
                    G_CALLBACK (gtk_widget_destroy), NULL);

  ephy_embed_add_top_widget (embed, info_bar, FALSE);
  gtk_widget_show_all (info_bar);
}

#ifndef HAVE_WEBKIT2
static void
ephy_web_view_run_print_action (EphyWebView *view, GtkPrintOperationAction action)
{
  WebKitWebFrame *main_frame;
  GtkPrintOperation *operation;
  GError *error;
  EphyEmbedShell *shell;

  shell = ephy_embed_shell_get_default ();
  error = NULL;
  main_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view));

  operation = gtk_print_operation_new ();
  gtk_print_operation_set_embed_page_setup (operation, TRUE);
  gtk_print_operation_set_default_page_setup (operation, ephy_embed_shell_get_page_setup (shell));

  webkit_web_frame_print_full (main_frame, operation, action, &error);

  if (error) {
    ephy_web_view_print_failed (view, error);
    g_error_free (error);
  } else
    ephy_embed_shell_set_page_setup (shell, gtk_print_operation_get_default_page_setup (operation));

  g_object_unref (operation);
}
#endif

#ifdef HAVE_WEBKIT2
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
#endif

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
#ifdef HAVE_WEBKIT2
  WebKitPrintOperation *operation;
  EphyEmbedShell *shell;

  shell = ephy_embed_shell_get_default ();

  operation = webkit_print_operation_new (WEBKIT_WEB_VIEW (view));
  g_signal_connect (operation, "finished",
                    G_CALLBACK (print_operation_finished_cb),
                    view);
  g_signal_connect (operation, "failed",
                    G_CALLBACK (print_operation_failed_cb),
                    view);
  webkit_print_operation_set_page_setup (operation, ephy_embed_shell_get_page_setup (shell));
  webkit_print_operation_run_dialog (operation, NULL);
  g_object_unref (operation);
#else
  ephy_web_view_run_print_action (view, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
#endif
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

#ifdef HAVE_WEBKIT2
/* TODO: webkit_web_view_save() */
#else
static void
ephy_web_view_save_sub_resource_start (GList *subresources, char *destination_uri);

static void
ephy_web_view_close_cb (GOutputStream *ostream, GAsyncResult *result, GString *data)
{
  GList *subresources;
  char *destination_uri;
  GError *error = NULL;

  subresources = (GList*)g_object_get_data (G_OBJECT (ostream),
                                            "ephy-web-view-save-subresources");

  destination_uri = (char*)g_object_get_data (G_OBJECT (ostream),
                                              "ephy-web-view-save-dest-uri");

  g_output_stream_close_finish (ostream, result, &error);
  g_object_unref (ostream);

  if (error) {
    g_list_free (subresources);
    g_free (destination_uri);
    g_warning ("Unable to write to file: %s", error->message);
    g_error_free (error);
    return;
  }

  if (!subresources || !subresources->next) {
    g_list_free (subresources);
    g_free (destination_uri);
    return;
  }

  subresources = subresources->next;
  ephy_web_view_save_sub_resource_start (subresources, destination_uri);
}

static void
ephy_web_view_save_write_cb (GOutputStream *ostream, GAsyncResult *result, GString *data)
{
  GError *error = NULL;
  gssize written;

  written = g_output_stream_write_finish (ostream, result, &error);
  if (error) {
    GList *subresources;
    char *destination_uri;

    subresources = (GList*)g_object_get_data (G_OBJECT (ostream),
                                             "ephy-web-view-save-subresources");
    g_list_free (subresources);

    destination_uri = (char*)g_object_get_data (G_OBJECT (ostream),
                                                "ephy-web-view-save-dest-uri");
    g_free (destination_uri);

    g_string_free (data, FALSE);
    g_object_unref (ostream);

    g_warning ("Unable to write to file: %s", error->message);

    g_error_free (error);
    return;
  }

  if (written == data->len) {
    g_string_free (data, FALSE);
    g_output_stream_close_async (ostream, G_PRIORITY_DEFAULT, NULL,
                                 (GAsyncReadyCallback)ephy_web_view_close_cb,
                                 NULL);
    return;
  }

  data->len -= written;
  data->str += written;

  g_output_stream_write_async (ostream,
                               data->str, data->len,
                               G_PRIORITY_DEFAULT, NULL,
                               (GAsyncReadyCallback)ephy_web_view_save_write_cb,
                               data);
}

static void
ephy_web_view_save_replace_cb (GFile *file, GAsyncResult *result, GString *const_data)
{
  GFileOutputStream *ostream;
  GList *subresources;
  char *destination_uri;
  GString *data;
  GError *error = NULL;

  subresources = (GList*)g_object_get_data (G_OBJECT (file),
                                            "ephy-web-view-save-subresources");

  destination_uri = (char*)g_object_get_data (G_OBJECT (file),
                                              "ephy-web-view-save-dest-uri");

  ostream = g_file_replace_finish (file, result, &error);
  if (error) {
    g_warning ("Failed to save page: %s", error->message);
    g_list_free (subresources);
    g_free (destination_uri);
    g_error_free (error);
    return;
  }

  if (const_data) {
    data = g_string_sized_new (const_data->len);
    data->str = const_data->str;
    data->len = const_data->len;
  } else
    data = g_string_new ("");

  /* If we have subresources to handle, pass the information along */
  if (subresources) {
    g_object_set_data (G_OBJECT (ostream),
                       "ephy-web-view-save-subresources",
                       subresources);

    g_object_set_data (G_OBJECT (ostream),
                       "ephy-web-view-save-dest-uri",
                       destination_uri);
  }

  g_output_stream_write_async (G_OUTPUT_STREAM (ostream),
                               data->str, data->len,
                               G_PRIORITY_DEFAULT, NULL,
                               (GAsyncReadyCallback)ephy_web_view_save_write_cb,
                               data);
}

static void
ephy_web_view_save_sub_resource_start (GList *subresources, char *destination_uri)
{
  WebKitWebResource *resource;
  GFile *file;
  const char *resource_uri;
  char *resource_basename;
  char *resource_name;
  char *resource_dest_uri;
  const GString *data;

  resource = WEBKIT_WEB_RESOURCE (subresources->data);

  resource_uri = webkit_web_resource_get_uri (resource);
  resource_basename = g_path_get_basename (resource_uri);

  resource_name = g_uri_escape_string (resource_basename, NULL, TRUE);
  g_free (resource_basename);

  resource_dest_uri = g_strdup_printf ("%s/%s", destination_uri, resource_name);
  g_free (resource_name);

  file = g_file_new_for_uri (resource_dest_uri);
  g_free (resource_dest_uri);

  g_object_set_data (G_OBJECT (file),
                     "ephy-web-view-save-dest-uri",
                     destination_uri);

  g_object_set_data (G_OBJECT (file),
                     "ephy-web-view-save-subresources",
                     subresources);

  data = webkit_web_resource_get_data (resource);

  g_file_replace_async (file, NULL, FALSE,
                        G_FILE_CREATE_REPLACE_DESTINATION|G_FILE_CREATE_PRIVATE,
                        G_PRIORITY_DEFAULT, NULL,
                        (GAsyncReadyCallback)ephy_web_view_save_replace_cb,
                        (GString*)data);

  g_object_unref (file);
}

static void
ephy_web_view_save_sub_resources (EphyWebView *view, const char *uri, GList *subresources)
{
  GFile *file;
  char *filename;
  char *dotpos;
  char *directory_uri;
  char *tmp;
  char *destination_uri;
  GError *error = NULL;

  /* filename of the main resource without extension */
  filename = g_path_get_basename (uri);
  dotpos = g_strrstr (filename, ".");
  if (dotpos)
    *dotpos = '\0';

  directory_uri = g_path_get_dirname (uri);

  /* Translators: this is the directory name to store auxilary files
   * when saving html files.
   */
  tmp = g_strdup_printf (_("%s Files"), filename);
  g_free (filename);

  destination_uri = g_strdup_printf ("%s/%s", directory_uri, tmp);
  g_free (directory_uri);
  g_free (tmp);

  file = g_file_new_for_uri (destination_uri);

  if (!g_file_make_directory (file, NULL, &error)) {
    if (error->code != G_IO_ERROR_EXISTS) {
      g_warning ("Could not create directory: %s", error->message);
      g_error_free (error);
      g_object_unref (file);
      return;
    }
  }
  g_object_unref (file);

  /* Now, let's start saving sub resources */
  ephy_web_view_save_sub_resource_start (subresources, destination_uri);
}
#endif
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
#ifdef HAVE_WEBKIT2
  /* TODO: webkit_web_view_save() */
#else
  WebKitWebFrame *frame;
  WebKitWebDataSource *data_source;
  GList *subresources;
  const GString *data;
  GFile *file;

  /* Save main resource */
  frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW(view));
  data_source = webkit_web_frame_get_data_source (frame);
  data = webkit_web_data_source_get_data (data_source);

  file = g_file_new_for_uri (uri);
  g_file_replace_async (file, NULL, FALSE,
                        G_FILE_CREATE_REPLACE_DESTINATION|G_FILE_CREATE_PRIVATE,
                        G_PRIORITY_DEFAULT, NULL,
                        (GAsyncReadyCallback)ephy_web_view_save_replace_cb,
                        (GString*)data);

  g_object_unref (file);

  /* If subresources exist, save them */
  subresources = webkit_web_data_source_get_subresources (data_source);
  if (!subresources)
    return;

  ephy_web_view_save_sub_resources (view, uri, subresources);
#endif
}

/**
 * ephy_web_view_load_homepage:
 * @view: an #EphyWebView
 *
 * Loads the homepage, which is hardcoded to be "about:blank"
 *
 **/
void
ephy_web_view_load_homepage (EphyWebView *view)
{
  g_signal_emit_by_name (view, "loading-homepage");

  ephy_web_view_set_visit_type (view,
                                EPHY_PAGE_VISIT_HOMEPAGE);
  ephy_web_view_load_url (view, "about:blank");
}

/**
 * ephy_web_view_get_snapshot:
 * @view: the #EphyWebView
 * @x: the x coordinate of the snapshot
 * @y: the y coordinate of the snapshot
 * @width: the width of the snapshot
 * @height: the height of the snapshot
 *
 * Takes a snapshot of the requested region of a #EphyWebView.
 *
 * Returns: (transfer full): a #GdkPixbuf with a snapshot of the requested area.
 **/
GdkPixbuf *
ephy_web_view_get_snapshot (EphyWebView *view, int x, int y, int width, int height)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkPixbuf *snapshot;
  GtkAllocation allocation;

  g_return_val_if_fail (EPHY_IS_WEB_VIEW (view), NULL);

  gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);
  surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
                                        allocation.width,
                                        allocation.height);
  cr = cairo_create (surface);
  cairo_rectangle (cr, x, y, width, height);
  cairo_clip (cr);
  gtk_widget_draw (GTK_WIDGET (view), cr);

  snapshot = gdk_pixbuf_get_from_surface (surface, x, y, width, height);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return snapshot;
}

gboolean
ephy_web_view_is_loading_homepage (EphyWebView *view)
{
  g_return_val_if_fail (EPHY_IS_WEB_VIEW (view), FALSE);

  return view->priv->loading_homepage;
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
