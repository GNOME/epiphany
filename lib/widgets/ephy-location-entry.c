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

#include "ephy-widgets-type-builtins.h"
#include "ephy-about-handler.h"
#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-gui.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-settings.h"
#include "ephy-signal-accumulator.h"
#include "ephy-suggestion.h"
#include "ephy-title-widget.h"
#include "ephy-uri-helpers.h"

#include <dazzle.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit2/webkit2.h>

/**
 * SECTION:ephy-location-entry
 * @short_description: A location entry widget
 * @see_also: #GtkEntry
 *
 * #EphyLocationEntry implements the location bar in the main Epiphany window.
 */

struct _EphyLocationEntry {
  GtkBin parent_instance;

  GtkWidget *overlay;
  GtkWidget *url_entry;
  GtkWidget *page_action_box;
  GtkWidget *bookmark_icon;
  GtkWidget *bookmark_button;
  GtkWidget *reader_mode_icon;
  GtkWidget *reader_mode_button;

  GBinding *paste_binding;

  GtkPopover *add_bookmark_popover;
  GtkCssProvider *css_provider;

  gboolean reader_mode_active;
  gboolean button_release_is_blocked;

  char *saved_text;
  char *jump_tab;

  guint allocation_width;
  guint progress_timeout;
  gdouble progress_fraction;

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
  PROP_ADDRESS,
  PROP_SECURITY_LEVEL,
  LAST_PROP
};

enum {
  ACTIVATE,
  USER_CHANGED,
  READER_MODE_CHANGED,
  GET_LOCATION,
  GET_TITLE,
  LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };

static void ephy_location_entry_title_widget_interface_init (EphyTitleWidgetInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EphyLocationEntry, ephy_location_entry, GTK_TYPE_BIN,
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
    webkit_web_context_prefetch_dns (ephy_embed_shell_get_web_context (shell), g_uri_get_host (helper->uri));

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

  uri = g_uri_parse (url, G_URI_FLAGS_NONE, NULL);
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
suggestion_selected (DzlSuggestionEntry *entry,
                     DzlSuggestion      *suggestion,
                     gpointer            user_data)
{
  EphyLocationEntry *lentry = EPHY_LOCATION_ENTRY (user_data);
  const gchar *uri = dzl_suggestion_get_id (suggestion);

  g_signal_handlers_block_by_func (entry, G_CALLBACK (editable_changed_cb), user_data);
  g_clear_pointer (&lentry->jump_tab, g_free);

  if (g_str_has_prefix (uri, "ephy-tab://")) {
    lentry->jump_tab = g_strdup (uri);
    gtk_entry_set_text (GTK_ENTRY (entry), dzl_suggestion_get_subtitle (suggestion));
  } else {
    gtk_entry_set_text (GTK_ENTRY (entry), uri);
  }
  gtk_editable_set_position (GTK_EDITABLE (entry), -1);
  g_signal_handlers_unblock_by_func (entry, G_CALLBACK (editable_changed_cb), user_data);

  schedule_dns_prefetch (lentry, uri);
}

static void
ephy_location_entry_do_copy_clipboard (GtkEntry *entry)
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

  gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (entry),
                                                    GDK_SELECTION_CLIPBOARD),
                          text, -1);
}

static void
ephy_location_entry_copy_clipboard (GtkEntry *entry,
                                    gpointer  user_data)
{
  ephy_location_entry_do_copy_clipboard (entry);

  g_signal_stop_emission_by_name (entry, "copy-clipboard");
}

static void
ephy_location_entry_cut_clipboard (GtkEntry *entry)
{
  if (!gtk_editable_get_editable (GTK_EDITABLE (entry))) {
    gtk_widget_error_bell (GTK_WIDGET (entry));
    return;
  }

  ephy_location_entry_do_copy_clipboard (entry);
  gtk_editable_delete_selection (GTK_EDITABLE (entry));

  g_signal_stop_emission_by_name (entry, "cut-clipboard");
}

static void
entry_redo_activate_cb (GtkMenuItem       *item,
                        EphyLocationEntry *entry)
{
  ephy_location_entry_undo_reset (entry);
}

static void
entry_undo_activate_cb (GtkMenuItem       *item,
                        EphyLocationEntry *entry)
{
  ephy_location_entry_reset (entry);
}

static gboolean
entry_button_release (GtkWidget *widget,
                      GdkEvent  *event,
                      gpointer   user_data)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (user_data);

  if (((GdkEventButton *)event)->button != GDK_BUTTON_PRIMARY)
    return GDK_EVENT_PROPAGATE;

  gtk_editable_select_region (GTK_EDITABLE (entry->url_entry), 0, -1);

  g_signal_handlers_block_by_func (widget, G_CALLBACK (entry_button_release), entry);
  entry->button_release_is_blocked = TRUE;

  return GDK_EVENT_STOP;
}

static void
ephy_location_entry_activate (EphyLocationEntry *entry)
{
  g_signal_emit_by_name (entry->url_entry, "activate");
}

static gboolean
entry_key_press_cb (GtkEntry          *entry,
                    GdkEventKey       *event,
                    EphyLocationEntry *location_entry)
{
  guint state = event->state & gtk_accelerator_get_default_mod_mask ();


  if (event->keyval == GDK_KEY_Escape && state == 0) {
    ephy_location_entry_reset (location_entry);
  }

  if (event->keyval == GDK_KEY_l && state == GDK_CONTROL_MASK) {
    /* Make sure the location is activated on CTRL+l even when the
     * completion popup is shown and have an active keyboard grab.
     */
    ephy_location_entry_focus (location_entry);
  }

  if (event->keyval == GDK_KEY_Return ||
      event->keyval == GDK_KEY_KP_Enter ||
      event->keyval == GDK_KEY_ISO_Enter) {
    if (location_entry->jump_tab) {
      g_signal_handlers_block_by_func (location_entry->url_entry, G_CALLBACK (editable_changed_cb), location_entry);
      gtk_entry_set_text (GTK_ENTRY (location_entry->url_entry), location_entry->jump_tab);
      g_signal_handlers_unblock_by_func (location_entry->url_entry, G_CALLBACK (editable_changed_cb), location_entry);
      g_clear_pointer (&location_entry->jump_tab, g_free);
    } else {
      g_autofree gchar *text = g_strdup (gtk_entry_get_text (GTK_ENTRY (location_entry->url_entry)));
      gchar *url = g_strstrip (text);
      g_autofree gchar *new_url = NULL;

      gtk_entry_set_text (GTK_ENTRY (entry), location_entry->jump_tab ? location_entry->jump_tab : text);

      if (strlen (url) > 5 && g_str_has_prefix (url, "http:") && url[5] != '/')
        new_url = g_strdup_printf ("http://%s", url + 5);
      else if (strlen (url) > 6 && g_str_has_prefix (url, "https:") && url[6] != '/')
        new_url = g_strdup_printf ("https://%s", url + 6);

      if (new_url) {
        g_signal_handlers_block_by_func (location_entry->url_entry, G_CALLBACK (editable_changed_cb), location_entry);
        gtk_entry_set_text (GTK_ENTRY (location_entry->url_entry), new_url);
        g_signal_handlers_unblock_by_func (location_entry->url_entry, G_CALLBACK (editable_changed_cb), location_entry);
      }

      if (state == GDK_CONTROL_MASK) {
        /* Remove control mask to prevent opening address in a new window */
        event->state &= ~GDK_CONTROL_MASK;

        if (!g_utf8_strchr (url, -1, ' ') && !g_utf8_strchr (url, -1, '.')) {
          g_autofree gchar *new_url = g_strdup_printf ("www.%s.com", url);

          g_signal_handlers_block_by_func (location_entry->url_entry, G_CALLBACK (editable_changed_cb), location_entry);
          gtk_entry_set_text (GTK_ENTRY (location_entry->url_entry), new_url);
          g_signal_handlers_unblock_by_func (location_entry->url_entry, G_CALLBACK (editable_changed_cb), location_entry);
        }
      }
    }

    ephy_location_entry_activate (location_entry);

    return GDK_EVENT_STOP;
  }

  return GDK_EVENT_PROPAGATE;
}

static void
handle_forward_tab_key (GtkWidget *widget,
                        gpointer   user_data)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (user_data);
  GtkWidget *popover;

  popover = dzl_suggestion_entry_get_popover (DZL_SUGGESTION_ENTRY (entry->url_entry));
  if (gtk_widget_is_visible (popover)) {
    g_signal_emit_by_name (entry->url_entry, "move-suggestion", 1, G_TYPE_INT, 1, G_TYPE_NONE);
  } else {
    gtk_widget_child_focus (gtk_widget_get_toplevel (GTK_WIDGET (entry)), GTK_DIR_TAB_FORWARD);
  }
}

static void
handle_backward_tab_key (GtkWidget *widget,
                         gpointer   user_data)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (user_data);
  GtkWidget *popover;

  popover = dzl_suggestion_entry_get_popover (DZL_SUGGESTION_ENTRY (entry->url_entry));
  if (gtk_widget_is_visible (popover)) {
    g_signal_emit_by_name (entry->url_entry, "move-suggestion", -1, G_TYPE_INT, 1, G_TYPE_NONE);
  } else {
    gtk_widget_child_focus (gtk_widget_get_toplevel (GTK_WIDGET (entry)), GTK_DIR_TAB_BACKWARD);
  }
}

static void
update_entry_style (EphyLocationEntry *self)
{
  PangoAttrList *attrs;
  PangoAttribute *color_normal;
  PangoAttribute *color_dimmed;
  PangoAttribute *scaled;
  g_autoptr (GUri) uri = NULL;
  const char *text = gtk_entry_get_text (GTK_ENTRY (self->url_entry));
  const char *host;
  const char *base_domain;
  char *sub_string;

  attrs = pango_attr_list_new ();

  if (self->adaptive_mode == EPHY_ADAPTIVE_MODE_NARROW) {
    scaled = pango_attr_scale_new (PANGO_SCALE_SMALL);
    pango_attr_list_insert (attrs, scaled);
  }

  if (gtk_widget_has_focus (self->url_entry))
    goto out;

  uri = g_uri_parse (text, G_URI_FLAGS_NONE, NULL);
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
  gtk_entry_set_attributes (GTK_ENTRY (self->url_entry), attrs);
  pango_attr_list_unref (attrs);
}

static gboolean
entry_focus_in_event (GtkWidget *widget,
                      GdkEvent  *event,
                      gpointer   user_data)
{
  EphyLocationEntry *self = EPHY_LOCATION_ENTRY (user_data);

  update_entry_style (self);
  return GDK_EVENT_PROPAGATE;
}

static gboolean
entry_focus_out_event (GtkWidget *widget,
                       GdkEvent  *event,
                       gpointer   user_data)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (user_data);

  update_entry_style (entry);

  if (((GdkEventButton *)event)->button != GDK_BUTTON_PRIMARY)
    return GDK_EVENT_PROPAGATE;

  /* Unselect. */
  gtk_editable_select_region (GTK_EDITABLE (entry->url_entry), 0, 0);

  if (entry->button_release_is_blocked) {
    g_signal_handlers_unblock_by_func (widget, G_CALLBACK (entry_button_release), entry);
    entry->button_release_is_blocked = FALSE;
  }

  return GDK_EVENT_PROPAGATE;
}

static gboolean
icon_button_icon_press_event_cb (GtkWidget            *widget,
                                 GtkEntryIconPosition  position,
                                 GdkEventButton       *event,
                                 EphyLocationEntry    *entry)
{
  if (((event->type == GDK_BUTTON_PRESS &&
        event->button == 1) ||
       (event->type == GDK_TOUCH_BEGIN))) {
    if (position == GTK_ENTRY_ICON_PRIMARY) {
      GdkRectangle lock_position;
      gtk_entry_get_icon_area (GTK_ENTRY (entry->url_entry), GTK_ENTRY_ICON_PRIMARY, &lock_position);
      g_signal_emit_by_name (entry, "lock-clicked", &lock_position);
    }
    return TRUE;
  }

  return FALSE;
}

static GtkBorder
get_progress_margin (EphyLocationEntry *entry)
{
  g_autoptr (GtkWidgetPath) path = NULL;
  g_autoptr (GtkStyleContext) context = NULL;
  GtkBorder margin;
  gint pos;

  path = gtk_widget_path_copy (gtk_widget_get_path (entry->url_entry));

  pos = gtk_widget_path_append_type (path, GTK_TYPE_WIDGET);
  gtk_widget_path_iter_set_object_name (path, pos, "progress");

  context = gtk_style_context_new ();
  gtk_style_context_set_path (context, path);

  gtk_style_context_get_margin (context, gtk_style_context_get_state (context), &margin);

  return margin;
}

static GtkBorder
get_padding (EphyLocationEntry *entry)
{
  g_autoptr (GtkWidgetPath) path = NULL;
  g_autoptr (GtkStyleContext) context = NULL;
  GtkBorder padding;

  path = gtk_widget_path_copy (gtk_widget_get_path (entry->url_entry));

  /* Create a new context here, since the existing one has extra css loaded */
  context = gtk_style_context_new ();
  gtk_style_context_set_path (context, path);

  gtk_style_context_get_padding (context, gtk_style_context_get_state (context), &padding);

  return padding;
}

static void
button_box_size_allocated_cb (GtkWidget    *widget,
                              GdkRectangle *allocation,
                              gpointer      user_data)
{
  EphyLocationEntry *lentry = EPHY_LOCATION_ENTRY (user_data);
  g_autofree gchar *css = NULL;
  GtkBorder margin, padding;

  if (lentry->allocation_width == (guint)allocation->width)
    return;

  lentry->allocation_width = allocation->width;

  margin = get_progress_margin (lentry);
  padding = get_padding (lentry);

  /* We are using the CSS provider here to solve UI displaying issues:
   *  - padding-right is used to prevent text below the icons on the right side
   *    of the entry (removing the icon button box width (allocation width).
   *  - progress margin-right is used to allow progress bar below icons on the
   *    right side.
   *
   * FIXME: Loading CSS during size_allocate is ILLEGAL and BROKEN.
   */
  css = g_strdup_printf (".url_entry:dir(ltr) { padding-right: %dpx; }" \
                         ".url_entry:dir(rtl) { padding-left: %dpx; }" \
                         ".url_entry:dir(ltr) progress { margin-right: %dpx; }" \
                         ".url_entry:dir(rtl) progress { margin-left: %dpx; }",
                         lentry->allocation_width,
                         lentry->allocation_width,
                         margin.right + padding.right - lentry->allocation_width,
                         margin.left + padding.left - lentry->allocation_width);
  gtk_css_provider_load_from_data (lentry->css_provider, css, -1, NULL);
}

static gboolean
event_button_press_event_cb (GtkWidget *widget,
                             GdkEvent  *event,
                             gpointer   user_data)
{
  return GDK_EVENT_STOP;
}

static void
enter_notify_cb (GtkWidget *widget,
                 GdkEvent  *event,
                 gpointer   user_data)
{
  gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_PRELIGHT, FALSE);
}

static void
leave_notify_cb (GtkWidget *widget,
                 GdkEvent  *event,
                 gpointer   user_data)
{
  gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_PRELIGHT);
}

static void
editable_changed_cb (GtkEditable       *editable,
                     EphyLocationEntry *entry)
{
  if (entry->block_update == TRUE)
    return;
  else {
    entry->user_changed = TRUE;
    entry->can_redo = FALSE;
  }

  g_clear_pointer (&entry->jump_tab, g_free);

  g_signal_emit (entry, signals[USER_CHANGED], 0);
}

static void
reader_mode_clicked_cb (EphyLocationEntry *self)
{
  self->reader_mode_active = !self->reader_mode_active;

  g_signal_emit (G_OBJECT (self), signals[READER_MODE_CHANGED], 0,
                 self->reader_mode_active);
}

static void
ephy_location_entry_suggestion_activated (DzlSuggestionEntry *entry,
                                          DzlSuggestion      *arg1,
                                          gpointer            user_data)
{
  EphyLocationEntry *lentry = EPHY_LOCATION_ENTRY (user_data);
  DzlSuggestion *suggestion = dzl_suggestion_entry_get_suggestion (entry);
  const gchar *text = ephy_suggestion_get_uri (EPHY_SUGGESTION (suggestion));

  g_signal_handlers_block_by_func (entry, G_CALLBACK (editable_changed_cb), user_data);
  gtk_entry_set_text (GTK_ENTRY (entry), lentry->jump_tab ? lentry->jump_tab : text);
  g_clear_pointer (&lentry->jump_tab, g_free);
  g_signal_handlers_unblock_by_func (entry, G_CALLBACK (editable_changed_cb), user_data);

  g_signal_stop_emission_by_name (entry, "suggestion-activated");

  dzl_suggestion_entry_hide_suggestions (entry);

  /* Now trigger the load.... */
  ephy_location_entry_activate (EPHY_LOCATION_ENTRY (lentry));
}

static void
activate_cb (EphyLocationEntry *self)
{
  GdkModifierType modifiers;

  ephy_gui_get_current_event (NULL, &modifiers, NULL, NULL);

  g_signal_emit (G_OBJECT (self), signals[ACTIVATE], 0, modifiers);
}

static void
update_reader_icon (EphyLocationEntry *entry)
{
  GtkIconTheme *theme;
  const gchar *name;

  theme = gtk_icon_theme_get_default ();

  if (gtk_icon_theme_has_icon (theme, "view-reader-symbolic"))
    name = "view-reader-symbolic";
  else
    name = "ephy-reader-mode-symbolic";

  gtk_image_set_from_icon_name (GTK_IMAGE (entry->reader_mode_icon),
                                name, GTK_ICON_SIZE_MENU);
}

static void
paste_received (GtkClipboard      *clipboard,
                const gchar       *text,
                EphyLocationEntry *entry)
{
  if (text) {
    g_signal_handlers_block_by_func (entry->url_entry, G_CALLBACK (editable_changed_cb), entry);
    gtk_entry_set_text (GTK_ENTRY (entry->url_entry), text);
    ephy_location_entry_activate (entry);
    g_signal_handlers_unblock_by_func (entry->url_entry, G_CALLBACK (editable_changed_cb), entry);
  }
}

static void
entry_paste_and_go_activate_cb (GtkMenuItem       *item,
                                EphyLocationEntry *entry)
{
  GtkClipboard *clipboard;

  clipboard = gtk_clipboard_get_default (gdk_display_get_default ());
  gtk_clipboard_request_text (clipboard,
                              (GtkClipboardTextReceivedFunc)paste_received,
                              entry);
}

static void
entry_clear_activate_cb (GtkMenuItem       *item,
                         EphyLocationEntry *entry)
{
  entry->block_update = TRUE;
  g_signal_handlers_block_by_func (entry->url_entry, G_CALLBACK (editable_changed_cb), entry);
  gtk_entry_set_text (GTK_ENTRY (entry->url_entry), "");
  g_signal_handlers_unblock_by_func (entry->url_entry, G_CALLBACK (editable_changed_cb), entry);
  entry->block_update = FALSE;
  entry->user_changed = TRUE;
}

/* The build should fail here each time when upgrading to a new major version
 * of GTK+, so that we don't forget to update this domain.
 */
#if GTK_MAJOR_VERSION == 3
#define GTK_GETTEXT_DOMAIN "gtk30"
#endif

static void
entry_populate_popup_cb (GtkEntry          *entry,
                         GtkMenu           *menu,
                         EphyLocationEntry *lentry)
{
  GtkWidget *clear_menuitem;
  GtkWidget *undo_menuitem;
  GtkWidget *redo_menuitem;
  GtkWidget *paste_and_go_menuitem;
  GtkWidget *separator;
  GtkWidget *paste_menuitem = NULL;
  GList *children, *item;
  int pos = 0, sep = 0;
  gboolean is_editable;

  /* Translators: the mnemonic shouldn't conflict with any of the
   * standard items in the GtkEntry context menu (Cut, Copy, Paste, Delete,
   * Select All, Input Methods and Insert Unicode control character.)
   */
  clear_menuitem = gtk_menu_item_new_with_mnemonic (_("Cl_ear"));
  g_signal_connect (clear_menuitem, "activate",
                    G_CALLBACK (entry_clear_activate_cb), lentry);
  is_editable = gtk_editable_get_editable (GTK_EDITABLE (entry));
  gtk_widget_set_sensitive (clear_menuitem, is_editable);
  gtk_widget_show (clear_menuitem);

  /* search for the 2nd separator (the one after Select All) in the context
   * menu, and insert this menu item before it.
   * It's a bit of a hack, but there seems to be no better way to do it :/
   */
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  for (item = children; item != NULL && sep < 2; item = item->next, pos++) {
    if (GTK_IS_SEPARATOR_MENU_ITEM (item->data))
      sep++;
  }
  g_list_free (children);

  gtk_menu_shell_insert (GTK_MENU_SHELL (menu), clear_menuitem, pos - 1);

  paste_and_go_menuitem = gtk_menu_item_new_with_mnemonic (_("Paste and _Go"));

  /* Search for the Paste menu item and insert right after it. */
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  for (item = children, pos = 0; item != NULL; item = item->next, pos++) {
    if (g_strcmp0 (gtk_menu_item_get_label (item->data), g_dgettext (GTK_GETTEXT_DOMAIN, "_Paste")) == 0) {
      paste_menuitem = item->data;
      break;
    }
  }
  g_assert (paste_menuitem != NULL);
  g_list_free (children);

  g_signal_connect (paste_and_go_menuitem, "activate",
                    G_CALLBACK (entry_paste_and_go_activate_cb), lentry);
  lentry->paste_binding = g_object_bind_property (paste_menuitem, "sensitive",
                                                  paste_and_go_menuitem, "sensitive",
                                                  G_BINDING_SYNC_CREATE);
  gtk_widget_show (paste_and_go_menuitem);
  gtk_menu_shell_insert (GTK_MENU_SHELL (menu), paste_and_go_menuitem, pos + 1);

  undo_menuitem = gtk_menu_item_new_with_mnemonic (_("_Undo"));
  gtk_widget_set_sensitive (undo_menuitem, lentry->user_changed);
  g_signal_connect (undo_menuitem, "activate",
                    G_CALLBACK (entry_undo_activate_cb), lentry);
  gtk_widget_show (undo_menuitem);
  gtk_menu_shell_insert (GTK_MENU_SHELL (menu), undo_menuitem, 0);

  redo_menuitem = gtk_menu_item_new_with_mnemonic (_("_Redo"));
  gtk_widget_set_sensitive (redo_menuitem, lentry->can_redo);
  g_signal_connect (redo_menuitem, "activate",
                    G_CALLBACK (entry_redo_activate_cb), lentry);
  gtk_widget_show (redo_menuitem);
  gtk_menu_shell_insert (GTK_MENU_SHELL (menu), redo_menuitem, 1);

  separator = gtk_separator_menu_item_new ();
  gtk_widget_show (separator);
  gtk_menu_shell_insert (GTK_MENU_SHELL (menu), separator, 2);
}

static void
position_func (DzlSuggestionEntry *self,
               GdkRectangle       *area,
               gboolean           *is_absolute,
               gpointer            user_data)
{
  GtkStyleContext *style_context;
  GtkAllocation alloc;
  GtkStateFlags state;
  GtkBorder margin;

  g_assert (DZL_IS_SUGGESTION_ENTRY (self));
  g_assert (area != NULL);
  g_assert (is_absolute != NULL);

  *is_absolute = FALSE;

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  area->y += alloc.height;
  area->y += 6;
  area->height = 300;

  /* Adjust for bottom margin */
  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  state = gtk_style_context_get_state (style_context);
  gtk_style_context_get_margin (style_context, state, &margin);

  area->y -= margin.bottom;
  area->x += margin.left;
  area->width -= margin.left + margin.right;
}

static void
ephy_location_entry_get_preferred_height (GtkWidget *widget,
                                          gint      *minimum_height,
                                          gint      *natural_height)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (widget);

  gtk_widget_get_preferred_height (entry->url_entry, minimum_height, natural_height);
}

static void
ephy_location_entry_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (object);

  switch (prop_id) {
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

  switch (prop_id) {
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
ephy_location_entry_constructed (GObject *object)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (object);

  G_OBJECT_CLASS (ephy_location_entry_parent_class)->constructed (object);

  gtk_entry_set_input_hints (GTK_ENTRY (entry->url_entry), GTK_INPUT_HINT_NO_EMOJI);
}

static void
ephy_location_entry_dispose (GObject *object)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (object);

  g_clear_handle_id (&entry->progress_timeout, g_source_remove);

  g_clear_object (&entry->css_provider);

  G_OBJECT_CLASS (ephy_location_entry_parent_class)->dispose (object);
}

static void
ephy_location_entry_finalize (GObject *object)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (object);

  g_free (entry->saved_text);
  g_clear_pointer (&entry->jump_tab, g_free);

  G_OBJECT_CLASS (ephy_location_entry_parent_class)->finalize (object);
}

static void
ephy_location_entry_class_init (EphyLocationEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ephy_location_entry_get_property;
  object_class->set_property = ephy_location_entry_set_property;
  object_class->constructed = ephy_location_entry_constructed;
  object_class->finalize = ephy_location_entry_finalize;
  object_class->dispose = ephy_location_entry_dispose;

  widget_class->get_preferred_height = ephy_location_entry_get_preferred_height;

  g_object_class_override_property (object_class, PROP_ADDRESS, "address");
  g_object_class_override_property (object_class, PROP_SECURITY_LEVEL, "security-level");

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
                                        0,
                                        G_TYPE_NONE);

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
}

static void
ephy_location_entry_construct_contents (EphyLocationEntry *entry)
{
  GtkWidget *event;
  GtkWidget *box;
  GtkStyleContext *context;
  DzlShortcutController *controller;

  LOG ("EphyLocationEntry constructing contents %p", entry);

  /* Overlay */
  entry->overlay = gtk_overlay_new ();
  gtk_widget_show (GTK_WIDGET (entry->overlay));
  gtk_container_add (GTK_CONTAINER (entry), entry->overlay);

  /* URL entry */
  entry->url_entry = dzl_suggestion_entry_new ();
  dzl_suggestion_entry_set_compact (DZL_SUGGESTION_ENTRY (entry->url_entry), TRUE);
  dzl_suggestion_entry_set_position_func (DZL_SUGGESTION_ENTRY (entry->url_entry), position_func, NULL, NULL);
  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry->url_entry), GTK_ENTRY_ICON_PRIMARY, _("Show website security status and permissions"));
  gtk_entry_set_width_chars (GTK_ENTRY (entry->url_entry), 0);
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry->url_entry), _("Search for websites, bookmarks, and open tabs"));
  g_signal_connect_swapped (entry->url_entry, "activate", G_CALLBACK (activate_cb), entry);

  /* Add special widget css provider */
  context = gtk_widget_get_style_context (GTK_WIDGET (entry->url_entry));
  entry->css_provider = gtk_css_provider_new ();
  gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (entry->css_provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (entry->url_entry)), "url_entry");
  g_signal_connect (G_OBJECT (entry->url_entry), "copy-clipboard", G_CALLBACK (ephy_location_entry_copy_clipboard), NULL);
  g_signal_connect (G_OBJECT (entry->url_entry), "cut-clipboard", G_CALLBACK (ephy_location_entry_cut_clipboard), NULL);
  g_signal_connect (G_OBJECT (entry->url_entry), "changed", G_CALLBACK (editable_changed_cb), entry);
  g_signal_connect (G_OBJECT (entry->url_entry), "suggestion-selected", G_CALLBACK (suggestion_selected), entry);
  gtk_widget_show (GTK_WIDGET (entry->url_entry));
  gtk_container_add (GTK_CONTAINER (entry->overlay), GTK_WIDGET (entry->url_entry));

  /* Custom hover state. FIXME: Remove this for GTK4 */
  gtk_widget_add_events (GTK_WIDGET (entry->url_entry), GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
  g_signal_connect (G_OBJECT (entry->url_entry), "enter-notify-event", G_CALLBACK (enter_notify_cb), entry);
  g_signal_connect (G_OBJECT (entry->url_entry), "leave-notify-event", G_CALLBACK (leave_notify_cb), entry);

  /* Event box */
  event = gtk_event_box_new ();
  gtk_widget_set_halign (event, GTK_ALIGN_END);
  gtk_widget_show (event);
  g_signal_connect (G_OBJECT (event), "button-press-event", G_CALLBACK (event_button_press_event_cb), entry);
  gtk_overlay_add_overlay (GTK_OVERLAY (entry->overlay), event);

  /* Button Box */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (event), box);
  g_signal_connect (G_OBJECT (box), "size-allocate", G_CALLBACK (button_box_size_allocated_cb), entry);
  gtk_widget_set_valign (box, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (box, GTK_ALIGN_END);
  gtk_widget_show (box);

  /* Page action box */
  entry->page_action_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (box), entry->page_action_box, FALSE, TRUE, 0);

  context = gtk_widget_get_style_context (box);
  gtk_style_context_add_class (context, "entry_icon_box");

  /* Reader Mode */
  entry->reader_mode_button = gtk_button_new_from_icon_name (NULL, GTK_ICON_SIZE_MENU);
  gtk_widget_set_tooltip_text (entry->reader_mode_button, _("Toggle reader mode"));
  entry->reader_mode_icon = gtk_button_get_image (GTK_BUTTON (entry->reader_mode_button));
  gtk_box_pack_start (GTK_BOX (box), entry->reader_mode_button, FALSE, TRUE, 0);
  g_signal_connect_swapped (entry->reader_mode_button, "clicked",
                            G_CALLBACK (reader_mode_clicked_cb), entry);

  context = gtk_widget_get_style_context (entry->reader_mode_icon);
  gtk_style_context_add_class (context, "entry_icon");

  update_reader_icon (entry);
  g_signal_connect_object (gtk_settings_get_default (), "notify::gtk-icon-theme-name",
                           G_CALLBACK (update_reader_icon), entry, G_CONNECT_SWAPPED);

  /* Bookmark */
  entry->bookmark_icon = gtk_image_new_from_icon_name ("non-starred-symbolic", GTK_ICON_SIZE_MENU);
  context = gtk_widget_get_style_context (entry->bookmark_icon);
  gtk_style_context_add_class (context, "entry_icon");
  gtk_widget_show (entry->bookmark_icon);

  entry->bookmark_button = gtk_menu_button_new ();
  gtk_container_add (GTK_CONTAINER (entry->bookmark_button), entry->bookmark_icon);
  context = gtk_widget_get_style_context (entry->bookmark_button);
  gtk_style_context_add_class (context, "image-button");

  gtk_widget_set_tooltip_text (entry->bookmark_button, _("Bookmark this page"));
  gtk_box_pack_start (GTK_BOX (box), entry->bookmark_button, FALSE, TRUE, 0);

  g_settings_bind (EPHY_SETTINGS_LOCKDOWN,
                   EPHY_PREFS_LOCKDOWN_BOOKMARK_EDITING,
                   entry->bookmark_button,
                   "visible",
                   G_SETTINGS_BIND_GET | G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_object_connect (entry->url_entry,
                    "signal::icon-press", G_CALLBACK (icon_button_icon_press_event_cb), entry,
                    "signal::populate-popup", G_CALLBACK (entry_populate_popup_cb), entry,
                    "signal::key-press-event", G_CALLBACK (entry_key_press_cb), entry,
                    NULL);

  g_signal_connect (entry->url_entry, "suggestion-activated",
                    G_CALLBACK (ephy_location_entry_suggestion_activated), entry);

  g_signal_connect (entry->url_entry, "button-release-event", G_CALLBACK (entry_button_release), entry);
  g_signal_connect (entry->url_entry, "focus-in-event", G_CALLBACK (entry_focus_in_event), entry);
  g_signal_connect (entry->url_entry, "focus-out-event", G_CALLBACK (entry_focus_out_event), entry);

  controller = dzl_shortcut_controller_find (entry->url_entry);
  dzl_shortcut_controller_add_command_callback (controller,
                                                "org.gnome.Epiphany.complete-url-forward",
                                                "Tab",
                                                DZL_SHORTCUT_PHASE_DISPATCH,
                                                handle_forward_tab_key,
                                                entry,
                                                NULL);

  dzl_shortcut_controller_add_command_callback (controller,
                                                "org.gnome.Epiphany.complete-url-backward",
                                                "ISO_Left_Tab",
                                                DZL_SHORTCUT_PHASE_DISPATCH,
                                                handle_backward_tab_key,
                                                entry,
                                                NULL);
}

static void
ephy_location_entry_init (EphyLocationEntry *le)
{
  LOG ("EphyLocationEntry initialising %p", le);

  le->user_changed = FALSE;
  le->block_update = FALSE;
  le->button_release_is_blocked = FALSE;
  le->saved_text = NULL;

  ephy_location_entry_construct_contents (le);
}

static const char *
ephy_location_entry_title_widget_get_address (EphyTitleWidget *widget)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (widget);

  g_assert (entry);

  return gtk_entry_get_text (GTK_ENTRY (entry->url_entry));
}

static void
ephy_location_entry_title_widget_set_address (EphyTitleWidget *widget,
                                              const char      *address)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (widget);
  GtkClipboard *clipboard;
  const char *text;
  g_autofree char *effective_text = NULL;
  g_autofree char *selection = NULL;
  int start, end;
  const char *final_text;

  g_assert (widget);

  /* Setting a new text will clear the clipboard. This makes it impossible
   * to copy&paste from the location entry of one tab into another tab, see
   * bug #155824. So we save the selection iff the clipboard was owned by
   * the location entry.
   */
  if (gtk_widget_get_realized (GTK_WIDGET (entry))) {
    clipboard = gtk_widget_get_clipboard (GTK_WIDGET (entry->url_entry),
                                          GDK_SELECTION_PRIMARY);
    g_assert (clipboard != NULL);

    if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (entry->url_entry) &&
        gtk_editable_get_selection_bounds (GTK_EDITABLE (entry->url_entry),
                                           &start, &end)) {
      selection = gtk_editable_get_chars (GTK_EDITABLE (entry->url_entry),
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
  g_signal_handlers_block_by_func (entry->url_entry, G_CALLBACK (editable_changed_cb), entry);
  gtk_entry_set_text (GTK_ENTRY (entry->url_entry), final_text);
  update_entry_style (entry);
  g_signal_handlers_unblock_by_func (entry->url_entry, G_CALLBACK (editable_changed_cb), entry);

  dzl_suggestion_entry_hide_suggestions (DZL_SUGGESTION_ENTRY (entry->url_entry));
  entry->block_update = FALSE;

  /* Now restore the selection.
   * Note that it's not owned by the entry anymore!
   */
  if (selection != NULL) {
    gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY),
                            selection, strlen (selection));
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
ephy_location_entry_title_widget_set_security_level (EphyTitleWidget  *widget,
                                                     EphySecurityLevel security_level)

{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (widget);
  const char *icon_name;

  g_assert (entry);

  if (!entry->reader_mode_active) {
    icon_name = ephy_security_level_to_icon_name (security_level);
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry->url_entry),
                                       GTK_ENTRY_ICON_PRIMARY,
                                       icon_name);
  } else {
    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry->url_entry),
                                       GTK_ENTRY_ICON_PRIMARY,
                                       NULL);
  }

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

GtkWidget *
ephy_location_entry_new (void)
{
  return GTK_WIDGET (g_object_new (EPHY_TYPE_LOCATION_ENTRY, NULL));
}

/**
 * ephy_location_entry_get_can_undo:
 * @entry: an #EphyLocationEntry widget
 *
 * Wheter @entry can restore the displayed user modified text to the unmodified
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
 * Wheter @entry can restore the displayed text to the user modified version
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
  g_signal_handlers_block_by_func (entry->url_entry, G_CALLBACK (editable_changed_cb), entry);
  gtk_entry_set_text (GTK_ENTRY (entry->url_entry), entry->saved_text);
  g_signal_handlers_unblock_by_func (entry->url_entry, G_CALLBACK (editable_changed_cb), entry);
  entry->can_redo = FALSE;
  entry->user_changed = TRUE;
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
  old_text = gtk_entry_get_text (GTK_ENTRY (entry->url_entry));
  old_text = old_text != NULL ? old_text : "";

  g_free (entry->saved_text);
  entry->saved_text = g_strdup (old_text);
  entry->can_redo = TRUE;

  ephy_title_widget_set_address (EPHY_TITLE_WIDGET (entry), text);

  entry->user_changed = FALSE;

  return g_strcmp0 (text, old_text);
}

/**
 * ephy_location_entry_focus:
 * @entry: an #EphyLocationEntry widget
 *
 * Set focus on @entry and select the text whithin. This is called when the
 * user hits Control+L.
 *
 **/
void
ephy_location_entry_focus (EphyLocationEntry *entry)
{
  gtk_widget_grab_focus (GTK_WIDGET (entry->url_entry));
}

void
ephy_location_entry_set_bookmark_icon_state (EphyLocationEntry     *self,
                                             EphyBookmarkIconState  state)
{
  GtkStyleContext *context;

  self->icon_state = state;

  g_assert (EPHY_IS_LOCATION_ENTRY (self));

  context = gtk_widget_get_style_context (GTK_WIDGET (self->bookmark_icon));

  if (self->adaptive_mode == EPHY_ADAPTIVE_MODE_NARROW)
    state = EPHY_BOOKMARK_ICON_HIDDEN;

  switch (state) {
    case EPHY_BOOKMARK_ICON_HIDDEN:
      gtk_widget_set_visible (self->bookmark_button, FALSE);
      gtk_style_context_remove_class (context, "starred");
      gtk_style_context_remove_class (context, "non-starred");
      break;
    case EPHY_BOOKMARK_ICON_EMPTY:
      gtk_widget_set_visible (self->bookmark_button, TRUE);
      gtk_image_set_from_icon_name (GTK_IMAGE (self->bookmark_icon),
                                    "non-starred-symbolic",
                                    GTK_ICON_SIZE_MENU);
      gtk_style_context_remove_class (context, "starred");
      gtk_style_context_add_class (context, "non-starred");
      break;
    case EPHY_BOOKMARK_ICON_BOOKMARKED:
      gtk_widget_set_visible (self->bookmark_button, TRUE);
      gtk_image_set_from_icon_name (GTK_IMAGE (self->bookmark_icon),
                                    "starred-symbolic",
                                    GTK_ICON_SIZE_MENU);
      gtk_style_context_remove_class (context, "non-starred");
      gtk_style_context_add_class (context, "starred");
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
  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry->url_entry),
                                   GTK_ENTRY_ICON_PRIMARY,
                                   tooltip);
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
  GtkPopover *popover = gtk_menu_button_get_popover (GTK_MENU_BUTTON (entry->bookmark_button));

  gtk_popover_popup (popover);
}

GtkWidget *
ephy_location_entry_get_entry (EphyLocationEntry *entry)
{
  return GTK_WIDGET (entry->url_entry);
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
  if (active)
    gtk_style_context_add_class (gtk_widget_get_style_context (entry->reader_mode_icon), "selected");
  else
    gtk_style_context_remove_class (gtk_widget_get_style_context (entry->reader_mode_icon), "selected");

  entry->reader_mode_active = active;
}

gboolean
ephy_location_entry_get_reader_mode_state (EphyLocationEntry *entry)
{
  return entry->reader_mode_active;
}

static gboolean
progress_hide (gpointer user_data)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (user_data);

  gtk_entry_set_progress_fraction (GTK_ENTRY (entry->url_entry), 0);

  g_clear_handle_id (&entry->progress_timeout, g_source_remove);

  return G_SOURCE_REMOVE;
}

static gboolean
ephy_location_entry_set_fraction_internal (gpointer user_data)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (user_data);
  gint ms;
  gdouble progress;
  gdouble current;

  entry->progress_timeout = 0;
  current = gtk_entry_get_progress_fraction (GTK_ENTRY (entry->url_entry));

  if ((entry->progress_fraction - current) > 0.5 || entry->progress_fraction == 1.0)
    ms = 10;
  else
    ms = 25;

  progress = current + 0.025;
  if (progress < entry->progress_fraction) {
    gtk_entry_set_progress_fraction (GTK_ENTRY (entry->url_entry), progress);
    entry->progress_timeout = g_timeout_add (ms, ephy_location_entry_set_fraction_internal, entry);
  } else {
    gtk_entry_set_progress_fraction (GTK_ENTRY (entry->url_entry), entry->progress_fraction);
    if (entry->progress_fraction == 1.0)
      entry->progress_timeout = g_timeout_add (500, progress_hide, entry);
  }

  return G_SOURCE_REMOVE;
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
    current_progress = gtk_entry_get_progress_fraction (GTK_ENTRY (entry->url_entry));
    if (current_progress != 0.0)
      gtk_entry_set_progress_fraction (GTK_ENTRY (entry->url_entry), 0.0);
    return;
  }

  entry->progress_fraction = fraction;
  ephy_location_entry_set_fraction_internal (entry);
}

void
ephy_location_entry_set_adaptive_mode (EphyLocationEntry *entry,
                                       EphyAdaptiveMode   adaptive_mode)
{
  if (adaptive_mode == EPHY_ADAPTIVE_MODE_NARROW)
    dzl_suggestion_entry_set_position_func (DZL_SUGGESTION_ENTRY (entry->url_entry), dzl_suggestion_entry_window_position_func, NULL, NULL);
  else
    dzl_suggestion_entry_set_position_func (DZL_SUGGESTION_ENTRY (entry->url_entry), position_func, NULL, NULL);

  entry->adaptive_mode = adaptive_mode;

  update_entry_style (entry);

  ephy_location_entry_set_bookmark_icon_state (entry, entry->icon_state);
}

void
ephy_location_entry_page_action_add (EphyLocationEntry *entry,
                                     GtkWidget         *action)
{
  gtk_box_pack_end (GTK_BOX (entry->page_action_box), action, FALSE, TRUE, 0);

  gtk_widget_show (entry->page_action_box);
}

static void
clear_page_actions (GtkWidget *child,
                    gpointer   user_data)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (user_data);

  gtk_container_remove (GTK_CONTAINER (entry->page_action_box), child);
}

void
ephy_location_entry_page_action_clear (EphyLocationEntry *entry)
{
  gtk_container_foreach (GTK_CONTAINER (entry->page_action_box), clear_page_actions, entry);

  gtk_widget_hide (entry->page_action_box);
}
