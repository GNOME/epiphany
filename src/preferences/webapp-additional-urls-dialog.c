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
#include "webapp-additional-urls-list-item.h"

struct _EphyWebappAdditionalURLsDialog {
  AdwWindow parent_instance;

  GtkColumnView *columnview;
  GtkTreeViewColumn *url_column;
  GtkSignalListItemFactory *url_cell_factory;
  GtkSingleSelection *selection_model;

  GListStore *liststore;
  GActionGroup *action_group;
};

G_DEFINE_FINAL_TYPE (EphyWebappAdditionalURLsDialog, ephy_webapp_additional_urls_dialog, ADW_TYPE_WINDOW)

static void
ephy_webapp_additional_urls_update_settings (EphyWebappAdditionalURLsDialog *dialog)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);
  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (dialog->liststore)); i++) {
    g_autoptr (EphyWebappAdditionalURLsListItem) item = NULL;
    item = EPHY_WEBAPP_ADDITIONAL_URLS_LIST_ITEM (g_list_model_get_item (G_LIST_MODEL (dialog->liststore), i));
    ephy_webapp_additional_urls_list_item_add_to_builder (EPHY_WEBAPP_ADDITIONAL_URLS_LIST_ITEM (item), &builder);
  }
  g_settings_set (EPHY_SETTINGS_WEB_APP,
                  EPHY_PREFS_WEB_APP_ADDITIONAL_URLS,
                  "as", &builder);
}

static void
on_list_item_selected (GtkListItem *list_item,
                       GParamSpec  *pspec,
                       GtkWidget   *entry_widget)
{
  if (!gtk_list_item_get_selected (GTK_LIST_ITEM (list_item)))
    return;
  gtk_widget_grab_focus (entry_widget);
}

static void
submit_text_input_for_list_item (GtkListItem *list_item,
                                 GtkText     *entry,
                                 bool         select)
{
  EphyWebappAdditionalURLsListItem *model_item = EPHY_WEBAPP_ADDITIONAL_URLS_LIST_ITEM (gtk_list_item_get_item (list_item));
  const gchar *text = gtk_editable_get_text (GTK_EDITABLE (entry));
  ephy_webapp_additional_urls_list_item_set_url (model_item, text);
  if (select)
    gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
}

static void
on_url_entry_has_focus (GtkText     *entry,
                        GParamSpec  *pspec,
                        GtkListItem *list_item)
{
  EphyWebappAdditionalURLsDialog *dialog;
  guint position;

  if (!gtk_widget_has_focus (GTK_WIDGET (entry))) {
    submit_text_input_for_list_item (list_item, entry, false);
    return;
  }

  dialog = EPHY_WEBAPP_ADDITIONAL_URLS_DIALOG (gtk_widget_get_ancestor (GTK_WIDGET (entry), EPHY_TYPE_WEBAPP_ADDITIONAL_URLS_DIALOG));
  g_assert (dialog != NULL);
  position = gtk_list_item_get_position (list_item);
  gtk_single_selection_set_selected (dialog->selection_model, position);
}

static void
on_url_entry_activate (GtkText     *entry,
                       GtkListItem *list_item)
{
  submit_text_input_for_list_item (list_item, entry, true);
}

static void
on_url_cell_setup (GtkSignalListItemFactory       *factory,
                   GObject                        *object,
                   EphyWebappAdditionalURLsDialog *dialog)
{
  GtkWidget *entry_widget = gtk_text_new ();
  gtk_list_item_set_child (GTK_LIST_ITEM (object), entry_widget);
}

static void
on_url_cell_bind (GtkSignalListItemFactory       *factory,
                  GObject                        *object,
                  EphyWebappAdditionalURLsDialog *dialog)
{
  GtkWidget *entry_widget = gtk_list_item_get_child (GTK_LIST_ITEM (object));
  GObject *model_item = gtk_list_item_get_item (GTK_LIST_ITEM (object));
  const gchar *current_url;
  g_assert (entry_widget != NULL);
  g_assert (model_item != NULL);
  current_url = ephy_webapp_additional_urls_list_item_get_url (EPHY_WEBAPP_ADDITIONAL_URLS_LIST_ITEM (model_item));
  gtk_editable_set_text (GTK_EDITABLE (entry_widget),
                         current_url != NULL ? current_url : "");
  g_signal_connect_object (object,
                           "notify::selected",
                           G_CALLBACK (on_list_item_selected),
                           GTK_WIDGET (entry_widget),
                           G_CONNECT_DEFAULT);
  g_signal_connect_object (G_OBJECT (entry_widget),
                           "notify::has-focus",
                           G_CALLBACK (on_url_entry_has_focus),
                           GTK_LIST_ITEM (object),
                           G_CONNECT_DEFAULT);
  g_signal_connect_object (G_OBJECT (entry_widget),
                           "activate",
                           G_CALLBACK (on_url_entry_activate),
                           GTK_LIST_ITEM (object),
                           G_CONNECT_DEFAULT);
}

static void
on_url_cell_teardown (GtkSignalListItemFactory       *factory,
                      GObject                        *object,
                      EphyWebappAdditionalURLsDialog *dialog)
{
  gtk_list_item_set_child (GTK_LIST_ITEM (object), NULL);
}

static void
on_list_item_notify (EphyWebappAdditionalURLsListItem *model_item,
                     GParamSpec                       *pspec,
                     EphyWebappAdditionalURLsDialog   *dialog)
{
  const gchar *url;
  guint item_position;

  url = ephy_webapp_additional_urls_list_item_get_url (EPHY_WEBAPP_ADDITIONAL_URLS_LIST_ITEM (model_item));

  if ((!url || url[0] == '\0') &&
      g_list_store_find (dialog->liststore, model_item, &item_position))
    g_list_store_remove (dialog->liststore, item_position);

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
on_columnview_selection_changed (GtkSelectionModel              *selection,
                                 guint                           position,
                                 guint                           n_items,
                                 EphyWebappAdditionalURLsDialog *dialog)
{
  guint selected_position = gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (selection));
  update_selection_actions (G_ACTION_MAP (dialog->action_group),
                            selected_position != GTK_INVALID_LIST_POSITION);
}

static void
ephy_webapp_additional_urls_dialog_dispose (GObject *object)
{
  EphyWebappAdditionalURLsDialog *self = EPHY_WEBAPP_ADDITIONAL_URLS_DIALOG (object);

  g_clear_object (&self->liststore);
  g_clear_object (&self->action_group);

  G_OBJECT_CLASS (ephy_webapp_additional_urls_dialog_parent_class)->dispose (object);
}

static void
ephy_webapp_additional_urls_dialog_class_init (EphyWebappAdditionalURLsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ephy_webapp_additional_urls_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/webapp-additional-urls-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyWebappAdditionalURLsDialog, columnview);
  gtk_widget_class_bind_template_child (widget_class, EphyWebappAdditionalURLsDialog, url_column);
  gtk_widget_class_bind_template_child (widget_class, EphyWebappAdditionalURLsDialog, url_cell_factory);
  gtk_widget_class_bind_template_child (widget_class, EphyWebappAdditionalURLsDialog, selection_model);

  gtk_widget_class_bind_template_callback (widget_class, on_columnview_selection_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_url_cell_setup);
  gtk_widget_class_bind_template_callback (widget_class, on_url_cell_bind);
  gtk_widget_class_bind_template_callback (widget_class, on_url_cell_teardown);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Delete, 0, "webapp-additional-urls.forget", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Delete, 0, "webapp-additional-urls.forget", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Delete, GDK_SHIFT_MASK, "webapp-additional-urls.forget-all", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Delete, GDK_SHIFT_MASK, "webapp-additional-urls.forget-all", NULL);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);
}

static EphyWebappAdditionalURLsListItem *
append_url_list_item (EphyWebappAdditionalURLsDialog *dialog,
                      const gchar                    *url)
{
  g_autoptr (EphyWebappAdditionalURLsListItem) item = NULL;
  item = ephy_webapp_additional_urls_list_item_new (url);
  g_list_store_append (G_LIST_STORE (dialog->liststore), item);
  g_signal_connect_object (item,
                           "notify",
                           G_CALLBACK (on_list_item_notify),
                           dialog,
                           G_CONNECT_DEFAULT);
  return item;
}

static void
add_new (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       user_data)
{
  EphyWebappAdditionalURLsDialog *dialog = EPHY_WEBAPP_ADDITIONAL_URLS_DIALOG (user_data);
  append_url_list_item (dialog, "");
  gtk_single_selection_set_selected (dialog->selection_model,
                                     g_list_model_get_n_items (G_LIST_MODEL (dialog->liststore)) - 1);
}

static void
forget (GSimpleAction *action,
        GVariant      *parameter,
        gpointer       user_data)
{
  EphyWebappAdditionalURLsDialog *dialog = EPHY_WEBAPP_ADDITIONAL_URLS_DIALOG (user_data);
  guint selected_position;

  selected_position = gtk_single_selection_get_selected (dialog->selection_model);
  if (selected_position == GTK_INVALID_LIST_POSITION)
    return;
  g_list_store_remove (dialog->liststore, selected_position);
  ephy_webapp_additional_urls_update_settings (dialog);
}

static void
forget_all (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       user_data)
{
  EphyWebappAdditionalURLsDialog *dialog = EPHY_WEBAPP_ADDITIONAL_URLS_DIALOG (user_data);

  g_list_store_remove_all (dialog->liststore);
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
    append_url_list_item (dialog, urls[i]);
  }
  g_strfreev (urls);
}

static void
on_liststore_items_changed (GListModel                     *list_model,
                            guint                           position,
                            guint                           removed,
                            guint                           added,
                            EphyWebappAdditionalURLsDialog *dialog)
{
  guint n_items = g_list_model_get_n_items (list_model);
  gtk_single_selection_set_autoselect (dialog->selection_model, n_items > 1);

  if (n_items == 0)
    update_selection_actions (G_ACTION_MAP (dialog->action_group), false);
}

static void
ephy_webapp_additional_urls_dialog_init (EphyWebappAdditionalURLsDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));

  dialog->liststore = g_list_store_new (EPHY_TYPE_WEBAPP_ADDITIONAL_URLS_LIST_ITEM);
  gtk_single_selection_set_model (dialog->selection_model, G_LIST_MODEL (dialog->liststore));

  dialog->action_group = create_action_group (dialog);
  gtk_widget_insert_action_group (GTK_WIDGET (dialog), "webapp-additional-urls", dialog->action_group);

  update_selection_actions (G_ACTION_MAP (dialog->action_group), FALSE);

  g_signal_connect_object (dialog->liststore,
                           "items-changed",
                           G_CALLBACK (on_liststore_items_changed),
                           dialog,
                           G_CONNECT_DEFAULT);
  g_signal_connect (GTK_WIDGET (dialog), "show", G_CALLBACK (show_dialog_cb), NULL);
}

EphyWebappAdditionalURLsDialog *
ephy_webapp_additional_urls_dialog_new (void)
{
  return g_object_new (EPHY_TYPE_WEBAPP_ADDITIONAL_URLS_DIALOG, NULL);
}
