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
#include "ephy-gui.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-signal-accumulator.h"
#include "ephy-suggestion.h"
#include "ephy-title-widget.h"
#include "ephy-uri-helpers.h"

#include <dazzle.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#if 0
/* FIXME: Refactor the DNS prefetch, this is a layering violation */
#include <libsoup/soup.h>
#include <webkit2/webkit2.h>
#endif

/**
 * SECTION:ephy-location-entry
 * @short_description: A location entry widget
 * @see_also: #GtkEntry
 *
 * #EphyLocationEntry implements the location bar in the main Epiphany window.
 */

struct _EphyLocationEntry {
  GtkOverlay parent_instance;

  GtkWidget *url_entry;
  GtkWidget *bookmark;
  GtkWidget *bookmark_event_box;
  GtkWidget *reader_mode;
  GtkWidget *reader_mode_event_box;
  GtkTreeModel *model;

  GSList *search_terms;

  GBinding *paste_binding;

  GtkPopover *add_bookmark_popover;
  GtkCssProvider *css_provider;

  gboolean reader_mode_active;

  char *before_completion;
  char *saved_text;

  guint text_col;
  guint action_col;
  guint keywords_col;
  guint relevance_col;
  guint url_col;
  guint extra_col;
  guint favicon_col;

  guint hash;

  guint allocation_width;
  guint progress_timeout;
  gdouble progress_fraction;

  gulong dns_prefetch_handler;

  guint user_changed : 1;
  guint can_redo : 1;
  guint block_update : 1;
  guint original_address : 1;
  guint apply_colors : 1;
  guint needs_reset : 1;

  EphySecurityLevel security_level;
};

static gboolean ephy_location_entry_reset_internal (EphyLocationEntry *, gboolean);

enum {
  PROP_0,
  PROP_ADDRESS,
  PROP_SECURITY_LEVEL,
  LAST_PROP
};

enum signalsEnum {
  USER_CHANGED,
  BOOKMARK_CLICKED,
  GET_LOCATION,
  GET_TITLE,
  LAST_SIGNAL
};
static gint signals[LAST_SIGNAL] = { 0 };

static void ephy_location_entry_title_widget_interface_init (EphyTitleWidgetInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EphyLocationEntry, ephy_location_entry, GTK_TYPE_OVERLAY,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_TITLE_WIDGET,
                                                ephy_location_entry_title_widget_interface_init))

static void
editable_changed_cb (GtkEditable       *editable,
                     EphyLocationEntry *entry);

static void
ephy_location_entry_activate (EphyLocationEntry *entry)
{
  g_signal_emit_by_name (entry->url_entry, "activate");
}

static void
update_address_state (EphyLocationEntry *entry)
{
  const char *text;

  text = gtk_entry_get_text (GTK_ENTRY (entry->url_entry));
  entry->original_address = text != NULL &&
                            g_str_hash (text) == entry->hash;
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
  char *effective_text = NULL, *selection = NULL;
  int start, end;

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

  /* First record the new hash, then update the entry text */
  entry->hash = g_str_hash (effective_text ? effective_text : text);

  entry->block_update = TRUE;
  g_signal_handlers_block_by_func (entry->url_entry, G_CALLBACK (editable_changed_cb), entry);
  gtk_entry_set_text (GTK_ENTRY (entry->url_entry), effective_text ? effective_text : text);
  g_signal_handlers_unblock_by_func (entry->url_entry, G_CALLBACK (editable_changed_cb), entry);

  dzl_suggestion_entry_hide_suggestions (DZL_SUGGESTION_ENTRY (entry->url_entry));
  entry->block_update = FALSE;
  g_free (effective_text);

  /* We need to call update_address_state() here, as the 'changed' signal
   * may not get called if the user has typed in the exact correct url */
  update_address_state (entry);

  /* Now restore the selection.
   * Note that it's not owned by the entry anymore!
   */
  if (selection != NULL) {
    gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY),
                            selection, strlen (selection));
    g_free (selection);
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

#if GTK_CHECK_VERSION(3, 22, 20)
  gtk_entry_set_input_hints (GTK_ENTRY (entry->url_entry), GTK_INPUT_HINT_NO_EMOJI);
#endif
}

static void
ephy_location_entry_finalize (GObject *object)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (object);

  g_free (entry->saved_text);

  G_OBJECT_CLASS (ephy_location_entry_parent_class)->finalize (object);
}

static void
ephy_location_entry_get_preferred_width (GtkWidget *widget,
                                         gint      *minimum_width,
                                         gint      *natural_width)
{
  if (minimum_width)
    *minimum_width = -1;

  if (natural_width)
    *natural_width = 848;
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
ephy_location_entry_do_copy_clipboard (GtkEntry *entry)
{
  char *text;
  gint start;
  gint end;

  if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end))
    return;

  text = gtk_editable_get_chars (GTK_EDITABLE (entry), start, end);

  if (start == 0) {
    char *tmp = text;
    text = ephy_uri_normalize (tmp);
    g_free (tmp);
  }

  gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (entry),
                                                    GDK_SELECTION_CLIPBOARD),
                          text, -1);
  g_free (text);
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
ephy_location_entry_title_widget_interface_init (EphyTitleWidgetInterface *iface)
{
  iface->get_address = ephy_location_entry_title_widget_get_address;
  iface->set_address = ephy_location_entry_title_widget_set_address;
  iface->get_security_level = ephy_location_entry_title_widget_get_security_level;
  iface->set_security_level = ephy_location_entry_title_widget_set_security_level;
}

static void
ephy_location_entry_dispose (GObject *object)
{
  EphyLocationEntry *entry = EPHY_LOCATION_ENTRY (object);

  if (entry->progress_timeout) {
    g_source_remove (entry->progress_timeout);
    entry->progress_timeout = 0;
  }

  g_clear_object (&entry->css_provider);

  G_OBJECT_CLASS (ephy_location_entry_parent_class)->dispose (object);
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

  widget_class->get_preferred_width = ephy_location_entry_get_preferred_width;
  widget_class->get_preferred_height = ephy_location_entry_get_preferred_height;

  g_object_class_override_property (object_class, PROP_ADDRESS, "address");
  g_object_class_override_property (object_class, PROP_SECURITY_LEVEL, "security-level");

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
   * EphyLocationEntry::bookmark-clicked:
   * @entry: the object on which the signal is emitted
   *
   * Emitted when the user clicks the bookmark icon inside the
   * #EphyLocationEntry.
   *
   */
  signals[BOOKMARK_CLICKED] = g_signal_new ("bookmark-clicked", G_OBJECT_CLASS_TYPE (klass),
                                            G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                                            0, NULL, NULL, NULL,
                                            G_TYPE_NONE,
                                            0);

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
editable_changed_cb (GtkEditable       *editable,
                     EphyLocationEntry *entry)
{
  update_address_state (entry);

  if (entry->block_update == TRUE)
    return;
  else {
    entry->user_changed = TRUE;
    entry->can_redo = FALSE;
  }

  g_signal_emit (entry, signals[USER_CHANGED], 0);
}

static gboolean
entry_key_press_cb (GtkEntry          *entry,
                    GdkEventKey       *event,
                    EphyLocationEntry *location_entry)
{
  guint state = event->state & gtk_accelerator_get_default_mod_mask ();


  if (event->keyval == GDK_KEY_Escape && state == 0) {
    ephy_location_entry_reset_internal (location_entry, TRUE);
    /* don't return TRUE since we want to cancel the autocompletion popup too */
  }

  if (event->keyval == GDK_KEY_l && state == GDK_CONTROL_MASK) {
    /* Make sure the location is activated on CTRL+l even when the
     * completion popup is shown and have an active keyboard grab.
     */
    ephy_location_entry_focus (location_entry);
  }

  if (event->keyval == GDK_KEY_Return ||
      event->keyval == GDK_KEY_KP_Enter ||
      event->keyval == GDK_KEY_ISO_Enter)
    ephy_location_entry_activate (location_entry);

  return FALSE;
}

static void
entry_clear_activate_cb (GtkMenuItem       *item,
                         EphyLocationEntry *entry)
{
  entry->block_update = TRUE;
  gtk_entry_set_text (GTK_ENTRY (entry->url_entry), "");
  entry->block_update = FALSE;
  entry->user_changed = TRUE;
}

static void
paste_received (GtkClipboard      *clipboard,
                const gchar       *text,
                EphyLocationEntry *entry)
{
  if (text) {
    gtk_entry_set_text (GTK_ENTRY (entry->url_entry), text);
    ephy_location_entry_activate (entry);
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
entry_redo_activate_cb (GtkMenuItem       *item,
                        EphyLocationEntry *entry)
{
  ephy_location_entry_undo_reset (entry);
}

static void
entry_undo_activate_cb (GtkMenuItem       *item,
                        EphyLocationEntry *entry)
{
  ephy_location_entry_reset_internal (entry, FALSE);
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

static gboolean
icon_button_icon_press_event_cb (GtkWidget           *widget,
                                 GtkEntryIconPosition position,
                                 GdkEventButton      *event,
                                 EphyLocationEntry   *entry)
{
  if (((event->type == GDK_BUTTON_PRESS &&
        event->button == 1) ||
       (event->type == GDK_TOUCH_BEGIN))) {
    if (position == GTK_ENTRY_ICON_PRIMARY) {
      GdkRectangle lock_position;
      gtk_entry_get_icon_area (GTK_ENTRY (entry->url_entry), GTK_ENTRY_ICON_PRIMARY, &lock_position);
      g_signal_emit_by_name (entry, "lock-clicked", &lock_position);
    } else {
      g_signal_emit (entry, signals[BOOKMARK_CLICKED], 0);
    }
    return TRUE;
  }

  return FALSE;
}

static gboolean
bookmark_icon_button_press_event_cb (GtkWidget           *entry,
                                     GdkEventButton      *event,
                                     EphyLocationEntry   *lentry)
{
  if (((event->type == GDK_BUTTON_PRESS &&
        event->button == 1) ||
       (event->type == GDK_TOUCH_BEGIN))) {
      g_signal_emit (lentry, signals[BOOKMARK_CLICKED], 0);
  }

  return TRUE;
}

static void
button_box_size_allocated_cb (GtkWidget    *widget,
                              GdkRectangle *allocation,
                              gpointer      user_data)
{
  EphyLocationEntry *lentry = EPHY_LOCATION_ENTRY (user_data);
  gchar *css;

  if (lentry->allocation_width == allocation->width)
    return;

  lentry->allocation_width = allocation->width;

  /* We are using the CSS provider here to solve UI displaying issues:
   *  - padding-right is used to prevent text below the icons on the right side
   *    of the entry (removing the icon button box width (allocation width)
   *    including border spacing 5).
   *  - progress margin-right is used to allow progress bar below icons on the
   *    right side.
   */
  css = g_strdup_printf (".url_entry { padding-right: %dpx; }"\
                         ".url_entry progress { margin-right: -%dpx; }",
                         lentry->allocation_width + 5,
                         lentry->allocation_width);
  gtk_css_provider_load_from_data (lentry->css_provider, css, -1, NULL);

  g_free (css);
}

static void
ephy_location_entry_suggestion_activated (DzlSuggestionEntry *entry,
                                          DzlSuggestion      *arg1,
                                          gpointer            user_data)
{
  EphyLocationEntry *lentry = EPHY_LOCATION_ENTRY (user_data);
  DzlSuggestion *suggestion = dzl_suggestion_entry_get_suggestion (entry);
  g_signal_handlers_block_by_func (entry, G_CALLBACK (editable_changed_cb), user_data);
  gtk_entry_set_text (GTK_ENTRY (entry), ephy_suggestion_get_uri (EPHY_SUGGESTION (suggestion)));
  g_signal_handlers_unblock_by_func (entry, G_CALLBACK (editable_changed_cb), user_data);

  g_signal_stop_emission_by_name (entry, "suggestion-activated");

  dzl_suggestion_entry_hide_suggestions (entry);

  /* Now trigger the load.... */
  ephy_location_entry_activate (EPHY_LOCATION_ENTRY (lentry));
}

static void
suggestion_selected (DzlSuggestionEntry *entry,
                     DzlSuggestion      *suggestion,
                     gpointer            user_data)
{
  const gchar *uri = dzl_suggestion_get_id (suggestion);

  g_signal_handlers_block_by_func (entry, G_CALLBACK (editable_changed_cb), user_data);
  gtk_entry_set_text (GTK_ENTRY (entry), uri);
  gtk_editable_set_position (GTK_EDITABLE (entry), -1);
  g_signal_handlers_unblock_by_func (entry, G_CALLBACK (editable_changed_cb), user_data);
}

static void
ephy_location_entry_construct_contents (EphyLocationEntry *entry)
{
  GtkWidget *button_box;
  GtkStyleContext *context;

  LOG ("EphyLocationEntry constructing contents %p", entry);

  /* URL entry */
  entry->url_entry = dzl_suggestion_entry_new ();

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
  gtk_overlay_add_overlay (GTK_OVERLAY (entry), GTK_WIDGET (entry->url_entry));

  /* Button Box */
  button_box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_homogeneous (GTK_BOX (button_box), FALSE);
  g_signal_connect (G_OBJECT (button_box), "size-allocate", G_CALLBACK (button_box_size_allocated_cb), entry);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_EXPAND);
  gtk_widget_set_halign (button_box, GTK_ALIGN_END);
  gtk_widget_set_margin_end (button_box, 5);
  gtk_widget_show (button_box);
  gtk_overlay_add_overlay (GTK_OVERLAY (entry), button_box);

  /* Bookmark */
  entry->bookmark_event_box = gtk_event_box_new ();
  entry->bookmark = gtk_image_new_from_icon_name ("non-starred-symbolic", GTK_ICON_SIZE_MENU);
  gtk_widget_show (entry->bookmark);
  g_signal_connect (G_OBJECT (entry->bookmark_event_box), "button_press_event", G_CALLBACK (bookmark_icon_button_press_event_cb), entry);
  gtk_container_add (GTK_CONTAINER(entry->bookmark_event_box), entry->bookmark);
  gtk_box_pack_end (GTK_BOX (button_box), entry->bookmark_event_box, FALSE, FALSE, 6);

  context = gtk_widget_get_style_context (entry->bookmark);
  gtk_style_context_add_class (context, "entry_icon");

  /* Reader Mode */
  entry->reader_mode_event_box = gtk_event_box_new ();
  entry->reader_mode = gtk_image_new_from_icon_name ("ephy-reader-mode-symbolic", GTK_ICON_SIZE_MENU);
  gtk_widget_show (entry->reader_mode);
  gtk_container_add (GTK_CONTAINER(entry->reader_mode_event_box), entry->reader_mode);
  gtk_box_pack_end (GTK_BOX (button_box), entry->reader_mode_event_box, FALSE, FALSE, 6);

  context = gtk_widget_get_style_context (entry->reader_mode);
  gtk_style_context_add_class (context, "entry_icon");

  g_object_connect (entry->url_entry,
                    "signal::icon-press", G_CALLBACK (icon_button_icon_press_event_cb), entry,
                    "signal::populate-popup", G_CALLBACK (entry_populate_popup_cb), entry,
                    "signal::key-press-event", G_CALLBACK (entry_key_press_cb), entry,
                    NULL);

  g_signal_connect (entry->url_entry, "suggestion-activated",
                          G_CALLBACK (ephy_location_entry_suggestion_activated), entry);
}

static void
ephy_location_entry_init (EphyLocationEntry *le)
{
  LOG ("EphyLocationEntry initialising %p", le);

  le->user_changed = FALSE;
  le->block_update = FALSE;
  le->saved_text = NULL;
  le->dns_prefetch_handler = 0;

  ephy_location_entry_construct_contents (le);
}

GtkWidget *
ephy_location_entry_new (void)
{
  return GTK_WIDGET (g_object_new (EPHY_TYPE_LOCATION_ENTRY, NULL));
}

#if 0
/* FIXME: Refactor the DNS prefetch, this is a layering violation */
typedef struct {
  SoupURI *uri;
  EphyLocationEntry *entry;
} PrefetchHelper;

static void
free_prefetch_helper (PrefetchHelper *helper)
{
  soup_uri_free (helper->uri);
  g_object_unref (helper->entry);
  g_slice_free (PrefetchHelper, helper);
}

static gboolean
do_dns_prefetch (PrefetchHelper *helper)
{
  EphyEmbedShell *shell = ephy_embed_shell_get_default ();

  if (helper->uri)
    webkit_web_context_prefetch_dns (ephy_embed_shell_get_web_context (shell), helper->uri->host);

  helper->entry->dns_prefetch_handler = 0;

  return FALSE;
}

static void
schedule_dns_prefetch (EphyLocationEntry *entry, guint interval, const gchar *url)
{
  PrefetchHelper *helper;
  SoupURI *uri;

  uri = soup_uri_new (url);
  if (!uri || !uri->host) {
    soup_uri_free (uri);
    return;
  }

  if (entry->dns_prefetch_handler)
    g_source_remove (entry->dns_prefetch_handler);

  helper = g_slice_new0 (PrefetchHelper);
  helper->entry = g_object_ref (entry);
  helper->uri = uri;

  entry->dns_prefetch_handler =
    g_timeout_add_full (G_PRIORITY_DEFAULT, interval,
                        (GSourceFunc)do_dns_prefetch, helper,
                        (GDestroyNotify)free_prefetch_helper);
  g_source_set_name_by_id (entry->dns_prefetch_handler, "[epiphany] do_dns_prefetch");
}
#endif

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

static gboolean
ephy_location_entry_reset_internal (EphyLocationEntry *entry,
                                    gboolean           notify)
{
  const char *text, *old_text;
  char *url = NULL;
  gboolean retval;

  g_signal_emit (entry, signals[GET_LOCATION], 0, &url);
  text = url != NULL ? url : "";
  old_text = gtk_entry_get_text (GTK_ENTRY (entry->url_entry));
  old_text = old_text != NULL ? old_text : "";

  g_free (entry->saved_text);
  entry->saved_text = g_strdup (old_text);
  entry->can_redo = TRUE;

  retval = g_str_hash (text) != g_str_hash (old_text);

  ephy_title_widget_set_address (EPHY_TITLE_WIDGET (entry), text);
  g_free (url);

  if (notify) {
    g_signal_emit (entry, signals[USER_CHANGED], 0);
  }

  entry->user_changed = FALSE;

  return retval;
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
  gtk_entry_set_text (GTK_ENTRY (entry->url_entry), entry->saved_text);
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
  return ephy_location_entry_reset_internal (entry, FALSE);
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
  GtkWidget *toplevel, *widget = GTK_WIDGET (entry->url_entry);

  toplevel = gtk_widget_get_toplevel (widget);

  gtk_editable_select_region (GTK_EDITABLE (entry->url_entry),
                              0, -1);
  gtk_window_set_focus (GTK_WINDOW (toplevel),
                        widget);
}

void
ephy_location_entry_set_bookmark_icon_state (EphyLocationEntry                  *entry,
                                             EphyLocationEntryBookmarkIconState  state)
{
  GtkStyleContext *context;

  g_assert (EPHY_IS_LOCATION_ENTRY (entry));

  context = gtk_widget_get_style_context (GTK_WIDGET (entry->bookmark));

  switch (state) {
    case EPHY_LOCATION_ENTRY_BOOKMARK_ICON_HIDDEN:
      gtk_widget_set_visible (entry->bookmark_event_box, FALSE);
      gtk_style_context_remove_class (context, "starred");
      gtk_style_context_remove_class (context, "non-starred");
      break;
    case EPHY_LOCATION_ENTRY_BOOKMARK_ICON_EMPTY:
      gtk_widget_set_visible (entry->bookmark_event_box, TRUE);
      gtk_image_set_from_icon_name (GTK_IMAGE (entry->bookmark),
                                    "non-starred-symbolic",
                                    GTK_ICON_SIZE_MENU);
      gtk_style_context_remove_class (context, "starred");
      gtk_style_context_add_class (context, "non-starred");
      break;
    case EPHY_LOCATION_ENTRY_BOOKMARK_ICON_BOOKMARKED:
      gtk_widget_set_visible (entry->bookmark_event_box, TRUE);
      gtk_image_set_from_icon_name (GTK_IMAGE (entry->bookmark),
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

  entry->add_bookmark_popover = popover;
}

GtkPopover *
ephy_location_entry_get_add_bookmark_popover (EphyLocationEntry *entry)
{
  return entry->add_bookmark_popover;
}

/**
 * ephy_location_entry_get_search_terms:
 * @entry: an #EphyLocationEntry widget
 *
 * Return the internal #GSList containing the search terms as #GRegex
 * instances, formed in @entry on user changes.
 *
 * Return value: the internal #GSList
 *
 **/
GSList *
ephy_location_entry_get_search_terms (EphyLocationEntry *entry)
{
  return entry->search_terms;
}

GtkWidget *
ephy_location_entry_get_entry (EphyLocationEntry *entry)
{
  return GTK_WIDGET (entry->url_entry);
}

GtkWidget *
ephy_location_entry_get_bookmark_widget (EphyLocationEntry *entry)
{
  return entry->bookmark_event_box;
}

GtkWidget *
ephy_location_entry_get_reader_mode_widget (EphyLocationEntry *entry)
{
  return entry->reader_mode_event_box;
}

void
ephy_location_entry_set_reader_mode_visible (EphyLocationEntry *entry,
                                             gboolean           visible)
{
  gtk_widget_set_visible (entry->reader_mode_event_box, visible);
}

void
ephy_location_entry_set_reader_mode_state (EphyLocationEntry *entry,
                                           gboolean           active)
{
  if (active)
    gtk_style_context_add_class (gtk_widget_get_style_context (entry->reader_mode), "selected");
  else
    gtk_style_context_remove_class (gtk_widget_get_style_context (entry->reader_mode), "selected");

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

  if (entry->progress_timeout) {
    g_source_remove (entry->progress_timeout);
    entry->progress_timeout = 0;
  }

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
  if (entry->progress_timeout) {
    g_source_remove (entry->progress_timeout);
    entry->progress_timeout = 0;
 }

  if (!loading) {
    gtk_entry_set_progress_fraction (GTK_ENTRY (entry->url_entry), 0);
    return;
  }

  entry->progress_fraction = fraction;
  ephy_location_entry_set_fraction_internal (entry);
}
