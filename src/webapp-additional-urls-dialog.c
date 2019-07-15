/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2018 Igalia S.L.
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
#include "webapp-additional-urls-dialog.h"

#include "ephy-settings.h"

struct _EphyWebappAdditionalURLsDialog {
  GtkDialog parent_instance;

  GtkWidget *treeview;
  GtkTreeViewColumn *url_column;
  GtkTreeSelection *tree_selection;
  GtkTreeModel *liststore;

  GActionGroup *action_group;
};

G_DEFINE_TYPE (EphyWebappAdditionalURLsDialog, ephy_webapp_additional_urls_dialog, GTK_TYPE_DIALOG)

static gboolean
add_to_builder (GtkTreeModel    *model,
                GtkTreePath     *path,
                GtkTreeIter     *iter,
                GVariantBuilder *builder)
{
  char *url;

  gtk_tree_model_get (model, iter, 0, &url, -1);
  if (url && url[0] != '\0')
    g_variant_builder_add (builder, "s", url);
  g_free (url);

  return FALSE;
}

static void
ephy_webapp_additional_urls_update_settings (EphyWebappAdditionalURLsDialog *dialog)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);
  gtk_tree_model_foreach (dialog->liststore,
                          (GtkTreeModelForeachFunc)add_to_builder,
                          &builder);
  g_settings_set (EPHY_SETTINGS_WEB_APP,
                  EPHY_PREFS_WEB_APP_ADDITIONAL_URLS,
                  "as", &builder);
}

static void
on_cell_edited (GtkCellRendererText            *cell,
                const gchar                    *path_string,
                const gchar                    *new_text,
                EphyWebappAdditionalURLsDialog *dialog)
{
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;

  gtk_tree_model_get_iter (dialog->liststore, &iter, path);
  gtk_tree_path_free (path);

  if (!new_text || new_text[0] == '\0')
    gtk_list_store_remove (GTK_LIST_STORE (dialog->liststore), &iter);
  else
    gtk_list_store_set (GTK_LIST_STORE (dialog->liststore), &iter, 0, new_text, -1);

  ephy_webapp_additional_urls_update_settings (dialog);
}

static void
update_selection_actions (GActionMap *action_map,
                          gboolean    has_selection)
{
  GAction *action;

  action = g_action_map_lookup_action (action_map, "forget");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), has_selection);
}

static void
on_treeview_selection_changed (GtkTreeSelection               *selection,
                               EphyWebappAdditionalURLsDialog *dialog)
{
  update_selection_actions (G_ACTION_MAP (dialog->action_group),
                            gtk_tree_selection_count_selected_rows (selection) > 0);
}

static void
ephy_webapp_additional_urls_dialog_class_init (EphyWebappAdditionalURLsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/webapp-additional-urls-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyWebappAdditionalURLsDialog, liststore);
  gtk_widget_class_bind_template_child (widget_class, EphyWebappAdditionalURLsDialog, treeview);
  gtk_widget_class_bind_template_child (widget_class, EphyWebappAdditionalURLsDialog, url_column);
  gtk_widget_class_bind_template_child (widget_class, EphyWebappAdditionalURLsDialog, tree_selection);

  gtk_widget_class_bind_template_callback (widget_class, on_treeview_selection_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_cell_edited);
}

static void
add_new (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       user_data)
{
  EphyWebappAdditionalURLsDialog *dialog = EPHY_WEBAPP_ADDITIONAL_URLS_DIALOG (user_data);
  GtkTreeIter iter;
  GtkTreePath *path;

  gtk_list_store_insert_with_values (GTK_LIST_STORE (dialog->liststore), &iter, 0,
                                     0, "",
                                     -1);
  path = gtk_tree_model_get_path (dialog->liststore, &iter);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (dialog->treeview), path, dialog->url_column, TRUE);
  gtk_tree_path_free (path);
}

static void
forget (GSimpleAction *action,
        GVariant      *parameter,
        gpointer       user_data)
{
  EphyWebappAdditionalURLsDialog *dialog = EPHY_WEBAPP_ADDITIONAL_URLS_DIALOG (user_data);
  GList *llist, *rlist = NULL, *l, *r;
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter, iter2;
  GtkTreeRowReference *row_ref = NULL;

  llist = gtk_tree_selection_get_selected_rows (dialog->tree_selection, &model);
  if (llist == NULL)
    return;

  for (l = llist; l != NULL; l = l->next) {
    rlist = g_list_prepend (rlist, gtk_tree_row_reference_new (model, (GtkTreePath *)l->data));
  }

  /* Intelligent selection logic, no actual selection yet */

  path = gtk_tree_row_reference_get_path ((GtkTreeRowReference *)g_list_first (rlist)->data);

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);
  iter2 = iter;

  if (gtk_tree_model_iter_next (model, &iter)) {
    path = gtk_tree_model_get_path (model, &iter);
    row_ref = gtk_tree_row_reference_new (model, path);
  } else {
    path = gtk_tree_model_get_path (model, &iter2);
    if (gtk_tree_path_prev (path)) {
      row_ref = gtk_tree_row_reference_new (model, path);
    }
  }
  gtk_tree_path_free (path);

  /* Removal */
  for (r = rlist; r != NULL; r = r->next) {
    path = gtk_tree_row_reference_get_path ((GtkTreeRowReference *)r->data);
    gtk_tree_model_get_iter (model, &iter, path);
    gtk_list_store_remove (GTK_LIST_STORE (dialog->liststore), &iter);
    gtk_tree_row_reference_free ((GtkTreeRowReference *)r->data);
    gtk_tree_path_free (path);
  }
  ephy_webapp_additional_urls_update_settings (dialog);

  g_list_free_full (llist, (GDestroyNotify)gtk_tree_path_free);
  g_list_free (rlist);

  /* Selection */
  if (row_ref != NULL) {
    path = gtk_tree_row_reference_get_path (row_ref);

    if (path != NULL) {
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (dialog->treeview), path, NULL, FALSE);
      gtk_tree_path_free (path);
    }

    gtk_tree_row_reference_free (row_ref);
  }
}

static void
forget_all (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       user_data)
{
  EphyWebappAdditionalURLsDialog *dialog = EPHY_WEBAPP_ADDITIONAL_URLS_DIALOG (user_data);

  gtk_list_store_clear (GTK_LIST_STORE (dialog->liststore));
  g_settings_set_strv (EPHY_SETTINGS_WEB_APP, EPHY_PREFS_WEB_APP_ADDITIONAL_URLS, NULL);
}

static GActionGroup *
create_action_group (EphyWebappAdditionalURLsDialog *dialog)
{
  const GActionEntry entries[] = {
    { "new", add_new },
    { "forget", forget },
    { "forget-all", forget_all },
  };
  GSimpleActionGroup *group;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), dialog);

  return G_ACTION_GROUP (group);
}

static void
show_dialog_cb (GtkWidget *widget,
                gpointer   user_data)
{
  EphyWebappAdditionalURLsDialog *dialog = EPHY_WEBAPP_ADDITIONAL_URLS_DIALOG (widget);
  char **urls;
  guint i;

  urls = g_settings_get_strv (EPHY_SETTINGS_WEB_APP, EPHY_PREFS_WEB_APP_ADDITIONAL_URLS);
  for (i = 0; urls[i]; i++) {
    gtk_list_store_insert_with_values (GTK_LIST_STORE (dialog->liststore), NULL, -1,
                                       0, urls[i],
                                       -1);
  }
  g_strfreev (urls);
}

static void
ephy_webapp_additional_urls_dialog_init (EphyWebappAdditionalURLsDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));

  dialog->action_group = create_action_group (dialog);
  gtk_widget_insert_action_group (GTK_WIDGET (dialog), "webapp-additional-urls", dialog->action_group);

  update_selection_actions (G_ACTION_MAP (dialog->action_group), FALSE);

  g_signal_connect (GTK_WIDGET (dialog), "show", G_CALLBACK (show_dialog_cb), NULL);
}

EphyWebappAdditionalURLsDialog *
ephy_webapp_additional_urls_dialog_new (void)
{
  return EPHY_WEBAPP_ADDITIONAL_URLS_DIALOG (g_object_new (EPHY_TYPE_WEBAPP_ADDITIONAL_URLS_DIALOG,
                                                           "use-header-bar", TRUE,
                                                           NULL));
}
