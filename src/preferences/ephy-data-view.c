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
#include "ephy-data-view.h"

#include <adwaita.h>
#include <ctype.h>

typedef struct {
  GtkWidget *toast_overlay;
  GtkWidget *box;
  GtkWidget *child;
  GtkWidget *clear_button;
  GtkWidget *search_bar;
  GtkWidget *search_entry;
  GtkWidget *search_button;
  GtkWidget *stack;
  GtkWidget *empty_page;
  GtkWidget *spinner;
  GtkWidget *options_menu_button;

  gboolean is_loading : 1;
  gboolean has_data : 1;
  gboolean has_search_results : 1;
  gboolean can_clear : 1;
  char *search_text;
  char *search_description;
} EphyDataViewPrivate;

static void ephy_data_view_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (EphyDataView, ephy_data_view, ADW_TYPE_NAVIGATION_PAGE,
                         G_ADD_PRIVATE (EphyDataView)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                ephy_data_view_buildable_init))

static GtkBuildableIface *parent_buildable_iface;

enum {
  PROP_0,
  PROP_CLEAR_ACTION_NAME,
  PROP_CLEAR_ACTION_TARGET,
  PROP_CLEAR_BUTTON_LABEL,
  PROP_CLEAR_BUTTON_TOOLTIP,
  PROP_SEARCH_DESCRIPTION,
  PROP_EMPTY_TITLE,
  PROP_EMPTY_DESCRIPTION,
  PROP_SEARCH_TEXT,
  PROP_IS_LOADING,
  PROP_HAS_DATA,
  PROP_HAS_SEARCH_RESULTS,
  PROP_CAN_CLEAR,
  LAST_PROP,
};

static GParamSpec *obj_properties[LAST_PROP];

enum {
  CLEAR_BUTTON_CLICKED,
  LAST_SIGNAL,
};

static gint signals[LAST_SIGNAL] = { 0 };

static void
update (EphyDataView *self)
{
  EphyDataViewPrivate *priv = ephy_data_view_get_instance_private (self);
  gboolean has_data = priv->has_data && priv->child && gtk_widget_get_visible (priv->child);

  if (priv->is_loading) {
    gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "loading");
  } else {
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
  }

  gtk_widget_set_sensitive (priv->clear_button, has_data && priv->can_clear);
  gtk_widget_set_sensitive (priv->search_button, has_data);
}

static void
on_clear_button_clicked (EphyDataView *self)
{
  g_signal_emit (self, signals[CLEAR_BUTTON_CLICKED], 0);
}

static void
on_search_entry_changed (GtkSearchEntry *entry,
                         EphyDataView   *self)
{
  EphyDataViewPrivate *priv = ephy_data_view_get_instance_private (self);
  const char *text;

  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  g_free (priv->search_text);
  priv->search_text = g_strdup (text);

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_SEARCH_TEXT]);
}

static void
ephy_data_view_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  EphyDataView *self = EPHY_DATA_VIEW (object);
  EphyDataViewPrivate *priv = ephy_data_view_get_instance_private (self);

  switch (prop_id) {
    case PROP_CLEAR_ACTION_NAME:
      gtk_actionable_set_action_name (GTK_ACTIONABLE (priv->clear_button), g_value_get_string (value));
      break;
    case PROP_CLEAR_ACTION_TARGET:
      gtk_actionable_set_action_target_value (GTK_ACTIONABLE (priv->clear_button), g_value_get_variant (value));
      break;
    case PROP_CLEAR_BUTTON_LABEL:
      ephy_data_view_set_clear_button_label (self, g_value_get_string (value));
      break;
    case PROP_CLEAR_BUTTON_TOOLTIP:
      ephy_data_view_set_clear_button_tooltip (self, g_value_get_string (value));
      break;
    case PROP_SEARCH_DESCRIPTION:
      g_clear_pointer (&priv->search_description, g_free);
      priv->search_description = g_value_dup_string (value);
      gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                      GTK_ACCESSIBLE_PROPERTY_DESCRIPTION,
                                      priv->search_description,
                                      -1);
      break;
    case PROP_EMPTY_TITLE:
      adw_status_page_set_title (ADW_STATUS_PAGE (priv->empty_page), g_value_get_string (value));
      break;
    case PROP_EMPTY_DESCRIPTION:
      adw_status_page_set_description (ADW_STATUS_PAGE (priv->empty_page), g_value_get_string (value));
      break;
    case PROP_IS_LOADING:
      ephy_data_view_set_is_loading (self, g_value_get_boolean (value));
      break;
    case PROP_HAS_DATA:
      ephy_data_view_set_has_data (self, g_value_get_boolean (value));
      break;
    case PROP_HAS_SEARCH_RESULTS:
      ephy_data_view_set_has_search_results (self, g_value_get_boolean (value));
      break;
    case PROP_CAN_CLEAR:
      ephy_data_view_set_can_clear (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_data_view_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  EphyDataView *self = EPHY_DATA_VIEW (object);
  EphyDataViewPrivate *priv = ephy_data_view_get_instance_private (self);

  switch (prop_id) {
    case PROP_CLEAR_ACTION_NAME:
      g_value_set_string (value, gtk_actionable_get_action_name (GTK_ACTIONABLE (priv->clear_button)));
      break;
    case PROP_CLEAR_ACTION_TARGET:
      g_value_set_variant (value, gtk_actionable_get_action_target_value (GTK_ACTIONABLE (priv->clear_button)));
      break;
    case PROP_CLEAR_BUTTON_LABEL:
      g_value_set_string (value, ephy_data_view_get_clear_button_label (self));
      break;
    case PROP_CLEAR_BUTTON_TOOLTIP:
      g_value_set_string (value, ephy_data_view_get_clear_button_tooltip (self));
      break;
    case PROP_SEARCH_DESCRIPTION:
      g_value_set_string (value, priv->search_description);
      break;
    case PROP_EMPTY_TITLE:
      g_value_set_string (value, adw_status_page_get_title (ADW_STATUS_PAGE (priv->empty_page)));
      break;
    case PROP_EMPTY_DESCRIPTION:
      g_value_set_string (value, adw_status_page_get_description (ADW_STATUS_PAGE (priv->empty_page)));
      break;
    case PROP_SEARCH_TEXT:
      g_value_set_string (value, ephy_data_view_get_search_text (self));
      break;
    case PROP_IS_LOADING:
      g_value_set_boolean (value, ephy_data_view_get_is_loading (self));
      break;
    case PROP_HAS_DATA:
      g_value_set_boolean (value, ephy_data_view_get_has_data (self));
      break;
    case PROP_HAS_SEARCH_RESULTS:
      g_value_set_boolean (value, ephy_data_view_get_has_search_results (self));
      break;
    case PROP_CAN_CLEAR:
      g_value_set_boolean (value, ephy_data_view_get_can_clear (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_data_view_finalize (GObject *object)
{
  EphyDataView *self = EPHY_DATA_VIEW (object);
  EphyDataViewPrivate *priv = ephy_data_view_get_instance_private (self);

  g_free (priv->search_text);
  g_free (priv->search_description);

  G_OBJECT_CLASS (ephy_data_view_parent_class)->finalize (object);
}

static gboolean
find_shortuct_cb (EphyDataView *self)
{
  EphyDataViewPrivate *priv = ephy_data_view_get_instance_private (self);
  gboolean search;

  search = gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (priv->search_bar));

  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (priv->search_bar), !search);

  return GDK_EVENT_STOP;
}

static void
ephy_data_view_class_init (EphyDataViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ephy_data_view_set_property;
  object_class->get_property = ephy_data_view_get_property;
  object_class->finalize = ephy_data_view_finalize;

  obj_properties[PROP_CLEAR_ACTION_NAME] =
    g_param_spec_string ("clear-action-name",
                         NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_CLEAR_ACTION_TARGET] =
    g_param_spec_variant ("clear-action-target",
                          NULL, NULL,
                          G_VARIANT_TYPE_ANY, NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_CLEAR_BUTTON_LABEL] =
    g_param_spec_string ("clear-button-label",
                         NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_CLEAR_BUTTON_TOOLTIP] =
    g_param_spec_string ("clear-button-tooltip",
                         NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_SEARCH_DESCRIPTION] =
    g_param_spec_string ("search-description",
                         NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_EMPTY_TITLE] =
    g_param_spec_string ("empty-title",
                         NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_EMPTY_DESCRIPTION] =
    g_param_spec_string ("empty-description",
                         NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_SEARCH_TEXT] =
    g_param_spec_string ("search-text",
                         NULL, NULL,
                         NULL,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_IS_LOADING] =
    g_param_spec_boolean ("is-loading",
                          NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_HAS_DATA] =
    g_param_spec_boolean ("has-data",
                          NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_HAS_SEARCH_RESULTS] =
    g_param_spec_boolean ("has-search-results",
                          NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_CAN_CLEAR] =
    g_param_spec_boolean ("can-clear",
                          NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  signals[CLEAR_BUTTON_CLICKED] =
    g_signal_new ("clear-button-clicked",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0,
                  G_TYPE_NONE);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/data-view.ui");

  gtk_widget_class_bind_template_child_private (widget_class, EphyDataView, toast_overlay);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataView, box);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataView, clear_button);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataView, empty_page);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataView, search_bar);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataView, search_button);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataView, search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataView, spinner);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataView, stack);
  gtk_widget_class_bind_template_child_private (widget_class, EphyDataView, options_menu_button);

  gtk_widget_class_bind_template_callback (widget_class, on_clear_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);

  gtk_widget_class_add_binding (widget_class, GDK_KEY_F, GDK_CONTROL_MASK,
                                (GtkShortcutFunc)find_shortuct_cb, NULL);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Delete, GDK_SHIFT_MASK,
                                       "clear-button-clicked", NULL);
}

static void
ephy_data_view_init (EphyDataView *self)
{
  EphyDataViewPrivate *priv = ephy_data_view_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_search_bar_connect_entry (GTK_SEARCH_BAR (priv->search_bar),
                                GTK_EDITABLE (priv->search_entry));

  adw_status_page_set_icon_name (ADW_STATUS_PAGE (priv->empty_page),
                                 APPLICATION_ID "-symbolic");

  update (self);
}

static void
ephy_data_view_add_child (GtkBuildable *buildable,
                          GtkBuilder   *builder,
                          GObject      *child,
                          const char   *type)
{
  EphyDataView *self = EPHY_DATA_VIEW (buildable);
  EphyDataViewPrivate *priv = ephy_data_view_get_instance_private (self);

  if (!priv->box || !GTK_IS_WIDGET (child)) {
    parent_buildable_iface->add_child (buildable, builder, child, type);
    return;
  }

  g_assert (!priv->child);

  priv->child = GTK_WIDGET (child);
  gtk_stack_add_child (GTK_STACK (priv->stack), priv->child);

  update (self);
}

static void
ephy_data_view_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = ephy_data_view_add_child;
}

AdwToastOverlay *
ephy_data_view_get_toast_overlay (EphyDataView *self)
{
  EphyDataViewPrivate *priv;

  g_assert (EPHY_IS_DATA_VIEW (self));

  priv = ephy_data_view_get_instance_private (self);

  return ADW_TOAST_OVERLAY (priv->toast_overlay);
}

const gchar *
ephy_data_view_get_clear_button_label (EphyDataView *self)
{
  EphyDataViewPrivate *priv;

  g_assert (EPHY_IS_DATA_VIEW (self));

  priv = ephy_data_view_get_instance_private (self);

  return gtk_button_get_label (GTK_BUTTON (priv->clear_button));
}

void
ephy_data_view_set_clear_button_label (EphyDataView *self,
                                       const gchar  *label)
{
  EphyDataViewPrivate *priv;

  g_assert (EPHY_IS_DATA_VIEW (self));

  priv = ephy_data_view_get_instance_private (self);

  if (g_strcmp0 (gtk_button_get_label (GTK_BUTTON (priv->clear_button)), label) == 0)
    return;

  gtk_button_set_label (GTK_BUTTON (priv->clear_button), label);

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_CLEAR_BUTTON_LABEL]);
}

const gchar *
ephy_data_view_get_clear_button_tooltip (EphyDataView *self)
{
  EphyDataViewPrivate *priv;

  g_assert (EPHY_IS_DATA_VIEW (self));

  priv = ephy_data_view_get_instance_private (self);

  return gtk_widget_get_tooltip_text (GTK_WIDGET (priv->clear_button));
}

void
ephy_data_view_set_clear_button_tooltip (EphyDataView *self,
                                         const gchar  *tooltip)
{
  EphyDataViewPrivate *priv;

  g_assert (EPHY_IS_DATA_VIEW (self));

  priv = ephy_data_view_get_instance_private (self);

  if (g_strcmp0 (gtk_widget_get_tooltip_text (GTK_WIDGET (priv->clear_button)), tooltip) == 0)
    return;

  gtk_widget_set_tooltip_text (GTK_WIDGET (priv->clear_button), tooltip);

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_CLEAR_BUTTON_TOOLTIP]);
}

gboolean
ephy_data_view_get_is_loading (EphyDataView *self)
{
  EphyDataViewPrivate *priv;

  g_assert (EPHY_IS_DATA_VIEW (self));

  priv = ephy_data_view_get_instance_private (self);

  return priv->is_loading;
}

void
ephy_data_view_set_is_loading (EphyDataView *self,
                               gboolean      is_loading)
{
  EphyDataViewPrivate *priv;

  g_assert (EPHY_IS_DATA_VIEW (self));

  priv = ephy_data_view_get_instance_private (self);
  is_loading = !!is_loading;

  if (priv->is_loading == is_loading)
    return;

  priv->is_loading = is_loading;

  update (self);

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_IS_LOADING]);
}

gboolean
ephy_data_view_get_has_data (EphyDataView *self)
{
  EphyDataViewPrivate *priv;

  g_assert (EPHY_IS_DATA_VIEW (self));

  priv = ephy_data_view_get_instance_private (self);

  return priv->has_data;
}

void
ephy_data_view_set_has_data (EphyDataView *self,
                             gboolean      has_data)
{
  EphyDataViewPrivate *priv;

  g_assert (EPHY_IS_DATA_VIEW (self));

  priv = ephy_data_view_get_instance_private (self);
  has_data = !!has_data;

  if (priv->has_data == has_data)
    return;

  priv->has_data = has_data;

  update (self);

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_HAS_DATA]);
}

gboolean
ephy_data_view_get_has_search_results (EphyDataView *self)
{
  EphyDataViewPrivate *priv;

  g_assert (EPHY_IS_DATA_VIEW (self));

  priv = ephy_data_view_get_instance_private (self);

  return priv->has_search_results;
}

void
ephy_data_view_set_has_search_results (EphyDataView *self,
                                       gboolean      has_search_results)
{
  EphyDataViewPrivate *priv;

  g_assert (EPHY_IS_DATA_VIEW (self));

  priv = ephy_data_view_get_instance_private (self);
  has_search_results = !!has_search_results;

  if (priv->has_search_results == has_search_results)
    return;

  priv->has_search_results = has_search_results;

  update (self);

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_HAS_SEARCH_RESULTS]);
}

gboolean
ephy_data_view_get_can_clear (EphyDataView *self)
{
  EphyDataViewPrivate *priv;

  g_assert (EPHY_IS_DATA_VIEW (self));

  priv = ephy_data_view_get_instance_private (self);

  return priv->can_clear;
}

void
ephy_data_view_set_can_clear (EphyDataView *self,
                              gboolean      can_clear)
{
  EphyDataViewPrivate *priv;

  g_assert (EPHY_IS_DATA_VIEW (self));

  priv = ephy_data_view_get_instance_private (self);
  can_clear = !!can_clear;

  if (priv->can_clear == can_clear)
    return;

  priv->can_clear = can_clear;

  update (self);

  g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_CAN_CLEAR]);
}

const gchar *
ephy_data_view_get_search_text (EphyDataView *self)
{
  EphyDataViewPrivate *priv;

  g_assert (EPHY_IS_DATA_VIEW (self));

  priv = ephy_data_view_get_instance_private (self);

  return priv->search_text;
}

void
ephy_data_view_set_options_menu (EphyDataView *self,
                                 GMenuModel   *model)
{
  EphyDataViewPrivate *priv = ephy_data_view_get_instance_private (self);

  gtk_widget_set_visible (priv->options_menu_button, !!model);
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (priv->options_menu_button), model);
}
