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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#define SECRET_API_SUBJECT_TO_CHANGE
#include <libsecret/secret.h>

#include "ephy-uri-helpers.h"
#include "passwords-dialog.h"

typedef enum {
  COL_PASSWORDS_ORIGIN,
  COL_PASSWORDS_USER,
  COL_PASSWORDS_PASSWORD,
  COL_PASSWORDS_INVISIBLE,
  COL_PASSWORDS_DATA,
} PasswordsDialogColumn;

struct _EphyPasswordsDialog {
  GtkDialog parent_instance;

  EphyPasswordManager *manager;
  GList *records;
  GtkWidget *passwords_treeview;
  GtkTreeSelection *tree_selection;
  GtkWidget *liststore;
  GtkWidget *treemodelfilter;
  GtkWidget *treemodelsort;
  GtkWidget *show_passwords_button;
  GtkWidget *password_column;
  GtkWidget *password_renderer;
  GMenuModel *treeview_popup_menu_model;

  GActionGroup *action_group;

  gboolean filled;

  char *search_text;
};

G_DEFINE_TYPE (EphyPasswordsDialog, ephy_passwords_dialog, GTK_TYPE_DIALOG)

enum {
  PROP_0,
  PROP_PASSWORD_MANAGER,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

static void
ephy_passwords_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (object);

  switch (prop_id) {
    case PROP_PASSWORD_MANAGER:
      if (dialog->manager)
        g_object_unref (dialog->manager);
      dialog->manager = g_object_ref (g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_passwords_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (object);

  switch (prop_id) {
    case PROP_PASSWORD_MANAGER:
      g_value_set_object (value, dialog->manager);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_passwords_dialog_dispose (GObject *object)
{
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (object);

  g_clear_object (&dialog->manager);

  g_free (dialog->search_text);
  dialog->search_text = NULL;

  g_list_free_full (dialog->records, g_object_unref);
  dialog->records = NULL;

  G_OBJECT_CLASS (ephy_passwords_dialog_parent_class)->dispose (object);
}

static void
forget (GSimpleAction *action,
        GVariant      *parameter,
        gpointer       user_data)
{
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (user_data);
  GList *llist, *rlist = NULL, *l, *r;
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter, iter2;
  GtkTreeRowReference *row_ref = NULL;

  llist = gtk_tree_selection_get_selected_rows (dialog->tree_selection, &model);

  if (llist == NULL) {
    /* nothing to delete, return early */
    return;
  }

  for (l = llist; l != NULL; l = l->next) {
    rlist = g_list_prepend (rlist, gtk_tree_row_reference_new (model, (GtkTreePath *)l->data));
  }

  /* Intelligent selection logic, no actual selection yet */

  path = gtk_tree_row_reference_get_path ((GtkTreeRowReference *)g_list_first (rlist)->data);

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);
  iter2 = iter;

  if (gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &iter)) {
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
    row_ref = gtk_tree_row_reference_new (model, path);
  } else {
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter2);
    if (gtk_tree_path_prev (path)) {
      row_ref = gtk_tree_row_reference_new (model, path);
    }
  }
  gtk_tree_path_free (path);

  /* Removal */
  for (r = rlist; r != NULL; r = r->next) {
    GValue val = { 0, };
    EphyPasswordRecord *record;
    GtkTreeIter filter_iter;
    GtkTreeIter child_iter;

    path = gtk_tree_row_reference_get_path ((GtkTreeRowReference *)r->data);
    gtk_tree_model_get_iter (model, &iter, path);
    gtk_tree_model_get_value (model, &iter, COL_PASSWORDS_DATA, &val);
    record = g_value_get_object (&val);
    ephy_password_manager_forget (dialog->manager, ephy_password_record_get_id (record));
    dialog->records = g_list_remove (dialog->records, record);
    g_object_unref (record);
    g_value_unset (&val);

    gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (dialog->treemodelsort),
                                                    &filter_iter,
                                                    &iter);

    gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (dialog->treemodelfilter),
                                                      &child_iter,
                                                      &filter_iter);

    gtk_list_store_remove (GTK_LIST_STORE (dialog->liststore), &child_iter);

    gtk_tree_row_reference_free ((GtkTreeRowReference *)r->data);
    gtk_tree_path_free (path);
  }

  g_list_foreach (llist, (GFunc)gtk_tree_path_free, NULL);
  g_list_free (llist);
  g_list_free (rlist);

  /* Selection */
  if (row_ref != NULL) {
    path = gtk_tree_row_reference_get_path (row_ref);

    if (path != NULL) {
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (dialog->passwords_treeview), path, NULL, FALSE);
      gtk_tree_path_free (path);
    }

    gtk_tree_row_reference_free (row_ref);
  }
}

static void
show_passwords (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (user_data);
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->show_passwords_button));

  gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (dialog->password_column),
                                       GTK_CELL_RENDERER (dialog->password_renderer),
                                       "text", (active ? COL_PASSWORDS_PASSWORD : COL_PASSWORDS_INVISIBLE),
                                       NULL);
  gtk_widget_queue_draw (dialog->passwords_treeview);
}

static void
update_selection_actions (GActionMap *action_map,
                          gboolean    has_selection)
{
  GAction *forget_action;

  forget_action = g_action_map_lookup_action (action_map, "forget");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (forget_action), has_selection);
}

static void
on_treeview_selection_changed (GtkTreeSelection    *selection,
                               EphyPasswordsDialog *dialog)
{
  update_selection_actions (G_ACTION_MAP (dialog->action_group),
                            gtk_tree_selection_count_selected_rows (selection) > 0);
}

static void
on_search_entry_changed (GtkSearchEntry      *entry,
                         EphyPasswordsDialog *dialog)
{
  const char *text;

  text = gtk_entry_get_text (GTK_ENTRY (entry));
  g_free (dialog->search_text);
  dialog->search_text = g_strdup (text);
  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (dialog->treemodelfilter));
}

static char *
get_selected_item (EphyPasswordsDialog  *dialog,
                   PasswordsDialogColumn column)
{
  GtkTreeModel *model;
  GList *selected;
  GtkTreeIter iter;
  char *value;

  selected = gtk_tree_selection_get_selected_rows (dialog->tree_selection, &model);
  gtk_tree_model_get_iter (model, &iter, selected->data);
  gtk_tree_model_get (model, &iter,
                      column, &value,
                      -1);
  g_list_free_full (selected, (GDestroyNotify)gtk_tree_path_free);

  return value;
}

static void
copy_password (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (user_data);
  char *password;

  password = get_selected_item (dialog, COL_PASSWORDS_PASSWORD);
  if (password != NULL) {
    gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (dialog),
                                                      GDK_SELECTION_CLIPBOARD),
                            password, -1);
  }
  g_free (password);
}

static void
copy_username (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (user_data);
  char *username;

  username = get_selected_item (dialog, COL_PASSWORDS_USER);
  if (username != NULL) {
    gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (dialog),
                                                      GDK_SELECTION_CLIPBOARD),
                            username, -1);
  }
  g_free (username);
}

static void
update_popup_menu_actions (GActionGroup *action_group,
                           gboolean      only_one_selected_item)
{
  GAction *copy_password_action;
  GAction *copy_username_action;

  copy_password_action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "copy-password");
  copy_username_action = g_action_map_lookup_action (G_ACTION_MAP (action_group), "copy-username");

  g_simple_action_set_enabled (G_SIMPLE_ACTION (copy_password_action), only_one_selected_item);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (copy_username_action), only_one_selected_item);
}

static gboolean
on_passwords_treeview_button_press_event (GtkWidget           *widget,
                                          GdkEventButton      *event,
                                          EphyPasswordsDialog *dialog)
{
  if (event->button == 3) {
    int n;
    GtkWidget *menu;

    n = gtk_tree_selection_count_selected_rows (dialog->tree_selection);
    if (n == 0)
      return FALSE;

    update_popup_menu_actions (dialog->action_group, (n == 1));

    menu = gtk_menu_new_from_model (dialog->treeview_popup_menu_model);
    gtk_menu_attach_to_widget (GTK_MENU (menu), GTK_WIDGET (dialog), NULL);
    gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *)event);
    return TRUE;
  }

  return FALSE;
}

static void
ephy_passwords_dialog_class_init (EphyPasswordsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ephy_passwords_dialog_set_property;
  object_class->get_property = ephy_passwords_dialog_get_property;
  object_class->dispose = ephy_passwords_dialog_dispose;

  obj_properties[PROP_PASSWORD_MANAGER] =
    g_param_spec_object ("password-manager",
                         "Password manager",
                         "Password Manager",
                         EPHY_TYPE_PASSWORD_MANAGER,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/passwords-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, liststore);
  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, treemodelfilter);
  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, treemodelsort);
  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, passwords_treeview);
  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, tree_selection);
  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, show_passwords_button);
  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, password_column);
  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, password_renderer);
  gtk_widget_class_bind_template_child (widget_class, EphyPasswordsDialog, treeview_popup_menu_model);

  gtk_widget_class_bind_template_callback (widget_class, on_passwords_treeview_button_press_event);
  gtk_widget_class_bind_template_callback (widget_class, on_treeview_selection_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_search_entry_changed);
}

static void
forget_all (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       user_data)
{
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (user_data);

  ephy_password_manager_forget_all (dialog->manager);

  gtk_list_store_clear (GTK_LIST_STORE (dialog->liststore));
  dialog->filled = FALSE;

  g_list_free_full (dialog->records, g_object_unref);
  dialog->records = NULL;
}

static void
populate_model_cb (GList    *records,
                   gpointer  user_data)
{
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (user_data);

  for (GList *l = records; l && l->data; l = l->next) {
    EphyPasswordRecord *record = EPHY_PASSWORD_RECORD (l->data);
    GtkTreeIter iter;

    gtk_list_store_insert_with_values (GTK_LIST_STORE (dialog->liststore),
                                       &iter,
                                       -1,
                                       COL_PASSWORDS_ORIGIN, ephy_password_record_get_hostname (record),
                                       COL_PASSWORDS_USER, ephy_password_record_get_username (record),
                                       COL_PASSWORDS_PASSWORD, ephy_password_record_get_password (record),
                                       COL_PASSWORDS_INVISIBLE, "●●●●●●●●",
                                       COL_PASSWORDS_DATA, record,
                                       -1);
  }

  dialog->records = records;
}

static void
populate_model (EphyPasswordsDialog *dialog)
{
  g_assert (EPHY_IS_PASSWORDS_DIALOG (dialog));
  g_assert (dialog->filled == FALSE);

  /* Ask for all password records. */
  ephy_password_manager_query (dialog->manager,
                               NULL, NULL, NULL, NULL, NULL,
                               populate_model_cb, dialog);
}

static gboolean
row_visible_func (GtkTreeModel        *model,
                  GtkTreeIter         *iter,
                  EphyPasswordsDialog *dialog)
{
  char *username;
  char *origin;
  gboolean visible = FALSE;

  if (dialog->search_text == NULL)
    return TRUE;

  gtk_tree_model_get (model, iter,
                      COL_PASSWORDS_ORIGIN, &origin,
                      COL_PASSWORDS_USER, &username,
                      -1);

  if (origin != NULL && g_strrstr (origin, dialog->search_text) != NULL)
    visible = TRUE;
  else if (username != NULL && g_strrstr (username, dialog->search_text) != NULL)
    visible = TRUE;

  g_free (origin);
  g_free (username);

  return visible;
}

static GActionGroup *
create_action_group (EphyPasswordsDialog *dialog)
{
  const GActionEntry entries[] = {
    { "copy-password", copy_password },
    { "copy-username", copy_username },
    { "forget", forget },
    { "forget-all", forget_all },
    { "show-passwords", show_passwords }
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
  EphyPasswordsDialog *dialog = EPHY_PASSWORDS_DIALOG (widget);

  populate_model (dialog);
}

static void
ephy_passwords_dialog_init (EphyPasswordsDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (dialog->treemodelfilter),
                                          (GtkTreeModelFilterVisibleFunc)row_visible_func,
                                          dialog,
                                          NULL);

  dialog->action_group = create_action_group (dialog);
  gtk_widget_insert_action_group (GTK_WIDGET (dialog), "passwords", dialog->action_group);

  update_selection_actions (G_ACTION_MAP (dialog->action_group), FALSE);

  g_signal_connect (GTK_WIDGET (dialog), "show", G_CALLBACK (show_dialog_cb), NULL);
}

EphyPasswordsDialog *
ephy_passwords_dialog_new (EphyPasswordManager *manager)
{
  return EPHY_PASSWORDS_DIALOG (g_object_new (EPHY_TYPE_PASSWORDS_DIALOG,
                                              "password-manager", manager,
                                              "use-header-bar", TRUE,
                                              NULL));
}
