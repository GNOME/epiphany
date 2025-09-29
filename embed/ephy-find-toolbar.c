/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2004 Tommi Komulainen
 *  Copyright © 2004, 2005 Christian Persch
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
#include "ephy-find-toolbar.h"

#include "ephy-debug.h"
#include "ephy-search-entry.h"

#include <math.h>

#include <adwaita.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <string.h>
#include <webkit/webkit.h>

struct _EphyFindToolbar {
  AdwBin parent_instance;

  GCancellable *cancellable;
  WebKitWebView *web_view;
  WebKitFindController *controller;
  GtkWidget *search_bar;
  GtkWidget *entry;
  GtkWidget *options_button;
  GtkWidget *next;
  GtkWidget *prev;
  guint num_matches;
  guint current_match;
  guint find_again_source_id;
  guint find_source_id;
  char *find_string;
  gboolean case_sensitive;
  gboolean word_action;
};

G_DEFINE_FINAL_TYPE (EphyFindToolbar, ephy_find_toolbar, ADW_TYPE_BIN)

enum {
  PROP_0,
  PROP_WEB_VIEW,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

enum {
  CLOSE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef enum {
  EPHY_FIND_DIRECTION_NEXT,
  EPHY_FIND_DIRECTION_PREV
} EphyFindDirection;

/* private functions */

static void ephy_find_toolbar_set_web_view (EphyFindToolbar *toolbar,
                                            WebKitWebView   *web_view);

static void
update_entry_matches (EphyFindToolbar *toolbar)
{
  ephy_search_entry_set_show_matches (EPHY_SEARCH_ENTRY (toolbar->entry), TRUE);
  ephy_search_entry_set_n_matches (EPHY_SEARCH_ENTRY (toolbar->entry),
                                   toolbar->num_matches);
  ephy_search_entry_set_current_match (EPHY_SEARCH_ENTRY (toolbar->entry),
                                       toolbar->current_match);
}

static void
set_status (EphyFindToolbar *toolbar,
            EphyFindResult   result)
{
  update_entry_matches (toolbar);

  if (result == EPHY_FIND_RESULT_NOTFOUND)
    gtk_widget_error_bell (GTK_WIDGET (toolbar));

  gtk_widget_set_sensitive (toolbar->prev, result != EPHY_FIND_RESULT_NOTFOUND);
  gtk_widget_set_sensitive (toolbar->next, result != EPHY_FIND_RESULT_NOTFOUND);

  ephy_search_entry_set_find_result (EPHY_SEARCH_ENTRY (toolbar->entry), result);
}

static void
clear_status (EphyFindToolbar *toolbar)
{
  ephy_search_entry_set_find_result (EPHY_SEARCH_ENTRY (toolbar->entry),
                                     EPHY_FIND_RESULT_FOUND);
  ephy_search_entry_set_show_matches (EPHY_SEARCH_ENTRY (toolbar->entry), FALSE);

  gtk_widget_set_sensitive (toolbar->prev, FALSE);
  gtk_widget_set_sensitive (toolbar->next, FALSE);

  if (!toolbar->web_view)
    return;

  webkit_find_controller_search_finish (toolbar->controller);
}

static void
real_find (EphyFindToolbar   *toolbar,
           EphyFindDirection  direction)
{
  WebKitFindOptions options = WEBKIT_FIND_OPTIONS_NONE;

  if (!g_strcmp0 (toolbar->find_string, ""))
    return;

  if (direction == EPHY_FIND_DIRECTION_PREV)
    options |= WEBKIT_FIND_OPTIONS_BACKWARDS;

  if (!toolbar->case_sensitive)
    options |= WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE;

  if (toolbar->word_action)
    options |= WEBKIT_FIND_OPTIONS_AT_WORD_STARTS;

  webkit_find_controller_count_matches (toolbar->controller, toolbar->find_string, options, G_MAXUINT);
  webkit_find_controller_search (toolbar->controller, toolbar->find_string, options, G_MAXUINT);
}

static gboolean
do_search (EphyFindToolbar *toolbar)
{
  toolbar->find_source_id = 0;

  real_find (toolbar, EPHY_FIND_DIRECTION_NEXT);

  return FALSE;
}

static void
found_text_cb (WebKitFindController *controller,
               guint                 n_matches,
               EphyFindToolbar      *toolbar)
{
  WebKitFindOptions options;
  EphyFindResult result;

  update_entry_matches (toolbar);

  options = webkit_find_controller_get_options (controller);
  /* FIXME: it's not possible to remove the wrap flag, so the status is now always wrapped. */
  result = (options & WEBKIT_FIND_OPTIONS_WRAP_AROUND) ? EPHY_FIND_RESULT_FOUNDWRAPPED : EPHY_FIND_RESULT_FOUND;
  set_status (toolbar, result);
}

static void
failed_to_find_text_cb (WebKitFindController *controller,
                        EphyFindToolbar      *toolbar)
{
  WebKitFindOptions options;

  options = webkit_find_controller_get_options (controller);
  if (options & WEBKIT_FIND_OPTIONS_WRAP_AROUND) {
    set_status (toolbar, EPHY_FIND_RESULT_NOTFOUND);
    return;
  }

  options |= WEBKIT_FIND_OPTIONS_WRAP_AROUND;
  webkit_find_controller_search (controller, toolbar->find_string, options, G_MAXUINT);
}

static void
update_find_string (EphyFindToolbar *toolbar)
{
  g_free (toolbar->find_string);
  toolbar->find_string = g_strdup (gtk_editable_get_text (GTK_EDITABLE (toolbar->entry)));

  g_clear_handle_id (&toolbar->find_source_id, g_source_remove);

  if (strlen (toolbar->find_string) == 0) {
    clear_status (toolbar);
    return;
  }

  toolbar->find_source_id = g_timeout_add (300, (GSourceFunc)do_search, toolbar);
  g_source_set_name_by_id (toolbar->find_source_id, "[epiphany] do_search");
}

static void
search_entry_changed_cb (GtkEditable     *entry,
                         EphyFindToolbar *toolbar)
{
  ephy_search_entry_set_find_result (EPHY_SEARCH_ENTRY (toolbar->entry),
                                     EPHY_FIND_RESULT_FOUND);
  update_find_string (toolbar);
}

static gboolean
key_pressed_cb (EphyFindToolbar       *toolbar,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                GtkEventControllerKey *key_controller)
{
  if (keyval == GDK_KEY_Escape) {
    ephy_find_toolbar_close (toolbar);
    return TRUE;
  }

  return FALSE;
}

static void
ephy_find_toolbar_load_changed_cb (WebKitWebView   *web_view,
                                   WebKitLoadEvent  load_event,
                                   EphyFindToolbar *toolbar)
{
  const char *address = ephy_web_view_get_display_address (EPHY_WEB_VIEW (web_view));
  gboolean is_blank = ephy_web_view_get_is_blank (EPHY_WEB_VIEW (web_view));
  gboolean is_internal_page = g_str_has_prefix (address, "about:") || g_str_has_prefix (address, "ephy-about:");

  if (load_event == WEBKIT_LOAD_STARTED && (is_blank || is_internal_page))
    ephy_find_toolbar_close (toolbar);
}

static void
on_case_senstive (GSimpleAction   *action,
                  GVariant        *value,
                  EphyFindToolbar *toolbar)
{
  toolbar->case_sensitive = g_variant_get_boolean (value);
  g_simple_action_set_state (action, value);

  update_find_string (toolbar);
}

static void
on_word_action (GSimpleAction   *action,
                GVariant        *value,
                EphyFindToolbar *toolbar)
{
  toolbar->word_action = g_variant_get_boolean (value);
  g_simple_action_set_state (action, value);

  update_find_string (toolbar);
}

static void
ephy_find_toolbar_init (EphyFindToolbar *toolbar)
{
  GtkWidget *clamp;
  GtkWidget *box;
  GtkWidget *settings_popover;
  GtkEventController *event_controller;
  GMenu *model;
  g_autoptr (GSimpleActionGroup) group = NULL;
  g_autoptr (GSimpleAction) case_action = NULL;
  g_autoptr (GSimpleAction) word_action = NULL;

  toolbar->search_bar = gtk_search_bar_new ();
  adw_bin_set_child (ADW_BIN (toolbar), toolbar->search_bar);

  clamp = GTK_WIDGET (adw_clamp_new ());
  gtk_widget_set_hexpand (clamp, TRUE);
  adw_clamp_set_maximum_size (ADW_CLAMP (clamp), 400);
  adw_clamp_set_tightening_threshold (ADW_CLAMP (clamp), 300);
  gtk_search_bar_set_child (GTK_SEARCH_BAR (toolbar->search_bar), clamp);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  adw_clamp_set_child (ADW_CLAMP (clamp), box);

  toolbar->entry = ephy_search_entry_new ();
  gtk_widget_set_hexpand (GTK_WIDGET (toolbar->entry), TRUE);
  ephy_search_entry_set_placeholder_text (EPHY_SEARCH_ENTRY (toolbar->entry), _("Type to search…"));
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (toolbar->entry));

  /* Prev */
  toolbar->prev = gtk_button_new_from_icon_name ("go-up-symbolic");
  gtk_widget_set_tooltip_text (toolbar->prev,
                               _("Find previous occurrence of the search string"));
  gtk_box_append (GTK_BOX (box), toolbar->prev);
  gtk_widget_set_sensitive (toolbar->prev, FALSE);

  /* Next */
  toolbar->next = gtk_button_new_from_icon_name ("go-down-symbolic");
  gtk_widget_set_tooltip_text (toolbar->next,
                               _("Find next occurrence of the search string"));
  gtk_box_append (GTK_BOX (box), toolbar->next);
  gtk_widget_set_sensitive (toolbar->next, FALSE);

  /* Options */
  toolbar->options_button = gtk_menu_button_new ();
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (toolbar->options_button), "view-more-symbolic");

  group = g_simple_action_group_new ();
  case_action = g_simple_action_new_stateful ("case-sensitive", NULL, g_variant_new_boolean (FALSE));
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (case_action));
  g_signal_connect (case_action, "change-state", G_CALLBACK (on_case_senstive), toolbar);

  word_action = g_simple_action_new_stateful ("match-whole-word", NULL, g_variant_new_boolean (FALSE));
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (word_action));
  g_signal_connect (word_action, "change-state", G_CALLBACK (on_word_action), toolbar);

  gtk_widget_insert_action_group (GTK_WIDGET (toolbar), "search-options", G_ACTION_GROUP (group));

  model = g_menu_new ();
  g_menu_append_item (model, g_menu_item_new (_("_Case Sensitive"), "search-options.case-sensitive"));
  g_menu_append_item (model, g_menu_item_new (_("Match Whole _Word Only"), "search-options.match-whole-word"));
  settings_popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (model));
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (toolbar->options_button), settings_popover);
  gtk_widget_set_tooltip_text (toolbar->options_button, _("Search Options"));
  gtk_box_append (GTK_BOX (box), toolbar->options_button);

  /* connect signals */
  g_signal_connect_after (toolbar->entry, "changed",
                          G_CALLBACK (search_entry_changed_cb), toolbar);
  g_signal_connect_swapped (toolbar->entry, "next-match",
                            G_CALLBACK (ephy_find_toolbar_find_next), toolbar);
  g_signal_connect_swapped (toolbar->entry, "previous-match",
                            G_CALLBACK (ephy_find_toolbar_find_previous), toolbar);
  g_signal_connect_swapped (toolbar->entry, "stop-search",
                            G_CALLBACK (ephy_find_toolbar_request_close), toolbar);

  g_signal_connect_swapped (toolbar->next, "clicked",
                            G_CALLBACK (ephy_find_toolbar_find_next), toolbar);
  g_signal_connect_swapped (toolbar->prev, "clicked",
                            G_CALLBACK (ephy_find_toolbar_find_previous), toolbar);
  gtk_search_bar_connect_entry (GTK_SEARCH_BAR (toolbar->search_bar),
                                GTK_EDITABLE (toolbar->entry));

  /* Event controller for Escape key */
  event_controller = gtk_event_controller_key_new ();
  gtk_widget_add_controller (GTK_WIDGET (toolbar), event_controller);
  g_signal_connect_swapped (event_controller, "key-pressed",
                            G_CALLBACK (key_pressed_cb), toolbar);

  search_entry_changed_cb (GTK_EDITABLE (toolbar->entry), toolbar);

  toolbar->cancellable = g_cancellable_new ();
}

static void
ephy_find_toolbar_dispose (GObject *object)
{
  EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (object);

  g_clear_handle_id (&toolbar->find_again_source_id, g_source_remove);
  g_clear_handle_id (&toolbar->find_source_id, g_source_remove);

  g_cancellable_cancel (toolbar->cancellable);
  g_clear_object (&toolbar->cancellable);

  G_OBJECT_CLASS (ephy_find_toolbar_parent_class)->dispose (object);
}

#ifndef G_DISABLE_ASSERT
G_GNUC_NORETURN
#endif
static void
ephy_find_toolbar_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  /* no readable properties */
  g_assert_not_reached ();
}

static void
ephy_find_toolbar_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (object);

  switch (prop_id) {
    case PROP_WEB_VIEW:
      ephy_find_toolbar_set_web_view (toolbar, (WebKitWebView *)g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_find_toolbar_finalize (GObject *o)
{
  EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (o);

  g_free (toolbar->find_string);

  G_OBJECT_CLASS (ephy_find_toolbar_parent_class)->finalize (o);
}

static void
ephy_find_toolbar_class_init (EphyFindToolbarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ephy_find_toolbar_dispose;
  object_class->finalize = ephy_find_toolbar_finalize;
  object_class->get_property = ephy_find_toolbar_get_property;
  object_class->set_property = ephy_find_toolbar_set_property;

  signals[CLOSE] =
    g_signal_new ("close",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  obj_properties[PROP_WEB_VIEW] =
    g_param_spec_object ("web-view",
                         NULL, NULL,
                         WEBKIT_TYPE_WEB_VIEW,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);
}

/* public functions */

EphyFindToolbar *
ephy_find_toolbar_new (WebKitWebView *web_view)
{
  return g_object_new (EPHY_TYPE_FIND_TOOLBAR,
                       "web-view", web_view,
                       NULL);
}

const char *
ephy_find_toolbar_get_text (EphyFindToolbar *toolbar)
{
  return gtk_editable_get_text (GTK_EDITABLE (toolbar->entry));
}

static void
counted_matches_cb (WebKitFindController *find_controller,
                    guint                 match_count,
                    gpointer              user_data)
{
  EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (user_data);

  toolbar->num_matches = match_count;
  toolbar->current_match = toolbar->num_matches ? 1 : 0;

  update_entry_matches (toolbar);
}

static void
ephy_find_toolbar_set_web_view (EphyFindToolbar *toolbar,
                                WebKitWebView   *web_view)
{
  if (toolbar->web_view == web_view) return;

  if (toolbar->web_view) {
    g_signal_handlers_disconnect_matched (toolbar->controller,
                                          G_SIGNAL_MATCH_DATA,
                                          0, 0, NULL, NULL, toolbar);
  }

  toolbar->web_view = web_view;
  if (web_view) {
    toolbar->controller = webkit_web_view_get_find_controller (web_view);
    g_signal_connect_object (toolbar->controller, "found-text",
                             G_CALLBACK (found_text_cb),
                             toolbar, 0);
    g_signal_connect_object (toolbar->controller, "failed-to-find-text",
                             G_CALLBACK (failed_to_find_text_cb),
                             toolbar, 0);
    g_signal_connect_object (toolbar->controller, "counted_matches",
                             G_CALLBACK (counted_matches_cb),
                             toolbar, 0);
    g_signal_connect_object (web_view, "load-changed",
                             G_CALLBACK (ephy_find_toolbar_load_changed_cb),
                             toolbar, 0);

    clear_status (toolbar);
  }
}

void
ephy_find_toolbar_find_next (EphyFindToolbar *toolbar)
{
  if (toolbar->num_matches) {
    toolbar->current_match++;
    if (toolbar->current_match > toolbar->num_matches)
      toolbar->current_match = 1;
  }

  webkit_find_controller_search_next (toolbar->controller);
}

void
ephy_find_toolbar_find_previous (EphyFindToolbar *toolbar)
{
  if (toolbar->num_matches) {
    g_assert (toolbar->current_match > 0);
    toolbar->current_match--;
    if (toolbar->current_match < 1)
      toolbar->current_match = toolbar->num_matches;
  }

  webkit_find_controller_search_previous (toolbar->controller);
}

static void
ephy_find_toolbar_selection_async (GObject      *source_object,
                                   GAsyncResult *res,
                                   gpointer      user_data)
{
  WebKitWebView *web_view = WEBKIT_WEB_VIEW (source_object);
  EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (user_data);
  g_autoptr (GError) error = NULL;
  g_autoptr (JSCValue) value = NULL;

  value = webkit_web_view_evaluate_javascript_finish (web_view, res, &error);
  if (!value) {
    g_warning ("Error running javascript: %s", error->message);
    return;
  }

  if (jsc_value_is_string (value)) {
    JSCException *exception;
    g_autofree gchar *str_value = NULL;

    str_value = jsc_value_to_string (value);
    exception = jsc_context_get_exception (jsc_value_get_context (value));
    if (exception) {
      g_warning ("Error running javascript: %s", jsc_exception_get_message (exception));
    } else if (strlen (str_value)) {
      gtk_editable_set_text (GTK_EDITABLE (toolbar->entry), str_value);
      gtk_editable_select_region (GTK_EDITABLE (toolbar->entry), 0, -1);
    }
  }
}

void
ephy_find_toolbar_open (EphyFindToolbar *toolbar)
{
  g_assert (toolbar->web_view);

  webkit_web_view_evaluate_javascript (toolbar->web_view, "window.getSelection().toString();", -1, NULL, NULL, toolbar->cancellable, ephy_find_toolbar_selection_async, toolbar);

  gtk_editable_select_region (GTK_EDITABLE (toolbar->entry), 0, -1);

  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (toolbar->search_bar), TRUE);
  gtk_search_bar_set_show_close_button (GTK_SEARCH_BAR (toolbar->search_bar), TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (toolbar->entry));
}

void
ephy_find_toolbar_close (EphyFindToolbar *toolbar)
{
  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (toolbar->search_bar), FALSE);

  if (!toolbar->web_view) return;

  webkit_find_controller_search_finish (toolbar->controller);
}

void
ephy_find_toolbar_request_close (EphyFindToolbar *toolbar)
{
  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (toolbar->search_bar))) {
    g_signal_emit (toolbar, signals[CLOSE], 0);
  }
}
