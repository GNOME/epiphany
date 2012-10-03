/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2012 Igalia S.L.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-overview.h"

#include "ephy-embed-private.h"
#include "ephy-embed-shell.h"
#include "ephy-frecent-store.h"

#include <gtk/gtk.h>

#define EPHY_OVERVIEW_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_OVERVIEW, EphyOverviewPrivate))

struct _EphyOverviewPrivate
{
  GtkWidget *frecent_view;
};

G_DEFINE_TYPE (EphyOverview, ephy_overview, GTK_TYPE_GRID)

static gboolean
frecent_view_item_deleted (GtkWidget *widget,
                           gchar *path,
                           gpointer data)
{
  EphyFrecentStore *store;
  GtkTreeIter iter;
  GtkTreePath *tree_path;

  store = EPHY_FRECENT_STORE (gd_main_view_get_model (GD_MAIN_VIEW (widget)));
  tree_path = gtk_tree_path_new_from_string (path);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, tree_path);
  ephy_frecent_store_set_hidden (store, &iter);
  gtk_tree_path_free (tree_path);

  return TRUE;
}

static void
main_view_item_activated (GtkWidget *widget,
                          gchar *id,
                          GtkTreePath *path,
                          EphyOverview *overview)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  char *url;

  model = gd_main_view_get_model (GD_MAIN_VIEW (widget));
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter,
                      EPHY_OVERVIEW_STORE_URI, &url,
                      -1);
  g_signal_emit_by_name (overview, "open-link", url);
  g_free (url);
}

static gboolean
iconview_motion_notify (GtkWidget *widget,
                        GdkEvent *event,
                        EphyOverview *overview)
{
  GdkCursor *cursor;
  GtkIconView *iconview = GTK_ICON_VIEW (widget);
  GdkEventMotion *ev = (GdkEventMotion *)event;

  if (gtk_icon_view_get_item_at_pos (iconview, ev->x, ev->y, NULL, NULL)) {
    cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget), GDK_HAND2);
    gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
    g_object_unref (cursor);
  } else
    gdk_window_set_cursor (gtk_widget_get_window (widget), NULL);

  return FALSE;
}

static void
ephy_overview_constructed (GObject *object)
{
  EphyOverviewStore *store;
  EphyOverview *self = EPHY_OVERVIEW (object);
  GtkWidget *iconview;

  if (G_OBJECT_CLASS (ephy_overview_parent_class)->constructed)
    G_OBJECT_CLASS (ephy_overview_parent_class)->constructed (object);

  self->priv->frecent_view = GTK_WIDGET (gd_main_view_new (GD_MAIN_VIEW_ICON));
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (self->priv->frecent_view),
                                       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->priv->frecent_view),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  iconview = gtk_bin_get_child (GTK_BIN (self->priv->frecent_view));
  gtk_icon_view_set_columns (GTK_ICON_VIEW (iconview), 5);
  g_signal_connect (iconview, "motion-notify-event",
                    G_CALLBACK (iconview_motion_notify),
                    self);
  g_object_set (self->priv->frecent_view,
                "halign", GTK_ALIGN_FILL,
                "valign", GTK_ALIGN_FILL, NULL);
  g_object_set (iconview,
                "halign", GTK_ALIGN_CENTER,
                "valign", GTK_ALIGN_CENTER, NULL);

  g_signal_connect (self->priv->frecent_view, "item-activated",
                    G_CALLBACK (main_view_item_activated), object);
  g_signal_connect (self->priv->frecent_view, "item-deleted",
                    G_CALLBACK (frecent_view_item_deleted), NULL);

  store = EPHY_OVERVIEW_STORE (ephy_embed_shell_get_frecent_store (ephy_embed_shell_get_default ()));
  gd_main_view_set_model (GD_MAIN_VIEW (self->priv->frecent_view),
                          GTK_TREE_MODEL (store));
  gtk_grid_attach (GTK_GRID (self), self->priv->frecent_view,
                   0, 0, 1, 1);

  gtk_widget_show_all (GTK_WIDGET (self));
}

static void
ephy_overview_class_init (EphyOverviewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed  = ephy_overview_constructed;

  g_signal_new ("open-link",
                EPHY_TYPE_OVERVIEW,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL,
                g_cclosure_marshal_generic,
                G_TYPE_NONE,
                1,
                G_TYPE_STRING);

  g_type_class_add_private (object_class, sizeof (EphyOverviewPrivate));
}

static void
ephy_overview_init (EphyOverview *self)
{
  self->priv = EPHY_OVERVIEW_GET_PRIVATE (self);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_VERTICAL);
}

GtkWidget *
ephy_overview_new (void)
{
  return g_object_new (EPHY_TYPE_OVERVIEW,
                       NULL);
}
