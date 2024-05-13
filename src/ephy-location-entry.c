/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002  Ricardo Fernández Pascual
 *  Copyright © 2003, 2004  Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005  Christian Persch
 *  Copyright © 2008  Xan López
 *  Copyright © 2016  Igalia S.L.
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
#include "ephy-embed-shell.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-signal-accumulator.h"
#include "ephy-suggestion.h"
#include "ephy-title-widget.h"
#include "ephy-uri-helpers.h"
#include "ephy-web-view.h"

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
  GtkWidget parent_instance;

  GtkWidget *text;
  GtkWidget *progress;
  GtkWidget *security_button;
  GtkWidget *clear_button;
  GtkWidget *password_button;
  GtkWidget *bookmark_button;
  GtkWidget *reader_mode_button;
  GList *page_actions;
  GList *permission_buttons;

  GtkWidget *suggestions_popover;
  GtkWidget *scrolled_window;
  GtkWidget *suggestions_view;
  GtkSingleSelection *suggestions_model;

  GtkWidget *context_menu;

  char *saved_text;
  char *jump_tab;

  guint progress_timeout;
  gdouble progress_fraction;

  gboolean reader_mode_active;
  gboolean show_suggestions;

  guint dns_prefetch_handle_id;

  guint user_changed : 1;
  guint can_redo : 1;
  guint block_update : 1;

  EphySecurityLevel security_level;
  EphyAdaptiveMode adaptive_mode;
  EphyBookmarkIconState icon_state;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_SHOW_SUGGESTIONS,
  PROP_ADDRESS,
  PROP_SECURITY_LEVEL,
  LAST_TITLE_WIDGET_PROP,
  LAST_PROP = PROP_ADDRESS
};
static GParamSpec *props[LAST_PROP] = { 0 };

enum {
  ACTIVATE,
  USER_CHANGED,
  READER_MODE_CHANGED,
  GET_LOCATION,
  GET_TITLE,
  LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };

static void ephy_location_entry_editable_init (GtkEditableInterface *iface);
static void ephy_location_entry_accessible_init (GtkAccessibleInterface *iface);
static void ephy_location_entry_title_widget_interface_init (EphyTitleWidgetInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyLocationEntry, ephy_location_entry, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                      ephy_location_entry_editable_init)
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE,
                                                      ephy_location_entry_accessible_init)
                               G_IMPLEMENT_INTERFACE (EPHY_TYPE_TITLE_WIDGET,
                                                      ephy_location_entry_title_widget_interface_init))

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
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  if (helper->uri)
    webkit_network_session_prefetch_dns (ephy_embed_shell_get_network_session (shell), g_uri_get_host (helper->uri));

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
  if (error != NULL) {
    free_prefetch_helper (helper);
    return;
  }

  if (proxies != NULL && (g_strv_length (proxies) > 1 || g_strcmp0 (proxies[0], "direct://") != 0)) {
    free_prefetch_helper (helper);
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
schedule_dns_prefetch (EphyLocationEntry *entry,
                       const gchar       *url)
{
  GProxyResolver *resolver = g_proxy_resolver_get_default ();
  PrefetchHelper *helper;
  g_autoptr (GUri) uri = NULL;

  if (resolver == NULL)
    return;

  uri = g_uri_parse (url, G_URI_FLAGS_PARSE_RELAXED, NULL);
  if (!uri || !g_uri_get_host (uri))
    return;

  helper = g_new0 (PrefetchHelper, 1);
  helper->entry = g_object_ref (entry);
  helper->uri = g_steal_pointer (&uri);

  g_proxy_resolver_lookup_async (resolver, url, NULL, proxy_resolver_ready_cb, helper);
}

static void
editable_changed_cb (GtkEditable       *editable,
                     EphyLocationEntry *entry);

static void
update_selected_url (EphyLocationEntry *entry)
{
  DzlSuggestion *suggestion;
  const gchar *uri;

  suggestion = gtk_single_selection_get_selected_item (entry->suggestions_model);

  if (!suggestion)
    return;

  uri = dzl_suggestion_get_id (suggestion);

  g_signal_handlers_block_by_func (entry, G_CALLBACK (editable_changed_cb), entry);
  g_clear_pointer (&entry->jump_tab, g_free);

  if (g_str_has_prefix (uri, "ephy-tab://")) {
    entry->jump_tab = g_strdup (uri);
    gtk_editable_set_text (GTK_EDITABLE (entry), dzl_suggestion_get_subtitle (suggestion));
  } else {
    gtk_editable_set_text (GTK_EDITABLE (entry), uri);
  }
  gtk_editable_set_position (GTK_EDITABLE (entry), -1);
  g_signal_handlers_unblock_by_func (entry, G_CALLBACK (editable_changed_cb), entry);

  schedule_dns_prefetch (entry, uri);
}

static void
update_actions (EphyLocationEntry *entry)
{
  GdkClipboard *clipboard;
  GtkEntryBuffer *buffer;
  gboolean has_clipboard;
  gboolean has_selection;
  gboolean has_content;
  gboolean editable;

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (entry));
  buffer = gtk_text_get_buffer (GTK_TEXT (entry->text));

  has_clipboard = gdk_content_formats_contain_gtype (gdk_clipboard_get_formats (clipboard), G_TYPE_STRING);
  has_selection = gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), NULL, NULL);
  has_content = buffer && (gtk_entry_buffer_get_length (buffer) > 0);
  editable = gtk_editable_get_editable (GTK_EDITABLE (entry));

  /* This is a copy of the GtkText logic. We need it as GtkText normally
   * refreshes these actions upon opening context menu, and we don't allow it
   * to do that and show our own menu instead */
  gtk_widget_action_set_enabled (entry->text, "clipboard.cut",
                                 editable && has_selection);
  gtk_widget_action_set_enabled (entry->text, "clipboard.copy",
                                 has_selection);
  gtk_widget_action_set_enabled (entry->text, "clipboard.paste",
                                 editable && has_clipboard);

  gtk_widget_action_set_enabled (entry->text, "selection.delete",
                                 editable && has_selection);
  gtk_widget_action_set_enabled (entry->text, "selection.select-all",
                                 has_content);

  gtk_widget_action_set_enabled (GTK_WIDGET (entry), "clipboard.paste-and-go",
                                 editable && has_clipboard);
  gtk_widget_action_set_enabled (entry->text, "edit.clear",
                                 has_content);
  gtk_widget_action_set_enabled (entry->text, "edit.undo-extra",
                                 entry->user_changed);
  gtk_widget_action_set_enabled (entry->text, "edit.redo-extra",
                                 entry->can_redo);
}

static void
update_suggestions_popover (EphyLocationEntry *entry)
{
  guint n_items;

  n_items = g_list_model_get_n_items (G_LIST_MODEL (entry->suggestions_model));

  if (entry->show_suggestions && n_items > 0) {
    if (entry->adaptive_mode == EPHY_ADAPTIVE_MODE_NARROW) {
      GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (entry));
      double offset;

      gtk_widget_translate_coordinates (GTK_WIDGET (entry),
                                        GTK_WIDGET (root),
                                        0, 0,
                                        &offset, NULL);

      gtk_widget_set_halign (entry->suggestions_popover, GTK_ALIGN_START);
      gtk_popover_set_offset (GTK_POPOVER (entry->suggestions_popover), -offset, 0);
    } else {
      gtk_widget_set_halign (entry->suggestions_popover, GTK_ALIGN_FILL);
      gtk_popover_set_offset (GTK_POPOVER (entry->suggestions_popover), 0, 0);
    }

    gtk_popover_popup (GTK_POPOVER (entry->suggestions_popover));
  } else {
    gtk_popover_popdown (GTK_POPOVER (entry->suggestions_popover));
  }
}

static void
set_show_suggestions (EphyLocationEntry *entry,
                      gboolean           show)
{
  if (entry->show_suggestions == show)
    return;

  entry->show_suggestions = show;

  update_suggestions_popover (entry);

  g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_SHOW_SUGGESTIONS]);
}

static void
show_context_menu (EphyLocationEntry *entry,
                   double             x,
                   double             y)
{
  update_actions (entry);

  if (x != -1 && y != -1) {
    GdkRectangle rect = { x, y, 1, 1 };
    gtk_popover_set_pointing_to (GTK_POPOVER (entry->context_menu), &rect);
  } else {
    gtk_popover_set_pointing_to (GTK_POPOVER (entry->context_menu), NULL);
  }

  gtk_popover_popup (GTK_POPOVER (entry->context_menu));
}

static void
copy_clipboard (EphyLocationEntry *entry)
{
  g_autofree char *text = NULL;
  gint start;
  gint end;

  if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end))
    return;

  text = gtk_editable_get_chars (GTK_EDITABLE (entry), start, end);

  if (start == 0) {
    g_autofree char *tmp = text;
    text = ephy_uri_normalize (tmp);
  }

  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (entry)), text);
}

static void
copy_clipboard_cb (EphyLocationEntry *entry)
{
  copy_clipboard (entry);

  g_signal_stop_emission_by_name (entry->text, "copy-clipboard");
}

static void
cut_clipboard_cb (EphyLocationEntry *entry)
{
  if (!gtk_editable_get_editable (GTK_EDITABLE (entry))) {
    gtk_widget_error_bell (GTK_WIDGET (entry));
    return;
  }

  copy_clipboard (entry);
  gtk_editable_delete_selection (GTK_EDITABLE (entry));

  g_signal_stop_emission_by_name (entry->text, "cut-clipboard");
}

static void
click_pressed_cb (EphyLocationEntry *entry,
                  int                n_click,
                  double             x,
                  double             y,
                  GtkGesture        *gesture)
{
  if (n_click > 1) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  if (gtk_widget_has_focus (entry->text)) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  if (gtk_widget_pick (GTK_WIDGET (entry), x, y, GTK_PICK_DEFAULT) != entry->text)
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
}

static void
click_released_cb (EphyLocationEntry *entry,
                   int                n_click,
                   double             x,
                   double             y,
                   GtkGesture        *gesture)
{
  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), NULL, NULL)) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
}

static void
long_press_cb (EphyLocationEntry *entry,
               double             x,
               double             y,
               GtkGesture        *gesture)
{
  if (gtk_widget_pick (GTK_WIDGET (entry), x, y, GTK_PICK_DEFAULT) == entry->text)
    gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);

  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
}

static gboolean
key_pressed_cb (EphyLocationEntry     *entry,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                GtkEventControllerKey *controller)
{
  guint selected, matches;

  if (state & (GDK_SHIFT_MASK | GDK_ALT_MASK | GDK_CONTROL_MASK))
    return FALSE;

  if (keyval != GDK_KEY_Up && keyval != GDK_KEY_KP_Up &&
      keyval != GDK_KEY_Down && keyval != GDK_KEY_KP_Down &&
      keyval != GDK_KEY_Page_Up && keyval != GDK_KEY_KP_Page_Up &&
      keyval != GDK_KEY_Page_Down && keyval != GDK_KEY_KP_Page_Down) {
    return FALSE;
  }

  if (!entry->show_suggestions)
    set_show_suggestions (entry, TRUE);

  if (!gtk_widget_get_visible (entry->suggestions_popover))
    return FALSE;

  matches = g_list_model_get_n_items (G_LIST_MODEL (entry->suggestions_model));
  selected = gtk_single_selection_get_selected (entry->suggestions_model);

  if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up) {
    if (selected == GTK_INVALID_LIST_POSITION)
      selected = matches - 1;
    else if (selected == 0)
      selected = GTK_INVALID_LIST_POSITION;
    else
      selected--;
  }

  if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down) {
    if (selected == GTK_INVALID_LIST_POSITION)
      selected = 0;
    else if (selected == matches - 1)
      selected = GTK_INVALID_LIST_POSITION;
    else
      selected++;
  }

  if (keyval == GDK_KEY_Page_Up || keyval == GDK_KEY_KP_Page_Up) {
    if (selected == GTK_INVALID_LIST_POSITION)
      selected = matches - 1;
    else if (selected == 0)
      selected = GTK_INVALID_LIST_POSITION;
    else if (selected < PAGE_STEP)
      selected = 0;
    else
      selected -= PAGE_STEP;
  }

  if (keyval == GDK_KEY_Page_Down || keyval == GDK_KEY_KP_Page_Down) {
    if (selected == GTK_INVALID_LIST_POSITION)
      selected = 0;
    else if (selected == matches - 1)
      selected = GTK_INVALID_LIST_POSITION;
    else if (selected + PAGE_STEP > matches - 1)
      selected = matches - 1;
    else
      selected += PAGE_STEP;
  }

  if (selected == GTK_INVALID_LIST_POSITION) {
    gtk_widget_error_bell (GTK_WIDGET (entry));
    return TRUE;
  }

  gtk_single_selection_set_selected (entry->suggestions_model, selected);
  gtk_list_view_scroll_to (GTK_LIST_VIEW (entry->suggestions_view), selected, GTK_LIST_SCROLL_NONE, NULL);
  update_selected_url (entry);
  return TRUE;
}

static void
text_pressed_cb (EphyLocationEntry *entry,
                 int                n_click,
                 double             x,
                 double             y,
                 GtkGesture        *gesture)
{
  GdkEventSequence *current;
  GdkEvent *event;

  current = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  event = gtk_gesture_get_last_event (gesture, current);

  if (gdk_event_triggers_context_menu (event)) {
    show_context_menu (entry, x, y);
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  }
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

    return G_ICON (gdk_pixbuf_get_from_surface (surface, 0, 0, width, height));
  }

  if (icon)
    return g_object_ref (icon);

  return NULL;
}

static GIcon *
get_suggestion_secondary_icon (GtkListItem *item,
                               GIcon       *icon)
{
  DzlSuggestion *suggestion = gtk_list_item_get_item (item);
  GtkWidget *widget = gtk_list_item_get_child (item);
  cairo_surface_t *surface;

  surface = dzl_suggestion_get_secondary_icon_surface (suggestion, widget);

  if (surface) {
    int width, height;

    if (cairo_surface_get_type (surface) != CAIRO_SURFACE_TYPE_IMAGE)
      return NULL;

    width = cairo_image_surface_get_width (surface);
    height = cairo_image_surface_get_height (surface);

    return G_ICON (gdk_pixbuf_get_from_surface (surface, 0, 0, width, height));
  }

  if (icon)
    return g_object_ref (icon);

  return NULL;
}

static void
update_entry_style (EphyLocationEntry *self,
                    gboolean           focus)
{
  PangoAttrList *attrs;
  PangoAttribute *color_normal;
  PangoAttribute *color_dimmed;
  g_autoptr (GUri) uri = NULL;
  const char *text = gtk_editable_get_text (GTK_EDITABLE (self));
  const char *host;
  const char *base_domain;
  char *sub_string;

  gtk_widget_set_visible (self->clear_button, focus);

  attrs = pango_attr_list_new ();

  if (focus)
    goto out;

  uri = g_uri_parse (text, G_URI_FLAGS_PARSE_RELAXED, NULL);
  if (!uri)
    goto out;

  host = g_uri_get_host (uri);
  if (!host || strlen (host) == 0)
    goto out;

  base_domain = soup_tld_get_base_domain (host, NULL);
  if (!base_domain)
    goto out;

  sub_string = strstr (text, base_domain);
  if (!sub_string)
    goto out;

  /* Complete text is dimmed */
  color_dimmed = pango_attr_foreground_alpha_new (32768);
  pango_attr_list_insert (attrs, color_dimmed);

  /* Base domain with normal style */
  color_normal = pango_attr_foreground_alpha_new (65535);
  color_normal->start_index = sub_string - text;
  color_normal->end_index = color_normal->start_index + strlen (base_domain);
  pango_attr_list_insert (attrs, color_normal);

out:
  gtk_text_set_attributes (GTK_TEXT (self->text), attrs);
  pango_attr_list_unref (attrs);
}

static void
focus_enter_cb (EphyLocationEntry *entry)
{
  update_entry_style (entry, TRUE);
}

static void
focus_leave_cb (EphyLocationEntry *entry)
{
  update_entry_style (entry, FALSE);
  gtk_editable_select_region (GTK_EDITABLE (entry), 0, 0);
  set_show_suggestions (entry, FALSE);
}

static void
emit_activate (EphyLocationEntry *entry,
               GdkModifierType    modifiers)
{
  if (entry->jump_tab) {
    g_signal_handlers_block_by_func (entry, G_CALLBACK (editable_changed_cb), entry);
    gtk_editable_set_text (GTK_EDITABLE (entry), entry->jump_tab);
    g_signal_handlers_unblock_by_func (entry, G_CALLBACK (editable_changed_cb), entry);
    g_clear_pointer (&entry->jump_tab, g_free);
  } else {
    g_autofree gchar *text = g_strdup (gtk_editable_get_text (GTK_EDITABLE (entry)));
    gchar *url = g_strstrip (text);
    g_autofree gchar *new_url = NULL;

    gtk_editable_set_text (GTK_EDITABLE (entry), entry->jump_tab ? entry->jump_tab : text);

    if (strlen (url) > 5 && g_str_has_prefix (url, "http:") && url[5] != '/')
      new_url = g_strdup_printf ("http://%s", url + 5);
    else if (strlen (url) > 6 && g_str_has_prefix (url, "https:") && url[6] != '/')
      new_url = g_strdup_printf ("https://%s", url + 6);

    if (new_url) {
      g_signal_handlers_block_by_func (entry, G_CALLBACK (editable_changed_cb), entry);
      gtk_editable_set_text (GTK_EDITABLE (entry), new_url);
      g_signal_handlers_unblock_by_func (entry, G_CALLBACK (editable_changed_cb), entry);
    }

    if (modifiers == GDK_CONTROL_MASK) {
      /* Remove control mask to prevent opening address in a new window */
      modifiers &= ~GDK_CONTROL_MASK;

      if (!g_utf8_strchr (url, -1, ' ') && !g_utf8_strchr (url, -1, '.')) {
        g_autofree gchar *new_url = g_strdup_printf ("www.%s.com", url);

        g_signal_handlers_block_by_func (entry, G_CALLBACK (editable_changed_cb), entry);
        gtk_editable_set_text (GTK_EDITABLE (entry), new_url);
        g_signal_handlers_unblock_by_func (entry, G_CALLBACK (editable_changed_cb), entry);
      }
    }

    g_signal_emit (entry, signals[ACTIVATE], 0, modifiers);
  }
}

static gboolean
activate_shortcut_cb (EphyLocationEntry *entry,
                      GVariant          *args)
{
  emit_activate (entry, g_variant_get_int32 (args));

  return TRUE;
}

static void
editable_changed_cb (GtkEditable       *editable,
                     EphyLocationEntry *entry)
{
  if (entry->block_update)
    return;

  entry->user_changed = TRUE;
  entry->can_redo = FALSE;
  update_actions (entry);

  g_clear_pointer (&entry->jump_tab, g_free);

  g_signal_emit (entry, signals[USER_CHANGED], 0, gtk_editable_get_text (editable));

  set_show_suggestions (entry, TRUE);
}

static void
reader_mode_clicked_cb (EphyLocationEntry *entry)
{
  entry->reader_mode_active = !entry->reader_mode_active;

  g_signal_emit (G_OBJECT (entry), signals[READER_MODE_CHANGED], 0,
                 entry->reader_mode_active);
}

static void
clear_button_clicked_cb (EphyLocationEntry *self)
{
  gtk_editable_set_text (GTK_EDITABLE (self), "");
}

static void
suggestions_popover_notify_visible_cb (EphyLocationEntry *entry)
{
  GtkAdjustment *adj;

  if (!gtk_widget_get_visible (entry->suggestions_popover)) {
    gtk_single_selection_set_selected (entry->suggestions_model, GTK_INVALID_LIST_POSITION);
    entry->show_suggestions = FALSE;
    return;
  }

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (entry->scrolled_window));

  g_assert (adj);

  gtk_adjustment_set_value (adj, 0);
}

static void
suggestion_activated_cb (EphyLocationEntry *entry,
                         guint              position)
{
  g_autoptr (EphySuggestion) suggestion = NULL;
  const gchar *text;

  suggestion = g_list_model_get_item (G_LIST_MODEL (entry->suggestions_model), position);
  text = ephy_suggestion_get_uri (suggestion);

  g_signal_handlers_block_by_func (entry, G_CALLBACK (editable_changed_cb), entry);
  gtk_editable_set_text (GTK_EDITABLE (entry), entry->jump_tab ? entry->jump_tab : text);
  g_clear_pointer (&entry->jump_tab, g_free);
  g_signal_handlers_unblock_by_func (entry, G_CALLBACK (editable_changed_cb), entry);

  set_show_suggestions (entry, FALSE);

  /* Now trigger the load.... */
  emit_activate (entry, 0);
}

static void
activate_cb (EphyLocationEntry *entry)
{
  if (gtk_widget_get_visible (entry->suggestions_popover)) {
    guint position = gtk_single_selection_get_selected (entry->suggestions_model);

    if (position != GTK_INVALID_LIST_POSITION) {
      suggestion_activated_cb (entry, position);
      return;
    }
  }

  emit_activate (entry, 0);
}

static void
item_pressed_cb (GtkListItem *item,
                 int          n_click,
                 double       x,
                 double       y,
                 GtkGesture  *gesture)
{
  GtkWidget *widget;
  EphyLocationEntry *entry;
  guint position;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  entry = EPHY_LOCATION_ENTRY (gtk_widget_get_ancestor (widget, EPHY_TYPE_LOCATION_ENTRY));
  position = gtk_list_item_get_position (item);

  gtk_single_selection_set_selected (entry->suggestions_model, position);
}

static void
item_released_cb (GtkListItem *item,
                  int          n_click,
                  double       x,
                  double       y,
                  GtkGesture  *gesture)
{
  GtkWidget *widget;
  EphyLocationEntry *entry;
  guint position;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  entry = EPHY_LOCATION_ENTRY (gtk_widget_get_ancestor (widget, EPHY_TYPE_LOCATION_ENTRY));
  position = gtk_list_item_get_position (item);

  /* We only want to handle clicks with press and release on the same row */
  if (!gtk_widget_contains (widget, x, y)) {
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
    return;
  }

  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);
  suggestion_activated_cb (entry, position);
}

static void
root_notify_is_active_cb (EphyLocationEntry *entry)
{
  GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (entry));

  if (!gtk_window_is_active (GTK_WINDOW (root)))
    set_show_suggestions (entry, FALSE);
}

static void
on_permission_popover_response (EphyPermissionPopover *popover,
                                GtkMenuButton         *button)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (gtk_widget_get_parent (GTK_WIDGET (button)));
  gtk_widget_unparent (GTK_WIDGET (button));
  entry->permission_buttons = g_list_remove (entry->permission_buttons, button);
}

static void
update_reader_icon (EphyLocationEntry *entry)
{
  GdkDisplay *display;
  GtkIconTheme *theme;
  const gchar *name;

  display = gtk_widget_get_display (GTK_WIDGET (entry));
  theme = gtk_icon_theme_get_for_display (display);

  if (gtk_icon_theme_has_icon (theme, "view-reader-symbolic"))
    name = "view-reader-symbolic";
  else
    name = "ephy-reader-mode-symbolic";

  gtk_button_set_icon_name (GTK_BUTTON (entry->reader_mode_button), name);
}

static void
create_security_popup_cb (GtkMenuButton     *button,
                          EphyLocationEntry *entry)
{
  g_signal_emit_by_name (entry, "lock-clicked", button);
}

static void
paste_received (GdkClipboard      *clipboard,
                GAsyncResult      *result,
                EphyLocationEntry *entry)
{
  g_autofree char *text = NULL;

  text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (clipboard), result, NULL);
  if (text == NULL) {
    gtk_widget_error_bell (GTK_WIDGET (entry));
    return;
  }

  g_signal_handlers_block_by_func (entry, G_CALLBACK (editable_changed_cb), entry);
  gtk_editable_set_text (GTK_EDITABLE (entry), text);
  emit_activate (entry, 0);
  g_signal_handlers_unblock_by_func (entry, G_CALLBACK (editable_changed_cb), entry);

  g_object_unref (entry);
}

static void
paste_and_go_activate (EphyLocationEntry *entry)
{
  GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (entry));

  gdk_clipboard_read_text_async (clipboard, NULL,
                                 (GAsyncReadyCallback)paste_received,
                                 g_object_ref (entry));
}

static void
clear_activate (EphyLocationEntry *entry)
{
  entry->block_update = TRUE;
  g_signal_handlers_block_by_func (entry, G_CALLBACK (editable_changed_cb), entry);
  gtk_editable_set_text (GTK_EDITABLE (entry), "");
  g_signal_handlers_unblock_by_func (entry, G_CALLBACK (editable_changed_cb), entry);
  entry->block_update = FALSE;
  entry->user_changed = TRUE;
  update_actions (entry);
}

static void
menu_popup_activate (EphyLocationEntry *entry)
{
  show_context_menu (entry, -1, -1);
}

static void
ephy_location_entry_measure (GtkWidget      *widget,
                             GtkOrientation  orientation,
                             int             for_size,
                             int            *minimum,
                             int            *natural,
                             int            *minimum_baseline,
                             int            *natural_baseline)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (widget);

  if (orientation == GTK_ORIENTATION_VERTICAL) {
    GtkWidget *child;

    for (child = gtk_widget_get_first_child (widget);
         child;
         child = gtk_widget_get_next_sibling (child)) {
      int child_min, child_nat, child_min_baseline, child_nat_baseline;

      if (!gtk_widget_should_layout (child))
        return;

      gtk_widget_measure (child, orientation, for_size, &child_min, &child_nat,
                          &child_min_baseline, &child_nat_baseline);

      if (minimum)
        *minimum = MAX (*minimum, child_min);
      if (natural)
        *natural = MAX (*natural, child_nat);
      if (minimum_baseline)
        *minimum_baseline = MAX (*minimum_baseline, child_min_baseline);
      if (natural_baseline)
        *natural_baseline = MAX (*natural_baseline, child_nat_baseline);
    }
  } else {
    int min, nat, child_min, child_nat;
    GList *l;

    gtk_widget_measure (entry->text, orientation, for_size, &min, &nat, NULL, NULL);

    /* Any other icons need to be similarly added to the text width */
    for (l = entry->permission_buttons; l; l = l->next) {
      if (gtk_widget_should_layout (GTK_WIDGET (l->data))) {
        gtk_widget_measure (GTK_WIDGET (l->data), orientation, for_size,
                            &child_min, &child_nat, NULL, NULL);
        min += child_min;
        nat += child_nat;
      }
    }

    if (gtk_widget_should_layout (entry->security_button)) {
      gtk_widget_measure (entry->security_button, orientation, for_size,
                          &child_min, &child_nat, NULL, NULL);
      min += child_min;
      nat += child_nat;
    }

    if (gtk_widget_should_layout (entry->password_button)) {
      gtk_widget_measure (entry->password_button, orientation, for_size,
                          &child_min, &child_nat, NULL, NULL);
      min += child_min;
      nat += child_nat;
    }

    if (gtk_widget_should_layout (entry->bookmark_button)) {
      gtk_widget_measure (entry->bookmark_button, orientation, for_size,
                          &child_min, &child_nat, NULL, NULL);
      min += child_min;
      nat += child_nat;
    }

    if (gtk_widget_should_layout (entry->reader_mode_button)) {
      gtk_widget_measure (entry->reader_mode_button, orientation, for_size,
                          &child_min, &child_nat, NULL, NULL);
      min += child_min;
      nat += child_nat;
    }

    if (gtk_widget_should_layout (entry->clear_button)) {
      gtk_widget_measure (entry->clear_button, orientation, for_size,
                          &child_min, &child_nat, NULL, NULL);
      min += child_min;
      nat += child_nat;
    }

    /* Since progress bar spans the whole width, we MAX() it instead of adding */
    gtk_widget_measure (entry->progress, orientation, for_size,
                        &child_min, &child_nat, NULL, NULL);
    min = MAX (min, child_min);
    nat = MAX (nat, child_nat);

    for (l = entry->page_actions; l; l = l->next) {
      gtk_widget_measure (l->data, orientation, for_size,
                          &child_min, &child_nat, NULL, NULL);
      min = MAX (min, child_min);
      nat = MAX (nat, child_nat);
    }

    if (minimum)
      *minimum = min;
    if (natural)
      *natural = nat;
    if (minimum_baseline)
      *minimum_baseline = -1;
    if (natural_baseline)
      *natural_baseline = -1;
  }
}

static void
allocate_icon (GtkWidget   *widget,
               int          height,
               int          baseline,
               GtkWidget   *icon,
               GtkPackType  pack_type,
               int         *left_pos,
               int         *right_pos)
{
  GskTransform *transform;
  int icon_width;

  if (!gtk_widget_should_layout (icon))
    return;

  gtk_widget_measure (icon, GTK_ORIENTATION_HORIZONTAL, -1,
                      NULL, &icon_width, NULL, NULL);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    pack_type = (pack_type == GTK_PACK_START) ? GTK_PACK_END : GTK_PACK_START;

  if (pack_type == GTK_PACK_START) {
    transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (*left_pos, 0));
    *left_pos += icon_width;
  } else {
    *right_pos -= icon_width;
    transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (*right_pos, 0));
  }

  gtk_widget_allocate (icon, icon_width, height, baseline, transform);
}

static void
ephy_location_entry_size_allocate (GtkWidget *widget,
                                   int        width,
                                   int        height,
                                   int        baseline)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (widget);
  int icon_left_pos = 0;
  int icon_right_pos = width;
  GskTransform *transform;
  GList *l;
  GtkWidget *root = GTK_WIDGET (gtk_widget_get_root (widget));

  for (l = entry->permission_buttons; l; l = l->next) {
    allocate_icon (widget, height, baseline, l->data,
                   GTK_PACK_START, &icon_left_pos, &icon_right_pos);
  }
  allocate_icon (widget, height, baseline, entry->security_button,
                 GTK_PACK_START, &icon_left_pos, &icon_right_pos);
  allocate_icon (widget, height, baseline, entry->password_button,
                 GTK_PACK_END, &icon_left_pos, &icon_right_pos);
  allocate_icon (widget, height, baseline, entry->bookmark_button,
                 GTK_PACK_END, &icon_left_pos, &icon_right_pos);
  allocate_icon (widget, height, baseline, entry->reader_mode_button,
                 GTK_PACK_END, &icon_left_pos, &icon_right_pos);
  allocate_icon (widget, height, baseline, entry->clear_button,
                 GTK_PACK_END, &icon_left_pos, &icon_right_pos);

  for (l = entry->page_actions; l; l = l->next) {
    allocate_icon (widget, height, baseline, l->data,
                   GTK_PACK_END, &icon_left_pos, &icon_right_pos);
  }

  transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (icon_left_pos, 0));
  gtk_widget_allocate (entry->text, icon_right_pos - icon_left_pos,
                       height, baseline, transform);

  gtk_widget_allocate (entry->progress, width, height, baseline, NULL);

  if (entry->adaptive_mode == EPHY_ADAPTIVE_MODE_NARROW)
    gtk_widget_set_size_request (entry->suggestions_popover,
                                 gtk_widget_get_width (root), -1);
  else
    gtk_widget_set_size_request (entry->suggestions_popover,
                                 gtk_widget_get_allocated_width (widget), -1);

  gtk_widget_queue_resize (entry->suggestions_popover);

  gtk_popover_present (GTK_POPOVER (entry->suggestions_popover));
  gtk_popover_present (GTK_POPOVER (entry->context_menu));
}

static void
ephy_location_entry_root (GtkWidget *widget)
{
  GtkRoot *root;

  GTK_WIDGET_CLASS (ephy_location_entry_parent_class)->root (widget);

  root = gtk_widget_get_root (widget);

  g_assert (GTK_IS_WINDOW (root));

  g_signal_connect_swapped (root, "notify::is-active",
                            G_CALLBACK (root_notify_is_active_cb), widget);
}

static void
ephy_location_entry_unroot (GtkWidget *widget)
{
  GtkRoot *root = gtk_widget_get_root (widget);

  g_signal_handlers_disconnect_by_func (root, G_CALLBACK (root_notify_is_active_cb), widget);

  GTK_WIDGET_CLASS (ephy_location_entry_parent_class)->unroot (widget);
}

static gboolean
ephy_location_entry_focus (GtkWidget        *widget,
                           GtkDirectionType  direction)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (widget);

  if (entry->show_suggestions && (direction == GTK_DIR_TAB_FORWARD ||
                                  direction == GTK_DIR_TAB_BACKWARD)) {
    guint selected, matches;

    if (!entry->show_suggestions)
      return FALSE;

    matches = g_list_model_get_n_items (G_LIST_MODEL (entry->suggestions_model));
    selected = gtk_single_selection_get_selected (entry->suggestions_model);

    if (direction == GTK_DIR_TAB_FORWARD) {
      if (selected == GTK_INVALID_LIST_POSITION || selected == matches - 1)
        selected = 0;
      else
        selected++;
    } else {
      if (selected == GTK_INVALID_LIST_POSITION || selected == 0)
        selected = matches - 1;
      else
        selected--;
    }

    gtk_single_selection_set_selected (entry->suggestions_model, selected);
    update_selected_url (entry);
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
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (object);

  if (gtk_editable_delegate_set_property (object, prop_id, value, pspec)) {
    if (prop_id == LAST_TITLE_WIDGET_PROP + GTK_EDITABLE_PROP_EDITABLE) {
      gtk_accessible_update_property (GTK_ACCESSIBLE (entry),
                                      GTK_ACCESSIBLE_PROPERTY_READ_ONLY, !g_value_get_boolean (value),
                                      -1);
    }
    return;
  }

  switch (prop_id) {
    case PROP_MODEL:
      ephy_location_entry_set_model (entry, g_value_get_object (value));
      break;
    case PROP_SHOW_SUGGESTIONS:
      set_show_suggestions (entry, g_value_get_boolean (value));
      break;
    case PROP_ADDRESS:
      ephy_title_widget_set_address (EPHY_TITLE_WIDGET (entry),
                                     g_value_get_string (value));
      break;
    case PROP_SECURITY_LEVEL:
      ephy_title_widget_set_security_level (EPHY_TITLE_WIDGET (entry),
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
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (object);

  if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
    return;

  switch (prop_id) {
    case PROP_MODEL:
      g_value_set_object (value, ephy_location_entry_get_model (entry));
      break;
    case PROP_SHOW_SUGGESTIONS:
      g_value_set_boolean (value, entry->show_suggestions);
      break;
    case PROP_ADDRESS:
      g_value_set_string (value, ephy_title_widget_get_address (EPHY_TITLE_WIDGET (entry)));
      break;
    case PROP_SECURITY_LEVEL:
      g_value_set_enum (value, ephy_title_widget_get_security_level (EPHY_TITLE_WIDGET (entry)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_location_entry_dispose (GObject *object)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (object);
  GList *l;

  g_clear_handle_id (&entry->progress_timeout, g_source_remove);

  if (entry->text)
    gtk_editable_finish_delegate (GTK_EDITABLE (entry));

  ephy_location_entry_page_action_clear (entry);

  for (l = entry->permission_buttons; l; l = l->next)
    gtk_widget_unparent (GTK_WIDGET (l->data));

  gtk_widget_unparent (entry->context_menu);
  gtk_widget_unparent (entry->text);
  gtk_widget_unparent (entry->progress);
  gtk_widget_unparent (entry->security_button);
  gtk_widget_unparent (entry->password_button);
  gtk_widget_unparent (entry->bookmark_button);
  gtk_widget_unparent (entry->reader_mode_button);
  gtk_widget_unparent (entry->clear_button);
  gtk_widget_unparent (entry->suggestions_popover);

  G_OBJECT_CLASS (ephy_location_entry_parent_class)->dispose (object);
}

static void
ephy_location_entry_finalize (GObject *object)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (object);

  g_free (entry->saved_text);
  g_free (entry->jump_tab);

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
ephy_location_entry_class_init (EphyLocationEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ephy_location_entry_get_property;
  object_class->set_property = ephy_location_entry_set_property;
  object_class->dispose = ephy_location_entry_dispose;
  object_class->finalize = ephy_location_entry_finalize;

  widget_class->measure = ephy_location_entry_measure;
  widget_class->size_allocate = ephy_location_entry_size_allocate;
  widget_class->root = ephy_location_entry_root;
  widget_class->unroot = ephy_location_entry_unroot;
  widget_class->focus = ephy_location_entry_focus;

  g_object_class_override_property (object_class, PROP_ADDRESS, "address");
  g_object_class_override_property (object_class, PROP_SECURITY_LEVEL, "security-level");

  props[PROP_MODEL] =
    g_param_spec_object ("model",
                         NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  props[PROP_SHOW_SUGGESTIONS] =
    g_param_spec_boolean ("show-suggestions",
                          NULL, NULL,
                          FALSE,
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
   * EphyLocationEntry::reader-mode-changed:
   * @entry: the object on which the signal is emitted
   * @active: whether reader mode is active
   *
   * Emitted when the user clicks the reader mode icon inside the
   * #EphyLocationEntry.
   *
   */
  signals[READER_MODE_CHANGED] = g_signal_new ("reader-mode-changed", G_OBJECT_CLASS_TYPE (klass),
                                               G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                                               0, NULL, NULL, NULL,
                                               G_TYPE_NONE,
                                               1,
                                               G_TYPE_BOOLEAN);

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

  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, text);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, progress);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, security_button);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, password_button);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, bookmark_button);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, reader_mode_button);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, clear_button);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, suggestions_popover);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, suggestions_model);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, suggestions_view);
  gtk_widget_class_bind_template_child (widget_class, EphyLocationEntry, context_menu);

  gtk_widget_class_bind_template_callback (widget_class, editable_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, update_actions);
  gtk_widget_class_bind_template_callback (widget_class, activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, cut_clipboard_cb);
  gtk_widget_class_bind_template_callback (widget_class, copy_clipboard_cb);
  gtk_widget_class_bind_template_callback (widget_class, reader_mode_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, suggestions_popover_notify_visible_cb);
  gtk_widget_class_bind_template_callback (widget_class, suggestion_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, update_suggestions_popover);
  gtk_widget_class_bind_template_callback (widget_class, focus_enter_cb);
  gtk_widget_class_bind_template_callback (widget_class, focus_leave_cb);
  gtk_widget_class_bind_template_callback (widget_class, click_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, click_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, long_press_cb);
  gtk_widget_class_bind_template_callback (widget_class, key_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, text_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, item_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, item_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, get_suggestion_icon);
  gtk_widget_class_bind_template_callback (widget_class, get_suggestion_secondary_icon);
  gtk_widget_class_bind_template_callback (widget_class, clear_button_clicked_cb);

  gtk_widget_class_set_css_name (widget_class, "entry");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TEXT_BOX);

  gtk_widget_class_install_action (widget_class, "clipboard.paste-and-go", NULL,
                                   (GtkWidgetActionActivateFunc)paste_and_go_activate);
  gtk_widget_class_install_action (widget_class, "edit.clear", NULL,
                                   (GtkWidgetActionActivateFunc)clear_activate);
  gtk_widget_class_install_action (widget_class, "edit.undo-extra", NULL,
                                   (GtkWidgetActionActivateFunc)ephy_location_entry_reset);
  gtk_widget_class_install_action (widget_class, "edit.redo-extra", NULL,
                                   (GtkWidgetActionActivateFunc)ephy_location_entry_undo_reset);
  gtk_widget_class_install_action (widget_class, "menu.popup-extra", NULL,
                                   (GtkWidgetActionActivateFunc)menu_popup_activate);

  register_activate_shortcuts (widget_class, GDK_CONTROL_MASK);
  register_activate_shortcuts (widget_class, GDK_ALT_MASK);
  register_activate_shortcuts (widget_class, GDK_SHIFT_MASK | GDK_CONTROL_MASK);
  register_activate_shortcuts (widget_class, GDK_SHIFT_MASK | GDK_ALT_MASK);

  gtk_widget_class_add_binding (widget_class, GDK_KEY_Escape, 0,
                                (GtkShortcutFunc)ephy_location_entry_reset,
                                NULL);
}

static void
ephy_location_entry_init (EphyLocationEntry *entry)
{
  GdkClipboard *clipboard;

  LOG ("EphyLocationEntry initialising %p", entry);

  entry->user_changed = FALSE;
  entry->block_update = FALSE;
  entry->saved_text = NULL;
  entry->page_actions = NULL;
  entry->adaptive_mode = EPHY_ADAPTIVE_MODE_NORMAL;

  gtk_widget_init_template (GTK_WIDGET (entry));

  gtk_menu_button_set_create_popup_func (GTK_MENU_BUTTON (entry->security_button),
                                         (GtkMenuButtonCreatePopupFunc)create_security_popup_cb,
                                         entry,
                                         NULL);

  g_settings_bind (EPHY_SETTINGS_LOCKDOWN,
                   EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING,
                   entry->bookmark_button,
                   "visible",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_INVERT_BOOLEAN);

  update_reader_icon (entry);
  g_signal_connect_object (gtk_settings_get_default (), "notify::gtk-icon-theme-name",
                           G_CALLBACK (update_reader_icon), entry, G_CONNECT_SWAPPED);

  gtk_editable_init_delegate (GTK_EDITABLE (entry));

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (entry));
  update_actions (entry);
  g_signal_connect_object (clipboard, "changed", G_CALLBACK (update_actions),
                           entry, G_CONNECT_SWAPPED);
}

static const char *
ephy_location_entry_title_widget_get_address (EphyTitleWidget *widget)
{
  return gtk_editable_get_text (GTK_EDITABLE (widget));
}

static void
ephy_location_entry_title_widget_set_address (EphyTitleWidget *widget,
                                              const char      *address)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (widget);
  GdkClipboard *clipboard;
  const char *text;
  g_autofree char *effective_text = NULL;
  g_autofree char *selection = NULL;
  int start, end;
  const char *final_text;

  /* Setting a new text will clear the clipboard. This makes it impossible
   * to copy&paste from the location entry of one tab into another tab, see
   * bug #155824. So we save the selection iff the clipboard was owned by
   * the location entry.
   */
  if (gtk_widget_get_realized (GTK_WIDGET (entry))) {
    clipboard = gtk_widget_get_primary_clipboard (GTK_WIDGET (entry));
    g_assert (clipboard != NULL);

    if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry),
                                           &start, &end)) {
      selection = gtk_editable_get_chars (GTK_EDITABLE (entry),
                                          start, end);
    }
  }

  if (address != NULL) {
    if (g_str_has_prefix (address, EPHY_ABOUT_SCHEME))
      effective_text = g_strdup_printf ("about:%s",
                                        address + strlen (EPHY_ABOUT_SCHEME) + 1);
    text = address;
  } else {
    text = "";
  }

  final_text = effective_text ? effective_text : text;

  entry->block_update = TRUE;
  g_signal_handlers_block_by_func (entry, G_CALLBACK (editable_changed_cb), entry);
  gtk_editable_set_text (GTK_EDITABLE (widget), final_text);
  g_signal_handlers_unblock_by_func (entry, G_CALLBACK (editable_changed_cb), entry);
  update_entry_style (entry, gtk_widget_has_focus (entry->text));

  set_show_suggestions (entry, FALSE);
  entry->block_update = FALSE;

  gtk_editable_set_enable_undo (GTK_EDITABLE (entry), FALSE);
  gtk_editable_set_enable_undo (GTK_EDITABLE (entry), TRUE);

  /* Now restore the selection.
   * Note that it's not owned by the entry anymore!
   */
  if (selection != NULL) {
    gdk_clipboard_set_text (gtk_widget_get_primary_clipboard (GTK_WIDGET (entry)),
                            selection);
  }
}

static EphySecurityLevel
ephy_location_entry_title_widget_get_security_level (EphyTitleWidget *widget)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (widget);

  g_assert (entry);

  return entry->security_level;
}

static void
ephy_location_entry_title_widget_set_security_level (EphyTitleWidget   *widget,
                                                     EphySecurityLevel  security_level)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (widget);
  const char *icon_name = NULL;

  g_assert (entry);

  if (!entry->reader_mode_active)
    icon_name = ephy_security_level_to_icon_name (security_level);

  if (icon_name)
    gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (entry->security_button),
                                   icon_name);

  gtk_widget_set_visible (entry->security_button, !!icon_name);

  entry->security_level = security_level;
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
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (editable);

  return GTK_EDITABLE (entry->text);
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
 * ephy_location_entry_get_can_undo:
 * @entry: an #EphyLocationEntry widget
 *
 * Whether @entry can restore the displayed user modified text to the unmodified
 * previous text.
 *
 * Return value: TRUE or FALSE indicating if the text can be restored
 *
 **/
gboolean
ephy_location_entry_get_can_undo (EphyLocationEntry *entry)
{
  return entry->user_changed;
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
ephy_location_entry_get_can_redo (EphyLocationEntry *entry)
{
  return entry->can_redo;
}

/**
 * ephy_location_entry_undo_reset:
 * @entry: an #EphyLocationEntry widget
 *
 * Undo a previous ephy_location_entry_reset.
 *
 **/
void
ephy_location_entry_undo_reset (EphyLocationEntry *entry)
{
  g_signal_handlers_block_by_func (entry, G_CALLBACK (editable_changed_cb), entry);
  gtk_editable_set_text (GTK_EDITABLE (entry), entry->saved_text);
  g_signal_handlers_unblock_by_func (entry, G_CALLBACK (editable_changed_cb), entry);
  entry->can_redo = FALSE;
  entry->user_changed = TRUE;
  update_actions (entry);
}

/**
 * ephy_location_entry_reset:
 * @entry: an #EphyLocationEntry widget
 *
 * Restore the @entry to the text corresponding to the current location, this
 * does not fire the user_changed signal. This is called each time the user
 * presses Escape while the location entry is selected.
 *
 * Return value: TRUE on success, FALSE otherwise
 *
 **/
gboolean
ephy_location_entry_reset (EphyLocationEntry *entry)
{
  const char *text, *old_text;
  g_autofree char *url = NULL;

  g_signal_emit (entry, signals[GET_LOCATION], 0, &url);
  text = url != NULL ? url : "";
  old_text = gtk_editable_get_text (GTK_EDITABLE (entry));
  old_text = old_text != NULL ? old_text : "";

  g_free (entry->saved_text);
  entry->saved_text = g_strdup (old_text);
  entry->can_redo = TRUE;

  ephy_title_widget_set_address (EPHY_TITLE_WIDGET (entry), text);

  entry->user_changed = FALSE;
  update_actions (entry);

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
}

void
ephy_location_entry_set_bookmark_icon_state (EphyLocationEntry     *self,
                                             EphyBookmarkIconState  state)
{
  self->icon_state = state;

  g_assert (EPHY_IS_LOCATION_ENTRY (self));

  if (self->adaptive_mode == EPHY_ADAPTIVE_MODE_NARROW)
    state = EPHY_BOOKMARK_ICON_HIDDEN;

  switch (state) {
    case EPHY_BOOKMARK_ICON_HIDDEN:
      gtk_widget_set_visible (self->bookmark_button, FALSE);
      gtk_widget_remove_css_class (self->bookmark_button, "starred");
      break;
    case EPHY_BOOKMARK_ICON_EMPTY:
      gtk_widget_set_visible (self->bookmark_button, TRUE);
      gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (self->bookmark_button),
                                     "ephy-non-starred-symbolic");
      gtk_widget_remove_css_class (self->bookmark_button, "starred");
      /* Translators: tooltip for the empty bookmark button */
      gtk_widget_set_tooltip_text (self->bookmark_button, _("Bookmark Page"));
      break;
    case EPHY_BOOKMARK_ICON_BOOKMARKED:
      gtk_widget_set_visible (self->bookmark_button, TRUE);
      gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (self->bookmark_button),
                                     "ephy-starred-symbolic");
      gtk_widget_add_css_class (self->bookmark_button, "starred");
      /* Translators: tooltip for the bookmarked button */
      gtk_widget_set_tooltip_text (self->bookmark_button, _("Edit Bookmark"));
      break;
    default:
      g_assert_not_reached ();
  }
}

/**
 * ephy_location_entry_set_lock_tooltip:
 * @entry: an #EphyLocationEntry widget
 * @tooltip: the text to be set in the tooltip for the lock icon
 *
 * Set the text to be displayed when hovering the lock icon of @entry.
 *
 **/
void
ephy_location_entry_set_lock_tooltip (EphyLocationEntry *entry,
                                      const char        *tooltip)
{
  gtk_widget_set_tooltip_text (entry->security_button, tooltip);
}

void
ephy_location_entry_add_permission_popover (EphyLocationEntry     *entry,
                                            EphyPermissionPopover *popover)
{
  GtkMenuButton *menu_button;

  g_assert (EPHY_IS_LOCATION_ENTRY (entry));
  g_assert (EPHY_IS_PERMISSION_POPOVER (popover));

  menu_button = GTK_MENU_BUTTON (gtk_menu_button_new ());

  switch (ephy_permission_popover_get_permission_type (popover)) {
    case EPHY_PERMISSION_TYPE_SHOW_NOTIFICATIONS:
      gtk_menu_button_set_icon_name (menu_button, "ephy-permission-notifications-symbolic");
      gtk_widget_set_tooltip_text (GTK_WIDGET (menu_button), _("Notification Request"));
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_WEBCAM:
      gtk_menu_button_set_icon_name (menu_button, "ephy-permission-camera-symbolic");
      gtk_widget_set_tooltip_text (GTK_WIDGET (menu_button), _("Camera Request"));
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_MICROPHONE:
      gtk_menu_button_set_icon_name (menu_button, "ephy-permission-microphone-symbolic");
      gtk_widget_set_tooltip_text (GTK_WIDGET (menu_button), _("Microphone Request"));
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_LOCATION:
      gtk_menu_button_set_icon_name (menu_button, "ephy-permission-location-symbolic");
      gtk_widget_set_tooltip_text (GTK_WIDGET (menu_button), _("Location Request"));
      break;
    case EPHY_PERMISSION_TYPE_ACCESS_WEBCAM_AND_MICROPHONE:
      gtk_menu_button_set_icon_name (menu_button, "ephy-permission-generic-symbolic");
      gtk_widget_set_tooltip_text (GTK_WIDGET (menu_button), _("Webcam and Microphone Request"));
      break;
    default:
      gtk_menu_button_set_icon_name (menu_button, "ephy-permission-generic-symbolic");
      gtk_widget_set_tooltip_text (GTK_WIDGET (menu_button), _("Permission Request"));
  }

  gtk_widget_set_valign (GTK_WIDGET (menu_button), GTK_ALIGN_CENTER);
  gtk_menu_button_set_popover (menu_button, GTK_WIDGET (popover));

  gtk_widget_add_css_class (GTK_WIDGET (menu_button), "entry-icon");
  gtk_widget_add_css_class (GTK_WIDGET (menu_button), "start");

  gtk_widget_set_parent (GTK_WIDGET (menu_button), GTK_WIDGET (entry));
  entry->permission_buttons = g_list_prepend (entry->permission_buttons, menu_button);
  g_signal_connect (popover, "allow", G_CALLBACK (on_permission_popover_response), menu_button);
  g_signal_connect (popover, "deny", G_CALLBACK (on_permission_popover_response), menu_button);
}

void
ephy_location_entry_show_best_permission_popover (EphyLocationEntry *entry)
{
  g_assert (EPHY_IS_LOCATION_ENTRY (entry));

  if (entry->permission_buttons) {
    GtkWidget *menu_button = g_list_last (entry->permission_buttons)->data;

    gtk_menu_button_popup (GTK_MENU_BUTTON (menu_button));
  }
}

void
ephy_location_entry_clear_permission_buttons (EphyLocationEntry *entry)
{
  GList *l;

  g_assert (EPHY_IS_LOCATION_ENTRY (entry));

  for (l = entry->permission_buttons; l; l = l->next) {
    GtkMenuButton *button = l->data;
    GtkPopover *popover = gtk_menu_button_get_popover (button);

    g_signal_handlers_disconnect_by_func (popover, G_CALLBACK (on_permission_popover_response), button);

    gtk_widget_unparent (GTK_WIDGET (button));
  }

  g_clear_pointer (&entry->permission_buttons, g_list_free);
}

void
ephy_location_entry_set_password_popover (EphyLocationEntry   *entry,
                                          EphyPasswordPopover *popover)
{
  g_assert (EPHY_IS_LOCATION_ENTRY (entry));
  g_assert (popover == NULL || EPHY_IS_PASSWORD_POPOVER (popover));

  gtk_menu_button_set_popover (GTK_MENU_BUTTON (entry->password_button),
                               GTK_WIDGET (popover));
  gtk_widget_set_visible (entry->password_button, popover != NULL);
}

void
ephy_location_entry_show_password_popover (EphyLocationEntry *entry)
{
  g_assert (EPHY_IS_LOCATION_ENTRY (entry));

  gtk_menu_button_popup (GTK_MENU_BUTTON (entry->password_button));
}

void
ephy_location_entry_set_add_bookmark_popover (EphyLocationEntry *entry,
                                              GtkPopover        *popover)
{
  g_assert (EPHY_IS_LOCATION_ENTRY (entry));
  g_assert (GTK_IS_POPOVER (popover));

  gtk_menu_button_set_popover (GTK_MENU_BUTTON (entry->bookmark_button),
                               GTK_WIDGET (popover));
}

void
ephy_location_entry_show_add_bookmark_popover (EphyLocationEntry *entry)
{
  gtk_menu_button_popup (GTK_MENU_BUTTON (entry->bookmark_button));
}

void
ephy_location_entry_set_reader_mode_visible (EphyLocationEntry *entry,
                                             gboolean           visible)
{
  gtk_widget_set_visible (entry->reader_mode_button, visible);
}

void
ephy_location_entry_set_reader_mode_state (EphyLocationEntry *entry,
                                           gboolean           active)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (entry->reader_mode_button), active);

  entry->reader_mode_active = active;
}

gboolean
ephy_location_entry_get_reader_mode_state (EphyLocationEntry *entry)
{
  return entry->reader_mode_active;
}

static void
progress_hide (gpointer user_data)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (user_data);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (entry->progress), 0);
  gtk_widget_set_visible (entry->progress, FALSE);

  g_clear_handle_id (&entry->progress_timeout, g_source_remove);
}

static void
ephy_location_entry_set_fraction_internal (gpointer user_data)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (user_data);
  gint ms;
  gdouble progress;
  gdouble current;

  entry->progress_timeout = 0;
  current = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (entry->progress));

  if ((entry->progress_fraction - current) > 0.5 || entry->progress_fraction == 1.0)
    ms = 10;
  else
    ms = 25;

  progress = current + 0.025;
  if (progress < entry->progress_fraction) {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (entry->progress), progress);
    entry->progress_timeout = g_timeout_add_once (ms, ephy_location_entry_set_fraction_internal, entry);
  } else {
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (entry->progress), entry->progress_fraction);
    if (entry->progress_fraction == 1.0)
      entry->progress_timeout = g_timeout_add_once (500, progress_hide, entry);
  }

  gtk_widget_set_visible (entry->progress, TRUE);
}

void
ephy_location_entry_set_progress (EphyLocationEntry *entry,
                                  gdouble            fraction,
                                  gboolean           loading)
{
  gdouble current_progress;

  g_clear_handle_id (&entry->progress_timeout, g_source_remove);

  if (!loading) {
    /* Setting progress to 0 when it is already 0 can actually cause the
     * progress bar to be shown. Yikes....
     */
    current_progress = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (entry->progress));
    if (current_progress != 0.0) {
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (entry->progress), 0.0);
      gtk_widget_set_visible (entry->progress, FALSE);
    }
    return;
  }

  entry->progress_fraction = fraction;
  ephy_location_entry_set_fraction_internal (entry);
}

void
ephy_location_entry_set_adaptive_mode (EphyLocationEntry *entry,
                                       EphyAdaptiveMode   adaptive_mode)
{
  entry->adaptive_mode = adaptive_mode;

  ephy_location_entry_set_bookmark_icon_state (entry, entry->icon_state);
}

void
ephy_location_entry_page_action_add (EphyLocationEntry *entry,
                                     GtkWidget         *action)
{
  entry->page_actions = g_list_prepend (entry->page_actions, action);

  gtk_widget_set_parent (action, GTK_WIDGET (entry));
}

void
ephy_location_entry_page_action_remove (EphyLocationEntry *entry,
                                        GtkWidget         *action)
{
  entry->page_actions = g_list_remove (entry->page_actions, action);

  gtk_widget_unparent (action);
}

void
ephy_location_entry_page_action_clear (EphyLocationEntry *entry)
{
  if (entry->page_actions)
    g_list_free_full (entry->page_actions, (GDestroyNotify)gtk_widget_unparent);

  entry->page_actions = NULL;
}

void
ephy_location_entry_grab_focus_without_selecting (EphyLocationEntry *entry)
{
  gtk_text_grab_focus_without_selecting (GTK_TEXT (entry->text));
}

GListModel *
ephy_location_entry_get_model (EphyLocationEntry *entry)
{
  return gtk_single_selection_get_model (entry->suggestions_model);
}

void
ephy_location_entry_set_model (EphyLocationEntry *entry,
                               GListModel        *model)
{
  gtk_single_selection_set_model (entry->suggestions_model, model);

  g_object_notify_by_pspec (G_OBJECT (entry), props[PROP_MODEL]);
}
