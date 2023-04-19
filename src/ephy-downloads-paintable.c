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
#include "ephy-downloads-paintable.h"

#include <adwaita.h>

#define SIZE 16

struct _EphyDownloadsPaintable {
  GObject parent_instance;

  GtkWidget *widget;

  double progress;

  GtkIconPaintable *arrow_paintable;
  GtkIconPaintable *check_paintable;

  double check_progress;
  AdwAnimation *done_animation;
  guint timeout_id;
};

static void ephy_downloads_paintable_paintable_init (GdkPaintableInterface *iface);
static void ephy_downloads_paintable_symbolic_paintable_init (GtkSymbolicPaintableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyDownloadsPaintable, ephy_downloads_paintable, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                      ephy_downloads_paintable_paintable_init)
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_SYMBOLIC_PAINTABLE,
                                                      ephy_downloads_paintable_symbolic_paintable_init))

enum {
  PROP_0,
  PROP_WIDGET,
  PROP_PROGRESS,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

static void
cache_icons (EphyDownloadsPaintable *self)
{
  GdkDisplay *display = gtk_widget_get_display (self->widget);
  GtkIconTheme *theme = gtk_icon_theme_get_for_display (display);
  int scale = gtk_widget_get_scale_factor (self->widget);
  GtkTextDirection direction = gtk_widget_get_direction (self->widget);

  g_set_object (&self->arrow_paintable,
                gtk_icon_theme_lookup_icon (theme, "ephy-download-symbolic",
                                            NULL, SIZE, scale, direction,
                                            GTK_ICON_LOOKUP_FORCE_SYMBOLIC));
  g_set_object (&self->check_paintable,
                gtk_icon_theme_lookup_icon (theme, "ephy-download-done-symbolic",
                                            NULL, SIZE, scale, direction,
                                            GTK_ICON_LOOKUP_FORCE_SYMBOLIC));
}

static void
scale_factor_changed_cb (EphyDownloadsPaintable *self)
{
  cache_icons (self);
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
}

static void
ephy_downloads_paintable_constructed (GObject *object)
{
  EphyDownloadsPaintable *self = EPHY_DOWNLOADS_PAINTABLE (object);

  g_signal_connect_swapped (self->widget, "notify::scale-factor",
                            G_CALLBACK (scale_factor_changed_cb), self);

  cache_icons (self);

  G_OBJECT_CLASS (ephy_downloads_paintable_parent_class)->constructed (object);
}

static void
ephy_downloads_paintable_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  EphyDownloadsPaintable *self = EPHY_DOWNLOADS_PAINTABLE (object);

  switch (prop_id) {
    case PROP_WIDGET:
      g_value_set_object (value, self->widget);
      break;
    case PROP_PROGRESS:
      g_value_set_double (value, self->progress);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_downloads_paintable_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  EphyDownloadsPaintable *self = EPHY_DOWNLOADS_PAINTABLE (object);

  switch (prop_id) {
    case PROP_WIDGET:
      g_set_object (&self->widget, g_value_get_object (value));
      break;
    case PROP_PROGRESS:
      self->progress = g_value_get_double (value);
      gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_downloads_paintable_dispose (GObject *object)
{
  EphyDownloadsPaintable *self = EPHY_DOWNLOADS_PAINTABLE (object);

  g_clear_object (&self->widget);
  g_clear_object (&self->arrow_paintable);
  g_clear_object (&self->check_paintable);
  g_clear_object (&self->done_animation);
  g_clear_handle_id (&self->timeout_id, g_source_remove);

  G_OBJECT_CLASS (ephy_downloads_paintable_parent_class)->dispose (object);
}

static void
ephy_downloads_paintable_class_init (EphyDownloadsPaintableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ephy_downloads_paintable_constructed;
  object_class->get_property = ephy_downloads_paintable_get_property;
  object_class->set_property = ephy_downloads_paintable_set_property;
  object_class->dispose = ephy_downloads_paintable_dispose;

  properties[PROP_WIDGET] =
    g_param_spec_object ("widget",
                         NULL, NULL,
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_PROGRESS] =
    g_param_spec_double ("progress",
                         NULL, NULL,
                         0, 1, 0,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ephy_downloads_paintable_init (EphyDownloadsPaintable *self)
{
  self->progress = 0;
  self->check_progress = 0;
  self->timeout_id = -1;
}

static int
ephy_downloads_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  EphyDownloadsPaintable *self = EPHY_DOWNLOADS_PAINTABLE (paintable);

  return SIZE * gtk_widget_get_scale_factor (self->widget);
}

static int
ephy_downloads_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  EphyDownloadsPaintable *self = EPHY_DOWNLOADS_PAINTABLE (paintable);

  return SIZE * gtk_widget_get_scale_factor (self->widget);
}

static void
ephy_downloads_paintable_paintable_init (GdkPaintableInterface *iface)
{
  iface->get_intrinsic_width = ephy_downloads_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = ephy_downloads_paintable_get_intrinsic_height;
}

static void
ephy_downloads_paintable_snapshot_symbolic (GtkSymbolicPaintable *paintable,
                                            GdkSnapshot          *gdk_snapshot,
                                            double                width,
                                            double                height,
                                            const GdkRGBA        *colors,
                                            gsize                 n_colors)
{
  EphyDownloadsPaintable *self = EPHY_DOWNLOADS_PAINTABLE (paintable);
  GtkSnapshot *snapshot = GTK_SNAPSHOT (gdk_snapshot);
  cairo_t *cr;
  double arc_end;
  GdkRGBA rgba;

  if (self->check_progress < 1) {
    gtk_snapshot_save (snapshot);
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (width / 2.0f, height / 2.0f));
    gtk_snapshot_scale (snapshot, 1.0f - self->check_progress, 1.0f - self->check_progress);
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-width / 2.0f, -height / 2.0f));
    gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (self->arrow_paintable),
                                              gdk_snapshot, width, height, colors, n_colors);
    gtk_snapshot_restore (snapshot);
  }

  if (self->check_progress > 0) {
    gtk_snapshot_save (snapshot);
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (width / 2.0f, height / 2.0f));
    gtk_snapshot_scale (snapshot, self->check_progress, self->check_progress);
    gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (-width / 2.0f, -height / 2.0f));
    gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (self->check_paintable),
                                              gdk_snapshot, width, height, colors, n_colors);
    gtk_snapshot_restore (snapshot);
  }

  cr = gtk_snapshot_append_cairo (snapshot, &GRAPHENE_RECT_INIT (-2, -2, width + 4, width + 4));
  arc_end = self->progress * G_PI * 2 - G_PI / 2;

  cairo_translate (cr, width / 2.0, height / 2.0);

  gdk_cairo_set_source_rgba (cr, colors);
  cairo_arc (cr, 0, 0, width / 2.0 + 1, -G_PI / 2, arc_end);
  cairo_stroke (cr);

  rgba = *colors;
  rgba.alpha *= 0.25f;
  gdk_cairo_set_source_rgba (cr, &rgba);
  cairo_arc (cr, 0, 0, width / 2.0 + 1, arc_end, 3.0 * G_PI / 2.0);
  cairo_stroke (cr);
}

static void
ephy_downloads_paintable_symbolic_paintable_init (GtkSymbolicPaintableInterface *iface)
{
  iface->snapshot_symbolic = ephy_downloads_paintable_snapshot_symbolic;
}

GdkPaintable *
ephy_downloads_paintable_new (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return GDK_PAINTABLE (g_object_new (EPHY_TYPE_DOWNLOADS_PAINTABLE,
                                      "widget", widget,
                                      NULL));
}

static void
animate_done_cb (double                  value,
                 EphyDownloadsPaintable *self)
{
  self->check_progress = value;
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

static void
animation_timeout_cb (EphyDownloadsPaintable *self)
{
  adw_animation_play (self->done_animation);
  self->timeout_id = -1;
}

static void
animation_done_done_cb (EphyDownloadsPaintable *self)
{
  if (self->check_progress > 0.5) {
    int delay = adw_get_enable_animations (self->widget) ? 500 : 1000;

    adw_timed_animation_set_value_from (ADW_TIMED_ANIMATION (self->done_animation), 1);
    adw_timed_animation_set_value_to (ADW_TIMED_ANIMATION (self->done_animation), 0);

    self->timeout_id = g_timeout_add_once (delay, (GSourceOnceFunc)animation_timeout_cb, self);
  } else {
    g_clear_object (&self->done_animation);
  }
}

void
ephy_downloads_paintable_animate_done (EphyDownloadsPaintable *self)
{
  AdwAnimationTarget *target;

  g_return_if_fail (EPHY_IS_DOWNLOADS_PAINTABLE (self));

  if (self->done_animation)
    return;

  target = adw_callback_animation_target_new ((AdwAnimationTargetFunc)animate_done_cb, self, NULL);
  self->done_animation = adw_timed_animation_new (self->widget, 0, 1, 500, target);

  g_signal_connect_swapped (self->done_animation, "done",
                            G_CALLBACK (animation_done_done_cb), self);

  adw_timed_animation_set_easing (ADW_TIMED_ANIMATION (self->done_animation),
                                  ADW_EASE_IN_OUT_CUBIC);
  adw_animation_play (self->done_animation);
}
