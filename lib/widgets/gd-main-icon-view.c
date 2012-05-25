/*
 * Copyright (c) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public 
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License 
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "ephy-removable-pixbuf-renderer.h"
#include "gd-main-icon-view.h"
#include "gd-main-view.h"
#include "gd-main-view-generic.h"
#include "gd-toggle-pixbuf-renderer.h"
#include "gd-two-lines-renderer.h"

#include <math.h>
#include <glib/gi18n.h>

#define VIEW_ITEM_WIDTH 140
#define VIEW_ITEM_WRAP_WIDTH 128
#define VIEW_COLUMN_SPACING 20
#define VIEW_MARGIN 16

struct _GdMainIconViewPrivate {
  GtkCellRenderer *pixbuf_cell;
  gboolean selection_mode;
};

static void gd_main_view_generic_iface_init (GdMainViewGenericIface *iface);
G_DEFINE_TYPE_WITH_CODE (GdMainIconView, gd_main_icon_view, GTK_TYPE_ICON_VIEW,
                         G_IMPLEMENT_INTERFACE (GD_TYPE_MAIN_VIEW_GENERIC,
                                                gd_main_view_generic_iface_init))

static GtkTreePath*
get_source_row (GdkDragContext *context)
{
  GtkTreeRowReference *ref;

  ref = g_object_get_data (G_OBJECT (context), "gtk-icon-view-source-row");

  if (ref)
    return gtk_tree_row_reference_get_path (ref);
  else
    return NULL;
}

static void
gd_main_icon_view_drag_data_get (GtkWidget *widget,
                                 GdkDragContext *drag_context,
                                 GtkSelectionData *data,
                                 guint info,
                                 guint time)
{
  GdMainIconView *self = GD_MAIN_ICON_VIEW (widget);
  GtkTreeModel *model = gtk_icon_view_get_model (GTK_ICON_VIEW (self));

  if (info != 0)
    return;

  _gd_main_view_generic_dnd_common (model, self->priv->selection_mode,
                                    get_source_row (drag_context), data);

  GTK_WIDGET_CLASS (gd_main_icon_view_parent_class)->drag_data_get (widget, drag_context,
                                                                    data, info, time);
}

static void
on_cell_delete_clicked (EphyRemovablePixbufRenderer *cell,
			const gchar *path,
			GdMainIconView *self)
{
  _gd_main_view_generic_item_delete_clicked (GD_MAIN_VIEW_GENERIC (self), path);
}

static void
gd_main_icon_view_constructed (GObject *obj)
{
  GdMainIconView *self = GD_MAIN_ICON_VIEW (obj);
  GtkCellRenderer *cell;
  const GtkTargetEntry targets[] = {
    { "text/uri-list", GTK_TARGET_OTHER_APP, 0 }
  };

  G_OBJECT_CLASS (gd_main_icon_view_parent_class)->constructed (obj);

  gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
  gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (self), GTK_SELECTION_NONE);

  g_object_set (self,
                "column-spacing", VIEW_COLUMN_SPACING,
                "margin", VIEW_MARGIN,
                NULL);

  self->priv->pixbuf_cell = cell = ephy_removable_pixbuf_renderer_new ();
  g_object_set (cell,
                "xalign", 0.5,
                "yalign", 0.5,
                NULL);
  g_signal_connect (cell, "delete-clicked",
		    G_CALLBACK (on_cell_delete_clicked),
		    obj);

  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self), cell, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self), cell,
                                 "active", GD_MAIN_COLUMN_SELECTED);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self), cell,
                                 "pixbuf", GD_MAIN_COLUMN_ICON);

  cell = gd_two_lines_renderer_new ();
  g_object_set (cell,
                "alignment", PANGO_ALIGN_CENTER,
                "wrap-mode", PANGO_WRAP_WORD_CHAR,
                "wrap-width", VIEW_ITEM_WRAP_WIDTH,
                "text-lines", 3,
                NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self), cell, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self), cell,
                                 "text", GD_MAIN_COLUMN_TITLE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self), cell,
                                 "line-two", GD_MAIN_COLUMN_AUTHOR);

  gtk_icon_view_enable_model_drag_source (GTK_ICON_VIEW (self),
                                          GDK_BUTTON1_MASK,
                                          targets, 1,
                                          GDK_ACTION_COPY);
}

static void
gd_main_icon_view_class_init (GdMainIconViewClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);

  oclass->constructed = gd_main_icon_view_constructed;
  wclass->drag_data_get = gd_main_icon_view_drag_data_get;

  gtk_widget_class_install_style_property (wclass,
                                           g_param_spec_int ("check-icon-size",
                                                             "Check icon size",
                                                             "Check icon size",
                                                             -1, G_MAXINT, 40,
                                                             G_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (GdMainIconViewPrivate));
}

static void
gd_main_icon_view_init (GdMainIconView *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GD_TYPE_MAIN_ICON_VIEW, GdMainIconViewPrivate);
}

static GtkTreePath *
gd_main_icon_view_get_path_at_pos (GdMainViewGeneric *mv,
                                   gint x,
                                   gint y)
{
  return gtk_icon_view_get_path_at_pos (GTK_ICON_VIEW (mv), x, y);
}

static void
gd_main_icon_view_set_selection_mode (GdMainViewGeneric *mv,
                                      gboolean selection_mode)
{
  GdMainIconView *self = GD_MAIN_ICON_VIEW (mv);

  self->priv->selection_mode = selection_mode;

  g_object_set (self->priv->pixbuf_cell,
                "toggle-visible", selection_mode,
                NULL);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
gd_main_icon_view_scroll_to_path (GdMainViewGeneric *mv,
                                  GtkTreePath *path)
{
  gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (mv), path, TRUE, 0.5, 0.5);
}

static void
gd_main_icon_view_set_model (GdMainViewGeneric *mv,
                             GtkTreeModel *model)
{
  gtk_icon_view_set_model (GTK_ICON_VIEW (mv), model);
}

static void
gd_main_view_generic_iface_init (GdMainViewGenericIface *iface)
{
  iface->set_model = gd_main_icon_view_set_model;
  iface->get_path_at_pos = gd_main_icon_view_get_path_at_pos;
  iface->scroll_to_path = gd_main_icon_view_scroll_to_path;
  iface->set_selection_mode = gd_main_icon_view_set_selection_mode;
}

GtkWidget *
gd_main_icon_view_new (void)
{
  return g_object_new (GD_TYPE_MAIN_ICON_VIEW, NULL);
}
