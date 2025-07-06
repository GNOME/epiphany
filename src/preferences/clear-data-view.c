/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2013 Red Hat, Inc.
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
#include "clear-data-view.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit/webkit.h>

#include "ephy-history-service.h"
#include "ephy-embed-shell.h"
#include "ephy-settings.h"

struct _ClearDataView {
  EphyDataView parent_instance;

  GtkWidget *treeview;
  GtkTreeModel *treestore;
  GtkTreeModelFilter *treemodelfilter;
  GtkTreeViewColumn *active_column;
  GtkTreeViewColumn *name_column;

  GCancellable *cancellable;
};

enum {
  TYPE_COLUMN,
  ACTIVE_COLUMN,
  NAME_COLUMN,
  DATA_COLUMN,
  SENSITIVE_COLUMN
};

G_DEFINE_FINAL_TYPE (ClearDataView, clear_data_view, EPHY_TYPE_DATA_VIEW)

#define PERSISTENT_DATA_TYPES WEBKIT_WEBSITE_DATA_COOKIES | \
        WEBKIT_WEBSITE_DATA_DISK_CACHE | \
        WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE | \
        WEBKIT_WEBSITE_DATA_LOCAL_STORAGE | \
        WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES | \
        WEBKIT_WEBSITE_DATA_HSTS_CACHE | \
        WEBKIT_WEBSITE_DATA_ITP

typedef struct {
  guint id;
  WebKitWebsiteDataTypes type;
  const char *name;
} DataEntry;

static const DataEntry data_entries[] = {
  { 0x001, WEBKIT_WEBSITE_DATA_COOKIES, N_("Cookies") },
  { 0x002, WEBKIT_WEBSITE_DATA_DISK_CACHE, N_("HTTP disk cache") },
  { 0x004, WEBKIT_WEBSITE_DATA_LOCAL_STORAGE, N_("Local storage data") },
  { 0x008, WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE, N_("Offline web application cache") },
  { 0x010, WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES, N_("IndexedDB databases") },
  { 0x020, WEBKIT_WEBSITE_DATA_HSTS_CACHE, N_("HSTS policies cache") },
  { 0x040, WEBKIT_WEBSITE_DATA_ITP, N_("Intelligent Tracking Prevention data") }
};

static WebKitWebsiteDataManager *
get_website_data_manager (void)
{
  WebKitNetworkSession *network_session;

  network_session = ephy_embed_shell_get_network_session (ephy_embed_shell_get_default ());
  return webkit_network_session_get_website_data_manager (network_session);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static void
website_data_fetched_cb (WebKitWebsiteDataManager *manager,
                         GAsyncResult             *result,
                         ClearDataView            *clear_data_view)
{
  GList *data_list;
  GtkTreeStore *treestore;
  GError *error = NULL;
  int active_items;

  data_list = webkit_website_data_manager_fetch_finish (manager, result, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    g_error_free (error);
    return;
  }

  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (clear_data_view), FALSE);

  if (!data_list) {
    ephy_data_view_set_has_data (EPHY_DATA_VIEW (clear_data_view), FALSE);

    if (error)
      g_error_free (error);
    return;
  }

  ephy_data_view_set_has_data (EPHY_DATA_VIEW (clear_data_view), TRUE);
  active_items = g_settings_get_int (EPHY_SETTINGS_MAIN, EPHY_PREFS_ACTIVE_CLEAR_DATA_ITEMS);

  treestore = GTK_TREE_STORE (clear_data_view->treestore);
  for (guint i = 0; i < G_N_ELEMENTS (data_entries); i++) {
    GtkTreeIter parent_iter;
    gboolean empty = TRUE;

    gtk_tree_store_insert_with_values (treestore, &parent_iter, NULL, -1,
                                       TYPE_COLUMN, data_entries[i].type,
                                       ACTIVE_COLUMN, active_items & data_entries[i].id,
                                       NAME_COLUMN, _(data_entries[i].name),
                                       DATA_COLUMN, NULL,
                                       SENSITIVE_COLUMN, TRUE,
                                       -1);
    for (GList *l = data_list; l && l->data; l = g_list_next (l)) {
      WebKitWebsiteData *data = (WebKitWebsiteData *)l->data;

      if (!(webkit_website_data_get_types (data) & data_entries[i].type))
        continue;

      gtk_tree_store_insert_with_values (treestore, NULL, &parent_iter, -1,
                                         TYPE_COLUMN, data_entries[i].type,
                                         ACTIVE_COLUMN, active_items & data_entries[i].id,
                                         NAME_COLUMN, webkit_website_data_get_name (data),
                                         DATA_COLUMN, webkit_website_data_ref (data),
                                         SENSITIVE_COLUMN, TRUE,
                                         -1);
      empty = FALSE;
    }

    if (empty)
      gtk_tree_store_remove (treestore, &parent_iter);
  }

  g_list_free_full (data_list, (GDestroyNotify)webkit_website_data_unref);
}

static gboolean
all_children_visible (GtkTreeModel       *model,
                      GtkTreeIter        *child_iter,
                      GtkTreeModelFilter *filter)
{
  GtkTreeIter filter_iter;

  gtk_tree_model_filter_convert_child_iter_to_iter (filter, &filter_iter, child_iter);
  return gtk_tree_model_iter_n_children (model, child_iter) == gtk_tree_model_iter_n_children (GTK_TREE_MODEL (filter), &filter_iter);
}

static void
on_clear_button_clicked (ClearDataView *clear_data_view)
{
  GtkTreeIter top_iter;
  WebKitWebsiteDataTypes types_to_clear = 0;
  GList *data_to_remove = NULL;
  WebKitWebsiteDataTypes types_to_remove = 0;

  if (!gtk_tree_model_get_iter_first (clear_data_view->treestore, &top_iter))
    return;

  do {
    guint type;
    gboolean active;
    GtkTreeIter child_iter;

    gtk_tree_model_get (clear_data_view->treestore, &top_iter,
                        TYPE_COLUMN, &type,
                        ACTIVE_COLUMN, &active,
                        -1);
    if (active && all_children_visible (clear_data_view->treestore, &top_iter, clear_data_view->treemodelfilter)) {
      types_to_clear |= type;
    } else if (gtk_tree_model_iter_children (clear_data_view->treestore, &child_iter, &top_iter)) {
      gboolean empty = TRUE;

      do {
        WebKitWebsiteData *data;
        GtkTreeIter filter_iter;

        if (gtk_tree_model_filter_convert_child_iter_to_iter (clear_data_view->treemodelfilter, &filter_iter, &child_iter)) {
          gtk_tree_model_get (clear_data_view->treestore, &child_iter,
                              ACTIVE_COLUMN, &active,
                              DATA_COLUMN, &data,
                              -1);

          if (active) {
            data_to_remove = g_list_prepend (data_to_remove, data);
            empty = FALSE;
          } else
            webkit_website_data_unref (data);
        }
      } while (gtk_tree_model_iter_next (clear_data_view->treestore, &child_iter));

      if (!empty)
        types_to_remove |= type;
    }
  } while (gtk_tree_model_iter_next (clear_data_view->treestore, &top_iter));

  if (types_to_clear) {
    webkit_website_data_manager_clear (get_website_data_manager (),
                                       types_to_clear, 0,
                                       NULL, NULL, NULL);
  }

  if (types_to_remove) {
    webkit_website_data_manager_remove (get_website_data_manager (),
                                        types_to_remove, data_to_remove,
                                        NULL, NULL, NULL);
  }

  if (types_to_clear || types_to_remove) {
    AdwToastOverlay *toast_overlay = ephy_data_view_get_toast_overlay (EPHY_DATA_VIEW (clear_data_view));
    AdwToast *toast = adw_toast_new (_("Data cleared"));

    adw_toast_overlay_add_toast (toast_overlay, toast);
  }

  g_list_free_full (data_to_remove, (GDestroyNotify)webkit_website_data_unref);

  /* Reload tree */
  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (clear_data_view), TRUE);
  gtk_tree_store_clear (GTK_TREE_STORE (clear_data_view->treestore));
  webkit_website_data_manager_fetch (get_website_data_manager (),
                                     PERSISTENT_DATA_TYPES,
                                     clear_data_view->cancellable,
                                     (GAsyncReadyCallback)website_data_fetched_cb,
                                     clear_data_view);
}

static gboolean
any_item_checked (ClearDataView *self)
{
  GtkTreeIter top_iter;

  if (!gtk_tree_model_get_iter_first (self->treestore, &top_iter))
    return FALSE;

  do {
    gboolean active;
    GtkTreeIter child_iter;

    gtk_tree_model_get (self->treestore, &top_iter,
                        ACTIVE_COLUMN, &active, -1);
    if (active) {
      return TRUE;
    } else if (gtk_tree_model_iter_children (self->treestore, &child_iter, &top_iter)) {
      do {
        GtkTreeIter filter_iter;

        if (gtk_tree_model_filter_convert_child_iter_to_iter (self->treemodelfilter, &filter_iter, &child_iter)) {
          gtk_tree_model_get (self->treestore, &child_iter,
                              ACTIVE_COLUMN, &active, -1);
          if (active)
            return TRUE;
        }
      } while (gtk_tree_model_iter_next (self->treestore, &child_iter));
    }
  } while (gtk_tree_model_iter_next (self->treestore, &top_iter));

  return FALSE;
}

static void
item_toggled_cb (GtkCellRendererToggle *renderer,
                 const char            *path_str,
                 ClearDataView         *clear_data_view)
{
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  GtkTreeIter filter_iter, iter;
  gboolean active;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (clear_data_view->treemodelfilter),
                           &filter_iter, path);
  gtk_tree_model_filter_convert_iter_to_child_iter (clear_data_view->treemodelfilter,
                                                    &iter, &filter_iter);
  gtk_tree_model_get (clear_data_view->treestore, &iter,
                      ACTIVE_COLUMN, &active,
                      -1);
  gtk_tree_store_set (GTK_TREE_STORE (clear_data_view->treestore), &iter,
                      ACTIVE_COLUMN, !active,
                      -1);

  if (gtk_tree_model_iter_has_child (clear_data_view->treestore, &iter)) {
    GtkTreeIter child_iter;
    g_autofree char *name = NULL;
    int active_items;

    active_items = g_settings_get_int (EPHY_SETTINGS_MAIN, EPHY_PREFS_ACTIVE_CLEAR_DATA_ITEMS);

    gtk_tree_model_get (clear_data_view->treestore, &iter,
                        NAME_COLUMN, &name,
                        -1);
    for (guint i = 0; i < G_N_ELEMENTS (data_entries); i++) {
      if (g_strcmp0 (gettext (data_entries[i].name), name) == 0) {
        if (active)
          active_items &= ~data_entries[i].id;
        else
          active_items |= data_entries[i].id;

        break;
      }
    }

    g_settings_set_int (EPHY_SETTINGS_MAIN, EPHY_PREFS_ACTIVE_CLEAR_DATA_ITEMS, active_items);

    gtk_tree_model_iter_children (clear_data_view->treestore, &child_iter, &iter);
    do {
      gtk_tree_store_set (GTK_TREE_STORE (clear_data_view->treestore), &child_iter,
                          ACTIVE_COLUMN, !active,
                          -1);
    } while (gtk_tree_model_iter_next (clear_data_view->treestore, &child_iter));
  } else {
    GtkTreeIter parent_iter;

    /* Update the parent */
    gtk_tree_model_iter_parent (clear_data_view->treestore, &parent_iter, &iter);
    if (active) {
      /* When unchecking a child we know the parent should be unchecked too */
      gtk_tree_store_set (GTK_TREE_STORE (clear_data_view->treestore), &parent_iter,
                          ACTIVE_COLUMN, FALSE,
                          -1);
    } else {
      GtkTreeIter child_iter;
      gboolean all_active = TRUE;

      /* When checking a child, parent should be checked if all its children are */
      gtk_tree_model_iter_children (clear_data_view->treestore, &child_iter, &parent_iter);
      do {
        gtk_tree_model_get (clear_data_view->treestore, &child_iter,
                            ACTIVE_COLUMN, &all_active,
                            -1);
      } while (all_active && gtk_tree_model_iter_next (clear_data_view->treestore, &child_iter));

      if (all_active) {
        gtk_tree_store_set (GTK_TREE_STORE (clear_data_view->treestore), &parent_iter,
                            ACTIVE_COLUMN, TRUE,
                            -1);
      }
    }
  }

  gtk_tree_path_free (path);

  ephy_data_view_set_can_clear (EPHY_DATA_VIEW (clear_data_view), any_item_checked (clear_data_view));
}

static void
search_text_changed_cb (ClearDataView *clear_data_view)
{
  gtk_tree_model_filter_refilter (clear_data_view->treemodelfilter);
}

static gboolean
row_visible_func (GtkTreeModel  *model,
                  GtkTreeIter   *iter,
                  ClearDataView *clear_data_view)
{
  const char *search_text;
  char *name;
  gboolean visible;

  if (gtk_tree_model_iter_has_child (model, iter))
    return TRUE;

  search_text = ephy_data_view_get_search_text (EPHY_DATA_VIEW (clear_data_view));
  if (!search_text || search_text[0] == '\0')
    return TRUE;

  gtk_tree_model_get (model, iter,
                      NAME_COLUMN, &name,
                      -1);

  visible = name && strstr (name, search_text);
  g_free (name);

  if (visible) {
    GtkTreeIter parent_iter;
    GtkTreePath *path;

    gtk_tree_model_iter_parent (model, &parent_iter, iter);
    path = gtk_tree_model_get_path (model, &parent_iter);
    gtk_tree_view_expand_row (GTK_TREE_VIEW (clear_data_view->treeview), path, FALSE);
    gtk_tree_path_free (path);
  }

  return visible;
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void
clear_data_view_dispose (GObject *object)
{
  ClearDataView *clear_data_view = (ClearDataView *)object;

  if (clear_data_view->cancellable) {
    g_cancellable_cancel (clear_data_view->cancellable);
    g_clear_object (&clear_data_view->cancellable);
  }

  G_OBJECT_CLASS (clear_data_view_parent_class)->dispose (object);
}

static void
clear_data_view_class_init (ClearDataViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = clear_data_view_dispose;

  g_type_ensure (WEBKIT_TYPE_WEBSITE_DATA);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/clear-data-view.ui");

  gtk_widget_class_bind_template_child (widget_class, ClearDataView, treeview);
  gtk_widget_class_bind_template_child (widget_class, ClearDataView, active_column);
  gtk_widget_class_bind_template_child (widget_class, ClearDataView, name_column);

  gtk_widget_class_bind_template_callback (widget_class, item_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_clear_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, search_text_changed_cb);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static void
clear_data_view_init (ClearDataView *clear_data_view)
{
  GtkCellRenderer *cell_renderer;

  gtk_widget_init_template (GTK_WIDGET (clear_data_view));

  cell_renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (cell_renderer, "toggled", G_CALLBACK (item_toggled_cb), clear_data_view);
  gtk_tree_view_column_pack_start (clear_data_view->active_column, cell_renderer, TRUE);
  gtk_tree_view_column_set_attributes (clear_data_view->active_column, cell_renderer, "active", 1, "sensitive", 4, NULL);

  cell_renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (clear_data_view->name_column, cell_renderer, TRUE);
  gtk_tree_view_column_set_attributes (clear_data_view->name_column, cell_renderer, "text", 2, "sensitive", 4, NULL);

  clear_data_view->treestore = GTK_TREE_MODEL (gtk_tree_store_new (5, G_TYPE_UINT, G_TYPE_BOOLEAN, G_TYPE_STRING, WEBKIT_TYPE_WEBSITE_DATA, G_TYPE_BOOLEAN));
  clear_data_view->treemodelfilter = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (clear_data_view->treestore, NULL));
  gtk_tree_view_set_model (GTK_TREE_VIEW (clear_data_view->treeview), GTK_TREE_MODEL (clear_data_view->treemodelfilter));

  gtk_tree_model_filter_set_visible_func (clear_data_view->treemodelfilter,
                                          (GtkTreeModelFilterVisibleFunc)row_visible_func,
                                          clear_data_view,
                                          NULL);

  ephy_data_view_set_is_loading (EPHY_DATA_VIEW (clear_data_view), TRUE);

  clear_data_view->cancellable = g_cancellable_new ();

  webkit_website_data_manager_fetch (get_website_data_manager (),
                                     PERSISTENT_DATA_TYPES,
                                     clear_data_view->cancellable,
                                     (GAsyncReadyCallback)website_data_fetched_cb,
                                     clear_data_view);
}
G_GNUC_END_IGNORE_DEPRECATIONS
