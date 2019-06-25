/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019 Purism SPC
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
#include "ephy-data-dialog.h"

#include <ctype.h>
#include <glib/gi18n.h>
#define HANDY_USE_UNSTABLE_API
#include <handy.h>

typedef struct {
  GtkWidget *box;
  GtkWidget *child;
  GtkWidget *clear_all_button;
  GtkWidget *search_bar;
  GtkWidget *search_entry;
  GtkWidget *search_button;
  GtkWidget *stack;
  GtkWidget *empty_title_label;
  GtkWidget *empty_description_label;

  gboolean has_data : 1;
  gboolean has_search_results : 1;
  gboolean can_clear : 1;
  char *search_text;
} EphyDataDialogPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (EphyDataDialog, ephy_data_dialog, GTK_TYPE_WINDOW)

enum {
  PROP_0,
  PROP_CLEAR_ALL_ACTION_NAME,
  PROP_CLEAR_ALL_ACTION_TARGET,
  PROP_CLEAR_ALL_DESCRIPTION,
  PROP_SEARCH_DESCRIPTION,
  PROP_EMPTY_TITLE,
  PROP_EMPTY_DESCRIPTION,
  PROP_SEARCH_TEXT,
  PROP_HAS_DATA,
  PROP_HAS_SEARCH_RESULTS,
  PROP_CAN_CLEAR,
  LAST_PROP,
};

static GParamSpec *obj_properties[LAST_PROP];

enum {
  CLEAR_ALL_CLICKED,
  LAST_SIGNAL,
};

static gint signals[LAST_SIGNAL] = { 0 };

static void
update (EphyDataDialog *self)
{
  EphyDataDialogPrivate *priv = ephy_data_dialog_get_instance_private (self);
  gboolean has_data = priv->has_data && priv->child && gtk_widget_get_visible (priv->child);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->search_button))) {
    if (has_data && priv->has_search_results)
      gtk_stack_set_visible_child (GTK_STACK (priv->stack), priv->child);
    else
      gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "no-results");
  } else {
    if (has_data)
      gtk_stack_set_visible_child (GTK_STACK (priv->stack), priv->child);
    else
      gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "empty");
  }

  gtk_widget_set_sensitive (priv->clear_all_button, has_data && priv->can_clear);
  gtk_widget_set_sensitive (priv->search_button, has_data);
}

static void
on_clear_all_button_clicked (EphyDataDialog *self)
{
  g_signal_emit (self, signals[CLEAR_ALL_CLICKED], 0);
}

static void
on_search_entry_changed (GtkSearchEntry *entry,
                         EphyDataDialog *self)
{
  EphyDataDialogPrivate *priv = ephy_data_dialog_get_instance_private (self);
  const char *text;

  text = gtk_entry_get_text (GTK_ENTRY (entry));
  g_free (priv->search_text);
  priv->search_text = g_strdup (text);

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_SEARCH_TEXT]);
}

static gboolean
on_key_press_event (EphyDataDialog *self,
                    GdkEvent       *event,
                    gpointer        user_data)
{
  EphyDataDialogPrivate *priv = ephy_data_dialog_get_instance_private (self);
  GdkEventKey *key = (GdkEventKey *)event;
  gint result;

  result = hdy_search_bar_handle_event (HDY_SEARCH_BAR (priv->search_bar), event);

  if (result == GDK_EVENT_STOP)
    return result;

  if (key->keyval == GDK_KEY_Escape) {
    if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->search_button)))
      gtk_widget_destroy (GTK_WIDGET (self));
    else
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->search_button), FALSE);
  } else if (isprint (key->keyval))
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->search_button), TRUE);

  return result;
}

static void
ephy_data_dialog_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  EphyDataDialog *self = EPHY_DATA_DIALOG (object);
  EphyDataDialogPrivate *priv = ephy_data_dialog_get_instance_private (self);

  switch (prop_id) {
    case PROP_CLEAR_ALL_ACTION_NAME:
      gtk_actionable_set_action_name (GTK_ACTIONABLE (priv->clear_all_button), g_value_get_string (value));
      break;
    case PROP_CLEAR_ALL_ACTION_TARGET:
      gtk_actionable_set_action_target_value (GTK_ACTIONABLE (priv->clear_all_button), g_value_get_variant (value));
      break;
    case PROP_CLEAR_ALL_DESCRIPTION:
      ephy_data_dialog_set_clear_all_description (self, g_value_get_string (value));
      break;
    case PROP_SEARCH_DESCRIPTION:
      gtk_entry_set_placeholder_text (GTK_ENTRY (priv->search_entry), g_value_get_string (value));
      atk_object_set_description (gtk_widget_get_accessible (GTK_WIDGET (self)), g_value_get_string (value));
      break;
    case PROP_EMPTY_TITLE:
      gtk_label_set_text (GTK_LABEL (priv->empty_title_label), g_value_get_string (value));
      break;
    case PROP_EMPTY_DESCRIPTION:
      gtk_label_set_text (GTK_LABEL (priv->empty_description_label), g_value_get_string (value));
      break;
    case PROP_HAS_DATA:
      ephy_data_dialog_set_has_data (self, g_value_get_boolean (value));
      break;
    case PROP_HAS_SEARCH_RESULTS:
      ephy_data_dialog_set_has_search_results (self, g_value_get_boolean (value));
      break;
    case PROP_CAN_CLEAR:
      ephy_data_dialog_set_can_clear (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_data_dialog_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  EphyDataDialog *self = EPHY_DATA_DIALOG (object);
  EphyDataDialogPrivate *priv = ephy_data_dialog_get_instance_private (self);

  switch (prop_id) {
    case PROP_CLEAR_ALL_ACTION_NAME:
      g_value_set_string (value, gtk_actionable_get_action_name (GTK_ACTIONABLE (priv->clear_all_button)));
      break;
    case PROP_CLEAR_ALL_ACTION_TARGET:
      g_value_set_variant (value, gtk_actionable_get_action_target_value (GTK_ACTIONABLE (priv->clear_all_button)));
      break;
    case PROP_CLEAR_ALL_DESCRIPTION:
      g_value_set_string (value, ephy_data_dialog_get_clear_all_description (self));
      break;
    case PROP_SEARCH_DESCRIPTION:
      g_value_set_string (value, gtk_entry_get_placeholder_text (GTK_ENTRY (priv->search_entry)));
      break;
    case PROP_EMPTY_TITLE:
      g_value_set_string (value, gtk_label_get_text (GTK_LABEL (priv->empty_title_label)));
      break;
    case PROP_EMPTY_DESCRIPTION:
      g_value_set_string (value, gtk_label_get_text (GTK_LABEL (priv->empty_description_label)));
      break;
    case PROP_SEARCH_TEXT:
      g_value_set_string (value, ephy_data_dialog_get_search_text (self));
      break;
    case PROP_HAS_DATA:
      g_value_set_boolean (value, ephy_data_dialog_get_has_data (self));
      break;
    case PROP_HAS_SEARCH_RESULTS:
      g_value_set_boolean (value, ephy_data_dialog_get_has_search_results (self));
      break;
    case PROP_CAN_CLEAR:
      g_value_set_boolean (value, ephy_data_dialog_get_can_clear (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_data_dialog_finalize (GObject *object)
{
  EphyDataDialog *self = EPHY_DATA_DIALOG (object);
  EphyDataDialogPrivate *priv = ephy_data_dialog_get_instance_private (self);

  g_free (priv->search_text);

  G_OBJECT_CLASS (ephy_data_dialog_parent_class)->finalize (object);
}

static void
ephy_data_dialog_add (GtkContainer *container,
                      GtkWidget    *child)
{
  EphyDataDialog *self = EPHY_DATA_DIALOG (container);
  EphyDataDialogPrivate *priv = ephy_data_dialog_get_instance_private (self);

  if (!priv->box) {
    GTK_CONTAINER_CLASS (ephy_data_dialog_parent_class)->add (container, child);
    return;
  }

  g_assert (!priv->child);

  priv->child = child;
  gtk_container_add (GTK_CONTAINER (priv->stack), child);

  update (self);
}

static void
ephy_data_dialog_class_init (EphyDataDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->set_property = ephy_data_dialog_set_property;
  object_class->get_property = ephy_data_dialog_get_property;
  object_class->finalize = ephy_data_dialog_finalize;

  container_class->add = ephy_data_dialog_add;

  obj_properties[PROP_CLEAR_ALL_ACTION_NAME] =
    g_param_spec_string ("clear-all-action-name",
                         _("'Clear all' action name"),
                         _("The name of the action associated to the 'Clear all' button"),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_CLEAR_ALL_ACTION_TARGET] =
    g_param_spec_variant ("clear-all-action-target",
                          _("'Clear all' action target value"),
                          _("The parameter for 'Clear all' action invocations"),
                          G_VARIANT_TYPE_ANY, NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_CLEAR_ALL_DESCRIPTION] =
    g_param_spec_string ("clear-all-description",
                         _("'Clear all' description"),
                         _("The description of the 'Clear all' action"),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_SEARCH_DESCRIPTION] =
    g_param_spec_string ("search-description",
                         _("'Search' description"),
                         _("The description of the 'Search' action"),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_EMPTY_TITLE] =
    g_param_spec_string ("empty-title",
                         _("'Empty' title"),
                         _("The title of the 'Empty' state page"),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_EMPTY_DESCRIPTION] =
    g_param_spec_string ("empty-description",
                         _("'Empty' description"),
                         _("The description of the 'Empty' state page"),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_SEARCH_TEXT] =
    g_param_spec_string ("search-text",
                         _("Search text"),
                         _("The text of the search entry"),
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);


  obj_properties[PROP_HAS_DATA] =
    g_param_spec_boolean ("has-data",
                          _("Has data"),
                          _("Whether the dialog has data"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_HAS_SEARCH_RESULTS] =
    g_param_spec_boolean ("has-search-results",
                          _("Has search results"),
                          _("Whether the dialog has search results"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_CAN_CLEAR] =
    g_param_spec_boolean ("can-clear",
                          _("Can clear"),
                          _("Whether the data can be cleared"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  /**
   * EphyLocationEntry::user-changed:
   * @entry: the object on which the signal is emitted
   *
   * Emitted when the user changes the contents of the internal #GtkEntry
   *
   */
  signals[CLEAR_ALL_CLICKED] =
    g_signal_new ("clear-all-clicked",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0,
                  G_TYPE_NONE);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/data-dialog.ui");
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataDialog, box);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataDialog, clear_all_button);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataDialog, search_bar);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataDialog, search_button);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataDialog, search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataDialog, stack);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataDialog, empty_title_label);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataDialog, empty_description_label);

  gtk_widget_class_bind_template_callback (widget_class, on_key_press_event);
  gtk_widget_class_bind_template_callback (widget_class, on_clear_all_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);
}

static void
ephy_data_dialog_init (EphyDataDialog *self)
{
  EphyDataDialogPrivate *priv = ephy_data_dialog_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  hdy_search_bar_connect_entry (HDY_SEARCH_BAR (priv->search_bar), GTK_ENTRY (priv->search_entry));

  update (self);
}

const gchar *
ephy_data_dialog_get_clear_all_description (EphyDataDialog *self)
{
  EphyDataDialogPrivate *priv;

  g_assert (EPHY_IS_DATA_DIALOG (self));

  priv = ephy_data_dialog_get_instance_private (self);

  return gtk_widget_get_tooltip_text (GTK_WIDGET (priv->clear_all_button));
}

void
ephy_data_dialog_set_clear_all_description (EphyDataDialog *self,
                                            const gchar    *description)
{
  EphyDataDialogPrivate *priv;

  g_assert (EPHY_IS_DATA_DIALOG (self));

  priv = ephy_data_dialog_get_instance_private (self);

  if (g_strcmp0 (gtk_widget_get_tooltip_text (GTK_WIDGET (priv->clear_all_button)), description) == 0)
    return;

  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->clear_all_button), description);

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_CLEAR_ALL_DESCRIPTION]);
}

gboolean
ephy_data_dialog_get_has_data (EphyDataDialog *self)
{
  EphyDataDialogPrivate *priv;

  g_assert (EPHY_IS_DATA_DIALOG (self));

  priv = ephy_data_dialog_get_instance_private (self);

  return priv->has_data;
}

void
ephy_data_dialog_set_has_data (EphyDataDialog *self,
                               gboolean        has_data)
{
  EphyDataDialogPrivate *priv;

  g_assert (EPHY_IS_DATA_DIALOG (self));

  priv = ephy_data_dialog_get_instance_private (self);
  has_data = !!has_data;

  if (priv->has_data == has_data)
    return;

  priv->has_data = has_data;

  update (self);

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_HAS_DATA]);
}

gboolean
ephy_data_dialog_get_has_search_results (EphyDataDialog *self)
{
  EphyDataDialogPrivate *priv;

  g_assert (EPHY_IS_DATA_DIALOG (self));

  priv = ephy_data_dialog_get_instance_private (self);

  return priv->has_search_results;
}

void
ephy_data_dialog_set_has_search_results (EphyDataDialog *self,
                                         gboolean        has_search_results)
{
  EphyDataDialogPrivate *priv;

  g_assert (EPHY_IS_DATA_DIALOG (self));

  priv = ephy_data_dialog_get_instance_private (self);
  has_search_results = !!has_search_results;

  if (priv->has_search_results == has_search_results)
    return;

  priv->has_search_results = has_search_results;

  update (self);

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_HAS_SEARCH_RESULTS]);
}

gboolean
ephy_data_dialog_get_can_clear (EphyDataDialog *self)
{
  EphyDataDialogPrivate *priv;

  g_assert (EPHY_IS_DATA_DIALOG (self));

  priv = ephy_data_dialog_get_instance_private (self);

  return priv->can_clear;
}

void
ephy_data_dialog_set_can_clear (EphyDataDialog *self,
                                gboolean        can_clear)
{
  EphyDataDialogPrivate *priv;

  g_assert (EPHY_IS_DATA_DIALOG (self));

  priv = ephy_data_dialog_get_instance_private (self);
  can_clear = !!can_clear;

  if (priv->can_clear == can_clear)
    return;

  priv->can_clear = can_clear;

  update (self);

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_CAN_CLEAR]);
}

const gchar *
ephy_data_dialog_get_search_text (EphyDataDialog *self)
{
  EphyDataDialogPrivate *priv;

  g_assert (EPHY_IS_DATA_DIALOG (self));

  priv = ephy_data_dialog_get_instance_private (self);

  return priv->search_text;
}
