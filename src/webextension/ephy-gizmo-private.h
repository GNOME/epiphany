/*
 * Copyright (C) 2020 Purism SPC
 *
 * Based on gtkgizmoprivate.h
 * https://gitlab.gnome.org/GNOME/gtk/-/blob/5d5625dec839c00fdb572af82fbbe872ea684859/gtk/gtkgizmoprivate.h
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_GIZMO (ephy_gizmo_get_type())

G_DECLARE_FINAL_TYPE (EphyGizmo, ephy_gizmo, EPHY, GIZMO, GtkWidget)

typedef void     (* EphyGizmoMeasureFunc)  (EphyGizmo       *self,
                                           GtkOrientation  orientation,
                                           int             for_size,
                                           int            *minimum,
                                           int            *natural,
                                           int            *minimum_baseline,
                                           int            *natural_baseline);
typedef void     (* EphyGizmoAllocateFunc) (EphyGizmo *self,
                                           int       width,
                                           int       height,
                                           int       baseline);
typedef void     (* EphyGizmoSnapshotFunc) (EphyGizmo    *self,
                                           GtkSnapshot *snapshot);
typedef gboolean (* EphyGizmoContainsFunc) (EphyGizmo *self,
                                           double    x,
                                           double    y);
typedef gboolean (* EphyGizmoFocusFunc)    (EphyGizmo         *self,
                                           GtkDirectionType  direction);
typedef gboolean (* EphyGizmoGrabFocusFunc)(EphyGizmo         *self);

GtkWidget *ephy_gizmo_new (const char            *css_name,
                          EphyGizmoMeasureFunc    measure_func,
                          EphyGizmoAllocateFunc   allocate_func,
                          EphyGizmoSnapshotFunc   snapshot_func,
                          EphyGizmoContainsFunc   contains_func,
                          EphyGizmoFocusFunc      focus_func,
                          EphyGizmoGrabFocusFunc  grab_focus_func) G_GNUC_WARN_UNUSED_RESULT;

GtkWidget *ephy_gizmo_new_with_role (const char            *css_name,
                                    GtkAccessibleRole      role,
                                    EphyGizmoMeasureFunc    measure_func,
                                    EphyGizmoAllocateFunc   allocate_func,
                                    EphyGizmoSnapshotFunc   snapshot_func,
                                    EphyGizmoContainsFunc   contains_func,
                                    EphyGizmoFocusFunc      focus_func,
                                    EphyGizmoGrabFocusFunc  grab_focus_func) G_GNUC_WARN_UNUSED_RESULT;

void ephy_gizmo_set_measure_func    (EphyGizmo              *self,
                                    EphyGizmoMeasureFunc    measure_func);
void ephy_gizmo_set_allocate_func   (EphyGizmo              *self,
                                    EphyGizmoAllocateFunc   allocate_func);
void ephy_gizmo_set_snapshot_func   (EphyGizmo              *self,
                                    EphyGizmoSnapshotFunc   snapshot_func);
void ephy_gizmo_set_contains_func   (EphyGizmo              *self,
                                    EphyGizmoContainsFunc   contains_func);
void ephy_gizmo_set_focus_func      (EphyGizmo              *self,
                                    EphyGizmoFocusFunc      focus_func);
void ephy_gizmo_set_grab_focus_func (EphyGizmo              *self,
                                    EphyGizmoGrabFocusFunc  grab_focus_func);

G_END_DECLS

