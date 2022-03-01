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
#include "contrib/gd-tagged-entry.h"

#include <math.h>

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <handy.h>
#include <string.h>
#include <webkit2/webkit2.h>

struct _EphyFindToolbar {
  GtkBin parent_instance;

  GCancellable *cancellable;
  WebKitWebView *web_view;
  WebKitFindController *controller;
  GtkWidget *search_bar;
  GdTaggedEntry *entry;
  GdTaggedEntryTag *entry_tag;
  GtkWidget *next;
  GtkWidget *prev;
  guint num_matches;
  guint current_match;
  guint find_again_source_id;
  guint find_source_id;
  char *find_string;
};

G_DEFINE_TYPE (EphyFindToolbar, ephy_find_toolbar, GTK_TYPE_BIN)

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
  EPHY_FIND_RESULT_FOUND = 0,
  EPHY_FIND_RESULT_NOTFOUND = 1,
  EPHY_FIND_RESULT_FOUNDWRAPPED = 2
} EphyFindResult;

typedef enum {
  EPHY_FIND_DIRECTION_NEXT,
  EPHY_FIND_DIRECTION_PREV
} EphyFindDirection;

/* private functions */

static void ephy_find_toolbar_set_web_view (EphyFindToolbar *toolbar,
                                            WebKitWebView   *web_view);

static void
update_search_tag (EphyFindToolbar *toolbar)
{
  g_autofree gchar *label = NULL;

  label = g_strdup_printf ("%u/%u", toolbar->current_match, toolbar->num_matches);
  gd_tagged_entry_tag_set_label (toolbar->entry_tag, label);
  gd_tagged_entry_add_tag (toolbar->entry, toolbar->entry_tag);
}

static void
set_status (EphyFindToolbar *toolbar,
            EphyFindResult   result)
{
  const char *icon_name = "edit-find-symbolic";
  const char *tooltip = NULL;

  update_search_tag (toolbar);

  switch (result) {
    case EPHY_FIND_RESULT_FOUND:
      break;
    case EPHY_FIND_RESULT_NOTFOUND:
      icon_name = "face-uncertain-symbolic";
      tooltip = _("Text not found");
      gtk_widget_error_bell (GTK_WIDGET (toolbar));

      break;
    case EPHY_FIND_RESULT_FOUNDWRAPPED:
      icon_name = "view-wrapped-symbolic";
      tooltip = _("Search wrapped back to the top");
      break;

    default:
      g_assert_not_reached ();
  }

  gtk_widget_set_sensitive (toolbar->prev, result != EPHY_FIND_RESULT_NOTFOUND);
  gtk_widget_set_sensitive (toolbar->next, result != EPHY_FIND_RESULT_NOTFOUND);

  g_object_set (toolbar->entry,
                "primary-icon-name", icon_name,
                "primary-icon-activatable", FALSE,
                "primary-icon-sensitive", FALSE,
                "primary-icon-tooltip-text", tooltip,
                NULL);
}

static void
clear_status (EphyFindToolbar *toolbar)
{
  g_object_set (toolbar->entry,
                "primary-icon-name", "edit-find-symbolic",
                NULL);

  gd_tagged_entry_remove_tag (toolbar->entry, toolbar->entry_tag);

  gtk_widget_set_sensitive (toolbar->prev, FALSE);
  gtk_widget_set_sensitive (toolbar->next, FALSE);

  if (toolbar->web_view == NULL) return;

  webkit_find_controller_search_finish (toolbar->controller);
}

static void
real_find (EphyFindToolbar   *toolbar,
           EphyFindDirection  direction)
{
  WebKitFindOptions options = WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE;

  if (!g_strcmp0 (toolbar->find_string, ""))
    return;

  if (direction == EPHY_FIND_DIRECTION_PREV)
    options |= WEBKIT_FIND_OPTIONS_BACKWARDS;

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

  update_search_tag (toolbar);

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
  toolbar->find_string = g_strdup (gtk_entry_get_text (GTK_ENTRY (toolbar->entry)));

  g_clear_handle_id (&toolbar->find_source_id, g_source_remove);

  if (strlen (toolbar->find_string) == 0) {
    clear_status (toolbar);
    return;
  }

  toolbar->find_source_id = g_timeout_add (300, (GSourceFunc)do_search, toolbar);
  g_source_set_name_by_id (toolbar->find_source_id, "[epiphany] do_search");
}

static gboolean
entry_key_press_event_cb (GtkEntry        *entry,
                          GdkEventKey     *event,
                          EphyFindToolbar *toolbar)
{
  guint mask = gtk_accelerator_get_default_mod_mask ();
  gboolean handled = FALSE;

  if ((event->state & mask) == 0) {
    handled = TRUE;
    switch (event->keyval) {
      case GDK_KEY_Escape:
        /* Hide the toolbar when ESC is pressed */
        ephy_find_toolbar_request_close (toolbar);
        break;
      default:
        handled = FALSE;
        break;
    }
  } else if ((event->state & mask) == GDK_CONTROL_MASK &&
             (gdk_keyval_to_lower (event->keyval) == GDK_KEY_g)) {
    handled = TRUE;
    ephy_find_toolbar_find_next (toolbar);
  } else if ((event->state & mask) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK) &&
             (gdk_keyval_to_lower (event->keyval) == GDK_KEY_g)) {
    handled = TRUE;
    ephy_find_toolbar_find_previous (toolbar);
  } else if ((event->state & mask) == GDK_SHIFT_MASK &&
             (event->keyval == GDK_KEY_Return ||
              event->keyval == GDK_KEY_KP_Enter ||
              event->keyval == GDK_KEY_ISO_Enter)) {
    handled = TRUE;
    ephy_find_toolbar_find_previous (toolbar);
  }

  return handled;
}

static void
ephy_find_toolbar_grab_focus (GtkWidget *widget)
{
  EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (widget);

  gtk_widget_grab_focus (GTK_WIDGET (toolbar->entry));
}

static gboolean
ephy_find_toolbar_draw (GtkWidget *widget,
                        cairo_t   *cr)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, gtk_widget_get_state_flags (widget));

  gtk_render_background (context, cr, 0, 0,
                         gtk_widget_get_allocated_width (widget),
                         gtk_widget_get_allocated_height (widget));

  gtk_render_frame (context, cr, 0, 0,
                    gtk_widget_get_allocated_width (widget),
                    gtk_widget_get_allocated_height (widget));

  gtk_style_context_restore (context);

  return GTK_WIDGET_CLASS (ephy_find_toolbar_parent_class)->draw (widget, cr);
}

static void
search_entry_clear_cb (GtkEntry *entry,
                       gpointer  user_data)
{
  gtk_entry_set_text (entry, "");
}

static void
search_entry_changed_cb (GtkEntry        *entry,
                         EphyFindToolbar *toolbar)
{
  const char *str;
  const char *primary_icon_name = "edit-find-symbolic";
  const char *secondary_icon_name = NULL;
  gboolean primary_active = FALSE;
  gboolean secondary_active = FALSE;

  str = gtk_entry_get_text (entry);

  if (str == NULL || *str == '\0') {
    primary_icon_name = "edit-find-symbolic";
  } else {
    secondary_icon_name = "edit-clear-symbolic";
    secondary_active = TRUE;
  }

  g_object_set (entry,
                "primary-icon-name", primary_icon_name,
                "primary-icon-activatable", primary_active,
                "primary-icon-sensitive", primary_active,
                "secondary-icon-name", secondary_icon_name,
                "secondary-icon-activatable", secondary_active,
                "secondary-icon-sensitive", secondary_active,
                NULL);

  update_find_string (toolbar);
}

static void
ephy_find_toolbar_load_changed_cb (WebKitWebView   *web_view,
                                   WebKitLoadEvent  load_event,
                                   EphyFindToolbar *toolbar)
{
  if (load_event == WEBKIT_LOAD_STARTED &&
      hdy_search_bar_get_search_mode (HDY_SEARCH_BAR (toolbar->search_bar))) {
    ephy_find_toolbar_close (toolbar);
  }
}

static void
ephy_find_toolbar_init (EphyFindToolbar *toolbar)
{
  GtkWidget *clamp;
  GtkWidget *box;

  toolbar->search_bar = hdy_search_bar_new ();
  gtk_container_add (GTK_CONTAINER (toolbar), toolbar->search_bar);

  clamp = GTK_WIDGET (hdy_clamp_new ());
  hdy_clamp_set_maximum_size (HDY_CLAMP (clamp), 400);
  hdy_clamp_set_tightening_threshold (HDY_CLAMP (clamp), 300);
  gtk_container_add (GTK_CONTAINER (toolbar->search_bar), clamp);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (box), "linked");
  gtk_container_add (GTK_CONTAINER (clamp), box);

  toolbar->entry = gd_tagged_entry_new ();
  toolbar->entry_tag = gd_tagged_entry_tag_new ("");
  gd_tagged_entry_tag_set_style (toolbar->entry_tag, "search-entry-occurrences-tag");
  gd_tagged_entry_tag_set_has_close_button (toolbar->entry_tag, FALSE);

  gtk_widget_set_hexpand (GTK_WIDGET (toolbar->entry), TRUE);
  gtk_entry_set_placeholder_text (GTK_ENTRY (toolbar->entry), _("Type to search…"));
  gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (toolbar->entry));

  /* Prev */
  toolbar->prev = gtk_button_new_from_icon_name ("go-up-symbolic", GTK_ICON_SIZE_MENU);
  gtk_widget_set_tooltip_text (toolbar->prev,
                               _("Find previous occurrence of the search string"));
  gtk_container_add (GTK_CONTAINER (box), toolbar->prev);
  gtk_widget_set_sensitive (toolbar->prev, FALSE);

  /* Next */
  toolbar->next = gtk_button_new_from_icon_name ("go-down-symbolic", GTK_ICON_SIZE_MENU);
  gtk_widget_set_tooltip_text (toolbar->next,
                               _("Find next occurrence of the search string"));
  gtk_container_add (GTK_CONTAINER (box), toolbar->next);
  gtk_widget_set_sensitive (toolbar->next, FALSE);

  /* connect signals */
  g_signal_connect (toolbar->entry, "icon-release",
                    G_CALLBACK (search_entry_clear_cb), toolbar);
  g_signal_connect (toolbar->entry, "key-press-event",
                    G_CALLBACK (entry_key_press_event_cb), toolbar);
  g_signal_connect_after (toolbar->entry, "changed",
                          G_CALLBACK (search_entry_changed_cb), toolbar);
  g_signal_connect_swapped (toolbar->entry, "activate",
                            G_CALLBACK (ephy_find_toolbar_find_next), toolbar);
  g_signal_connect_swapped (toolbar->next, "clicked",
                            G_CALLBACK (ephy_find_toolbar_find_next), toolbar);
  g_signal_connect_swapped (toolbar->prev, "clicked",
                            G_CALLBACK (ephy_find_toolbar_find_previous), toolbar);
  hdy_search_bar_connect_entry (HDY_SEARCH_BAR (toolbar->search_bar),
                                GTK_ENTRY (toolbar->entry));

  search_entry_changed_cb (GTK_ENTRY (toolbar->entry), toolbar);

  toolbar->cancellable = g_cancellable_new ();

  gtk_widget_show_all (GTK_WIDGET (toolbar));
}

static void
ephy_find_toolbar_dispose (GObject *object)
{
  EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (object);

  if (toolbar->find_again_source_id != 0) {
    g_source_remove (toolbar->find_again_source_id);
    toolbar->find_again_source_id = 0;
  }

  if (toolbar->find_source_id != 0) {
    g_source_remove (toolbar->find_source_id);
    toolbar->find_source_id = 0;
  }

  if (toolbar->cancellable) {
    g_cancellable_cancel (toolbar->cancellable);
    g_clear_object (&toolbar->cancellable);
  }

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
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ephy_find_toolbar_dispose;
  object_class->finalize = ephy_find_toolbar_finalize;
  object_class->get_property = ephy_find_toolbar_get_property;
  object_class->set_property = ephy_find_toolbar_set_property;

  widget_class->draw = ephy_find_toolbar_draw;
  widget_class->grab_focus = ephy_find_toolbar_grab_focus;

  signals[CLOSE] =
    g_signal_new ("close",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  obj_properties[PROP_WEB_VIEW] =
    g_param_spec_object ("web-view",
                         "WebView",
                         "Parent web view",
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
  return gtk_entry_get_text (GTK_ENTRY (toolbar->entry));
}

static void
counted_matches_cb (WebKitFindController *find_controller,
                    guint                 match_count,
                    gpointer              user_data)
{
  EphyFindToolbar *toolbar = EPHY_FIND_TOOLBAR (user_data);

  toolbar->num_matches = match_count;
  toolbar->current_match = toolbar->num_matches ? 1 : 0;

  update_search_tag (toolbar);
}

static void
ephy_find_toolbar_set_web_view (EphyFindToolbar *toolbar,
                                WebKitWebView   *web_view)
{
  if (toolbar->web_view == web_view) return;

  if (toolbar->web_view != NULL) {
    g_signal_handlers_disconnect_matched (toolbar->controller,
                                          G_SIGNAL_MATCH_DATA,
                                          0, 0, NULL, NULL, toolbar);
  }

  toolbar->web_view = web_view;
  if (web_view != NULL) {
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
    g_signal_connect (web_view, "load-changed",
                      G_CALLBACK (ephy_find_toolbar_load_changed_cb),
                      toolbar);

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
  g_autoptr (WebKitJavascriptResult) js_result = NULL;
  JSCValue *value = NULL;

  js_result = webkit_web_view_run_javascript_finish (web_view, res, &error);
  if (!js_result) {
    g_warning ("Error running javascript: %s", error->message);
    return;
  }

  value = webkit_javascript_result_get_js_value (js_result);
  if (jsc_value_is_string (value)) {
    JSCException *exception;
    g_autofree gchar *str_value = NULL;

    str_value = jsc_value_to_string (value);
    exception = jsc_context_get_exception (jsc_value_get_context (value));
    if (exception) {
      g_warning ("Error running javascript: %s", jsc_exception_get_message (exception));
    } else if (strlen (str_value)) {
      gtk_entry_set_text (GTK_ENTRY (toolbar->entry), str_value);
      gtk_editable_select_region (GTK_EDITABLE (toolbar->entry), 0, -1);
    }
  }
}

void
ephy_find_toolbar_open (EphyFindToolbar *toolbar)
{
  g_assert (toolbar->web_view != NULL);

  webkit_web_view_run_javascript (toolbar->web_view, "window.getSelection().toString();", toolbar->cancellable, ephy_find_toolbar_selection_async, toolbar);

  gtk_editable_select_region (GTK_EDITABLE (toolbar->entry), 0, -1);

  hdy_search_bar_set_search_mode (HDY_SEARCH_BAR (toolbar->search_bar), TRUE);
  hdy_search_bar_set_show_close_button (HDY_SEARCH_BAR (toolbar->search_bar), TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (toolbar->entry));
}

void
ephy_find_toolbar_close (EphyFindToolbar *toolbar)
{
  hdy_search_bar_set_search_mode (HDY_SEARCH_BAR (toolbar->search_bar), FALSE);

  if (toolbar->web_view == NULL) return;

  webkit_find_controller_search_finish (toolbar->controller);
}

void
ephy_find_toolbar_request_close (EphyFindToolbar *toolbar)
{
  if (hdy_search_bar_get_search_mode (HDY_SEARCH_BAR (toolbar->search_bar))) {
    g_signal_emit (toolbar, signals[CLOSE], 0);
  }
}
