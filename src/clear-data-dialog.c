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
#include "clear-data-dialog.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <webkit2/webkit2.h>

#include "ephy-history-service.h"
#include "ephy-embed-shell.h"

typedef enum {
  TIMESPAN_HOUR,
  TIMESPAN_DAY,
  TIMESPAN_WEEK,
  TIMESPAN_MONTH,
  TIMESPAN_FOREVER
} Timespan;

struct _ClearDataDialog {
  GtkDialog parent_instance;

  GtkWidget *clear_button;
  GtkWidget *treeview;
  GtkTreeModel *treestore;
  GtkTreeModelFilter *treemodelfilter;
  GtkWidget *timespan_combo;
  GtkWidget *search_entry;
  GtkSpinner *spinner;
  GtkStack *stack;

  Timespan timespan;
  GCancellable *cancellable;
};

enum {
  TYPE_COLUMN,
  ACTIVE_COLUMN,
  NAME_COLUMN,
  DATA_COLUMN,
  SENSITIVE_COLUMN
};

G_DEFINE_TYPE (ClearDataDialog, clear_data_dialog, GTK_TYPE_DIALOG)

#define PERSISTENT_DATA_TYPES WEBKIT_WEBSITE_DATA_DISK_CACHE | \
  WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE | \
  WEBKIT_WEBSITE_DATA_LOCAL_STORAGE | \
  WEBKIT_WEBSITE_DATA_WEBSQL_DATABASES | \
  WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES | \
  WEBKIT_WEBSITE_DATA_PLUGIN_DATA | \
  WEBKIT_WEBSITE_DATA_RESOURCE_LOAD_STATISTICS

typedef struct {
  WebKitWebsiteDataTypes type;
  gboolean initial_state;
  const char* name;
} DataEntry;

static const DataEntry data_entries[] = {
  { WEBKIT_WEBSITE_DATA_DISK_CACHE, TRUE, N_("HTTP disk cache") },
  { WEBKIT_WEBSITE_DATA_LOCAL_STORAGE, FALSE, N_("Local storage data") },
  { WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE, TRUE, N_("Offline web application cache") },
  { WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES, FALSE, N_("IndexedDB databases") },
  { WEBKIT_WEBSITE_DATA_WEBSQL_DATABASES, FALSE, N_("WebSQL databases") },
  { WEBKIT_WEBSITE_DATA_PLUGIN_DATA, FALSE, N_("Plugins data") },
  { WEBKIT_WEBSITE_DATA_RESOURCE_LOAD_STATISTICS, FALSE, N_("Resource load statistics") }
};

static WebKitWebsiteDataManager *
get_website_data_manger (void)
{
  WebKitWebContext *web_context;

  web_context = ephy_embed_shell_get_web_context (ephy_embed_shell_get_default ());
  return webkit_web_context_get_website_data_manager (web_context);
}

static inline GTimeSpan
get_timespan_for_combo_value (Timespan timespan)
{
  switch (timespan) {
  case TIMESPAN_HOUR:
    return G_TIME_SPAN_HOUR;
  case TIMESPAN_DAY:
    return G_TIME_SPAN_DAY;
  case TIMESPAN_WEEK:
    return G_TIME_SPAN_DAY * 7;
  case TIMESPAN_MONTH:
    return G_TIME_SPAN_DAY * 7 * 4;
  case TIMESPAN_FOREVER:
    return 0;
  default:
    break;
  }

  g_assert_not_reached ();
  return 0;
}

static void
website_data_fetched_cb (WebKitWebsiteDataManager *manager,
                         GAsyncResult             *result,
                         ClearDataDialog          *dialog)
{
  GList *data_list;
  GtkTreeStore *treestore;
  GError *error = NULL;

  data_list = webkit_website_data_manager_fetch_finish (manager, result, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    g_error_free (error);
    return;
  }

  gtk_spinner_stop (dialog->spinner);

  if (!data_list) {
    gtk_stack_set_visible_child_name (dialog->stack, "nodata");

    if (error)
      g_error_free (error);
    return;
  }

  gtk_widget_set_sensitive (dialog->clear_button, TRUE);
  gtk_stack_set_visible_child_name (dialog->stack, "view");

  treestore = GTK_TREE_STORE (dialog->treestore);
  for (guint i = 0; i < G_N_ELEMENTS (data_entries); i++) {
    GtkTreeIter parent_iter;
    gboolean empty = TRUE;

    gtk_tree_store_insert_with_values (treestore, &parent_iter, NULL, -1,
                                       TYPE_COLUMN, data_entries[i].type,
                                       ACTIVE_COLUMN, data_entries[i].initial_state,
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
                                         ACTIVE_COLUMN, data_entries[i].initial_state,
                                         NAME_COLUMN, webkit_website_data_get_name (data),
                                         DATA_COLUMN, webkit_website_data_ref (data),
                                         SENSITIVE_COLUMN, dialog->timespan == TIMESPAN_FOREVER,
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
clear_data_dialog_response_cb (GtkDialog       *widget,
                               int              response,
                               ClearDataDialog *dialog)
{
  GtkTreeIter top_iter;
  WebKitWebsiteDataTypes types_to_clear = 0;
  GList *data_to_remove = NULL;
  WebKitWebsiteDataTypes types_to_remove = 0;
  GTimeSpan timespan;

  if (response != GTK_RESPONSE_OK) {
    gtk_widget_destroy (GTK_WIDGET (dialog));
    return;
  }

  if (!gtk_tree_model_get_iter_first (dialog->treestore, &top_iter)) {
    gtk_widget_destroy (GTK_WIDGET (dialog));
    return;
  }

  timespan = get_timespan_for_combo_value (dialog->timespan);

  do {
    guint type;
    gboolean active;
    GtkTreeIter child_iter;

    gtk_tree_model_get (dialog->treestore, &top_iter,
                        TYPE_COLUMN, &type,
                        ACTIVE_COLUMN, &active,
                        -1);
    if (active && (timespan || all_children_visible (dialog->treestore, &top_iter, dialog->treemodelfilter))) {
      types_to_clear |= type;
    } else if (!timespan && gtk_tree_model_iter_children (dialog->treestore, &child_iter, &top_iter)) {
      gboolean empty = TRUE;

      do {
        WebKitWebsiteData *data;
        GtkTreeIter filter_iter;

        if (gtk_tree_model_filter_convert_child_iter_to_iter (dialog->treemodelfilter, &filter_iter, &child_iter)) {
          gtk_tree_model_get (dialog->treestore, &child_iter,
                              ACTIVE_COLUMN, &active,
                              DATA_COLUMN, &data,
                              -1);

          if (active) {
            data_to_remove = g_list_prepend (data_to_remove, data);
            empty = FALSE;
          } else
            webkit_website_data_unref (data);
        }
      } while (gtk_tree_model_iter_next (dialog->treestore, &child_iter));

      if (!empty)
        types_to_remove |= type;
    }
  } while (gtk_tree_model_iter_next (dialog->treestore, &top_iter));

  if (types_to_clear) {
    webkit_website_data_manager_clear (get_website_data_manger (),
                                       types_to_clear, timespan,
                                       NULL, NULL, NULL);
  }

  if (types_to_remove) {
    webkit_website_data_manager_remove (get_website_data_manger (),
                                        types_to_remove, data_to_remove,
                                        NULL, NULL, NULL);
  }

  g_list_free_full (data_to_remove, (GDestroyNotify)webkit_website_data_unref);
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
item_toggled_cb (GtkCellRendererToggle *renderer,
                 const char            *path_str,
                 ClearDataDialog       *dialog)
{
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  GtkTreeIter filter_iter, iter;
  gboolean active;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (dialog->treemodelfilter),
                           &filter_iter, path);
  gtk_tree_model_filter_convert_iter_to_child_iter (dialog->treemodelfilter,
                                                    &iter, &filter_iter);
  gtk_tree_model_get (dialog->treestore, &iter,
                      ACTIVE_COLUMN, &active,
                      -1);
  gtk_tree_store_set (GTK_TREE_STORE (dialog->treestore), &iter,
                      ACTIVE_COLUMN, !active,
                      -1);

  if (gtk_tree_model_iter_has_child (dialog->treestore, &iter)) {
    GtkTreeIter child_iter;

    gtk_tree_model_iter_children (dialog->treestore, &child_iter, &iter);
    do {
      gtk_tree_store_set (GTK_TREE_STORE (dialog->treestore), &child_iter,
                          ACTIVE_COLUMN, !active,
                          -1);
    } while (gtk_tree_model_iter_next (dialog->treestore, &child_iter));
  } else {
    GtkTreeIter parent_iter;

    /* Update the parent */
    gtk_tree_model_iter_parent (dialog->treestore, &parent_iter, &iter);
    if (active) {
      /* When unchecking a child we know the parent should be unchecked too */
      gtk_tree_store_set (GTK_TREE_STORE (dialog->treestore), &parent_iter,
                          ACTIVE_COLUMN, FALSE,
                          -1);
    } else {
      GtkTreeIter child_iter;
      gboolean all_active = TRUE;

      /* When checking a child, parent should be checked if all its children are */
      gtk_tree_model_iter_children (dialog->treestore, &child_iter, &parent_iter);
      do {
        gtk_tree_model_get (dialog->treestore, &child_iter,
                            ACTIVE_COLUMN, &all_active,
                            -1);
      } while (all_active && gtk_tree_model_iter_next (dialog->treestore, &child_iter));

      if (all_active) {
        gtk_tree_store_set (GTK_TREE_STORE (dialog->treestore), &parent_iter,
                            ACTIVE_COLUMN, TRUE,
                            -1);
      }
    }
  }

  gtk_tree_path_free (path);
}

static gboolean
update_item_sensitivity (GtkTreeModel    *model,
                         GtkTreePath     *path,
                         GtkTreeIter     *iter,
                         ClearDataDialog *dialog)
{
  if (!gtk_tree_model_iter_has_child (model, iter)) {
    gtk_tree_store_set (GTK_TREE_STORE (model), iter,
                        SENSITIVE_COLUMN, dialog->timespan == TIMESPAN_FOREVER,
                        -1);
  }

  return FALSE;
}

static void
timespan_combo_changed_cb (GtkComboBox     *combo,
                           ClearDataDialog *dialog)
{
  gint active;
  gboolean was_forever;

  active = gtk_combo_box_get_active (combo);
  was_forever = dialog->timespan == TIMESPAN_FOREVER;
  dialog->timespan = active;
  if (active == TIMESPAN_FOREVER || was_forever) {
    gtk_tree_model_foreach (dialog->treestore,
                            (GtkTreeModelForeachFunc)update_item_sensitivity,
                            dialog);
  }
}

static void
search_entry_changed_cb (GtkSearchEntry  *entry,
                         ClearDataDialog *dialog)
{
  gtk_tree_model_filter_refilter (dialog->treemodelfilter);
}

static gboolean
row_visible_func (GtkTreeModel    *model,
                  GtkTreeIter     *iter,
                  ClearDataDialog *dialog)
{
  const char *search_text;
  char *name;
  gboolean visible;

  if (gtk_tree_model_iter_has_child (model, iter))
    return TRUE;

  search_text = gtk_entry_get_text (GTK_ENTRY (dialog->search_entry));
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
    gtk_tree_view_expand_row (GTK_TREE_VIEW (dialog->treeview), path, FALSE);
    gtk_tree_path_free (path);
  }

  return visible;
}

static void
clear_data_dialog_dispose (GObject *object)
{
  ClearDataDialog *dialog = (ClearDataDialog *)object;

  g_cancellable_cancel (dialog->cancellable);
  g_clear_object (&dialog->cancellable);

  G_OBJECT_CLASS (clear_data_dialog_parent_class)->dispose (object);
}

static void
clear_data_dialog_class_init (ClearDataDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = clear_data_dialog_dispose;

  g_type_ensure (WEBKIT_TYPE_WEBSITE_DATA);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/clear-data-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, ClearDataDialog, clear_button);
  gtk_widget_class_bind_template_child (widget_class, ClearDataDialog, treeview);
  gtk_widget_class_bind_template_child (widget_class, ClearDataDialog, treestore);
  gtk_widget_class_bind_template_child (widget_class, ClearDataDialog, treemodelfilter);
  gtk_widget_class_bind_template_child (widget_class, ClearDataDialog, timespan_combo);
  gtk_widget_class_bind_template_child (widget_class, ClearDataDialog, search_entry);
  gtk_widget_class_bind_template_child (widget_class, ClearDataDialog, spinner);
  gtk_widget_class_bind_template_child (widget_class, ClearDataDialog, stack);
  gtk_widget_class_bind_template_callback (widget_class, item_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, timespan_combo_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, clear_data_dialog_response_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_entry_changed_cb);
}

static void
clear_data_dialog_init (ClearDataDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));

  gtk_tree_model_filter_set_visible_func (dialog->treemodelfilter,
                                          (GtkTreeModelFilterVisibleFunc)row_visible_func,
                                          dialog,
                                          NULL);

  gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->timespan_combo),
                            dialog->timespan);

  gtk_widget_set_sensitive (dialog->clear_button, FALSE);
  gtk_spinner_start (dialog->spinner);
  gtk_stack_set_visible_child_name (dialog->stack, "spinner");

  dialog->cancellable = g_cancellable_new ();

  webkit_website_data_manager_fetch (get_website_data_manger (),
                                     PERSISTENT_DATA_TYPES,
                                     dialog->cancellable,
                                     (GAsyncReadyCallback)website_data_fetched_cb,
                                     dialog);
}
