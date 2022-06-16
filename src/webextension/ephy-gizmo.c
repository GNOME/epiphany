/*
 * Copyright (C) 2020 Purism SPC
 *
 * Based on gtkgizmo.c
 * https://gitlab.gnome.org/GNOME/gtk/-/blob/5d5625dec839c00fdb572af82fbbe872ea684859/gtk/gtkgizmo.c
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "ephy-gizmo-private.h"

#include "adw-widget-utils-private.h"

struct _EphyGizmo {
  GtkWidget parent_instance;

  EphyGizmoMeasureFunc measure_func;
  EphyGizmoAllocateFunc allocate_func;
  EphyGizmoSnapshotFunc snapshot_func;
  EphyGizmoContainsFunc contains_func;
  EphyGizmoFocusFunc focus_func;
  EphyGizmoGrabFocusFunc grab_focus_func;
};

G_DEFINE_FINAL_TYPE (EphyGizmo, ephy_gizmo, GTK_TYPE_WIDGET)

static void
ephy_gizmo_measure (GtkWidget      *widget,
                    GtkOrientation  orientation,
                    int             for_size,
                    int            *minimum,
                    int            *natural,
                    int            *minimum_baseline,
                    int            *natural_baseline)
{
  EphyGizmo *self = EPHY_GIZMO (widget);

  if (self->measure_func)
    self->measure_func (self, orientation, for_size,
                        minimum, natural,
                        minimum_baseline, natural_baseline);
}

static void
ephy_gizmo_size_allocate (GtkWidget *widget,
                          int        width,
                          int        height,
                          int        baseline)
{
  EphyGizmo *self = EPHY_GIZMO (widget);

  if (self->allocate_func)
    self->allocate_func (self, width, height, baseline);
}

static void
ephy_gizmo_snapshot (GtkWidget   *widget,
                     GtkSnapshot *snapshot)
{
  EphyGizmo *self = EPHY_GIZMO (widget);

  if (self->snapshot_func)
    self->snapshot_func (self, snapshot);
  else
    GTK_WIDGET_CLASS (ephy_gizmo_parent_class)->snapshot (widget, snapshot);
}

static gboolean
ephy_gizmo_contains (GtkWidget *widget,
                     double     x,
                     double     y)
{
  EphyGizmo *self = EPHY_GIZMO (widget);

  if (self->contains_func)
    return self->contains_func (self, x, y);
  else
    return GTK_WIDGET_CLASS (ephy_gizmo_parent_class)->contains (widget, x, y);
}

static gboolean
ephy_gizmo_focus (GtkWidget        *widget,
                  GtkDirectionType  direction)
{
  EphyGizmo *self = EPHY_GIZMO (widget);

  if (self->focus_func)
    return self->focus_func (self, direction);

  return FALSE;
}

static gboolean
ephy_gizmo_grab_focus (GtkWidget *widget)
{
  EphyGizmo *self = EPHY_GIZMO (widget);

  if (self->grab_focus_func)
    return self->grab_focus_func (self);

  return FALSE;
}

static void
ephy_gizmo_dispose (GObject *object)
{
  EphyGizmo *self = EPHY_GIZMO (object);
  GtkWidget *widget = gtk_widget_get_first_child (GTK_WIDGET (self));

  while (widget) {
    GtkWidget *next = gtk_widget_get_next_sibling (widget);

    gtk_widget_unparent (widget);

    widget = next;
  }

  G_OBJECT_CLASS (ephy_gizmo_parent_class)->dispose (object);
}

static void
ephy_gizmo_class_init (EphyGizmoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ephy_gizmo_dispose;

  widget_class->measure = ephy_gizmo_measure;
  widget_class->size_allocate = ephy_gizmo_size_allocate;
  widget_class->snapshot = ephy_gizmo_snapshot;
  widget_class->contains = ephy_gizmo_contains;
  widget_class->grab_focus = ephy_gizmo_grab_focus;
  widget_class->focus = ephy_gizmo_focus;
  widget_class->compute_expand = adw_widget_compute_expand;
}

static void
ephy_gizmo_init (EphyGizmo *self)
{
}

GtkWidget *
ephy_gizmo_new (const char             *css_name,
                EphyGizmoMeasureFunc    measure_func,
                EphyGizmoAllocateFunc   allocate_func,
                EphyGizmoSnapshotFunc   snapshot_func,
                EphyGizmoContainsFunc   contains_func,
                EphyGizmoFocusFunc      focus_func,
                EphyGizmoGrabFocusFunc  grab_focus_func)
{
  EphyGizmo *gizmo = g_object_new (EPHY_TYPE_GIZMO,
                                   "css-name", css_name,
                                   NULL);

  gizmo->measure_func = measure_func;
  gizmo->allocate_func = allocate_func;
  gizmo->snapshot_func = snapshot_func;
  gizmo->contains_func = contains_func;
  gizmo->focus_func = focus_func;
  gizmo->grab_focus_func = grab_focus_func;

  return GTK_WIDGET (gizmo);
}

GtkWidget *
ephy_gizmo_new_with_role (const char             *css_name,
                          GtkAccessibleRole       role,
                          EphyGizmoMeasureFunc    measure_func,
                          EphyGizmoAllocateFunc   allocate_func,
                          EphyGizmoSnapshotFunc   snapshot_func,
                          EphyGizmoContainsFunc   contains_func,
                          EphyGizmoFocusFunc      focus_func,
                          EphyGizmoGrabFocusFunc  grab_focus_func)
{
  EphyGizmo *gizmo = EPHY_GIZMO (g_object_new (EPHY_TYPE_GIZMO,
                                               "css-name", css_name,
                                               "accessible-role", role,
                                               NULL));

  gizmo->measure_func = measure_func;
  gizmo->allocate_func = allocate_func;
  gizmo->snapshot_func = snapshot_func;
  gizmo->contains_func = contains_func;
  gizmo->focus_func = focus_func;
  gizmo->grab_focus_func = grab_focus_func;

  return GTK_WIDGET (gizmo);
}

void
ephy_gizmo_set_measure_func (EphyGizmo            *self,
                             EphyGizmoMeasureFunc  measure_func)
{
  self->measure_func = measure_func;

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

void
ephy_gizmo_set_allocate_func (EphyGizmo             *self,
                              EphyGizmoAllocateFunc  allocate_func)
{
  self->allocate_func = allocate_func;

  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

void
ephy_gizmo_set_snapshot_func (EphyGizmo             *self,
                              EphyGizmoSnapshotFunc  snapshot_func)
{
  self->snapshot_func = snapshot_func;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
ephy_gizmo_set_contains_func (EphyGizmo             *self,
                              EphyGizmoContainsFunc  contains_func)
{
  self->contains_func = contains_func;

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

void
ephy_gizmo_set_focus_func (EphyGizmo          *self,
                           EphyGizmoFocusFunc  focus_func)
{
  self->focus_func = focus_func;
}

void
ephy_gizmo_set_grab_focus_func (EphyGizmo              *self,
                                EphyGizmoGrabFocusFunc  grab_focus_func)
{
  self->grab_focus_func = grab_focus_func;
}
