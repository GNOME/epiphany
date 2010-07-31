/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2008, 2009 Gustavo Noronha Silva
 *  Copyright © 2009 Igalia S.L.
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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit/webkit.h>
#include <gnome-keyring.h>

#include "eel-gconf-extensions.h"
#include "ephy-debug.h"
#include "ephy-embed.h"
#include "ephy-embed-container.h"
#include "ephy-embed-prefs.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-embed-utils.h"
#include "ephy-file-helpers.h"
#include "ephy-prefs.h"
#include "ephy-marshal.h"
#include "ephy-permission-manager.h"
#include "ephy-profile-migration.h"
#include "ephy-favicon-cache.h"
#include "ephy-history.h"
#include "ephy-string.h"
#include "ephy-web-view.h"
#include "ephy-zoom.h"

/**
 * SECTION:ephy-web-view
 * @short_description: Epiphany custom #WebkitWebView
 *
 * #EphyWebView wraps #WebkitWebView implementing custom functionality on top of
 * it.
 */

static void     ephy_web_view_class_init   (EphyWebViewClass *klass);
static void     ephy_web_view_init         (EphyWebView *gs);

#define MAX_HIDDEN_POPUPS       5
#define MAX_TITLE_LENGTH        512 /* characters */
#define RELOAD_DELAY            250 /* ms */
#define RELOAD_DELAY_MAX_TICKS  40  /* RELOAD_DELAY * RELOAD_DELAY_MAX_TICKS = 10 s */
#define EMPTY_PAGE              _("Blank page") /* Title for the empty page */

struct _EphyWebViewPrivate {
  EphyWebViewSecurityLevel security_level;
  EphyWebViewDocumentType document_type;
  EphyWebViewNavigationFlags nav_flags;
  WebKitLoadStatus load_status;

  /* Flags */
  guint is_blank : 1;
  guint visibility : 1;

  char *address;
  char *typed_address;
  char *title;
  char *loading_title;
  char *status_message;
  char *link_message;
  GdkPixbuf *icon;
  gboolean expire_address_now;

  /* File watch */
  GFileMonitor *monitor;
  gboolean monitor_directory;
  guint reload_scheduled_id;
  guint reload_delay_ticks;

  /* Regex to figure out if we're dealing with a wanna-be URI */
  GRegex *non_search_regex;

  GSList *hidden_popups;
  GSList *shown_popups;

  GSList *messages;
  GSList *keys;

  guint seq_context_id;
  guint seq_message_id;

  guint tab_message_id;

  char *text;
  GdkRectangle text_rectangle;

  GtkWidget *password_info_bar;
};

typedef struct {
  char *url;
  char *name;
  char *features;
} PopupInfo;

typedef struct {
  gchar *text;
  guint context_id;
  guint message_id;
} EphyWebViewStatusbarMsg;

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
             ("width=%d,height=%d,menubar=%d,status=%d,toolbar=%d",
              allocation.width,
              allocation.height,
              (chrome & EPHY_WEB_VIEW_CHROME_MENUBAR) > 0,
              (chrome & EPHY_WEB_VIEW_CHROME_STATUSBAR) > 0,
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
  char *location;
  char *features;

  embed = ephy_embed_container_get_active_child (container);
  g_return_if_fail (EPHY_IS_EMBED (embed));

  location = ephy_web_view_get_location (ephy_embed_get_web_view (embed), TRUE);
  if (location == NULL) return;

  features = popups_manager_new_window_info (container);

  popups_manager_add (parent_view, location, "" /* FIXME? maybe _blank? */, features);

  gtk_widget_destroy (GTK_WIDGET (container));

  g_free (location);
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
  char *location;
  EphyPermissionManager *manager;
  EphyPermission permission;

  location = ephy_web_view_get_location (view, TRUE);
  g_return_if_fail (location != NULL);

  manager = EPHY_PERMISSION_MANAGER
            (ephy_embed_shell_get_embed_single (embed_shell));
  g_return_if_fail (EPHY_IS_PERMISSION_MANAGER (manager));

  permission = allowed ? EPHY_PERMISSION_ALLOWED
               : EPHY_PERMISSION_DENIED;

  ephy_permission_manager_add_permission (manager, location, EPT_POPUP, permission);

  if (allowed) {
    popups_manager_show_all (view);
  } else {
    popups_manager_hide_all (view);
  }

  g_free (location);
}

static gboolean
ephy_web_view_get_popups_allowed (EphyWebView *view)
{
  EphyPermissionManager *permission_manager;
  EphyPermission response;
  char *location;
  gboolean allow;

  permission_manager = EPHY_PERMISSION_MANAGER
                       (ephy_embed_shell_get_embed_single (embed_shell));
  g_return_val_if_fail (EPHY_IS_PERMISSION_MANAGER (permission_manager),
                        FALSE);

  location = ephy_web_view_get_location (view, TRUE);
  if (location == NULL) return FALSE;/* FALSE, TRUE… same thing */

  response = ephy_permission_manager_test_permission
             (permission_manager, location, EPT_POPUP);

  switch (response) {
    case EPHY_PERMISSION_ALLOWED:
      allow = TRUE;
      break;
    case EPHY_PERMISSION_DENIED:
      allow = FALSE;
      break;
    case EPHY_PERMISSION_DEFAULT:
    default:
      allow = eel_gconf_get_boolean
              (CONF_SECURITY_ALLOW_POPUPS);
      break;
  }

  g_free (location);

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
      /* read only */
      break;
    default:
      break;
  }
}

static void
ephy_web_view_file_monitor_cancel (EphyWebView *view)
{
  EphyWebViewPrivate *priv = view->priv;

  if (priv->monitor != NULL) {
    LOG ("Cancelling file monitor");
    
    g_file_monitor_cancel (G_FILE_MONITOR (priv->monitor));
    priv->monitor = NULL;
  }

  if (priv->reload_scheduled_id != 0) {
    LOG ("Cancelling scheduled reload");

    g_source_remove (priv->reload_scheduled_id);
    priv->reload_scheduled_id = 0;
  }

  priv->reload_delay_ticks = 0;
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
  ephy_web_view_file_monitor_cancel (EPHY_WEB_VIEW (object));

  G_OBJECT_CLASS (ephy_web_view_parent_class)->dispose (object);
}

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

      if (g_str_equal ("text", element_type)) {
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
}

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
  SoupURI *soup_uri;

  /* We are no longer showing a store password infobar */
  web_view->priv->password_info_bar = NULL;

  if (response_id != GTK_RESPONSE_YES) {
    LOG ("Response is %d - not saving.", response_id);
    store_password_data_free (store_data);
    gtk_widget_destroy (GTK_WIDGET (info_bar));
    return;
  }

  LOG ("Response is GTK_RESPONSE_YES - saving!");
  _ephy_profile_store_form_auth_data (uri,
                                      name_field_name,
                                      password_field_name,
                                      name_field_value,
                                      password_field_value);

  /* Update internal caching */
  soup_uri = soup_uri_new (uri);

  ephy_embed_single_add_form_auth (EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell)),
                                   soup_uri->host,
                                   name_field_name,
                                   password_field_name,
                                   name_field_value);
  soup_uri_free (soup_uri);

  store_password_data_free (store_data);
  gtk_widget_destroy (GTK_WIDGET (info_bar));
}

static void
send_no_response_cb (GtkButton *button, GtkInfoBar *info_bar)
{
  gtk_info_bar_response (info_bar, GTK_RESPONSE_NO);
}

static void
send_yes_response_cb (GtkButton *button, GtkInfoBar *info_bar)
{
  gtk_info_bar_response (info_bar, GTK_RESPONSE_YES);
}

static void
request_decision_on_storing (StorePasswordData *store_data)
{
  EphyEmbed *embed = store_data->embed;
  EphyWebView *web_view = ephy_embed_get_web_view (embed);
  GtkWidget *info_bar;
  GtkWidget *action_area;
  GtkWidget *button_box;
  GtkWidget *action_button;
  GtkWidget *content_area;
  GtkWidget *label;
  char *message;
  char *hostname;

  LOG ("Going to show infobar about %s", store_data->uri);

  info_bar = gtk_info_bar_new ();

  action_area = gtk_info_bar_get_action_area (GTK_INFO_BAR (info_bar));
  button_box = gtk_hbutton_box_new ();
  gtk_container_add (GTK_CONTAINER (action_area), button_box);

  action_button = gtk_button_new_with_label (_("Not now"));
  g_signal_connect (action_button, "clicked",
                    G_CALLBACK (send_no_response_cb), info_bar);
  gtk_box_pack_start (GTK_BOX (button_box), action_button, FALSE, FALSE, 0);

  action_button = gtk_button_new_with_label (_("Store password"));
  g_signal_connect (action_button, "clicked",
                    G_CALLBACK (send_yes_response_cb), info_bar);
  gtk_box_pack_start (GTK_BOX (button_box), action_button, FALSE, FALSE, 0);

  label = gtk_label_new (NULL);
  hostname = ephy_string_get_host_name (store_data->uri);
  /* Translators: The first %s is the username and the second one is the
   * hostname where this is happening. Example: gnome@gmail.com and
   * mail.google.com.
   */
  message = g_strdup_printf (_("<big>Would you like to store the password for <b>%s</b> in <b>%s</b>?</big>"),
                             store_data->name_value,
                             hostname);
  gtk_label_set_markup (GTK_LABEL (label), message);
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

  _ephy_profile_query_form_auth_data (store_data->uri,
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

      _ephy_profile_query_form_auth_data (uri_str,
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
      g_signal_connect (form, "submit-event",
                        G_CALLBACK (form_submitted_cb), web_view);

      pre_fill_form (username_node, password_node, web_view);

      g_object_unref (username_node);
      g_object_unref (password_node);
    } else
      LOG ("No pre-fillable/hookable form found");
  }
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
}

static void
ephy_web_view_finalize (GObject *object)
{
  EphyWebViewPrivate *priv = EPHY_WEB_VIEW (object)->priv;
  GSList *list;

  if (priv->icon != NULL) {
    g_object_unref (priv->icon);
    priv->icon = NULL;
  }

  if (priv->non_search_regex != NULL) {
    g_regex_unref (priv->non_search_regex);
    priv->non_search_regex = NULL;
  }

  for (list = priv->messages; list; list = list->next) {
    EphyWebViewStatusbarMsg *msg;

    msg = list->data;
    g_free (msg->text);
    g_slice_free (EphyWebViewStatusbarMsg, msg);
  }

  g_slist_free (priv->messages);
  priv->messages = NULL;


  for (list = priv->keys; list; list = list->next)
    g_free (list->data);

  g_slist_free (priv->keys);
  priv->keys = NULL;

  g_free (priv->text);

  ephy_web_view_popups_manager_reset (EPHY_WEB_VIEW (object));

  g_free (priv->address);
  g_free (priv->typed_address);
  g_free (priv->title);
  g_free (priv->status_message);
  g_free (priv->link_message);
  g_free (priv->loading_title);

  G_OBJECT_CLASS (ephy_web_view_parent_class)->finalize (object);
}

static void
title_changed_cb (WebKitWebView *web_view,
                  GParamSpec *spec,
                  gpointer data)
{
  const char *uri;
  char *title;
  WebKitWebFrame *frame;
  EphyHistory *history = EPHY_HISTORY (ephy_embed_shell_get_global_history (ephy_embed_shell_get_default ()));

  g_object_get (web_view, "title", &title, NULL);

  ephy_web_view_set_title (EPHY_WEB_VIEW (web_view),
                           title);

  frame = webkit_web_view_get_main_frame (web_view);
  uri = webkit_web_frame_get_uri (frame);
  ephy_history_set_page_title (history,
                               uri,
                               title);
  g_free (title);

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

  /* We need to check if we get URI notifications without going
     through the usual load process, as this can happen when changing
     location within a page */
  if (g_str_equal (uri, current_address) == FALSE)
    ephy_web_view_set_address (EPHY_WEB_VIEW (web_view), uri);

  g_free (uri);
}

static void
hovering_over_link_cb (EphyWebView *web_view,
                       char *title,
                       char *location,
                       gpointer data)
{
  ephy_web_view_set_link_message (web_view, location);
}

static void
ephy_web_view_constructed (GObject *object)
{
  EphyWebView *web_view = EPHY_WEB_VIEW (object);

  g_object_connect (web_view,
                    "signal::notify::title", G_CALLBACK (title_changed_cb), NULL,
                    "signal::notify::uri", G_CALLBACK (uri_changed_cb), NULL,
                    "signal::hovering-over-link", G_CALLBACK (hovering_over_link_cb), NULL,
                    NULL);
}

static void
_ephy_web_view_draw_statusbar(GtkWidget *widget)
{
  gint width, height;
  guint border_width, statusbar_border_width;
  PangoLayout *layout;
  GtkAllocation allocation;
  GdkWindow *window;
  GtkStyle *style;
  EphyWebViewPrivate *priv;

  priv = EPHY_WEB_VIEW (widget)->priv;

  gtk_widget_get_allocation (widget, &allocation);

  layout = gtk_widget_create_pango_layout (widget, priv->text);
  pango_layout_set_width (layout, PANGO_SCALE * (allocation.width * 0.9));
  pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);

  pango_layout_get_pixel_size (layout, &width, &height);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  window = gtk_widget_get_window (widget);
  style = gtk_widget_get_style (widget);

  statusbar_border_width = 4; /* FIXME: what should we use here? */

  priv->text_rectangle.x = border_width;
  priv->text_rectangle.y = allocation.height - height - border_width - (statusbar_border_width * 2);
  priv->text_rectangle.width = width + (statusbar_border_width * 2);
  priv->text_rectangle.height = height + (statusbar_border_width * 2);

  gtk_paint_box (style, window,
                 GTK_STATE_NORMAL, GTK_SHADOW_IN,
                 NULL, widget, NULL,
                 priv->text_rectangle.x,
                 priv->text_rectangle.y,
                 priv->text_rectangle.width,
                 priv->text_rectangle.height);

  gtk_paint_layout (style, window,
                    GTK_STATE_NORMAL, FALSE,
                    NULL, widget, NULL,
                    priv->text_rectangle.x + statusbar_border_width,
                    priv->text_rectangle.y + statusbar_border_width,
                    layout);

  g_object_unref (layout);
}

static gboolean
ephy_web_view_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
  EphyWebViewPrivate *priv;

  GTK_WIDGET_CLASS (ephy_web_view_parent_class)->expose_event (widget, event);

  priv = EPHY_WEB_VIEW (widget)->priv;

  if (priv->text && priv->text[0] != '\0')
    _ephy_web_view_draw_statusbar (widget);

  return FALSE;
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
  widget_class->expose_event = ephy_web_view_expose_event;

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
            ephy_marshal_VOID__STRING_STRING_STRING,
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
            ephy_marshal_VOID__STRING_STRING_STRING,
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
            ephy_marshal_VOID__STRING_STRING_STRING,
            G_TYPE_NONE,
            3,
            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
/**
 * EphyWebView::ge-dom-mouse-click:
 * @view: the #EphyWebView that received the signal
 * @event: the #EphyEmbedEvent which triggered this signal
 *
 * The ::ge_dom_mouse_click signal is emitted when the user clicks in the @view.
 **/
    g_signal_new ("ge_dom_mouse_click",
            EPHY_TYPE_WEB_VIEW,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (EphyWebViewClass, dom_mouse_click),
            g_signal_accumulator_true_handled, NULL,
            ephy_marshal_BOOLEAN__OBJECT,
            G_TYPE_BOOLEAN,
            1,
            G_TYPE_OBJECT);
/**
 * EphyWebView::ge-dom-mouse-down:
 * @view: the #EphyWebView that received the signal
 * @event: the #EphyEmbedEvent which triggered this signal
 *
 * The ::ge_dom_mouse_down signal is emitted when the user depresses a mouse
 * button.
 **/
    g_signal_new ("ge_dom_mouse_down",
            EPHY_TYPE_WEB_VIEW,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (EphyWebViewClass, dom_mouse_down),
            g_signal_accumulator_true_handled, NULL,
            ephy_marshal_BOOLEAN__OBJECT,
            G_TYPE_BOOLEAN,
            1,
            G_TYPE_OBJECT);
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
            ephy_marshal_BOOLEAN__VOID,
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
            ephy_marshal_BOOLEAN__BOXED,
            G_TYPE_BOOLEAN,
            1,
            GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

/**
 * EphyWebView::close-request
 * @view: the #EphyWebView that received the signal
 *
 * The ::close signal is emitted when the embed requests closing.
 * Return %TRUE to prevent closing. You HAVE to process removal of the embed
 * as soon as possible after that.
 **/
    g_signal_new ("close-request",
            EPHY_TYPE_WEB_VIEW,
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (EphyWebViewClass, close_request),
            g_signal_accumulator_true_handled, NULL,
            ephy_marshal_BOOLEAN__VOID,
            G_TYPE_BOOLEAN,
            0);
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
 * EphyWebView::loading-homepage
 * @view: the #EphyWebView that received the signal
 *
 * The ::loading-homepage signal is emitted when the @view is about to
 * load the homepage set by the user.
 **/
    g_signal_new ("loading-homepage",
            EPHY_TYPE_WEB_VIEW,
            G_SIGNAL_RUN_FIRST,
            0, NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

  g_type_class_add_private (gobject_class, sizeof (EphyWebViewPrivate));
}

static void
_ephy_web_view_load_icon (EphyWebView *view)
{
  EphyWebViewPrivate *priv = view->priv;
  EphyEmbedShell *shell;
  EphyFaviconCache *cache;
  const char *icon_address;

  icon_address = webkit_web_view_get_icon_uri (WEBKIT_WEB_VIEW (view));

  if (icon_address == NULL || priv->icon != NULL) return;

  shell = ephy_embed_shell_get_default ();
  cache = EPHY_FAVICON_CACHE (ephy_embed_shell_get_favicon_cache (shell));

  /* ephy_favicon_cache_get returns a reference already */
  priv->icon = ephy_favicon_cache_get (cache, icon_address);

  g_object_notify (G_OBJECT (view), "icon");
}

static void
icon_cache_changed_cb (EphyFaviconCache *cache,
                       const char *address,
                       EphyWebView *view)
{
  const char *icon_address;

  g_return_if_fail (address != NULL);

  icon_address = webkit_web_view_get_icon_uri (WEBKIT_WEB_VIEW (view));

  /* is this for us? */
  if (icon_address != NULL &&
      strcmp (icon_address, address) == 0) {
    _ephy_web_view_load_icon (view);
  }
}

static void
_ephy_web_view_set_icon_address (EphyWebView *view,
                                 const char *icon_address)
{
  GObject *object = G_OBJECT (view);
  EphyWebViewPrivate *priv = view->priv;
  EphyHistory *history;

  if (priv->icon != NULL) {
    g_object_unref (priv->icon);
    priv->icon = NULL;

    g_object_notify (object, "icon");
  }

  if (icon_address && priv->address) {
    history = EPHY_HISTORY (ephy_embed_shell_get_global_history (embed_shell));
    ephy_history_set_icon (history, priv->address, icon_address);

    _ephy_web_view_load_icon (view);
  }
}

static void
favicon_cb (EphyWebView *view,
            const char *address,
            gpointer user_data)
{
  _ephy_web_view_set_icon_address (view, address);
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

static void
update_navigation_flags (EphyWebView *view)
{
  EphyWebViewPrivate *priv = view->priv;
  guint flags = 0;
  WebKitWebView *web_view = WEBKIT_WEB_VIEW (view);

  if (ephy_web_view_can_go_up (view))
    flags |= EPHY_WEB_VIEW_NAV_UP;

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
load_status_cb (WebKitWebView *web_view,
                GParamSpec *pspec,
                gpointer user_data)
{
  WebKitLoadStatus status = webkit_web_view_get_load_status (web_view);
  EphyWebView *view = EPHY_WEB_VIEW (web_view);
  EphyWebViewPrivate *priv = view->priv;
  GObject *object = G_OBJECT (web_view);

  g_object_freeze_notify (object);

  switch (status)
  {
  /* FIXME: add REDIRECTING and NEGOTIATING states to WebKitGTK */
  case WEBKIT_LOAD_PROVISIONAL:
    {
      const gchar *loading_uri = NULL;
      WebKitWebFrame *frame;

      WebKitWebDataSource *source;
      WebKitNetworkRequest *request;

      frame = webkit_web_view_get_main_frame (web_view);

      source = webkit_web_frame_get_provisional_data_source (frame);
      request = webkit_web_data_source_get_initial_request (source);
      loading_uri = webkit_network_request_get_uri (request);

      g_signal_emit_by_name (view, "new-document-now", loading_uri);

      if ((priv->address == NULL || priv->address[0] == '\0') &&
          priv->expire_address_now == TRUE) {
        ephy_web_view_set_address (view, loading_uri);
        ephy_web_view_set_title (view, NULL);
      }

      ephy_web_view_set_loading_title (view, loading_uri, TRUE);

      g_free (priv->status_message);
      priv->status_message = g_strdup (priv->loading_title);
      g_object_notify (object, "status-message");

      priv->expire_address_now = TRUE;
    }
    break;
  case WEBKIT_LOAD_COMMITTED:
    {
      const gchar* uri;
      EphyWebViewSecurityLevel security_level;

      uri = webkit_web_view_get_uri (web_view);
      ephy_web_view_location_changed (view,
                                      uri);

      ephy_web_view_set_title (view, NULL);

#ifdef GTLS_SYSTEM_CA_FILE
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
#else
      security_level = EPHY_WEB_VIEW_STATE_IS_UNKNOWN;
#endif

      ephy_web_view_set_security_level (EPHY_WEB_VIEW (web_view), security_level);
    }
    break;
  case WEBKIT_LOAD_FINISHED:
    g_free (priv->status_message);
    priv->status_message = NULL;
    g_object_notify (object, "status-message");
    ephy_web_view_set_loading_title (view, NULL, FALSE);

    if (ephy_web_view_get_is_blank (view))
      g_object_notify (object, "embed-title");

    if (ephy_has_private_profile () == FALSE &&
        eel_gconf_get_boolean (CONF_PRIVACY_REMEMBER_PASSWORDS))
      _ephy_web_view_hook_into_forms (view);

    _ephy_web_view_hook_into_links (view);
    break;
  case WEBKIT_LOAD_FAILED:
    ephy_web_view_set_link_message (view, NULL);
    ephy_web_view_set_loading_title (view, NULL, FALSE);

    g_free (priv->status_message);
    priv->status_message = NULL;
    g_object_notify (object, "status-message");

    update_navigation_flags (view);
    break;
  default:
    break;
  }

  g_object_thaw_notify (object);
}

 static gboolean
load_error_cb (WebKitWebView *web_view,
               WebKitWebFrame *frame,
               gchar *uri,
               GError *error,
               gpointer user_data)
{
  EphyWebView *view = EPHY_WEB_VIEW (web_view);

  if (error->code != WEBKIT_NETWORK_ERROR_CANCELLED) {
    gchar *message;

    message = g_strdup_printf (_("A problem occurred while loading %s"),
                               uri);
    ephy_web_view_set_title (view, message);
    g_free (message);

    _ephy_web_view_set_icon_address (view, NULL);
  } else {
    EphyWebViewPrivate *priv = view->priv;

    if (priv->expire_address_now) {
      const gchar* prev_uri;

      prev_uri = webkit_web_view_get_uri (web_view);

      ephy_web_view_set_typed_address (view, NULL);
      ephy_web_view_set_address (view, prev_uri);
    }
  }

  return FALSE;
}

static gboolean
close_web_view_cb (WebKitWebView *web_view,
                   gpointer user_data)
{
  EphyEmbedContainer *window;
  GList *children;
  GtkWidget *widget = gtk_widget_get_toplevel (GTK_WIDGET (web_view));
  window = EPHY_EMBED_CONTAINER (widget);

  children = ephy_embed_container_get_children (window);

  if (g_list_length (children) == 1)
    gtk_widget_destroy (GTK_WIDGET (window));
  else
    ephy_embed_container_remove_child (window, EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (web_view));

  g_list_free (children);

  return TRUE;
}

static void
adj_changed_cb (GtkAdjustment *adj, EphyWebView *view)
{
  EphyWebViewPrivate *priv;
  GdkWindow *window;

  priv = view->priv;

  window = gtk_widget_get_window (GTK_WIDGET (view));

  if (view && window)
    gdk_window_invalidate_rect (window, &priv->text_rectangle, TRUE);
}

static void
set_scroll_adjustments_cb (EphyWebView *view, GtkAdjustment *hadj, GtkAdjustment *vadj)
{
  if (hadj)
    g_signal_connect (hadj, "value-changed", G_CALLBACK (adj_changed_cb), view);

  if (vadj)
    g_signal_connect (vadj, "value-changed", G_CALLBACK (adj_changed_cb), view);
}

static void
status_message_notify_cb (EphyWebView *view, GParamSpec *pspec, gpointer data)
{
  const char *message;
  EphyWebViewPrivate *priv;

  message = ephy_web_view_get_status_message (view);

  priv = view->priv;

  ephy_web_view_statusbar_pop (view, priv->tab_message_id);

  if (message)
    ephy_web_view_statusbar_push (view, priv->tab_message_id, message);

}

static void
ephy_web_view_init (EphyWebView *web_view)
{
  EphyWebViewPrivate *priv;
  EphyFaviconCache *cache;

  priv = web_view->priv = EPHY_WEB_VIEW_GET_PRIVATE (web_view);

  priv->expire_address_now = TRUE;
  priv->is_blank = TRUE;
  priv->load_status = WEBKIT_LOAD_PROVISIONAL;
  priv->title = g_strdup (EMPTY_PAGE);
  priv->document_type = EPHY_WEB_VIEW_DOCUMENT_HTML;
  priv->security_level = EPHY_WEB_VIEW_STATE_IS_UNKNOWN;
  priv->monitor_directory = FALSE;
  priv->seq_context_id = 1;
  priv->seq_message_id = 1;
  priv->tab_message_id = ephy_web_view_statusbar_get_context_id (web_view, TAB_MESSAGE_CONTEXT_DESCRIPTION);

  priv->non_search_regex = g_regex_new ("(^localhost(\\.[^[:space:]]+)?(:\\d+)?(/.*)?$|"
                                        "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]$|"
                                        "^::[0-9a-f:]*$|" /* IPv6 literals */
                                        "^[0-9a-f:]+:[0-9a-f:]*$|" /* IPv6 literals */
                                        "^[^\\.[:space:]]+\\.[^\\.[:space:]]+.*$|" /* foo.bar... */
                                        "^https?://[^/\\.[:space:]]+.*$|"
                                        "^about:.*$|"
                                        "^data:.*$|"
                                        "^file:.*$)",
                                        G_REGEX_OPTIMIZE, G_REGEX_MATCH_NOTEMPTY, NULL);

  g_signal_connect (web_view, "mime-type-policy-decision-requested",
                    G_CALLBACK (mime_type_policy_decision_requested_cb),
                    NULL);

  g_signal_connect (web_view, "notify::load-status",
                    G_CALLBACK (load_status_cb),
                    NULL);

  g_signal_connect (web_view, "close-web-view",
                    G_CALLBACK (close_web_view_cb),
                    NULL);

  g_signal_connect (web_view, "load-error",
                    G_CALLBACK (load_error_cb),
                    NULL);

  g_signal_connect_object (web_view, "icon-loaded",
                           G_CALLBACK (favicon_cb),
                           web_view, (GConnectFlags)0);

  g_signal_connect_object (web_view, "new-window",
                           G_CALLBACK (new_window_cb),
                           web_view, (GConnectFlags)0);

  g_signal_connect_object (web_view, "ge_popup_blocked",
                           G_CALLBACK (ge_popup_blocked_cb),
                           web_view, (GConnectFlags)0);

  g_signal_connect (web_view, "set-scroll-adjustments",
                    G_CALLBACK (set_scroll_adjustments_cb),
                    NULL);

  g_signal_connect (web_view, "notify::status-message",
                    G_CALLBACK (status_message_notify_cb),
                    NULL);

  cache = EPHY_FAVICON_CACHE
          (ephy_embed_shell_get_favicon_cache (embed_shell));
  g_signal_connect_object (G_OBJECT (cache), "changed",
                           G_CALLBACK (icon_cache_changed_cb),
                           web_view, (GConnectFlags)0);
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

static char*
normalize_or_autosearch_url (EphyWebView *view, const char *url)
{
  char *effective_url;
  SoupURI *soup_uri = NULL;
  EphyWebViewPrivate *priv = view->priv;

  /* We use SoupURI as an indication of whether the value given in url
   * is not something we want to search; we only do that, though, if
   * the address has a web scheme, because SoupURI will consider any
   * string: as a valid scheme, and we will end up prepending http://
   * to it */
  if (ephy_embed_utils_address_has_web_scheme (url))
    soup_uri = soup_uri_new (url);

  /* If the string doesn't look like an URI, let's search it; */
  if (soup_uri == NULL &&
      priv->non_search_regex &&
      !g_regex_match (priv->non_search_regex, url, 0, NULL)) {
    char *query_param, *url_search;

    url_search = eel_gconf_get_string (CONF_URL_SEARCH);

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

  if (soup_uri)
    soup_uri_free (soup_uri);

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
                            WebKitNetworkRequest *request)
{
  WebKitWebFrame *main_frame;
  const char *url;
  char *effective_url;

  g_return_if_fail (EPHY_IS_WEB_VIEW(view));
  g_return_if_fail (WEBKIT_IS_NETWORK_REQUEST(request));

  url = webkit_network_request_get_uri (request);
  effective_url = normalize_or_autosearch_url (view, url);
  webkit_network_request_set_uri (request, effective_url);
  g_free (effective_url);

  main_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW(view));
  webkit_web_frame_load_request(main_frame, request);
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

  if (g_str_has_prefix (effective_url, "javascript:")) {
    char *decoded_url;
    
    decoded_url = soup_uri_decode (effective_url);
    webkit_web_view_execute_script (WEBKIT_WEB_VIEW (view), decoded_url);
    g_free (decoded_url);
  } else
    webkit_web_view_open (WEBKIT_WEB_VIEW (view), effective_url);

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
  WebKitWebView *source_view, *dest_view;
  WebKitWebBackForwardList* source_bflist, *dest_bflist;
  WebKitWebHistoryItem *item;
  GList *items;

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
  for (; items; items = items->next) {
    item = webkit_web_history_item_copy ((WebKitWebHistoryItem*)items->data);
    webkit_web_back_forward_list_add_item (dest_bflist, item);
    g_object_unref (item);
  }
  g_list_free (items);

  /* The ephy/gecko behavior is to add the current item of the source
     embed at the end of the back history, so keep doing that */
  item = webkit_web_back_forward_list_get_current_item (source_bflist);
  if (item)
    webkit_web_back_forward_list_add_item (dest_bflist, item);
}

/**
 * ephy_web_view_clear_history:
 * @view: the #EphyWebView to clear the history from
 *
 * Clears history of @view.
 **/
void
ephy_web_view_clear_history (EphyWebView *view)
{
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
}

/**
 * ephy_web_view_set_address:
 * @view: an #EphyWebView
 * @address: address to set @view to
 *
 * Sets @view location to be @address. Note that this function might also set
 * the typed-address property to %NULL.
 */
void
ephy_web_view_set_address (EphyWebView *view,
                           const char *address)
{
  EphyWebViewPrivate *priv = view->priv;
  GObject *object = G_OBJECT (view);

  g_free (priv->address);
  priv->address = g_strdup (address);

  priv->is_blank = address == NULL ||
                   strcmp (address, "about:blank") == 0;

  if (ephy_web_view_is_loading (view) &&
      priv->expire_address_now == TRUE &&
      priv->typed_address != NULL) {
    g_free (priv->typed_address);
    priv->typed_address = NULL;

    g_object_notify (object, "typed-address");
  }

  g_object_notify (object, "address");
}

static char*
get_title_from_address (const char *address)
{
  if (g_str_has_prefix (address, "file://")) 
    return g_strdup (address + 7);
  else
    return ephy_string_get_host_name (address);
}

/**
 * ephy_web_view_set_title:
 * @view: an #EphyWebView
 * @view_title: new title for @view
 *
 * Sets @view title to @view_title.
 */
void
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
      priv->is_blank = TRUE;
    }
  } else if (priv->is_blank) {
    g_free (title);
    title = g_strdup (EMPTY_PAGE);
  }

  g_free (priv->title);
  priv->title = ephy_string_shorten (title, MAX_TITLE_LENGTH);

  g_object_notify (G_OBJECT (view), "embed-title");
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

static gboolean
ephy_web_view_file_monitor_reload_cb (EphyWebView *view)
{
  EphyWebViewPrivate *priv = view->priv;

  if (priv->reload_delay_ticks > 0) {
    priv->reload_delay_ticks--;

    /* Run again */
    return TRUE;
  }

  if (ephy_web_view_is_loading (view)) {
    /* Wait a bit to reload if we're still loading! */
    priv->reload_delay_ticks = RELOAD_DELAY_MAX_TICKS / 2;

    /* Run again */
    return TRUE;
  }

  priv->reload_scheduled_id = 0;

  LOG ("Reloading file '%s'", ephy_web_view_get_address (view));
  webkit_web_view_reload (WEBKIT_WEB_VIEW (view));

  /* don't run again */
  return FALSE;
}

static void
ephy_web_view_file_monitor_cb (GFileMonitor *monitor,
                               GFile *file,
                               GFile *other_file,
                               GFileMonitorEvent event_type,
                               EphyWebView *view)
{
  gboolean should_reload;
  EphyWebViewPrivate *priv = view->priv;

  switch (event_type) {
    /* These events will always trigger a reload: */
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CREATED:
      should_reload = TRUE;
      break;

    /* These events will only trigger a reload for directories: */
    case G_FILE_MONITOR_EVENT_DELETED:
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
      should_reload = priv->monitor_directory;
      break;

    /* These events don't trigger a reload: */
    case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
    case G_FILE_MONITOR_EVENT_UNMOUNTED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    default:
      should_reload = FALSE;
      break;
  }

  if (should_reload) {
    /* We make a lot of assumptions here, but basically we know
     * that we just have to reload, by construction.
     * Delay the reload a little bit so we don't endlessly
     * reload while a file is written.
     */
    if (priv->reload_delay_ticks == 0) {
      priv->reload_delay_ticks = 1;
    } else {
      /* Exponential backoff */
      priv->reload_delay_ticks = MIN (priv->reload_delay_ticks * 2,
                                      RELOAD_DELAY_MAX_TICKS);
    }

    if (priv->reload_scheduled_id == 0) {
      priv->reload_scheduled_id =
        g_timeout_add (RELOAD_DELAY,
                       (GSourceFunc)ephy_web_view_file_monitor_reload_cb, view);
    }
  }
}

static void
ephy_web_view_update_file_monitor (EphyWebView *view,
                                   const gchar *address)
{
  EphyWebViewPrivate *priv = view->priv;
  gboolean local;
  gchar *anchor;
  gchar *url;
  GFile *file;
  GFileType file_type;
  GFileInfo *file_info;
  GFileMonitor *monitor = NULL;

  if (priv->monitor != NULL &&
      priv->address != NULL && address != NULL &&
      strcmp (priv->address, address) == 0) {
    /* same address, no change needed */
    return;
  }

  ephy_web_view_file_monitor_cancel (view);

  local = g_str_has_prefix (address, "file://");
  if (local == FALSE) return;

  /* strip off anchors */
  anchor = strchr (address, '#');
  if (anchor != NULL) {
    url = g_strndup (address, anchor - address);
  } else {
    url = g_strdup (address);
  }

  file = g_file_new_for_uri (url);
  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                 0, NULL, NULL);
  if (file_info == NULL) {
    g_object_unref (file);
    g_free (url);
    return;
  }

  file_type = g_file_info_get_file_type (file_info);
  g_object_unref (file_info);

  if (file_type == G_FILE_TYPE_DIRECTORY) {
    monitor = g_file_monitor_directory (file, 0, NULL, NULL);
    g_signal_connect (monitor, "changed",
                      G_CALLBACK (ephy_web_view_file_monitor_cb),
                      view);
    priv->monitor_directory = TRUE;
    LOG ("Installed monitor for directory '%s'", url);
  }
  else if (file_type == G_FILE_TYPE_REGULAR) {
    monitor = g_file_monitor_file (file, 0, NULL, NULL);
    g_signal_connect (monitor, "changed",
                      G_CALLBACK (ephy_web_view_file_monitor_cb),
                      view);
    priv->monitor_directory = FALSE;
    LOG ("Installed monitor for file '%s'", url);
  }
  priv->monitor = monitor;
  g_object_unref (file);
  g_free (url);
}

/**
 * ephy_web_view_location_changed:
 * @view: an #EphyWebView
 * @location: new location for @view
 *
 * Sets @location as the new address for @view while preventing notify events
 * in @view.
 **/
void
ephy_web_view_location_changed (EphyWebView *view,
                                const char *location)
{
  GObject *object = G_OBJECT (view);

  g_object_freeze_notify (object);

  /* do this up here so we still have the old address around */
  ephy_web_view_update_file_monitor (view, location);

  /* Do not expose about:blank to the user, an empty address
     bar will do better */
  if (location == NULL || location[0] == '\0' ||
      strcmp (location, "about:blank") == 0) {
    ephy_web_view_set_address (view, NULL);
    ephy_web_view_set_title (view, EMPTY_PAGE);
  } else {
    char *view_address;

    /* we do this to get rid of an eventual password in the URL */
    view_address = ephy_web_view_get_location (view, TRUE);
    ephy_web_view_set_address (view, view_address);
    ephy_web_view_set_loading_title (view, view_address, TRUE);
    g_free (view_address);
  }

  ephy_web_view_set_link_message (view, NULL);
  _ephy_web_view_set_icon_address (view, NULL);
  update_navigation_flags (view);

  g_object_notify (object, "embed-title");

  g_object_thaw_notify (object);
}

/**
 * ephy_web_view_can_go_up:
 * @view: an #EphyWebView
 *
 * Returns whether @view can travel to a higher-level directory on the server.
 * For example, for http://www.example.com/subdir/foo.html, returns %TRUE; for
 * http://www.example.com/, returns %FALSE.
 *
 * Return value: %TRUE if @view can browse to a higher-level directory
 **/
gboolean
ephy_web_view_can_go_up (EphyWebView *view)
{
  SoupURI *uri;
  gboolean result;

  uri = soup_uri_new (ephy_web_view_get_address (view));
  if (uri == NULL)
    return FALSE;

  if (strcmp (uri->scheme, "about") == 0 || strcmp (uri->scheme, "data") == 0) {
    soup_uri_free (uri);
    return FALSE;
  }

  result = uri->fragment || uri->query || (uri->path && (strlen (uri->path) > 1));
  soup_uri_free (uri);

  return result;
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
                                char *link_message)
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
 * Returns the text that @view's #EphyWindow will display in its location toolbar
 * entry when @view is selected.
 *
 * This is not guaranteed to be the same as @view's location,
 * available through ephy_web_view_get_location(). As the user types a new address
 * into the location entry, ephy_web_view_get_location()'s returned string will
 * change.
 *
 * Return value: @view's #EphyWindow's location entry text when @view is selected
 **/
const char *
ephy_web_view_get_typed_address (EphyWebView *view)
{
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
  EphyWebViewPrivate *priv = EPHY_WEB_VIEW (view)->priv;

  g_free (priv->typed_address);
  priv->typed_address = g_strdup (address);
  /* If the page is loading prevent the typed address from going away,
     since Epiphany will try to overwrite the typed address with the
     confirmed full URL when passing through, for example, the
     COMMITTED state. */
  priv->expire_address_now = !ephy_web_view_is_loading (view);

  g_object_notify (G_OBJECT (view), "typed-address");
}

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
  return FALSE;
}

/**
 * ephy_web_view_get_location:
 * @view: an #EphyWebView
 * @toplevel: %FALSE to return the location of the focused frame only
 *
 * Returns the URL of the web page displayed in @view.
 *
 * If the web page contains frames, @toplevel will determine which location to
 * retrieve. If @toplevel is %TRUE, the return value will be the location of the
 * frameset document. If @toplevel is %FALSE, the return value will be the
 * location of the currently-focused frame.
 *
 * Return value: the URL of the web page displayed in @view
 **/
char *
ephy_web_view_get_location (EphyWebView *view,
                            gboolean toplevel)
{
  /* FIXME: follow the toplevel parameter */
  WebKitWebFrame *web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (view));
  return g_strdup (webkit_web_frame_get_uri (web_frame));
}

/**
 * ephy_web_view_go_up:
 * @view: an #EphyWebView
 *
 * Moves @view one level up in its web page's directory hierarchy.
 **/
void
ephy_web_view_go_up (EphyWebView *view)
{
}

/**
 * ephy_web_view_get_js_status:
 * @view: an #EphyWebView
 *
 * Displays the message JavaScript is attempting to display in the statusbar.
 *
 * Note that Epiphany does not display JavaScript statusbar messages.
 *
 * Return value: a message from JavaScript meant to be displayed in the
 *     statusbar
 **/
char *
ephy_web_view_get_js_status (EphyWebView *view)
{
  return NULL;
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

/**
 * ephy_web_view_show_page_certificate:
 * @view: an #EphyWebView
 *
 * Shows a dialogue displaying the certificate of the currently loaded page
 * of @view, if it was loaded over a secure connection; else does nothing.
 **/
void
ephy_web_view_show_page_certificate (EphyWebView *view)
{
}

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
  gtk_print_operation_set_default_page_setup (operation, ephy_embed_shell_get_page_setup  (shell));

  webkit_web_frame_print_full (main_frame, operation, action, &error);
  g_object_unref (operation);

  if (error) {
    GtkWidget *info_bar;
    GtkWidget *label;
    GtkContainer *content_area;
    EphyEmbed *embed = EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view);

    info_bar = gtk_info_bar_new_with_buttons (GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
    label = gtk_label_new (error->message);
    content_area = GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar)));
    g_error_free (error);

    gtk_info_bar_set_message_type (GTK_INFO_BAR (info_bar), GTK_MESSAGE_ERROR);
    gtk_container_add (content_area, label);
    g_signal_connect (info_bar, "response",
                      G_CALLBACK (gtk_widget_destroy), NULL);

    ephy_embed_add_top_widget (embed, info_bar, FALSE);
    gtk_widget_show_all (info_bar);
  }
}

/**
 * ephy_web_view_show_print_preview
 * @view: an #EphyWebView
 *
 * Generates a print preview of the specified view.
 * An external viewer is used to display the preview.
 *
 * Since: 2.30
 **/
void
ephy_web_view_show_print_preview (EphyWebView *view)
{
  ephy_web_view_run_print_action (view, GTK_PRINT_OPERATION_ACTION_PREVIEW);
}

/**
 * ephy_web_view_print
 * @view: an #EphyWebView
 *
 * Opens a dialog to print the specified view.
 *
 * Since: 2.30
 **/
void
ephy_web_view_print (EphyWebView *view)
{
  ephy_web_view_run_print_action (view, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

/**
 * ephy_web_view_get_go_up_list:
 * @view: an #EphyWebView
 *
 * Returns a list of (%char *) URLs to higher-level directories on the same
 * server, in order of deepest to shallowest. For example, given
 * "http://www.example.com/dir/subdir/file.html", will return a list containing
 * "http://www.example.com/dir/subdir/", "http://www.example.com/dir/" and
 * "http://www.example.com/".
 *
 * Return value: a list of URLs higher up in @view's web page's directory
 * hierarchy
 **/
GSList *
ephy_web_view_get_go_up_list (EphyWebView *view)
{
  SoupURI *uri;
  GSList *result = NULL;
  char *t1, *t2;

  uri = soup_uri_new (ephy_web_view_get_address (view));
  if (uri == NULL)
    return NULL;

  if (strcmp (uri->scheme, "about") == 0 || strcmp (uri->scheme, "data") == 0) {
    soup_uri_free (uri);
    return NULL;
  }

  /* remove fragment, then query, then go up path */
  if (uri->fragment) {
    soup_uri_set_fragment (uri, NULL);
    result = g_slist_prepend (result, soup_uri_to_string (uri, FALSE));
  }

  if (uri->query) {
    soup_uri_set_query (uri, NULL);
    result = g_slist_prepend (result, soup_uri_to_string (uri, FALSE));
  }

  if (uri->path[strlen(uri->path)-1] != '/') {
    /* not a trailing slash, remove "file" part */
    t1 = strrchr (uri->path, '/');
    t2 = g_strndup (uri->path, t1-uri->path+1);
    soup_uri_set_path (uri, t2);
    g_free (t2);
    result = g_slist_prepend (result, soup_uri_to_string (uri, FALSE));
  }

  while (strcmp(uri->path, "/") != 0) {
    /* chop trailing / */
    uri->path[strlen (uri->path)-1] = 0;
    t1 = strrchr (uri->path, '/');
    t2 = g_strndup (uri->path, t1-uri->path+1);
    soup_uri_set_path (uri, t2);
    g_free (t2);
    result = g_slist_prepend (result, soup_uri_to_string (uri, FALSE));
  }

  result = g_slist_reverse (result);

  soup_uri_free (uri);

  return result;
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
  gboolean is_loading, is_blank;

  g_return_val_if_fail (EPHY_IS_WEB_VIEW (view), NULL);

  is_loading = ephy_web_view_is_loading (view);
  is_blank = ephy_web_view_get_is_blank (view);
  loading_title = ephy_web_view_get_loading_title (view);
  title = ephy_web_view_get_title (view);

  if (is_blank)
  {
    if (is_loading)
      title = loading_title;
    else
      title = _("Blank page");
  }

  return title != NULL ? title : "";
}

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
}

/**
 * ephy_web_view_load_homepage:
 * @view: an #EphyWebView
 * 
 * Loads the homepage set by the user in @view.
 * 
 * Returns: %TRUE if there was a homepage set to be loaded in the user
 * preferences, %FALSE otherwise
 **/
gboolean
ephy_web_view_load_homepage (EphyWebView *view)
{
  char *home;
  gboolean is_empty;

  g_signal_emit_by_name (view, "loading-homepage");

  home = eel_gconf_get_string (CONF_GENERAL_HOMEPAGE);

  if (home == NULL || home[0] == '\0')
  {
    g_free (home);

    home = g_strdup ("about:blank");
  }

  is_empty = ephy_embed_utils_url_is_empty (home);
  ephy_web_view_load_url (view, home);

  g_free (home);

  return is_empty;
}

static void
ephy_web_view_statusbar_update (EphyWebView *view, const char *text)
{
  EphyWebViewPrivate *priv;
  GdkWindow *window;
  GdkRectangle rect;

  priv = view->priv;

  if (priv->text)
    g_free (priv->text);

  priv->text = g_strdup (text);

  /* FIXME: we should invalidate the union of the sizes of the
   * rectangles of the previous and next statusbar text */
  window = gtk_widget_get_window (GTK_WIDGET (view));
  if (window) {
    GtkAllocation allocation;

    gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);

    rect = priv->text_rectangle;
    rect.width = allocation.width;
    if (rect.height == 0)
      rect.height = allocation.height;
    
    gdk_window_invalidate_rect (window, &rect, TRUE);
  }
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
ephy_web_view_statusbar_get_context_id (EphyWebView *view, const char  *context_description)
{
  char *string;
  guint id;

  g_return_val_if_fail (EPHY_IS_WEB_VIEW (view), 0);
  g_return_val_if_fail (context_description != NULL, 0);

  /* we need to preserve namespaces on object datas */
  string = g_strconcat ("ephy-web-view-status-bar-context:", context_description, NULL);

  id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (view), string));
  if (id == 0) {
    EphyWebViewPrivate *priv = view->priv;

    id = priv->seq_context_id++;
    g_object_set_data_full (G_OBJECT (view), string, GUINT_TO_POINTER (id), NULL);
    priv->keys = g_slist_prepend (priv->keys, string);
  } else
    g_free (string);

  return id;
}

guint
ephy_web_view_statusbar_push (EphyWebView *view, guint context_id, const char *text)
{
  EphyWebViewPrivate *priv;
  EphyWebViewStatusbarMsg *msg;

  g_return_val_if_fail (EPHY_IS_WEB_VIEW (view), 0);
  g_return_val_if_fail (context_id != 0, 0);
  g_return_val_if_fail (text != NULL, 0);

  priv = view->priv;

  msg = g_slice_new (EphyWebViewStatusbarMsg);
  msg->text = g_strdup (text);
  msg->context_id = context_id;
  msg->message_id = priv->seq_message_id++;

  priv->messages = g_slist_prepend (priv->messages, msg);

  ephy_web_view_statusbar_update (view, text);

  return msg->message_id;
}

void
ephy_web_view_statusbar_pop (EphyWebView *view, guint context_id)
{
  EphyWebViewPrivate *priv;
  EphyWebViewStatusbarMsg *msg;
  GSList *list;

  g_return_if_fail (EPHY_IS_WEB_VIEW (view));
  g_return_if_fail (context_id != 0);

  priv = view->priv;

  for (list = priv->messages; list; list = list->next) {
    EphyWebViewStatusbarMsg *msg = list->data;

    if (msg->context_id == context_id) {
      priv->messages = g_slist_remove_link (priv->messages, list);
      g_free (msg->text);
      g_slice_free (EphyWebViewStatusbarMsg, msg);
      g_slist_free_1 (list);
      break;
    }
  }

  msg = priv->messages ? priv->messages->data : NULL;
  ephy_web_view_statusbar_update (view, msg ? msg->text : NULL);
}



