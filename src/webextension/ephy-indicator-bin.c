/*
 * Copyright (C) 2021 Purism SPC
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Alexander Mikhaylenko <alexander.mikhaylenko@puri.sm>
 */

#include "config.h"
#include "ephy-indicator-bin-private.h"

#include "ephy-gizmo-private.h"
#include "adw-widget-utils-private.h"

/**
 * EphyIndicatorBin:
 *
 * A helper object for [class@ViewSwitcherButton].
 *
 * The `EphyIndicatorBin` widget shows an unread indicator over the child widget
 * masking it if they overlap.
 */

struct _EphyIndicatorBin {
  GtkWidget parent_instance;

  GtkWidget *child;

  GtkWidget *mask;
  GtkWidget *indicator;
  GtkWidget *label;

  GskGLShader *shader;
  gboolean shader_compiled;
};

static void ephy_indicator_bin_buildable_init (GtkBuildableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyIndicatorBin, ephy_indicator_bin, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, ephy_indicator_bin_buildable_init))

static GtkBuildableIface *parent_buildable_iface;

enum {
  PROP_0,
  PROP_CHILD,
  PROP_BADGE,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP];


static void
ensure_shader (EphyIndicatorBin *self)
{
  GtkNative *native;
  GskRenderer *renderer;
  GError *error = NULL;

  if (self->shader)
    return;

  self->shader = gsk_gl_shader_new_from_resource ("/org/gnome/epiphany/mask.glsl");

  native = gtk_widget_get_native (GTK_WIDGET (self));
  renderer = gtk_native_get_renderer (native);

  self->shader_compiled = gsk_gl_shader_compile (self->shader, renderer, &error);

  if (error) {
    /* If shaders aren't supported, the error doesn't matter and we just
     * silently fall back */
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
      g_warning ("Couldn't compile shader: %s", error->message);
  }

  g_clear_error (&error);
}

static gboolean
has_badge (EphyIndicatorBin *self)
{
  const char *text = gtk_label_get_label (GTK_LABEL (self->label));

  return text && text[0];
}

static void
ephy_indicator_bin_measure (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *min,
                            int            *nat,
                            int            *min_baseline,
                            int            *nat_baseline)
{
  EphyIndicatorBin *self = EPHY_INDICATOR_BIN (widget);

  if (!self->child) {
    if (min)
      *min = 0;
    if (nat)
      *nat = 0;
    if (min_baseline)
      *min_baseline = -1;
    if (nat_baseline)
      *nat_baseline = -1;

    return;
  }

  gtk_widget_measure (self->child, orientation, for_size,
                      min, nat, min_baseline, nat_baseline);
}

static void
ephy_indicator_bin_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
  EphyIndicatorBin *self = EPHY_INDICATOR_BIN (widget);
  GtkRequisition mask_size, indicator_size, size;
  float x, y;

  if (self->child)
    gtk_widget_allocate (self->child, width, height, baseline, NULL);

  gtk_widget_get_preferred_size (self->mask, NULL, &mask_size);
  gtk_widget_get_preferred_size (self->indicator, NULL, &indicator_size);

  size.width = MAX (mask_size.width, indicator_size.width);
  size.height = MAX (mask_size.height, indicator_size.height);

  if (size.width > width * 2)
    x = (width - size.width) / 2.0f;
  else if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    x = -size.height / 2.0f;
  else
    x = width - size.width + size.height / 2.0f;

  y = -size.height / 2.0f;

  gtk_widget_allocate (self->mask, size.width, size.height, baseline,
                       gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (x, y)));
  gtk_widget_allocate (self->indicator, size.width, size.height, baseline,
                       gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (x, y)));
}

static void
ephy_indicator_bin_snapshot (GtkWidget   *widget,
                             GtkSnapshot *snapshot)
{
  EphyIndicatorBin *self = EPHY_INDICATOR_BIN (widget);

  if (!has_badge (self)) {
    if (self->child)
      gtk_widget_snapshot_child (widget, self->child, snapshot);

    return;
  }

  if (self->child) {
    GtkSnapshot *child_snapshot;
    GskRenderNode *child_node;

    child_snapshot = gtk_snapshot_new ();
    gtk_widget_snapshot_child (widget, self->child, child_snapshot);
    child_node = gtk_snapshot_free_to_node (child_snapshot);

    if (!child_node)
      return;

    ensure_shader (self);

    if (self->shader_compiled) {
      graphene_rect_t bounds;

      gsk_render_node_get_bounds (child_node, &bounds);
      gtk_snapshot_push_gl_shader (snapshot, self->shader, &bounds,
                                   gsk_gl_shader_format_args (self->shader, NULL));
    }

    gtk_snapshot_append_node (snapshot, child_node);

    if (self->shader_compiled) {
      gtk_snapshot_gl_shader_pop_texture (snapshot);

      gtk_widget_snapshot_child (widget, self->mask, snapshot);
      gtk_snapshot_gl_shader_pop_texture (snapshot);

      gtk_snapshot_pop (snapshot);
    }

    gsk_render_node_unref (child_node);
  }

  gtk_widget_snapshot_child (widget, self->indicator, snapshot);
}

static void
ephy_indicator_bin_unrealize (GtkWidget *widget)
{
  EphyIndicatorBin *self = EPHY_INDICATOR_BIN (widget);

  GTK_WIDGET_CLASS (ephy_indicator_bin_parent_class)->unrealize (widget);

  g_clear_object (&self->shader);
}

static void
ephy_indicator_bin_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  EphyIndicatorBin *self = EPHY_INDICATOR_BIN (object);

  switch (prop_id) {
    case PROP_CHILD:
      g_value_set_object (value, ephy_indicator_bin_get_child (self));
      break;

    case PROP_BADGE:
      g_value_set_string (value, ephy_indicator_bin_get_badge (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_indicator_bin_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  EphyIndicatorBin *self = EPHY_INDICATOR_BIN (object);

  switch (prop_id) {
    case PROP_CHILD:
      ephy_indicator_bin_set_child (self, g_value_get_object (value));
      break;

    case PROP_BADGE:
      ephy_indicator_bin_set_badge (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_indicator_bin_dispose (GObject *object)
{
  EphyIndicatorBin *self = EPHY_INDICATOR_BIN (object);

  g_clear_pointer (&self->child, gtk_widget_unparent);
  g_clear_pointer (&self->mask, gtk_widget_unparent);
  g_clear_pointer (&self->indicator, gtk_widget_unparent);
  self->label = NULL;

  G_OBJECT_CLASS (ephy_indicator_bin_parent_class)->dispose (object);
}
static void
ephy_indicator_bin_class_init (EphyIndicatorBinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ephy_indicator_bin_get_property;
  object_class->set_property = ephy_indicator_bin_set_property;
  object_class->dispose = ephy_indicator_bin_dispose;

  widget_class->measure = ephy_indicator_bin_measure;
  widget_class->size_allocate = ephy_indicator_bin_size_allocate;
  widget_class->snapshot = ephy_indicator_bin_snapshot;
  widget_class->unrealize = ephy_indicator_bin_unrealize;
  widget_class->get_request_mode = adw_widget_get_request_mode;
  widget_class->compute_expand = adw_widget_compute_expand;

  /**
   * EphyIndicatorBin:child:
   *
   * The child widget.
   */
  props[PROP_CHILD] =
    g_param_spec_object ("child", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * EphyIndicatorBin:badge:
   *
   * Additional information for the user.
   */
  props[PROP_BADGE] =
    g_param_spec_string ("badge", NULL, NULL,
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_css_name (widget_class, "indicatorbin");
}

static void
ephy_indicator_bin_init (EphyIndicatorBin *self)
{
  self->mask = ephy_gizmo_new ("mask", NULL, NULL, NULL, NULL, NULL, NULL);
  gtk_widget_set_can_target (self->mask, FALSE);
  gtk_widget_set_parent (self->mask, GTK_WIDGET (self));

  self->indicator = ephy_gizmo_new ("indicator", NULL, NULL, NULL, NULL, NULL, NULL);
  gtk_widget_set_can_target (self->indicator, FALSE);
  gtk_widget_set_parent (self->indicator, GTK_WIDGET (self));
  gtk_widget_set_layout_manager (self->indicator, gtk_bin_layout_new ());

  self->label = gtk_label_new (NULL);
  gtk_widget_set_visible (self->label, FALSE);
  gtk_widget_set_parent (self->label, self->indicator);
  gtk_widget_add_css_class (self->label, "numeric");
}

static void
ephy_indicator_bin_buildable_add_child (GtkBuildable *buildable,
                                        GtkBuilder   *builder,
                                        GObject      *child,
                                        const char   *type)
{
  if (GTK_IS_WIDGET (child))
    ephy_indicator_bin_set_child (EPHY_INDICATOR_BIN (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
ephy_indicator_bin_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = ephy_indicator_bin_buildable_add_child;
}

/**
 * ephy_indicator_bin_new:
 *
 * Creates a new `EphyIndicatorBin`.
 *
 * Returns: the newly created `EphyIndicatorBin`
 */
GtkWidget *
ephy_indicator_bin_new (void)
{
  return g_object_new (EPHY_TYPE_INDICATOR_BIN, NULL);
}

/**
 * ephy_indicator_bin_get_child:
 * @self: an indicator bin
 *
 * Gets the child widget of @self.
 *
 * Returns: (nullable) (transfer none): the child widget of @self
 */
GtkWidget *
ephy_indicator_bin_get_child (EphyIndicatorBin *self)
{
  g_return_val_if_fail (EPHY_IS_INDICATOR_BIN (self), NULL);

  return self->child;
}

/**
 * ephy_indicator_bin_set_child:
 * @self: an indicator bin
 * @child: (nullable): the child widget
 *
 * Sets the child widget of @self.
 */
void
ephy_indicator_bin_set_child (EphyIndicatorBin *self,
                              GtkWidget        *child)
{
  g_return_if_fail (EPHY_IS_INDICATOR_BIN (self));
  g_return_if_fail (!child || GTK_IS_WIDGET (child));

  if (self->child == child)
    return;

  if (self->child)
    gtk_widget_unparent (self->child);

  self->child = child;

  if (self->child)
    gtk_widget_set_parent (self->child, GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CHILD]);
}

const char *
ephy_indicator_bin_get_badge (EphyIndicatorBin *self)
{
  g_return_val_if_fail (EPHY_IS_INDICATOR_BIN (self), "");

  return gtk_label_get_label (GTK_LABEL (self->label));
}

void
ephy_indicator_bin_set_badge (EphyIndicatorBin *self,
                              const char       *badge)
{
  g_return_if_fail (EPHY_IS_INDICATOR_BIN (self));

  gtk_label_set_text (GTK_LABEL (self->label), badge);

  if (badge && badge[0])
    gtk_widget_add_css_class (GTK_WIDGET (self), "badge");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "badge");

  gtk_widget_set_visible (self->label, badge && badge[0]);

  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_BADGE]);
}

void
ephy_indicator_bin_set_badge_color (EphyIndicatorBin *self,
                                    GdkRGBA          *color)
{
  GtkCssProvider *provider;
  GtkStyleContext *context;
  g_autofree char *css = NULL;

  gtk_widget_remove_css_class (GTK_WIDGET (self), "needs-attention");

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (color) {
    g_autofree char *col_str = gdk_rgba_to_string (color);
    css = g_strdup_printf (".needs-attention > indicator { background-color: %s; }", col_str);
    provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (provider, css, -1);
    context = gtk_widget_get_style_context (GTK_WIDGET (self->indicator));
    gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_widget_add_css_class (GTK_WIDGET (self), "needs-attention");
  }
  G_GNUC_END_IGNORE_DEPRECATIONS

  gtk_widget_queue_draw (GTK_WIDGET (self));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_BADGE]);
}
