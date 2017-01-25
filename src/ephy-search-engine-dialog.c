/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Cedric Le Moigne <cedlemo@gmx.com>
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

#include "ephy-search-engine-dialog.h"
#include "ephy-shell.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

struct _EphySearchEngineDialog {
  GtkDialog parent_instance;

  EphySearchEngineManager *search_engine_manager;
  GtkWidget *search_engine_add_button;
  GtkWidget *search_engine_remove_button;
  GtkWidget *search_engine_adress_entry;
  GtkWidget *search_engine_default_switch;
  GtkWidget *search_engine_list_box;
  GtkWidget *search_engine_keyword_entry;
};

G_DEFINE_TYPE (EphySearchEngineDialog, ephy_search_engine_dialog, GTK_TYPE_DIALOG)

static void
remove_list_box_row_cb (GtkWidget *button,
                        gpointer   data)
{
  GtkListBoxRow *list_box_row;
  EphySearchEngineDialog *dialog;
  EphySearchEngineManager *manager;
  GtkLabel *search_engine_label;
  const char *search_engine_name;

  dialog = EPHY_SEARCH_ENGINE_DIALOG (data);
  list_box_row = gtk_list_box_get_selected_row (GTK_LIST_BOX (dialog->search_engine_list_box));

  gtk_container_remove (GTK_CONTAINER (dialog->search_engine_list_box),
                        list_box_row);
  search_engine_label = (gtk_container_get_children (GTK_CONTAINER (list_box_row)))->data;
  search_engine_name = gtk_label_get_text (GTK_LABEL (search_engine_label));
  manager = dialog->search_engine_manager;
  ephy_search_engine_manager_delete_engine (manager, search_engine_name);
}

static void
add_list_box_row (EphySearchEngineDialog *dialog,
                  const char             *name,
                  int                     position)
{
  GtkListBox *listbox;
  GtkWidget *label;
  GtkWidget *list_box_row;


  label = gtk_label_new (name);
  gtk_widget_set_halign (label, GTK_ALIGN_START);

  list_box_row = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (list_box_row), label);
  gtk_widget_set_size_request (list_box_row, 160, -1);
  gtk_widget_show_all (list_box_row);

  listbox = GTK_LIST_BOX (dialog->search_engine_list_box);
  gtk_list_box_insert (listbox, list_box_row, position);
  gtk_list_box_select_row (listbox, GTK_LIST_BOX_ROW (list_box_row));
}

static void
list_box_row_selected_cb (GtkListBox    *list_box,
                          GtkListBoxRow *list_box_row,
                          gpointer       data)
{
  EphySearchEngineDialog *dialog;
  EphySearchEngineManager *manager;
  GtkLabel *search_engine_label;
  const char *search_engine_name;
  const char *search_engine_url;
  const char *search_engine_default;
  gboolean is_default = FALSE;

  if (!list_box_row)
    return;

  dialog = EPHY_SEARCH_ENGINE_DIALOG (data);
  manager = dialog->search_engine_manager;

  search_engine_label = (gtk_container_get_children (GTK_CONTAINER (list_box_row)))->data;
  search_engine_name = gtk_label_get_text (GTK_LABEL (search_engine_label));

  search_engine_url = ephy_search_engine_manager_get_url (manager, search_engine_name);
  if (!search_engine_url)
    search_engine_url = "";

  gtk_entry_set_text (GTK_ENTRY (dialog->search_engine_adress_entry), search_engine_url);

  search_engine_default = ephy_search_engine_manager_get_default_engine (manager);
  if (strcmp (search_engine_name, search_engine_default) == 0)
    is_default = TRUE;

  gtk_switch_set_active (GTK_SWITCH (dialog->search_engine_default_switch),
                         is_default);
}

static void
ephy_search_engine_dialog_fill_list_box (EphySearchEngineDialog *dialog)
{

  GtkListBox *listbox;
  EphySearchEngineManager *manager;
  char **engines_names ;
  uint n_engines;

  listbox = GTK_LIST_BOX (dialog->search_engine_list_box);
  manager = dialog->search_engine_manager;
  engines_names = ephy_search_engine_manager_get_names (manager);
  n_engines = g_strv_length (engines_names);

  for (uint i = 0; i < n_engines; i++) {
    const char *name = engines_names[i];
    add_list_box_row (dialog, name, i);
  }
  g_strfreev (engines_names);

  g_signal_connect (listbox,
                    "row-selected",
                    G_CALLBACK (list_box_row_selected_cb),
                    dialog);
  gtk_list_box_select_row (listbox,
                           gtk_list_box_get_row_at_index (listbox, 0));
}
static void
on_search_engine_add_button_clicked (GtkButton              *button,
                                     EphySearchEngineDialog *dialog)
{
  EphySearchEngineManager *manager;
  const char *new_engine_name =  "New Search Engine";
  const char *new_engine_url = "New adress";
  manager = dialog->search_engine_manager;
  ephy_search_engine_manager_add_engine (manager, new_engine_name, new_engine_url);
  add_list_box_row (dialog, new_engine_name, -1);
}

static void
on_search_engine_remove_button_clicked (GtkButton              *button,
                                        EphySearchEngineDialog *dialog)
{
  GtkListBoxRow *list_box_row;
  EphySearchEngineManager *manager;
  GtkLabel *search_engine_label;
  const char *search_engine_name;

  list_box_row = gtk_list_box_get_selected_row (GTK_LIST_BOX (dialog->search_engine_list_box));

  gtk_container_remove (GTK_CONTAINER (dialog->search_engine_list_box),
                        list_box_row);
  search_engine_label = (gtk_container_get_children (GTK_CONTAINER (list_box_row)))->data;
  search_engine_name = gtk_label_get_text (GTK_LABEL (search_engine_label));
  manager = dialog->search_engine_manager;
  ephy_search_engine_manager_delete_engine (manager, search_engine_name);
}

static void
ephy_search_engine_dialog_class_init (EphySearchEngineDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/search-engine-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class,
                                        EphySearchEngineDialog,
                                        search_engine_add_button);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphySearchEngineDialog,
                                        search_engine_remove_button);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphySearchEngineDialog,
                                        search_engine_list_box);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphySearchEngineDialog,
                                        search_engine_keyword_entry);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphySearchEngineDialog,
                                        search_engine_adress_entry);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphySearchEngineDialog,
                                        search_engine_default_switch);

  gtk_widget_class_bind_template_callback (widget_class, on_search_engine_add_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_search_engine_remove_button_clicked);
}

static void
ephy_search_engine_dialog_init (EphySearchEngineDialog *dialog)
{
  EphyEmbedShell *shell;

  shell = ephy_embed_shell_get_default ();
  dialog->search_engine_manager = ephy_embed_shell_get_search_engine_manager (shell);

  gtk_widget_init_template (GTK_WIDGET (dialog));
  ephy_search_engine_dialog_fill_list_box (dialog);
}

EphySearchEngineDialog *
ephy_search_engine_dialog_new (void)
{
  return g_object_new (EPHY_TYPE_SEARCH_ENGINE_DIALOG,
                       "use-header-bar", TRUE,
                       NULL);
}
