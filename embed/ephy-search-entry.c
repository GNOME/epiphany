/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2022 Purism SPC
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
#include "ephy-search-entry.h"
#include "ephy-embed-type-builtins.h"

#include <glib/gi18n.h>

struct _EphySearchEntry {
  GtkWidget parent_instance;

  GtkWidget *text;
  GtkWidget *search_icon;
  GtkWidget *clear_icon;
  GtkWidget *matches_label;

  gboolean show_matches;
  guint n_matches;
  guint current_match;
  EphyFindResult find_result;
};

enum {
  PROP_0,
  PROP_PLACEHOLDER_TEXT,
  PROP_SHOW_MATCHES,
  PROP_N_MATCHES,
  PROP_CURRENT_MATCH,
  PROP_FIND_RESULT,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = {};

enum {
  NEXT_MATCH,
  PREVIOUS_MATCH,
  STOP_SEARCH,
  LAST_SIGNAL
};

static int signals[LAST_SIGNAL] = {};

static void ephy_search_entry_editable_init (GtkEditableInterface *iface);
static void ephy_search_entry_accessible_init (GtkAccessibleInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphySearchEntry, ephy_search_entry, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                      ephy_search_entry_editable_init)
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_ACCESSIBLE,
                                                      ephy_search_entry_accessible_init))

static void
update_matches (EphySearchEntry *self)
{
  g_autofree gchar *label = NULL;

  label = g_strdup_printf ("%u/%u", self->current_match, self->n_matches);

  gtk_label_set_label (GTK_LABEL (self->matches_label), label);
}

static void
text_changed_cb (GtkEditable     *editable,
                 EphySearchEntry *self)
{
  const char *text = gtk_editable_get_text (editable);

  gtk_widget_set_visible (self->clear_icon, text && *text);
}

static void
clear_icon_pressed_cb (GtkGesture *gesture)
{
  gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
clear_icon_released_cb (EphySearchEntry *self)
{
  gtk_editable_set_text (GTK_EDITABLE (self), "");
}

static void
activate_cb (EphySearchEntry *self)
{
  g_signal_emit (G_OBJECT (self), signals[NEXT_MATCH], 0);
}

static gboolean
ephy_search_entry_grab_focus (GtkWidget *widget)
{
  EphySearchEntry *self = EPHY_SEARCH_ENTRY (widget);

  return gtk_widget_grab_focus (self->text);
}

static void
ephy_search_entry_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  EphySearchEntry *self = EPHY_SEARCH_ENTRY (object);

  if (gtk_editable_delegate_set_property (object, prop_id, value, pspec)) {
    if (prop_id == LAST_PROP + GTK_EDITABLE_PROP_EDITABLE) {
      gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                      GTK_ACCESSIBLE_PROPERTY_READ_ONLY, !g_value_get_boolean (value),
                                      -1);
    }
    return;
  }

  switch (prop_id) {
    case PROP_PLACEHOLDER_TEXT:
      ephy_search_entry_set_placeholder_text (self, g_value_get_string (value));
      break;
    case PROP_SHOW_MATCHES:
      ephy_search_entry_set_show_matches (self, g_value_get_boolean (value));
      break;
    case PROP_N_MATCHES:
      ephy_search_entry_set_n_matches (self, g_value_get_uint (value));
      break;
    case PROP_CURRENT_MATCH:
      ephy_search_entry_set_current_match (self, g_value_get_uint (value));
      break;
    case PROP_FIND_RESULT:
      ephy_search_entry_set_find_result (self, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_search_entry_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  EphySearchEntry *self = EPHY_SEARCH_ENTRY (object);

  if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
    return;

  switch (prop_id) {
    case PROP_PLACEHOLDER_TEXT:
      g_value_set_string (value, ephy_search_entry_get_placeholder_text (self));
      break;
    case PROP_SHOW_MATCHES:
      g_value_set_boolean (value, ephy_search_entry_get_show_matches (self));
      break;
    case PROP_N_MATCHES:
      g_value_set_uint (value, ephy_search_entry_get_n_matches (self));
      break;
    case PROP_CURRENT_MATCH:
      g_value_set_uint (value, ephy_search_entry_get_current_match (self));
      break;
    case PROP_FIND_RESULT:
      g_value_set_enum (value, ephy_search_entry_get_find_result (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_search_entry_dispose (GObject *object)
{
  EphySearchEntry *self = EPHY_SEARCH_ENTRY (object);

  if (self->text)
    gtk_editable_finish_delegate (GTK_EDITABLE (self));

  g_clear_pointer (&self->search_icon, gtk_widget_unparent);
  g_clear_pointer (&self->text, gtk_widget_unparent);
  g_clear_pointer (&self->clear_icon, gtk_widget_unparent);
  g_clear_pointer (&self->matches_label, gtk_widget_unparent);

  G_OBJECT_CLASS (ephy_search_entry_parent_class)->dispose (object);
}

static void
ephy_search_entry_class_init (EphySearchEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ephy_search_entry_get_property;
  object_class->set_property = ephy_search_entry_set_property;
  object_class->dispose = ephy_search_entry_dispose;

  widget_class->grab_focus = ephy_search_entry_grab_focus;

  props[PROP_PLACEHOLDER_TEXT] =
    g_param_spec_string ("placeholder-text",
                         NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_SHOW_MATCHES] =
    g_param_spec_boolean ("show-matches",
                          NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_N_MATCHES] =
    g_param_spec_uint ("n-matches",
                       NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_CURRENT_MATCH] =
    g_param_spec_uint ("current-match",
                       NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_FIND_RESULT] =
    g_param_spec_enum ("find-result",
                       NULL, NULL,
                       EPHY_TYPE_FIND_RESULT,
                       EPHY_FIND_RESULT_FOUND,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);
  gtk_editable_install_properties (object_class, LAST_PROP);

  signals[NEXT_MATCH] =
    g_signal_new ("next-match",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[PREVIOUS_MATCH] =
    g_signal_new ("previous-match",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[STOP_SEARCH] =
    g_signal_new ("stop-search",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, "entry");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TEXT_BOX);

  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_g,
                                       GDK_CONTROL_MASK,
                                       "next-match", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_g,
                                       GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                       "previous-match", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Escape, 0,
                                       "stop-search", NULL);

  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Return,
                                       GDK_SHIFT_MASK,
                                       "previous-match", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_ISO_Enter,
                                       GDK_SHIFT_MASK,
                                       "previous-match", NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Enter,
                                       GDK_SHIFT_MASK,
                                       "previous-match", NULL);
}

static void
ephy_search_entry_init (EphySearchEntry *self)
{
  GtkGesture *gesture;

  self->find_result = EPHY_FIND_RESULT_FOUND;

  /* Icon */
  self->search_icon = g_object_new (GTK_TYPE_IMAGE,
                                    "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                                    "icon-name", "edit-find-symbolic",
                                    NULL);
  gtk_widget_set_parent (self->search_icon, GTK_WIDGET (self));

  /* Text */
  self->text = gtk_text_new ();
  gtk_widget_set_hexpand (self->text, TRUE);
  gtk_widget_set_parent (self->text, GTK_WIDGET (self));
  g_signal_connect_after (self->text, "changed", G_CALLBACK (text_changed_cb), self);
  g_signal_connect_swapped (self->text, "activate", G_CALLBACK (activate_cb), self);

  /* Clear */
  self->clear_icon = g_object_new (GTK_TYPE_IMAGE,
                                   "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                                   "icon-name", "edit-clear-symbolic",
                                   NULL);
  gtk_widget_set_visible (self->clear_icon, FALSE);
  gtk_widget_set_parent (self->clear_icon, GTK_WIDGET (self));

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "pressed", G_CALLBACK (clear_icon_pressed_cb), self);
  g_signal_connect_swapped (gesture, "released", G_CALLBACK (clear_icon_released_cb), self);
  gtk_widget_add_controller (self->clear_icon, GTK_EVENT_CONTROLLER (gesture));

  /* Matches */
  self->matches_label = gtk_label_new (NULL);
  gtk_widget_add_css_class (self->matches_label, "dim-label");
  gtk_widget_add_css_class (self->matches_label, "numeric");
  gtk_widget_set_visible (self->matches_label, FALSE);
  gtk_widget_set_parent (self->matches_label, GTK_WIDGET (self));

  gtk_editable_init_delegate (GTK_EDITABLE (self));
  gtk_widget_set_hexpand (GTK_WIDGET (self), FALSE);

  update_matches (self);
}

static GtkEditable *
ephy_search_entry_get_delegate (GtkEditable *editable)
{
  EphySearchEntry *self = EPHY_SEARCH_ENTRY (editable);

  return GTK_EDITABLE (self->text);
}

static void
ephy_search_entry_editable_init (GtkEditableInterface *iface)
{
  iface->get_delegate = ephy_search_entry_get_delegate;
}

static gboolean
ephy_search_entry_accessible_get_platform_state (GtkAccessible              *accessible,
                                                 GtkAccessiblePlatformState  state)
{
  return gtk_editable_delegate_get_accessible_platform_state (GTK_EDITABLE (accessible), state);
}

static void
ephy_search_entry_accessible_init (GtkAccessibleInterface *iface)
{
  iface->get_platform_state = ephy_search_entry_accessible_get_platform_state;
}

GtkWidget *
ephy_search_entry_new (void)
{
  return GTK_WIDGET (g_object_new (EPHY_TYPE_SEARCH_ENTRY, NULL));
}

const char *
ephy_search_entry_get_placeholder_text (EphySearchEntry *self)
{
  g_return_val_if_fail (EPHY_IS_SEARCH_ENTRY (self), NULL);

  return gtk_text_get_placeholder_text (GTK_TEXT (self->text));
}

void
ephy_search_entry_set_placeholder_text (EphySearchEntry *self,
                                        const char      *placeholder_text)
{
  g_return_if_fail (EPHY_IS_SEARCH_ENTRY (self));

  if (!g_strcmp0 (placeholder_text, ephy_search_entry_get_placeholder_text (self)))
    return;

  gtk_text_set_placeholder_text (GTK_TEXT (self->text), placeholder_text);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_PLACEHOLDER_TEXT]);
}

gboolean
ephy_search_entry_get_show_matches (EphySearchEntry *self)
{
  g_return_val_if_fail (EPHY_IS_SEARCH_ENTRY (self), 0);

  return self->show_matches;
}

void
ephy_search_entry_set_show_matches (EphySearchEntry *self,
                                    gboolean         show_matches)
{
  g_return_if_fail (EPHY_IS_SEARCH_ENTRY (self));

  if (self->show_matches == show_matches)
    return;

  self->show_matches = show_matches;

  gtk_widget_set_visible (self->matches_label, show_matches);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SHOW_MATCHES]);
}

guint
ephy_search_entry_get_n_matches (EphySearchEntry *self)
{
  g_return_val_if_fail (EPHY_IS_SEARCH_ENTRY (self), 0);

  return self->n_matches;
}

void
ephy_search_entry_set_n_matches (EphySearchEntry *self,
                                 guint            n_matches)
{
  g_return_if_fail (EPHY_IS_SEARCH_ENTRY (self));

  if (self->n_matches == n_matches)
    return;

  self->n_matches = n_matches;

  update_matches (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_N_MATCHES]);
}

guint
ephy_search_entry_get_current_match (EphySearchEntry *self)
{
  g_return_val_if_fail (EPHY_IS_SEARCH_ENTRY (self), 0);

  return self->current_match;
}

void
ephy_search_entry_set_current_match (EphySearchEntry *self,
                                     guint            current_match)
{
  g_return_if_fail (EPHY_IS_SEARCH_ENTRY (self));

  if (self->current_match == current_match)
    return;

  self->current_match = current_match;

  update_matches (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CURRENT_MATCH]);
}

EphyFindResult
ephy_search_entry_get_find_result (EphySearchEntry *self)
{
  g_return_val_if_fail (EPHY_IS_SEARCH_ENTRY (self), EPHY_FIND_RESULT_FOUND);

  return self->find_result;
}

void
ephy_search_entry_set_find_result (EphySearchEntry *self,
                                   EphyFindResult   result)
{
  const char *icon_name, *tooltip;

  g_return_if_fail (EPHY_IS_SEARCH_ENTRY (self));

  if (self->find_result == result)
    return;

  self->find_result = result;

  switch (result) {
    case EPHY_FIND_RESULT_FOUND:
      icon_name = "edit-find-symbolic";
      tooltip = NULL;
      break;
    case EPHY_FIND_RESULT_NOTFOUND:
      icon_name = "face-uncertain-symbolic";
      tooltip = _("Text not found");
      break;
    case EPHY_FIND_RESULT_FOUNDWRAPPED:
      icon_name = "view-wrapped-symbolic";
      tooltip = _("Search wrapped back to the top");
      break;
    default:
      g_assert_not_reached ();
  }

  gtk_image_set_from_icon_name (GTK_IMAGE (self->search_icon), icon_name);
  gtk_widget_set_tooltip_text (self->search_icon, tooltip);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_FIND_RESULT]);
}
