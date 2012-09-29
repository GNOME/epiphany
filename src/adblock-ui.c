/*
 *  Copyright © 2011 Igalia S.L.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  Some parts of this file based on the Midori's 'adblock' extension,
 *  licensed with the GNU Lesser General Public License 2.1, Copyright
 *  (C) 2009-2010 Christian Dywan <christian@twotoasts.de> and 2009
 *  Alexander Butenko <a.butenka@gmail.com>. Check Midori's web site
 *  at http://www.twotoasts.de
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "adblock-ui.h"

#include "ephy-adblock.h"
#include "ephy-adblock-manager.h"
#include "ephy-debug.h"
#include "ephy-embed-shell.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#define ADBLOCK_UI_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE((object), TYPE_ADBLOCK_UI, AdblockUIPrivate))

#define ADBLOCK_FILTER_VALID(__filter)              \
  (__filter && (g_str_has_prefix (__filter, "http") \
                || g_str_has_prefix (__filter, "file")))

struct _AdblockUIPrivate
{
  GtkWidget *dialog;

  /* The dialog buttons. */
  GtkEntry *new_filter;
  GtkButton *add, *edit, *remove;

  /* Data. */
  GtkTreeView *treeview;
  GtkTreeSelection *selection;
  GtkListStore *store;

  /* The uri tester. */
  UriTester *tester;

  /* Whether something has actually changed. */
  gboolean dirty;
};

enum
{
  PROP_0,
  PROP_TESTER,
};

enum
{
  COL_FILTER_URI,
  N_COLUMNS
};

G_DEFINE_DYNAMIC_TYPE (AdblockUI, adblock_ui, EPHY_TYPE_DIALOG);

/* Private functions. */

static gboolean
adblock_ui_foreach_save (GtkTreeModel *model,
                         GtkTreePath  *path,
                         GtkTreeIter  *iter,
                         GSList       **filters)
{
  char *filter = NULL;

  gtk_tree_model_get (model, iter, COL_FILTER_URI, &filter, -1);
  *filters = g_slist_prepend (*filters, filter);

  return FALSE;
}

static void
adblock_ui_save (AdblockUI *dialog)
{
  GSList *filters = NULL;
  gtk_tree_model_foreach (GTK_TREE_MODEL (dialog->priv->store),
                          (GtkTreeModelForeachFunc)adblock_ui_foreach_save,
                          &filters);

  uri_tester_set_filters (dialog->priv->tester, filters);
}

static void
adblock_ui_response_cb (GtkWidget *widget,
                        int response,
                        AdblockUI *dialog)
{
  if (response == GTK_RESPONSE_CLOSE && dialog->priv->dirty)
    {
      EphyAdBlockManager *manager;

      adblock_ui_save (dialog);

      /* Ask uri tester to reload all its patterns. */
      uri_tester_reload (dialog->priv->tester);

      /* Ask manager to emit a signal that rules have changed. */
      manager = EPHY_ADBLOCK_MANAGER (ephy_embed_shell_get_adblock_manager (embed_shell));

      g_signal_emit_by_name (manager, "rules_changed", NULL);
    }

  g_object_unref (dialog);
}

static void
adblock_ui_add_filter (AdblockUI *dialog)
{
  GtkTreeIter iter;

  const char *new_filter = gtk_entry_get_text (dialog->priv->new_filter);

  if (ADBLOCK_FILTER_VALID (new_filter))
    {
      GtkListStore *store = dialog->priv->store;

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, COL_FILTER_URI, new_filter, -1);

      /* Makes the pattern field blank. */
      gtk_entry_set_text (dialog->priv->new_filter, "");

      dialog->priv->dirty = TRUE;
    }
  else
    {
      GtkWidget *error_dialog = NULL;
      error_dialog = gtk_message_dialog_new (GTK_WINDOW (dialog->priv->dialog),
                                             GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_OK,
                                             "%s",
                                             _("Invalid filter"));
      gtk_dialog_run (GTK_DIALOG (error_dialog));
      gtk_widget_destroy (error_dialog);

      gtk_entry_set_text (dialog->priv->new_filter, "");
    }
}

static void
adblock_ui_add_cb (GtkButton *button,
                   AdblockUI *dialog)
{
  adblock_ui_add_filter (dialog);
}

static void
adblock_ui_edit_cb (GtkButton *button,
                    AdblockUI *dialog)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeSelection *selection;

  selection = dialog->priv->selection;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      char* path = gtk_tree_model_get_string_from_iter (model, &iter);
      GtkTreePath* tree_path = gtk_tree_path_new_from_string (path);
      GtkTreeView *treeview = dialog->priv->treeview;
      GtkTreeViewColumn *column = gtk_tree_view_get_column (treeview,
                                                            COL_FILTER_URI);

      gtk_tree_view_set_cursor (treeview, tree_path, column, TRUE);
      gtk_tree_path_free (tree_path);
      g_free (path);
    }
}

static void
adblock_ui_remove_cb (GtkButton *button,
                      AdblockUI *dialog)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeSelection *selection;

  selection = dialog->priv->selection;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_list_store_remove (GTK_LIST_STORE(model), &iter);
      gtk_entry_set_text (dialog->priv->new_filter, "");

      dialog->priv->dirty = TRUE;
    }
}

static void
adblock_ui_cell_edited_cb (GtkCellRendererText *cell,
                           char               *path_string,
                           char               *new_filter,
                           AdblockUI           *dialog)
{
  GtkTreeModel *model = GTK_TREE_MODEL (dialog->priv->store);
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_list_store_set (dialog->priv->store, &iter, COL_FILTER_URI, new_filter, -1);
  gtk_tree_path_free (path);

  dialog->priv->dirty = TRUE;
}

static void
adblock_ui_build_treeview (AdblockUI *dialog)
{
  GtkCellRenderer *renderer;

  dialog->priv->store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set(renderer, "editable", TRUE, NULL);
  g_signal_connect(renderer,
                   "edited",
                   (GCallback) adblock_ui_cell_edited_cb,
                   (gpointer)dialog);

  gtk_tree_view_insert_column_with_attributes (dialog->priv->treeview,
                                               COL_FILTER_URI, _("Filter URI"),
                                               renderer,
                                               "text", COL_FILTER_URI,
                                               NULL);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (dialog->priv->store),
                                        COL_FILTER_URI,
                                        GTK_SORT_ASCENDING);

  gtk_tree_view_set_model (dialog->priv->treeview, GTK_TREE_MODEL (dialog->priv->store));
  gtk_tree_view_set_search_column (dialog->priv->treeview, COL_FILTER_URI);

  g_object_unref (dialog->priv->store);

  dialog->priv->selection = gtk_tree_view_get_selection (dialog->priv->treeview);
  gtk_tree_selection_set_mode (dialog->priv->selection, GTK_SELECTION_SINGLE);

  dialog->priv->dirty = FALSE;
}

static void
adblock_ui_populate_store (AdblockUI *dialog)
{
  GSList *filters = NULL;
  GSList *item = NULL;
  const char *filter_uri = NULL;
  GtkTreeIter iter;

  filters = uri_tester_get_filters (dialog->priv->tester);
  for (item = filters; item; item = g_slist_next (item))
    {
      filter_uri = (const char *) item->data;

      gtk_list_store_append (dialog->priv->store, &iter);
      gtk_list_store_set (dialog->priv->store, &iter, COL_FILTER_URI, filter_uri, -1);
    }
}

static void
adblock_ui_init (AdblockUI *dialog)
{
  LOG ("AdblockUI initialising");
  dialog->priv = ADBLOCK_UI_GET_PRIVATE (dialog);
}

static void
adblock_ui_constructed (GObject *object)
{
  AdblockUI *dialog;
  AdblockUIPrivate *priv;
  EphyDialog *edialog;

  dialog = ADBLOCK_UI (object);
  edialog = EPHY_DIALOG (object);

  priv = dialog->priv;

  ephy_dialog_construct (EPHY_DIALOG (edialog),
                         "/org/gnome/epiphany/adblock.ui",
                         "adblock-ui",
                         GETTEXT_PACKAGE);

  ephy_dialog_get_controls (edialog,
                            "adblock-ui", &priv->dialog,
                            "new_filter_entry", &priv->new_filter,
                            "treeview", &priv->treeview,
                            "add_button", &priv->add,
                            "edit_button", &priv->edit,
                            "remove_button", &priv->remove,
                            NULL);

  g_signal_connect (priv->dialog, "response",
                    G_CALLBACK (adblock_ui_response_cb), dialog);

  g_signal_connect (priv->add, "clicked",
                    G_CALLBACK (adblock_ui_add_cb), dialog);
  g_signal_connect (priv->edit, "clicked",
                    G_CALLBACK (adblock_ui_edit_cb), dialog);
  g_signal_connect (priv->remove, "clicked",
                    G_CALLBACK (adblock_ui_remove_cb), dialog);
  g_signal_connect (priv->new_filter, "activate",
                    G_CALLBACK (adblock_ui_add_cb), dialog);

  /* Build and fill. */
  adblock_ui_build_treeview (dialog);
  adblock_ui_populate_store (dialog);

  /* Chain up. */
  G_OBJECT_CLASS (adblock_ui_parent_class)->constructed (object);
}

static void
adblock_ui_set_property (GObject *object,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
  AdblockUI *dialog = ADBLOCK_UI (object);

  switch (prop_id)
    {
    case PROP_TESTER:
      dialog->priv->tester = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
adblock_ui_class_init (AdblockUIClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = adblock_ui_constructed;
  object_class->set_property = adblock_ui_set_property;

  g_object_class_install_property
    (object_class,
     PROP_TESTER,
     g_param_spec_object ("tester",
                          "UriTester",
                          "UriTester",
                          TYPE_URI_TESTER,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (object_class, sizeof (AdblockUIPrivate));
}

static void adblock_ui_class_finalize (AdblockUIClass *klass)
{
}

/* Public functions. */

void adblock_ui_register (GTypeModule *module)
{
  adblock_ui_register_type (module);
}

AdblockUI *
adblock_ui_new (UriTester *tester)
{
  return g_object_new (TYPE_ADBLOCK_UI,
                       "tester", tester,
                       NULL);
}
