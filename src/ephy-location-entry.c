/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002  Ricardo Fernández Pascual
 *  Copyright © 2003, 2004  Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005  Christian Persch
 *  Copyright © 2008  Xan López
 *  Copyright © 2016  Igalia S.L.
 *  Copyright © 2025  Jan-Michael Brummer
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
#include "ephy-location-entry.h"

#include "ephy-about-handler.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-pixbuf-utils.h"
#include "ephy-reader-handler.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-signal-accumulator.h"
#include "ephy-site-menu-button.h"
#include "ephy-suggestion.h"
#include "ephy-title-widget.h"
#include "ephy-uri-helpers.h"
#include "ephy-web-view.h"

#include <adwaita.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit/webkit.h>

#define PAGE_STEP 20

/**
 * SECTION:ephy-location-entry
 * @short_description: A location entry widget
 * @see_also: #GtkEntry
 *
 * #EphyLocationEntry implements the location bar in the main Epiphany window.
 */

struct _EphyLocationEntry {
  GtkBox parent_instance;

  GtkWidget *stack;

  /* Display */
  GtkWidget *url_button;
  GtkWidget *url_button_label;
  GtkWidget *site_menu_button;
  GtkWidget *opensearch_button;
  GtkWidget *combined_stop_reload_button;
  GtkWidget *progress_bar;
  GList *page_actions;

  /* Edit */
  GtkWidget *text;

  /* Popover */
  GtkWidget *suggestions_popover;
  GtkWidget *scrolled_window;
  GtkWidget *suggestions_view;
  GtkSingleSelection *suggestions_model;

  /* States */
  char *saved_text;
  char *jump_tab;

  guint progress_timeout;
  guint target_progress;
  guint current_progress;
  gdouble progress_fraction;

  gboolean insert_completion;

  gint idle_id;
  guint dns_prefetch_handle_id;
  guint can_redo : 1;

  EphySecurityLevel security_level;
  EphyAdaptiveMode adaptive_mode;

  GtkCssProvider *css_provider;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_ADDRESS,
  PROP_SECURITY_LEVEL,
  LAST_TITLE_WIDGET_PROP,
  LAST_PROP = PROP_ADDRESS
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum {
  ACTIVATE,
  USER_CHANGED,
  GET_LOCATION,
  GET_TITLE,
  LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };

static void ephy_location_entry_editable_init (GtkEditableInterface *iface);
static void ephy_location_entry_accessible_init (GtkAccessibleInterface *iface);
static void ephy_location_entry_title_widget_interface_init (EphyTitleWidgetInterface *iface);
static void ephy_location_entry_title_widget_set_address (EphyTitleWidget *widget,
                                                          const char      *address);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyLocationEntry, ephy_location_entry, GTK_TYPE_BOX,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                      ephy_location_entry_editable_init)
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE,
                                                      ephy_location_entry_accessible_init)
                               G_IMPLEMENT_INTERFACE (EPHY_TYPE_TITLE_WIDGET,
                                                      ephy_location_entry_title_widget_interface_init))

static void
on_editable_changed (GtkEditable *editable,
                     GtkEntry    *entry);

static void
ephy_location_entry_set_text (EphyLocationEntry *self,
                              const char        *text)
{
  g_signal_handlers_block_by_func (self->text, G_CALLBACK (on_editable_changed), self);
  gtk_editable_set_text (GTK_EDITABLE (self->text), text);
  g_signal_handlers_unblock_by_func (self->text, G_CALLBACK (on_editable_changed), self);
}

typedef struct {
  GUri *uri;
  EphyLocationEntry *entry;
} PrefetchHelper;

static void
free_prefetch_helper (PrefetchHelper *helper)
{
  g_uri_unref (helper->uri);
  g_object_unref (helper->entry);
  g_free (helper);
}

static gboolean
do_dns_prefetch (PrefetchHelper *helper)
{
  if (helper->uri) {
    EphyEmbedShell *shell = ephy_embed_shell_get_default ();
    webkit_network_session_prefetch_dns (ephy_embed_shell_get_network_session (shell), g_uri_get_host (helper->uri));
  }

  helper->entry->dns_prefetch_handle_id = 0;

  return G_SOURCE_REMOVE;
}

/*
 * Note: As we do not have access to WebKitNetworkProxyMode, and because
 * Epiphany does not ever change it, we are just checking system default proxy.
 */
static void
proxy_resolver_ready_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  PrefetchHelper *helper = user_data;
  GProxyResolver *resolver = G_PROXY_RESOLVER (object);
  g_autoptr (GError) error = NULL;
  g_auto (GStrv) proxies = NULL;

  proxies = g_proxy_resolver_lookup_finish (resolver, result, &error);
  if (error) {
    g_clear_pointer (&helper, free_prefetch_helper);
    return;
  }

  if (proxies && (g_strv_length (proxies) > 1 || g_strcmp0 (proxies[0], "direct://") != 0)) {
    g_clear_pointer (&helper, free_prefetch_helper);
    return;
  }

  g_clear_handle_id (&helper->entry->dns_prefetch_handle_id, g_source_remove);
  helper->entry->dns_prefetch_handle_id =
    g_timeout_add_full (G_PRIORITY_DEFAULT,
                        250,
                        (GSourceFunc)do_dns_prefetch,
                        helper,
                        (GDestroyNotify)free_prefetch_helper);
  g_source_set_name_by_id (helper->entry->dns_prefetch_handle_id, "[epiphany] do_dns_prefetch");
}

static void
schedule_dns_prefetch (EphyLocationEntry *self,
                       const char        *url)
{
  GProxyResolver *resolver = g_proxy_resolver_get_default ();
  PrefetchHelper *helper;
  g_autoptr (GUri) uri = NULL;

  if (!resolver)
    return;

  uri = g_uri_parse (url, G_URI_FLAGS_PARSE_RELAXED, NULL);
  if (!uri || !g_uri_get_host (uri))
    return;

  helper = g_new0 (PrefetchHelper, 1);
  helper->entry = g_object_ref (self);
  helper->uri = g_steal_pointer (&uri);

  g_proxy_resolver_lookup_async (resolver, url, NULL, proxy_resolver_ready_cb, helper);
}

static void
set_selected_suggestion_as_url (EphyLocationEntry *self)
{
  DzlSuggestion *suggestion;
  const gchar *uri;

  suggestion = gtk_single_selection_get_selected_item (self->suggestions_model);
  if (!suggestion)
    return;

  uri = dzl_suggestion_get_id (suggestion);

  g_clear_pointer (&self->jump_tab, g_free);

  if (g_str_has_prefix (uri, "ephy-tab://")) {
    self->jump_tab = g_strdup (uri);
    ephy_location_entry_set_text (self, dzl_suggestion_get_subtitle (suggestion));
  } else {
    if (ephy_suggestion_is_completion (EPHY_SUGGESTION (suggestion))) {
      ephy_location_entry_set_text (self, ephy_suggestion_get_unescaped_title (EPHY_SUGGESTION (suggestion)));
    } else {
      ephy_location_entry_set_text (self, dzl_suggestion_get_subtitle (suggestion));
    }
  }
  gtk_editable_set_position (GTK_EDITABLE (self), -1);

  schedule_dns_prefetch (self, uri);
}

static void
update_suggestions_popover (EphyLocationEntry *self)
{
  guint n_items;

  /* Skip item-changed updates when we are no longer in edit mode */
  if (g_strcmp0 (gtk_stack_get_visible_child_name (GTK_STACK (self->stack)), "edit") != 0 ||
      !gtk_widget_has_focus (GTK_WIDGET (gtk_editable_get_delegate (GTK_EDITABLE (self->text))))) {
    return;
  }

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->suggestions_model));

  if (n_items > 0) {
    gtk_widget_set_size_request (self->suggestions_popover, gtk_widget_get_width (GTK_WIDGET (self)), -1);

    gtk_popover_present (GTK_POPOVER (self->suggestions_popover));
    gtk_popover_popup (GTK_POPOVER (self->suggestions_popover));
  } else {
    gtk_popover_popdown (GTK_POPOVER (self->suggestions_popover));
  }
}

static guint
get_port_from_url (const char *url)
{
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GError) error = NULL;

  uri = g_uri_parse (url, G_URI_FLAGS_PARSE_RELAXED, &error);
  if (!uri)
    g_warning ("Failed to parse URL %s: %s", url, error->message);
  return g_uri_get_port (uri);
}

static char *
get_actual_display_address (EphyLocationEntry *self)
{
  g_autofree char *location = NULL;

  g_signal_emit (self, signals[GET_LOCATION], 0, &location);

  if (!location)
    return NULL;

  return ephy_uri_decode (location);
}

static void
update_url_button_style (EphyLocationEntry *self)
{
  const PangoRectangle empty_rect = {};
  g_autoptr (PangoAttrList) attrs = NULL;
  PangoAttribute *color_normal;
  PangoAttribute *color_dimmed;
  PangoAttribute *start_hidden;
  PangoAttribute *end_hidden;
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *text = NULL;
  const char *base_domain;
  const char *sub_string;
  g_autofree char *host = NULL;
  g_autofree char *port_str = NULL;
  gint port;

  attrs = pango_attr_list_new ();

  /* Button label is bold by default, reset to normal */
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_NORMAL));

  /* Complete text is dimmed */
  color_dimmed = pango_attr_foreground_alpha_new (32768);
  pango_attr_list_insert (attrs, color_dimmed);

  text = get_actual_display_address (self);
  if (!text || strlen (text) == 0 || ephy_embed_utils_is_no_show_address (text)) {
    gtk_label_set_text (GTK_LABEL (self->url_button_label), _("Search for websites, bookmarks, and open tabs"));
    gtk_label_set_attributes (GTK_LABEL (self->url_button_label), attrs);
    LOG ("Failed to update URL button style: URL is null, empty, or no-show");
    return;
  }

  host = ephy_uri_get_decoded_host (text);
  if (!host) {
    LOG ("Failed to get host component for URL %s", text);
    goto out;
  }
  port = get_port_from_url (text);

  base_domain = ephy_uri_get_base_domain (host);
  if (!base_domain) {
    LOG ("Failed to update URL button style: failed to get base domain for URL %s: %s", text, error->message);
    goto out;
  }

  sub_string = strstr (text, base_domain);
  if (!sub_string) {
    LOG ("Failed to update URL button style: failed to find base domain %s in URL %s (is there no public suffix?)", base_domain, text);
    goto out;
  }

  if (text && strlen (text) > 0) {
    /* Base domain with normal style */
    color_normal = pango_attr_foreground_alpha_new (65535);
    color_normal->start_index = sub_string - text;
    color_normal->end_index = color_normal->start_index + strlen (base_domain);
    pango_attr_list_insert (attrs, color_normal);

    if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ALWAYS_SHOW_FULL_URL))
      goto out;

    /* Scheme is hidden */
    start_hidden = pango_attr_shape_new (&empty_rect, &empty_rect);
    start_hidden->start_index = 0;
    start_hidden->end_index = strstr (text, host) - text;
    pango_attr_list_insert (attrs, start_hidden);

    /* Everything after the port is hidden */
    end_hidden = pango_attr_shape_new (&empty_rect, &empty_rect);
    end_hidden->start_index = color_normal->end_index;
    end_hidden->end_index = strlen (text);
    pango_attr_list_insert (attrs, end_hidden);

    if (port != -1) {
      port_str = g_strdup_printf (":%i", port);
      end_hidden->start_index = end_hidden->start_index + strlen (port_str);
    }
  }

out:
  if (g_settings_get_boolean (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_ALWAYS_SHOW_FULL_URL))
    gtk_label_set_xalign (GTK_LABEL (self->url_button_label), 0);
  else
    gtk_label_set_xalign (GTK_LABEL (self->url_button_label), 0.5);

  gtk_label_set_text (GTK_LABEL (self->url_button_label), text);
  gtk_label_set_attributes (GTK_LABEL (self->url_button_label), attrs);
}

static void
on_focus_enter (GtkEventControllerFocus *controller,
                EphyLocationEntry       *self)
{
  EphyWindow *window;

  window = EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  if (window) {
    EphyEmbed *active_embed = ephy_window_get_active_embed (window);
    const char *typed_input = ephy_embed_get_typed_input (active_embed);

    if (typed_input) {
      ephy_location_entry_title_widget_set_address (EPHY_TITLE_WIDGET (self), typed_input);
    } else {
      EphyWebView *web_view = ephy_embed_get_web_view (active_embed);
      const char *text = ephy_web_view_get_address (web_view);

      if (!ephy_embed_utils_is_no_show_address (text))
        gtk_editable_set_text (GTK_EDITABLE (self->text), ephy_web_view_get_display_address (web_view));
      else
        gtk_editable_set_text (GTK_EDITABLE (self->text), "");
    }
  }
}

static void
on_focus_leave (GtkEventControllerFocus *controller,
                EphyLocationEntry       *self)
{
  GtkWidget *focus_widget = gtk_root_get_focus (gtk_widget_get_root (GTK_WIDGET (self)));

  if (focus_widget && gtk_widget_has_focus (self->text)
      && gtk_widget_is_ancestor (focus_widget, GTK_WIDGET (self->text)))
    return;

  g_clear_handle_id (&self->idle_id, g_source_remove);

  gtk_popover_popdown (GTK_POPOVER (self->suggestions_popover));

  /* Only switch to display if the window is still focused. The visible
   * child stays at edit if the user navigated outside the window. */
  if (gtk_widget_has_focus (self->text) || !GTK_IS_WIDGET (focus_widget)) {
    update_url_button_style (self);
    gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "display");
  }
}

/*
 * Handles keyboard navigation in suggestions popover
 */
static gboolean
on_key_pressed (EphyLocationEntry     *self,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                GtkEventControllerKey *controller)
{
  gint selected, matches;

  if (state & (GDK_SHIFT_MASK | GDK_ALT_MASK | GDK_CONTROL_MASK))
    return FALSE;

  if (keyval == GDK_KEY_Escape) {
    ephy_location_entry_reset (self);
    on_focus_leave (NULL, self);
    return TRUE;
  }

  if (keyval != GDK_KEY_Up && keyval != GDK_KEY_KP_Up &&
      keyval != GDK_KEY_Down && keyval != GDK_KEY_KP_Down &&
      keyval != GDK_KEY_Page_Up && keyval != GDK_KEY_KP_Page_Up &&
      keyval != GDK_KEY_Page_Down && keyval != GDK_KEY_KP_Page_Down) {
    return FALSE;
  }

  if (!gtk_widget_get_visible (self->suggestions_popover))
    return FALSE;

  matches = g_list_model_get_n_items (G_LIST_MODEL (self->suggestions_model));
  selected = gtk_single_selection_get_selected (self->suggestions_model);

  if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up) {
    if (selected <= 0)
      selected = matches - 1;
    else
      selected--;
  } else if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down) {
    if (selected < matches - 1)
      selected++;
    else if (selected == matches - 1)
      selected = 0;
    else
      selected = -1;
  } else if (keyval == GDK_KEY_Page_Up || keyval == GDK_KEY_KP_Page_Up) {
    if (selected == -1)
      selected = matches - 1;
    else if (selected == 0)
      selected = -1;
    else if (selected < PAGE_STEP)
      selected = 0;
    else
      selected -= PAGE_STEP;
  } else if (keyval == GDK_KEY_Page_Down || keyval == GDK_KEY_KP_Page_Down) {
    if (selected == -1)
      selected = 0;
    else if (selected == matches - 1)
      selected = -1;
    else if (selected + PAGE_STEP > matches - 1)
      selected = matches - 1;
    else
      selected += PAGE_STEP;
  }

  if (selected < 0) {
    /* Unselect and restore text */
    ephy_location_entry_reset (self);
    gtk_single_selection_set_selected (self->suggestions_model, GTK_INVALID_LIST_POSITION);
  } else if (selected < matches) {
    gtk_single_selection_set_selected (self->suggestions_model, selected);
    gtk_list_view_scroll_to (GTK_LIST_VIEW (self->suggestions_view), selected, GTK_LIST_SCROLL_NONE, NULL);
    set_selected_suggestion_as_url (self);
  }
  return TRUE;
}

static GIcon *
get_suggestion_icon (GtkListItem *item,
                     GIcon       *icon)
{
  DzlSuggestion *suggestion = gtk_list_item_get_item (item);
  GtkWidget *widget = gtk_list_item_get_child (item);
  cairo_surface_t *surface;

  surface = dzl_suggestion_get_icon_surface (suggestion, widget);

  if (surface) {
    int width, height;

    if (cairo_surface_get_type (surface) != CAIRO_SURFACE_TYPE_IMAGE)
      return NULL;

    width = cairo_image_surface_get_width (surface);
    height = cairo_image_surface_get_height (surface);

    return G_ICON (ephy_get_pixbuf_from_surface (surface, 0, 0, width, height));
  }

  if (icon)
    return g_object_ref (icon);

  return NULL;
}

static void
emit_activate (EphyLocationEntry *self,
               GdkModifierType    modifiers)
{
  if (self->jump_tab) {
    ephy_location_entry_set_text (self, self->jump_tab);
    g_clear_pointer (&self->jump_tab, g_free);
  } else {
    g_autofree char *text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self)));
    const char *url = g_strstrip (text);
    g_autofree gchar *new_url = NULL;

    ephy_location_entry_set_text (self, self->jump_tab ? self->jump_tab : text);

    if (strlen (url) > 5 && g_str_has_prefix (url, "http:") && url[5] != '/')
      new_url = g_strdup_printf ("http://%s", url + 5);
    else if (strlen (url) > 6 && g_str_has_prefix (url, "https:") && url[6] != '/')
      new_url = g_strdup_printf ("https://%s", url + 6);

    if (new_url)
      ephy_location_entry_set_text (self, new_url);

    if (modifiers == GDK_CONTROL_MASK) {
      /* Remove control mask to prevent opening address in a new window */
      modifiers &= ~GDK_CONTROL_MASK;

      if (!g_utf8_strchr (url, -1, ' ') && !g_utf8_strchr (url, -1, '.')) {
        g_autofree gchar *new_url = g_strdup_printf ("www.%s.com", url);

        ephy_location_entry_set_text (self, new_url);
      }
    }

    g_signal_emit (self, signals[ACTIVATE], 0, modifiers);
  }
}

static gboolean
activate_shortcut_cb (EphyLocationEntry *self,
                      GVariant          *args)
{
  emit_activate (self, g_variant_get_int32 (args));

  return TRUE;
}

static char *
compute_prefix (EphyLocationEntry *self,
                const char        *key)
{
  int n_items = g_list_model_get_n_items (G_LIST_MODEL (self->suggestions_model));
  g_autofree char *prefix = NULL;

  for (int idx = 0; idx < n_items; idx++) {
    g_autoptr (EphySuggestion) suggestion = NULL;
    g_autofree char *text = NULL;
    const char *subtitle;

    suggestion = g_list_model_get_item (G_LIST_MODEL (self->suggestions_model), idx);
    subtitle = ephy_suggestion_get_subtitle (suggestion);
    if (!subtitle)
      continue;

    if (g_str_has_prefix (subtitle, key)) {
      text = g_strdup (subtitle);
    } else {
      g_autoptr (GError) error = NULL;
      g_autoptr (GUri) uri = g_uri_parse (subtitle, G_URI_FLAGS_PARSE_RELAXED, &error);
      g_autofree char *base = NULL;

      if (error) {
        LOG ("Could not parse url: %s", error->message);
        continue;
      }

      if (!g_uri_get_host (uri))
        continue;

      base = ephy_uri_get_base_domain (g_uri_get_host (uri));
      if (!base) {
        LOG ("Could not get base domain for host %s", g_uri_get_host (uri));
        continue;
      }

      if (g_str_has_prefix (base, key))
        text = g_strdup (base);
    }

    if (!text)
      continue;

    if (!prefix) {
      prefix = g_strdup (text);
    } else {
      char *p = prefix;
      char *q = text;

      while (*p && *p == *q) {
        p++;
        q++;
      }

      *p = '\0';

      if (p > prefix) {
        q = g_utf8_find_prev_char (prefix, p);

        switch  (g_utf8_get_char_validated (q, p - q)) {
          case (gunichar) - 2:
          case (gunichar) - 1:
            *q = 0;
            break;
          default:
            break;
        }
      }
    }
  }

  return g_steal_pointer (&prefix);
}

static int
calc_and_set_prefix (gpointer user_data)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (user_data);
  char *prefix;

  prefix = compute_prefix (self, gtk_editable_get_text (GTK_EDITABLE (self->text)));
  if (prefix) {
    int key_len;
    int prefix_len;
    const char *key;

    prefix_len = g_utf8_strlen (prefix, -1);

    key = gtk_editable_get_text (GTK_EDITABLE (self));
    key_len = g_utf8_strlen (key, -1);

    if (prefix_len > key_len) {
      int pos = prefix_len;

      g_signal_handlers_block_by_func (self, G_CALLBACK (on_editable_changed), self);
      gtk_editable_insert_text (GTK_EDITABLE (self), prefix + strlen (key), -1, &pos);
      gtk_editable_select_region (GTK_EDITABLE (self), key_len, prefix_len);
      g_signal_handlers_unblock_by_func (self, G_CALLBACK (on_editable_changed), self);
    }
  }

  self->idle_id = 0;

  return G_SOURCE_REMOVE;
}

static void
on_editable_changed (GtkEditable *editable,
                     GtkEntry    *entry)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (gtk_widget_get_ancestor (GTK_WIDGET (entry), EPHY_TYPE_LOCATION_ENTRY));
  EphyWindow *window = EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  const char *text = gtk_editable_get_text (editable);

  if (text && strlen (text) > 0)
    gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, "edit-clear-symbolic");
  else
    gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, NULL);

  g_clear_handle_id (&self->idle_id, g_source_remove);
  g_signal_emit (self, signals[USER_CHANGED], 0, text);

  if (window) {
    EphyEmbed *embed = ephy_window_get_active_embed (EPHY_WINDOW (window));
    const char *text = gtk_editable_get_text (editable);

    ephy_embed_set_typed_input (embed, text);
  }

  if (self->insert_completion)
    self->idle_id = g_idle_add_full (G_PRIORITY_HIGH, calc_and_set_prefix, self, NULL);
}

static void
on_suggestions_popover_notify_visible (EphyLocationEntry *self)
{
  GtkAdjustment *adj;

  if (!gtk_widget_get_visible (self->suggestions_popover)) {
    gtk_single_selection_set_selected (self->suggestions_model, GTK_INVALID_LIST_POSITION);
    return;
  }

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->scrolled_window));

  g_assert (adj);

  gtk_adjustment_set_value (adj, 0);
}

static void
on_suggestion_activated (EphyLocationEntry *self,
                         guint              position)
{
  g_autoptr (EphySuggestion) suggestion = NULL;
  const gchar *text;

  suggestion = g_list_model_get_item (G_LIST_MODEL (self->suggestions_model), position);
  text = ephy_suggestion_get_uri (suggestion);

  ephy_location_entry_set_text (self, self->jump_tab ? self->jump_tab : text);

  g_clear_pointer (&self->jump_tab, g_free);

  /* Now trigger the load.... */
  emit_activate (self, 0);
}

static void
on_activate (EphyLocationEntry *self)
{
  if (gtk_widget_get_visible (self->suggestions_popover)) {
    guint position = gtk_single_selection_get_selected (self->suggestions_model);

    if (position != GTK_INVALID_LIST_POSITION) {
      on_suggestion_activated (self, position);
      return;
    }
  }

  emit_activate (self, 0);
}

static void
on_item_pressed (GtkGesture  *gesture,
                 int          n_click,
                 double       x,
                 double       y,
                 GtkListItem *item)
{
  GtkWidget *widget;
  EphyLocationEntry *self;
  guint position;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  self = EPHY_LOCATION_ENTRY (gtk_widget_get_ancestor (widget, EPHY_TYPE_LOCATION_ENTRY));
  position = gtk_list_item_get_position (item);

  gtk_single_selection_set_selected (self->suggestions_model, position);
}

static void
on_item_released (GtkGesture  *gesture,
                  int          n_click,
                  double       x,
                  double       y,
                  GtkListItem *item)
{
  GtkWidget *widget;
  EphyLocationEntry *self;
  guint position;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  self = EPHY_LOCATION_ENTRY (gtk_widget_get_ancestor (widget, EPHY_TYPE_LOCATION_ENTRY));
  position = gtk_list_item_get_position (item);

  /* We only want to handle clicks with press and release on the same row */
  if (!gtk_widget_contains (widget, x, y)) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);
  on_suggestion_activated (self, position);
}

static void
paste_received (GdkClipboard      *clipboard,
                GAsyncResult      *result,
                EphyLocationEntry *self)
{
  g_autofree char *text = NULL;

  text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (clipboard), result, NULL);
  if (!text) {
    gtk_widget_error_bell (GTK_WIDGET (self));
    return;
  }

  ephy_location_entry_set_text (self, text);
  emit_activate (self, 0);

  g_object_unref (self);
}

static void
paste_and_go_activate (EphyLocationEntry *self)
{
  GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self));

  gdk_clipboard_read_text_async (clipboard, NULL,
                                 (GAsyncReadyCallback)paste_received,
                                 g_object_ref (self));
}

static gboolean
ephy_location_entry_focus (GtkWidget        *widget,
                           GtkDirectionType  direction)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (widget);

  if (gtk_widget_get_visible (self->suggestions_popover) && (direction == GTK_DIR_TAB_FORWARD ||
                                                             direction == GTK_DIR_TAB_BACKWARD)) {
    gint selected, matches;

    matches = g_list_model_get_n_items (G_LIST_MODEL (self->suggestions_model));
    selected = gtk_single_selection_get_selected (self->suggestions_model);

    if (direction == GTK_DIR_TAB_FORWARD) {
      if (selected < matches - 1)
        selected++;
      else
        selected = -1;
    } else {
      if (selected < 0)
        selected = matches - 1;
      else
        selected--;
    }

    if (selected < 0) {
      /* Unselect and restore text */
      ephy_location_entry_reset (self);
      gtk_single_selection_set_selected (self->suggestions_model, GTK_INVALID_LIST_POSITION);
    } else if (selected < matches) {
      gtk_single_selection_set_selected (self->suggestions_model, selected);
      gtk_list_view_scroll_to (GTK_LIST_VIEW (self->suggestions_view), selected, GTK_LIST_SCROLL_NONE, NULL);
      set_selected_suggestion_as_url (self);
    }

    return TRUE;
  }

  return GTK_WIDGET_CLASS (ephy_location_entry_parent_class)->focus (widget, direction);
}

static void
ephy_location_entry_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (object);

  if (gtk_editable_delegate_set_property (object, prop_id, value, pspec)) {
    if (prop_id == LAST_TITLE_WIDGET_PROP + GTK_EDITABLE_PROP_EDITABLE) {
      gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                      GTK_ACCESSIBLE_PROPERTY_READ_ONLY, !g_value_get_boolean (value),
                                      -1);
    }
    return;
  }

  switch (prop_id) {
    case PROP_MODEL:
      ephy_location_entry_set_model (self, g_value_get_object (value));
      break;
    case PROP_ADDRESS:
      ephy_title_widget_set_address (EPHY_TITLE_WIDGET (self),
                                     g_value_get_string (value));
      break;
    case PROP_SECURITY_LEVEL:
      ephy_title_widget_set_security_level (EPHY_TITLE_WIDGET (self),
                                            g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_location_entry_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (object);

  if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
    return;

  switch (prop_id) {
    case PROP_MODEL:
      g_value_set_object (value, ephy_location_entry_get_model (self));
      break;
    case PROP_ADDRESS:
      g_value_set_string (value, ephy_title_widget_get_address (EPHY_TITLE_WIDGET (self)));
      break;
    case PROP_SECURITY_LEVEL:
      g_value_set_enum (value, ephy_title_widget_get_security_level (EPHY_TITLE_WIDGET (self)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_location_entry_dispose (GObject *object)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (object);

  g_clear_pointer (&self->suggestions_popover, gtk_widget_unparent);

  if (self->text) {
    gtk_editable_finish_delegate (GTK_EDITABLE (self));
    self->text = NULL;
  }

  g_clear_handle_id (&self->progress_timeout, g_source_remove);
  ephy_location_entry_page_action_clear (self);

  G_OBJECT_CLASS (ephy_location_entry_parent_class)->dispose (object);
}

static void
ephy_location_entry_finalize (GObject *object)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (object);

  g_free (self->saved_text);
  g_free (self->jump_tab);

  G_OBJECT_CLASS (ephy_location_entry_parent_class)->finalize (object);
}

static inline void
register_activate_shortcuts (GtkWidgetClass  *widget_class,
                             GdkModifierType  modifiers)
{
  gtk_widget_class_add_binding (widget_class, GDK_KEY_Return, modifiers,
                                (GtkShortcutFunc)activate_shortcut_cb,
                                "i", modifiers);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_ISO_Enter, modifiers,
                                (GtkShortcutFunc)activate_shortcut_cb,
                                "i", modifiers);
  gtk_widget_class_add_binding (widget_class, GDK_KEY_KP_Enter, modifiers,
                                (GtkShortcutFunc)activate_shortcut_cb,
                                "i", modifiers);
}

static void
on_url_button_clicked (GtkButton *button,
                       gpointer   user_data)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (user_data);

  ephy_location_entry_grab_focus (self);
}

static void
on_icon_press (GtkEntry             *entry,
               GtkEntryIconPosition  icon_pos,
               gpointer              user_data)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (user_data);

  if (icon_pos != GTK_ENTRY_ICON_SECONDARY)
    return;

  ephy_location_entry_set_text (self, "");
}

static void
on_insert_text (GtkEditable *editable,
                gchar       *text,
                gint         length,
                gint        *position,
                gpointer     user_data)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (user_data);
  self->insert_completion = TRUE;
}

static void
on_delete_text (GtkEditable *editable,
                gint         start_pos,
                gint         end_pos,
                gpointer     user_data)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (user_data);
  self->insert_completion = FALSE;
}

static void
middle_click_pressed_cb (GtkGesture *gesture)
{
  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
middle_click_released_cb (GtkGesture        *gesture,
                          int                n_click,
                          double             x,
                          double             y,
                          EphyLocationEntry *self)
{
  GtkWidget *widget;
  EphyWindow *window;
  GActionGroup *action_group;
  GAction *action;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  if (!gtk_widget_contains (widget, x, y)) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  window = EPHY_WINDOW (gtk_widget_get_root (widget));
  action_group = ephy_window_get_action_group (window, "toolbar");
  action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "duplicate-tab");
  g_action_activate (action, NULL);
}

static void
ephy_location_entry_class_init (EphyLocationEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ephy_location_entry_get_property;
  object_class->set_property = ephy_location_entry_set_property;
  object_class->dispose = ephy_location_entry_dispose;
  object_class->finalize = ephy_location_entry_finalize;

  widget_class->focus = ephy_location_entry_focus;

  g_object_class_override_property (object_class, PROP_ADDRESS, "address");
  g_object_class_override_property (object_class, PROP_SECURITY_LEVEL, "security-level");

  props[PROP_MODEL] =
    g_param_spec_object ("model",
                         NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_editable_install_properties (object_class, LAST_TITLE_WIDGET_PROP);

  /**
   * EphyLocationEntry::activate:
   * @flags: the #GdkModifierType from the activation event
   *
   * Emitted when the entry is activated.
   *
   */
  signals[ACTIVATE] = g_signal_new ("activate", G_OBJECT_CLASS_TYPE (klass),
                                    G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                                    0, NULL, NULL, NULL,
                                    G_TYPE_NONE,
                                    1,
                                    GDK_TYPE_MODIFIER_TYPE);

  /**
   * EphyLocationEntry::user-changed:
   * @entry: the object on which the signal is emitted
   *
   * Emitted when the user changes the contents of the internal #GtkEntry
   *
   */
  signals[USER_CHANGED] = g_signal_new ("user_changed", G_OBJECT_CLASS_TYPE (klass),
                                        G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                                        0, NULL, NULL, NULL,
                                        G_TYPE_NONE,
                                        1,
                                        G_TYPE_STRING);

  /**
   * EphyLocationEntry::get-location:
   * @entry: the object on which the signal is emitted
   * Returns: the current page address as a string
   *
   * For drag and drop purposes, the location bar will request you the
   * real address of where it is pointing to. The signal handler for this
   * function should return the address of the currently loaded site.
   *
   */
  signals[GET_LOCATION] = g_signal_new ("get-location", G_OBJECT_CLASS_TYPE (klass),
                                        G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                                        0, ephy_signal_accumulator_string,
                                        NULL, NULL,
                                        G_TYPE_STRING,
                                        0,
                                        G_TYPE_NONE);

  /**
   * EphyLocationEntry::get-title:
   * @entry: the object on which the signal is emitted
   * Returns: the current page title as a string
   *
   * For drag and drop purposes, the location bar will request you the
   * title of where it is pointing to. The signal handler for this
   * function should return the title of the currently loaded site.
   *
   */
  signals[GET_TITLE] = g_signal_new ("get-title", G_OBJECT_CLASS_TYPE (klass),
                                     G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                                     0, ephy_signal_accumulator_string,
                                     NULL, NULL,
                                     G_TYPE_STRING,
                                     0,
                                     G_TYPE_NONE);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/location-entry.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, stack);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, text);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, site_menu_button);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, suggestions_popover);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, suggestions_model);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, suggestions_view);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, url_button_label);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, combined_stop_reload_button);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, opensearch_button);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, url_button);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, progress_bar);

  gtk_widget_class_bind_template_callback (widget_class, on_editable_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_activate);
  gtk_widget_class_bind_template_callback (widget_class, on_suggestions_popover_notify_visible);
  gtk_widget_class_bind_template_callback (widget_class, on_suggestion_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_focus_enter);
  gtk_widget_class_bind_template_callback (widget_class, on_focus_leave);
  gtk_widget_class_bind_template_callback (widget_class, on_key_pressed);
  gtk_widget_class_bind_template_callback (widget_class, on_item_pressed);
  gtk_widget_class_bind_template_callback (widget_class, on_item_released);
  gtk_widget_class_bind_template_callback (widget_class, on_url_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_icon_press);
  gtk_widget_class_bind_template_callback (widget_class, on_insert_text);
  gtk_widget_class_bind_template_callback (widget_class, on_delete_text);
  gtk_widget_class_bind_template_callback (widget_class, get_suggestion_icon);
  gtk_widget_class_bind_template_callback (widget_class, update_suggestions_popover);
  gtk_widget_class_bind_template_callback (widget_class, middle_click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, middle_click_released_cb);

  gtk_widget_class_install_action (widget_class, "clipboard.paste-and-go", NULL, (GtkWidgetActionActivateFunc)paste_and_go_activate);

  register_activate_shortcuts (widget_class, GDK_CONTROL_MASK);
  register_activate_shortcuts (widget_class, GDK_ALT_MASK);
  register_activate_shortcuts (widget_class, GDK_SHIFT_MASK | GDK_CONTROL_MASK);
  register_activate_shortcuts (widget_class, GDK_SHIFT_MASK | GDK_ALT_MASK);

  gtk_widget_class_add_binding (widget_class, GDK_KEY_Escape, 0,
                                (GtkShortcutFunc)ephy_location_entry_reset,
                                NULL);
}

static void
ephy_location_entry_init (EphyLocationEntry *self)
{
  LOG ("EphyLocationEntry initializing %p", self);

  self->saved_text = NULL;
  self->page_actions = NULL;
  self->adaptive_mode = EPHY_ADAPTIVE_MODE_NORMAL;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (G_OBJECT (gtk_editable_get_delegate (GTK_EDITABLE (self->text))), "delete-text", G_CALLBACK (on_delete_text), self, 0);
  g_signal_connect_object (G_OBJECT (gtk_editable_get_delegate (GTK_EDITABLE (self->text))), "insert-text", G_CALLBACK (on_insert_text), self, 0);
}

static const char *
ephy_location_entry_title_widget_get_address (EphyTitleWidget *widget)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (widget);
  return gtk_editable_get_text (GTK_EDITABLE (self->text));
}

static void
ephy_location_entry_do_set_address (EphyTitleWidget *widget,
                                    const char      *address,
                                    gboolean         force)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (widget);
  g_autofree char *effective_text = NULL;
  const char *text = "";
  const char *final_text;

  if (!force && ephy_location_entry_has_focus (self))
    return;

  if (address) {
    if (g_str_has_prefix (address, EPHY_ABOUT_SCHEME))
      effective_text = g_strdup_printf ("about:%s", address + strlen (EPHY_ABOUT_SCHEME) + 1);
    else if (g_str_has_prefix (address, EPHY_READER_SCHEME))
      effective_text = g_strdup (address + strlen (EPHY_READER_SCHEME) + 1);

    text = address;
  }

  final_text = effective_text ? effective_text : text;

  ephy_location_entry_set_text (self, final_text);
  update_url_button_style (self);

  if (final_text != gtk_editable_get_text (GTK_EDITABLE (self->text)))
    ephy_site_menu_button_set_do_animation (EPHY_SITE_MENU_BUTTON (self->site_menu_button), TRUE);

  gtk_popover_popdown (GTK_POPOVER (self->suggestions_popover));
}

static void
ephy_location_entry_title_widget_set_address (EphyTitleWidget *widget,
                                              const char      *address)
{
  ephy_location_entry_do_set_address (widget, address, FALSE);
}

static EphySecurityLevel
ephy_location_entry_title_widget_get_security_level (EphyTitleWidget *widget)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (widget);
  return self->security_level;
}

static void
ephy_location_entry_title_widget_set_security_level (EphyTitleWidget   *widget,
                                                     EphySecurityLevel  security_level)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (widget);
  unsigned int state = 0;
  const char *description;

  self->security_level = security_level;

  if (security_level == EPHY_SECURITY_LEVEL_NO_SECURITY
      || security_level == EPHY_SECURITY_LEVEL_MIXED_CONTENT
      || security_level == EPHY_SECURITY_LEVEL_UNACCEPTABLE_CERTIFICATE)
    state = 2;

  if (self->security_level == EPHY_SECURITY_LEVEL_STRONG_SECURITY)
    description = _("Secure Site");
  else
    description = _("Insecure Site");

  ephy_site_menu_button_clear_description (EPHY_SITE_MENU_BUTTON (self->site_menu_button));
  ephy_site_menu_button_append_description (EPHY_SITE_MENU_BUTTON (self->site_menu_button), description);
  ephy_site_menu_button_set_state (EPHY_SITE_MENU_BUTTON (self->site_menu_button), state);
}

static void
ephy_location_entry_title_widget_interface_init (EphyTitleWidgetInterface *iface)
{
  iface->get_address = ephy_location_entry_title_widget_get_address;
  iface->set_address = ephy_location_entry_title_widget_set_address;
  iface->get_security_level = ephy_location_entry_title_widget_get_security_level;
  iface->set_security_level = ephy_location_entry_title_widget_set_security_level;
}

static GtkEditable *
ephy_location_entry_get_delegate (GtkEditable *editable)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (editable);

  return GTK_EDITABLE (self->text);
}

static void
ephy_location_entry_editable_init (GtkEditableInterface *iface)
{
  iface->get_delegate = ephy_location_entry_get_delegate;
}

static gboolean
ephy_location_entry_accessible_get_platform_state (GtkAccessible              *accessible,
                                                   GtkAccessiblePlatformState  state)
{
  return gtk_editable_delegate_get_accessible_platform_state (GTK_EDITABLE (accessible), state);
}

static void
ephy_location_entry_accessible_init (GtkAccessibleInterface *iface)
{
  iface->get_platform_state = ephy_location_entry_accessible_get_platform_state;
}

GtkWidget *
ephy_location_entry_new (void)
{
  return GTK_WIDGET (g_object_new (EPHY_TYPE_LOCATION_ENTRY, NULL));
}

/**
 * ephy_location_entry_get_can_redo:
 * @entry: an #EphyLocationEntry widget
 *
 * Whether @entry can restore the displayed text to the user modified version
 * before the undo.
 *
 * Return value: TRUE or FALSE indicating if the text can be restored
 *
 **/
gboolean
ephy_location_entry_get_can_redo (EphyLocationEntry *self)
{
  return self->can_redo;
}

/**
 * ephy_location_entry_undo_reset:
 * @entry: an #EphyLocationEntry widget
 *
 * Undo a previous ephy_location_entry_reset.
 *
 **/
void
ephy_location_entry_undo_reset (EphyLocationEntry *self)
{
  ephy_location_entry_set_text (self, self->saved_text);
  self->can_redo = FALSE;
}

/**
 * ephy_location_entry_reset:
 * @entry: an #EphyLocationEntry widget
 *
 * Restore the @entry to the text corresponding to the current location, this
 * does not fire the user_changed signal. This is called each time the user
 * presses Escape while the location entry is selected. If the entry is already
 * reset, the web view is focused.
 *
 * Return value: TRUE on success, FALSE otherwise
 *
 **/
gboolean
ephy_location_entry_reset (EphyLocationEntry *self)
{
  const char *text, *old_text;
  int position, offset;
  g_autofree char *url = NULL;

  url = get_actual_display_address (self);
  text = !!url ? url : "";
  old_text = gtk_editable_get_text (GTK_EDITABLE (self));
  old_text = !!old_text ? old_text : "";

  if (g_strcmp0 (text, old_text) == 0) {
    EphyWindow *window = EPHY_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
    EphyEmbed *embed = ephy_window_get_active_embed (window);
    EphyWebView *web_view = ephy_embed_get_web_view (embed);

    gtk_widget_grab_focus (GTK_WIDGET (web_view));
  }

  g_free (self->saved_text);
  self->saved_text = g_strdup (old_text);
  self->can_redo = TRUE;

  offset = strlen (text) - strlen (old_text);
  position = gtk_editable_get_position (GTK_EDITABLE (self));
  ephy_location_entry_do_set_address (EPHY_TITLE_WIDGET (self), text, TRUE);
  gtk_editable_set_position (GTK_EDITABLE (self), position + offset);

  return g_strcmp0 (text, old_text);
}

/**
 * ephy_location_entry_grab_focus:
 * @entry: an #EphyLocationEntry widget
 *
 * Set focus on @entry and select the text within. This is called when the
 * user hits Control+L.
 *
 **/
void
ephy_location_entry_grab_focus (EphyLocationEntry *self)
{
  gtk_widget_grab_focus (self->text);
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "edit");
}

static void
progress_hide (gpointer user_data)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (user_data);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), 0);
  gtk_widget_set_visible (self->progress_bar, FALSE);

  g_clear_handle_id (&self->progress_timeout, g_source_remove);
}

static void
ephy_location_entry_set_fraction_internal (gpointer user_data)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (user_data);
  gint ms;
  gdouble progress;
  gdouble current;

  self->progress_timeout = 0;
  current = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (self->progress_bar));

  /* Try to animated progress update, so increase by 0.25 and only if the change is big
   * enough update it immediately.
   */
  if ((self->progress_fraction - current) > 0.5 || self->progress_fraction == 1.0)
    ms = 10;
  else
    ms = 25;

  progress = current + 0.025;
  if (progress < self->progress_fraction) {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), progress);
    self->progress_timeout = g_timeout_add_once (ms, ephy_location_entry_set_fraction_internal, self);
  } else {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), self->progress_fraction);
    if (self->progress_fraction == 1.0)
      self->progress_timeout = g_timeout_add_once (500, progress_hide, self);
  }

  gtk_widget_set_visible (self->progress_bar, TRUE);
}

void
ephy_location_entry_set_progress (EphyLocationEntry *self,
                                  gdouble            fraction,
                                  gboolean           loading)
{
  gdouble current_progress;

  g_clear_handle_id (&self->progress_timeout, g_source_remove);

  if (!loading) {
    /* Setting progress to 0 when it is already 0 can actually cause the
     * progress bar to be shown. Yikes....
     */
    current_progress = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (self->progress_bar));
    if (current_progress != 0.0) {
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), 0.0);
      gtk_widget_set_visible (self->progress_bar, FALSE);
    }
    return;
  }

  self->progress_fraction = fraction;
  ephy_location_entry_set_fraction_internal (self);
}

void
ephy_location_entry_set_adaptive_mode (EphyLocationEntry *self,
                                       EphyAdaptiveMode   adaptive_mode)
{
  self->adaptive_mode = adaptive_mode;
  gtk_popover_set_position (GTK_POPOVER (self->suggestions_popover), adaptive_mode == EPHY_ADAPTIVE_MODE_NARROW ? GTK_POS_TOP : GTK_POS_BOTTOM);
}

void
ephy_location_entry_page_action_add (EphyLocationEntry *self,
                                     GtkWidget         *action)
{
  self->page_actions = g_list_prepend (self->page_actions, action);

  gtk_widget_set_parent (action, GTK_WIDGET (self));
}

void
ephy_location_entry_page_action_remove (EphyLocationEntry *self,
                                        GtkWidget         *action)
{
  self->page_actions = g_list_remove (self->page_actions, action);

  gtk_widget_unparent (action);
}

void
ephy_location_entry_page_action_clear (EphyLocationEntry *self)
{
  if (self->page_actions)
    g_list_free_full (self->page_actions, (GDestroyNotify)gtk_widget_unparent);

  self->page_actions = NULL;
}

void
ephy_location_entry_grab_focus_without_selecting (EphyLocationEntry *self)
{
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "edit");
  gtk_entry_grab_focus_without_selecting (GTK_ENTRY (self->text));
}

GListModel *
ephy_location_entry_get_model (EphyLocationEntry *self)
{
  return gtk_single_selection_get_model (self->suggestions_model);
}

void
ephy_location_entry_set_model (EphyLocationEntry *self,
                               GListModel        *model)
{
  gtk_single_selection_set_model (self->suggestions_model, model);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_MODEL]);
}

/* Translators: tooltip for the refresh button */
static const char *REFRESH_BUTTON_TOOLTIP = N_("Reload");

void
ephy_location_entry_start_change_combined_stop_reload_state (EphyLocationEntry *self,
                                                             gboolean           loading)
{
  if (loading) {
    gtk_button_set_icon_name (GTK_BUTTON (self->combined_stop_reload_button), "process-stop-symbolic");
    /* Translators: tooltip for the stop button */
    gtk_widget_set_tooltip_text (self->combined_stop_reload_button, _("Stop"));
  } else {
    gtk_button_set_icon_name (GTK_BUTTON (self->combined_stop_reload_button), "view-refresh-symbolic");
    gtk_widget_set_tooltip_text (self->combined_stop_reload_button, _(REFRESH_BUTTON_TOOLTIP));
  }
}

GtkWidget *
ephy_location_entry_get_opensearch_button (EphyLocationEntry *entry)
{
  return entry->opensearch_button;
}

GtkWidget *
ephy_location_entry_get_site_menu_button (EphyLocationEntry *self)
{
  return self->site_menu_button;
}

gboolean
ephy_location_entry_has_focus (EphyLocationEntry *self)
{
  GtkWidget *delegate = GTK_WIDGET (gtk_editable_get_delegate (GTK_EDITABLE (self->text)));

  return gtk_widget_has_focus (delegate);
}

void
ephy_location_entry_set_position (EphyLocationEntry *self,
                                  int                position)
{
  gtk_editable_set_position (GTK_EDITABLE (self->text), position);
}

void
ephy_location_entry_set_zoom_level (EphyLocationEntry *entry,
                                    char              *zoom_level)
{
  ephy_site_menu_button_set_zoom_level (EPHY_SITE_MENU_BUTTON (entry->site_menu_button), zoom_level);
}
