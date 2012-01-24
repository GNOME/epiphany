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

#include "gd-main-view.h"
#include "gd-main-view-generic.h"

typedef GdMainViewGenericIface GdMainViewGenericInterface;
G_DEFINE_INTERFACE (GdMainViewGeneric, gd_main_view_generic, GTK_TYPE_WIDGET)

static void
gd_main_view_generic_default_init (GdMainViewGenericInterface *iface)
{
  /* nothing */
}

/**
 * gd_main_view_generic_set_model:
 * @self:
 * @model: (allow-none):
 *
 */
void
gd_main_view_generic_set_model (GdMainViewGeneric *self,
                                GtkTreeModel *model)
{
  GdMainViewGenericInterface *iface;

  iface = GD_MAIN_VIEW_GENERIC_GET_IFACE (self);

  (* iface->set_model) (self, model);
}

GtkTreePath *
gd_main_view_generic_get_path_at_pos (GdMainViewGeneric *self,
                                      gint x,
                                      gint y)
{
  GdMainViewGenericInterface *iface;

  iface = GD_MAIN_VIEW_GENERIC_GET_IFACE (self);

  return (* iface->get_path_at_pos) (self, x, y);
}

void
gd_main_view_generic_set_selection_mode (GdMainViewGeneric *self,
                                         gboolean selection_mode)
{
  GdMainViewGenericInterface *iface;

  iface = GD_MAIN_VIEW_GENERIC_GET_IFACE (self);

  (* iface->set_selection_mode) (self, selection_mode);
}

void
gd_main_view_generic_scroll_to_path (GdMainViewGeneric *self,
                                     GtkTreePath *path)
{
  GdMainViewGenericInterface *iface;

  iface = GD_MAIN_VIEW_GENERIC_GET_IFACE (self);

  (* iface->scroll_to_path) (self, path);
}

static gboolean
build_selection_uris_foreach (GtkTreeModel *model,
                              GtkTreePath *path,
                              GtkTreeIter *iter,
                              gpointer user_data)
{
  GPtrArray *ptr_array = user_data;
  gchar *uri;
  gboolean is_selected;

  gtk_tree_model_get (model, iter,
                      GD_MAIN_COLUMN_URI, &uri,
                      GD_MAIN_COLUMN_SELECTED, &is_selected,
                      -1);

  if (is_selected)
    g_ptr_array_add (ptr_array, uri);
  else
    g_free (uri);

  return FALSE;
}

static gchar **
model_get_selection_uris (GtkTreeModel *model)
{
  GPtrArray *ptr_array = g_ptr_array_new ();

  gtk_tree_model_foreach (model,
                          build_selection_uris_foreach,
                          ptr_array);
  
  g_ptr_array_add (ptr_array, NULL);
  return (gchar **) g_ptr_array_free (ptr_array, FALSE);
}

void
_gd_main_view_generic_dnd_common (GtkTreeModel *model,
                                  gboolean selection_mode,
                                  GtkTreePath *path,
                                  GtkSelectionData *data)
{
  gchar **uris;

  if (selection_mode)
    {
      uris = model_get_selection_uris (model);
    }
  else
    {
      GtkTreeIter iter;
      gboolean res;
      gchar *uri = NULL;

      if (path != NULL)
        {
          res = gtk_tree_model_get_iter (model, &iter, path);
          if (res)
            gtk_tree_model_get (model, &iter,
                                GD_MAIN_COLUMN_URI, &uri,
                                -1);
        }

      uris = g_new0 (gchar *, 2);
      uris[0] = uri;
      uris[1] = NULL;
    }

  gtk_selection_data_set_uris (data, uris);
  g_strfreev (uris);
}
