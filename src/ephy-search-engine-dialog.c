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

#define NEW_SEARCH_ENGINE_NAME     _("New search engine")
#define NEW_SEARCH_ENGINE_ADDRESS  _("New address")
#define NEW_SEARCH_ENGINE_BANG     _("Bang")

struct _EphySearchEngineDialog {
  GtkDialog parent_instance;

  EphySearchEngineManager *search_engine_manager;
  GtkWidget *search_engine_add_button;
  GtkWidget *search_engine_address_entry;
  GtkWidget *search_engine_default_switch;
  GtkWidget *search_engine_list_box;
  GtkWidget *search_engine_name_entry;
  GtkWidget *search_engine_bang_entry;
  GtkWidget *search_engine_remove_button;
};

G_DEFINE_TYPE (EphySearchEngineDialog, ephy_search_engine_dialog, GTK_TYPE_DIALOG)

static GtkWidget *
container_get_first_child (GtkContainer *container)
{
  GList *children;
  GtkWidget *child;

  children = gtk_container_get_children (container);
  child = (GtkWidget *)children->data;
  g_list_free (children);
  return child;
}

static int
dialog_list_box_child_n_occurence (const char             *name,
                                   EphySearchEngineDialog *dialog)
{
  GList *children;
  GList *c;
  GtkWidget *label;
  int count = 0;
  const char *text;

  children = gtk_container_get_children (GTK_CONTAINER (dialog->search_engine_list_box));

  for (c = children; c != NULL; c = c->next) {
    label = container_get_first_child (GTK_CONTAINER (c->data));
    text = gtk_label_get_text (GTK_LABEL (label));
    if (g_strcmp0 (name, text) == 0)
      count++;
  }
  g_list_free (children);

  return count;
}

static GtkWidget *
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
  return list_box_row;
}

static void
dialog_set_entry_error_state (GtkWidget *entry)
{
  GtkStyleContext *context;

  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "dialog-warning-symbolic");
  context = gtk_widget_get_style_context (entry);
  gtk_style_context_add_class (context, "error");
}

static void
dialog_set_entry_normal_state (GtkWidget *entry)
{
  GtkStyleContext *context;

  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     NULL);
  context = gtk_widget_get_style_context (entry);
  gtk_style_context_remove_class (context, "error");
}

static gboolean
dialog_check_name_entry (const char             *name,
                         EphySearchEngineDialog *dialog)
{
  EphySearchEngineManager *manager;
  const char *search_engine_name;
  GtkWidget *search_engine_label;
  GtkListBoxRow *list_box_row;

  manager = dialog->search_engine_manager;
  list_box_row = gtk_list_box_get_selected_row (GTK_LIST_BOX (dialog->search_engine_list_box));
  search_engine_label = container_get_first_child (GTK_CONTAINER (list_box_row));
  search_engine_name = gtk_label_get_text (GTK_LABEL (search_engine_label));

  if (name == NULL || g_strcmp0 (name, "") == 0) {
    dialog_set_entry_error_state (dialog->search_engine_name_entry);
    return FALSE;
  }

  /* Check if name already exist in the search_engine_manager */
  if (ephy_search_engine_manager_get_address (manager, name) != NULL &&
      g_strcmp0 (name, search_engine_name) != 0 ) {
      dialog_set_entry_error_state (dialog->search_engine_name_entry);
      return FALSE;
  }

  dialog_set_entry_normal_state (dialog->search_engine_name_entry);
  return TRUE;
}

static gboolean
dialog_check_address_entry (const char             *address,
                            EphySearchEngineDialog *dialog)
{
  if (address == NULL || !soup_uri_new (address) || strstr (address, "%s") == NULL) {
    dialog_set_entry_error_state (dialog->search_engine_address_entry);
    return FALSE;
  }
  else
    dialog_set_entry_normal_state (dialog->search_engine_address_entry);

  return TRUE;
}

static gboolean
dialog_check_bang_entry (const char             *bang,
                         EphySearchEngineDialog *dialog)
{
  EphySearchEngineManager *manager;
  GtkListBoxRow *list_box_row;
  GtkWidget *search_engine_label;
  const char *engine_from_bang;
  const char *search_engine_name;

  /* Allow empty string */
  if (g_strcmp0 (bang, "") == 0) {
    dialog_set_entry_normal_state (dialog->search_engine_bang_entry);
    return TRUE;
  }

  manager = dialog->search_engine_manager;

  engine_from_bang = ephy_search_engine_manager_engine_from_bang (manager, bang);
  list_box_row = gtk_list_box_get_selected_row (GTK_LIST_BOX (dialog->search_engine_list_box));
  search_engine_label = container_get_first_child (GTK_CONTAINER (list_box_row));
  search_engine_name = gtk_label_get_text (GTK_LABEL (search_engine_label));

  if (engine_from_bang && (g_strcmp0 (engine_from_bang, search_engine_name) != 0)) {
    dialog_set_entry_error_state (dialog->search_engine_bang_entry);
    return FALSE;
  }
  else if (g_strcmp0 (bang, NEW_SEARCH_ENGINE_BANG) == 0) {
    dialog_set_entry_error_state (dialog->search_engine_bang_entry);
    return FALSE;
  }
  else
    dialog_set_entry_normal_state (dialog->search_engine_bang_entry);

  return TRUE;
}

static void
list_box_row_selected_cb (GtkListBox    *list_box,
                          GtkListBoxRow *list_box_row,
                          gpointer       data)
{
  EphySearchEngineDialog *dialog;
  EphySearchEngineManager *manager;
  GtkWidget *search_engine_label;
  const char *search_engine_name;
  const char *search_engine_address;
  const char *search_engine_bang;
  const char *search_engine_default;
  gboolean is_default = FALSE;

  if (!list_box_row)
    return;

  dialog = EPHY_SEARCH_ENGINE_DIALOG (data);
  manager = dialog->search_engine_manager;

  search_engine_label = container_get_first_child (GTK_CONTAINER (list_box_row));
  search_engine_name = gtk_label_get_text (GTK_LABEL (search_engine_label));
  search_engine_address = ephy_search_engine_manager_get_address (manager, search_engine_name);
  search_engine_bang = ephy_search_engine_manager_get_bang (manager, search_engine_name);

  if (!search_engine_address) {
    gtk_entry_set_text (GTK_ENTRY (dialog->search_engine_name_entry), "");
    gtk_entry_set_text (GTK_ENTRY (dialog->search_engine_address_entry), "");
    gtk_entry_set_text (GTK_ENTRY (dialog->search_engine_bang_entry), "");
    gtk_entry_set_placeholder_text (GTK_ENTRY (dialog->search_engine_address_entry),
                                    NEW_SEARCH_ENGINE_ADDRESS);
    gtk_entry_set_placeholder_text (GTK_ENTRY (dialog->search_engine_bang_entry),
                                    NEW_SEARCH_ENGINE_BANG);
    gtk_entry_set_placeholder_text (GTK_ENTRY (dialog->search_engine_name_entry),
                                    search_engine_name);
    search_engine_name = "";
  }
  else
  {
    gtk_entry_set_text (GTK_ENTRY (dialog->search_engine_name_entry), search_engine_name);
    gtk_entry_set_text (GTK_ENTRY (dialog->search_engine_address_entry), search_engine_address);
    gtk_entry_set_text (GTK_ENTRY (dialog->search_engine_bang_entry), search_engine_bang);

    search_engine_default = ephy_search_engine_manager_get_default_engine (manager);
    if (g_strcmp0 (search_engine_name, search_engine_default) == 0)
      is_default = TRUE;
  }

  gtk_switch_set_active (GTK_SWITCH (dialog->search_engine_default_switch),
                         is_default);
  gtk_widget_set_sensitive (dialog->search_engine_default_switch, !is_default);

  dialog_check_name_entry (search_engine_name,
                           dialog);
  dialog_check_address_entry (search_engine_address,
                              dialog);
  dialog_check_bang_entry (search_engine_bang,
                           dialog);
}

static void
ephy_search_engine_dialog_fill_list_box (EphySearchEngineDialog *dialog)
{

  GtkListBox *listbox;
  GtkWidget *list_box_row;
  EphySearchEngineManager *manager;
  char **engines_names ;

  listbox = GTK_LIST_BOX (dialog->search_engine_list_box);
  manager = dialog->search_engine_manager;
  engines_names = ephy_search_engine_manager_get_names (manager);

  for (guint i = 0; engines_names[i] != NULL; i++) {
    const char *name = engines_names[i];
    list_box_row = add_list_box_row (dialog, name, i);
    gtk_list_box_select_row (listbox, GTK_LIST_BOX_ROW (list_box_row));
  }
  g_strfreev (engines_names);

  g_signal_connect (listbox,
                    "row-selected",
                    G_CALLBACK (list_box_row_selected_cb),
                    dialog);
  gtk_list_box_select_row (listbox,
                           gtk_list_box_get_row_at_index (listbox, 0));
}

static char *
generate_new_unique_default_engine_name (EphySearchEngineDialog *dialog)
{
  guint i = 1;
  char *default_name = g_strdup_printf ("%s %d", NEW_SEARCH_ENGINE_NAME, i);

  while (dialog_list_box_child_n_occurence (default_name, dialog) != 0)
  {
    if (i == UINT_MAX)
      break;

    i++;
    g_free (default_name);
    default_name = g_strdup_printf ("%s %d", NEW_SEARCH_ENGINE_NAME, i);
  }

  return default_name;
}

static void
on_search_engine_add_button_clicked (GtkButton              *button,
                                     EphySearchEngineDialog *dialog)
{
  GtkWidget *list_box_row;
  char *new_engine_name;

  new_engine_name = generate_new_unique_default_engine_name (dialog);
  list_box_row = add_list_box_row (dialog, new_engine_name, -1);
  g_free (new_engine_name);
  gtk_list_box_select_row (GTK_LIST_BOX (dialog->search_engine_list_box),
                           GTK_LIST_BOX_ROW (list_box_row));
  gtk_list_box_invalidate_sort (GTK_LIST_BOX (dialog->search_engine_list_box));
}

static void
on_search_engine_remove_button_clicked (GtkButton              *button,
                                        EphySearchEngineDialog *dialog)
{
  GtkListBoxRow *list_box_row;
  GtkListBoxRow *prev_list_box_row;
  EphySearchEngineManager *manager;
  GtkWidget *search_engine_label;
  const char *default_search_engine;
  const char *search_engine_name;
  GList *children;
  guint children_number;
  guint index;

  /* It should remains at least one search engine */
  children = gtk_container_get_children (GTK_CONTAINER (dialog->search_engine_list_box));
  children_number = g_list_length (children);
  g_list_free (children);
  if (children_number <= 1)
    return;

  list_box_row = gtk_list_box_get_selected_row (GTK_LIST_BOX (dialog->search_engine_list_box));

  search_engine_label = container_get_first_child (GTK_CONTAINER (list_box_row));
  search_engine_name = gtk_label_get_text (GTK_LABEL (search_engine_label));
  manager = dialog->search_engine_manager;
  ephy_search_engine_manager_delete_engine (manager, search_engine_name);

  /* Select the previous row before removing the current one */
  index = gtk_list_box_row_get_index (list_box_row);
  if (index == 0)
    index = 2; // Trick in order to select the futur first row
  prev_list_box_row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (dialog->search_engine_list_box),
                                                     index - 1);
  /* If the removed engine is the default, choose the newly selected as default */
  default_search_engine = ephy_search_engine_manager_get_default_engine (dialog->search_engine_manager);

  if (g_strcmp0(default_search_engine, search_engine_name) == 0) {
    search_engine_label = container_get_first_child (GTK_CONTAINER (prev_list_box_row));
    search_engine_name = gtk_label_get_text (GTK_LABEL (search_engine_label));
    ephy_search_engine_manager_set_default_engine (dialog->search_engine_manager,
                                                   search_engine_name);
  }

  gtk_list_box_select_row (GTK_LIST_BOX (dialog->search_engine_list_box),
                           prev_list_box_row);

  gtk_container_remove (GTK_CONTAINER (dialog->search_engine_list_box),
                        GTK_WIDGET (list_box_row));
  gtk_list_box_invalidate_sort (GTK_LIST_BOX (dialog->search_engine_list_box));
}

static gboolean
on_default_search_engine_switch_state_modified (GtkWidget              *switch_widget,
                                                gboolean                state,
                                                EphySearchEngineDialog *dialog)
{
  GtkListBoxRow *list_box_row;
  GtkWidget *search_engine_label;
  const char *search_engine_name;
  const char *default_search_engine;
  gboolean active;
  gboolean is_set;

  active = gtk_widget_get_sensitive (switch_widget);
  if (!active)
    return TRUE;

  list_box_row = gtk_list_box_get_selected_row (GTK_LIST_BOX (dialog->search_engine_list_box));
  search_engine_label = container_get_first_child (GTK_CONTAINER (list_box_row));
  search_engine_name = gtk_label_get_text (GTK_LABEL (search_engine_label));
  default_search_engine = ephy_search_engine_manager_get_default_engine (dialog->search_engine_manager);

  if ( g_strcmp0(default_search_engine, search_engine_name) != 0) {
    is_set = ephy_search_engine_manager_set_default_engine (dialog->search_engine_manager,
                                                            search_engine_name);

    if (is_set == FALSE) {
      gtk_switch_set_active (GTK_SWITCH (dialog->search_engine_default_switch),
                             FALSE);
      return FALSE;
    }

    gtk_switch_set_active (GTK_SWITCH (dialog->search_engine_default_switch),
                           TRUE);
    gtk_widget_set_sensitive (dialog->search_engine_default_switch, FALSE);
    return TRUE;
  }
  else
    return FALSE;
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
                                        search_engine_bang_entry);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphySearchEngineDialog,
                                        search_engine_name_entry);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphySearchEngineDialog,
                                        search_engine_address_entry);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphySearchEngineDialog,
                                        search_engine_default_switch);

  gtk_widget_class_bind_template_callback (widget_class, on_search_engine_add_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_search_engine_remove_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_default_search_engine_switch_state_modified);
}

static int
sort_list_box_by_engine_name (GtkListBoxRow *row1,
                              GtkListBoxRow *row2,
                              gpointer       data)
{
  const char *row1_engine_name;
  const char *row2_engine_name;
  GtkWidget *label;

  label = container_get_first_child (GTK_CONTAINER (row1));
  row1_engine_name = gtk_label_get_text (GTK_LABEL (label));

  label = container_get_first_child (GTK_CONTAINER (row2));
  row2_engine_name = gtk_label_get_text (GTK_LABEL (label));

  return g_strcmp0 (row1_engine_name, row2_engine_name);
}

static void
dialog_entry_change_on_event (GtkWidget *entry,
                              EphySearchEngineDialog *dialog)
{
  EphySearchEngineManager *manager;
  GtkListBoxRow *list_box_row;
  GtkWidget *search_engine_label;
  const char *search_engine_name;
  const char *search_engine_bang;
  const char *search_engine_address;
  const char *search_engine_selected_name;
  gboolean valid_name = TRUE;
  gboolean valid_address = TRUE;
  gboolean valid_bang = TRUE;

  manager = dialog->search_engine_manager;

  list_box_row = gtk_list_box_get_selected_row (GTK_LIST_BOX (dialog->search_engine_list_box));
  search_engine_label = container_get_first_child (GTK_CONTAINER (list_box_row));
  search_engine_selected_name = gtk_label_get_text (GTK_LABEL (search_engine_label));
  search_engine_name = gtk_entry_get_text (GTK_ENTRY (dialog->search_engine_name_entry));
  search_engine_address = gtk_entry_get_text (GTK_ENTRY (dialog->search_engine_address_entry));
  search_engine_bang = gtk_entry_get_text (GTK_ENTRY (dialog->search_engine_bang_entry));

  /* If entry name is empty, use list box selected label */
  if (g_strcmp0 (search_engine_name, "") == 0) {
    search_engine_name = search_engine_selected_name;
    gtk_entry_set_text (GTK_ENTRY (dialog->search_engine_name_entry), search_engine_name);
  }
  valid_name = dialog_check_name_entry (search_engine_name,
                                        dialog);
  valid_address = dialog_check_address_entry (search_engine_address,
                                              dialog);
  valid_bang = dialog_check_bang_entry (search_engine_bang,
                                        dialog);
  if (!valid_name || !valid_address || !valid_bang)
    return;

  if (ephy_search_engine_manager_get_address (manager, search_engine_name) == NULL) {
    ephy_search_engine_manager_add_engine (manager,
                                           search_engine_name,
                                           search_engine_address,
                                           search_engine_bang);
  }
  else {
    ephy_search_engine_manager_modify_engine (manager,
                                              search_engine_name,
                                              search_engine_address,
                                              search_engine_bang);
  }

  /* If the name label was not the same as the list box label, update and sort
   * the list box */
  search_engine_name = gtk_entry_get_text (GTK_ENTRY (dialog->search_engine_name_entry));
  if (g_strcmp0 (search_engine_name, search_engine_selected_name) != 0) {
    gtk_label_set_text (GTK_LABEL (search_engine_label), search_engine_name);
    gtk_list_box_invalidate_sort (GTK_LIST_BOX (dialog->search_engine_list_box));
  }
  return;
}

static gboolean
address_entry_on_focus_out_cb (GtkWidget *entry,
                              GdkEvent  *event,
                              gpointer   data)
{
  EphySearchEngineDialog *dialog;
  dialog = EPHY_SEARCH_ENGINE_DIALOG (data);
  dialog_entry_change_on_event (entry, dialog);
  return FALSE;
}

static void
address_entry_on_activate_cb (GtkWidget *entry,
                             gpointer   data)
{
  EphySearchEngineDialog *dialog;
  dialog = EPHY_SEARCH_ENGINE_DIALOG (data);
  dialog_entry_change_on_event (entry, dialog);
}

static gboolean
bang_entry_on_focus_out_cb (GtkWidget *entry,
                            GdkEvent  *event,
                            gpointer   data)
{
  EphySearchEngineDialog *dialog;
  dialog = EPHY_SEARCH_ENGINE_DIALOG (data);
  dialog_entry_change_on_event (entry, dialog);
  return FALSE;
}

static void
bang_entry_on_activate_cb (GtkWidget *entry,
                           gpointer   data)
{
  EphySearchEngineDialog *dialog;
  dialog = EPHY_SEARCH_ENGINE_DIALOG (data);
  dialog_entry_change_on_event (entry, dialog);
}

static void
name_entry_change_on_event (GtkWidget              *entry,
                            EphySearchEngineDialog *dialog)
{
  EphySearchEngineManager *manager;
  GtkListBoxRow *list_box_row;
  GtkWidget *new_list_box_row;
  GtkWidget *search_engine_label;
  const char *search_engine_name;
  const char *new_search_engine_name;
  const char *search_engine_bang;
  const char *search_engine_address;
  const char *default_search_engine;
  gboolean valid_name = TRUE;
  gboolean valid_address = TRUE;
  gboolean valid_bang = TRUE;
  manager = dialog->search_engine_manager;

  list_box_row = gtk_list_box_get_selected_row (GTK_LIST_BOX (dialog->search_engine_list_box));
  search_engine_label = container_get_first_child (GTK_CONTAINER (list_box_row));
  search_engine_name = gtk_label_get_text (GTK_LABEL (search_engine_label));
  new_search_engine_name = gtk_entry_get_text (GTK_ENTRY (entry));
  search_engine_address =  gtk_entry_get_text (GTK_ENTRY (dialog->search_engine_address_entry));
  search_engine_bang = gtk_entry_get_text (GTK_ENTRY (dialog->search_engine_bang_entry));

  if (g_strcmp0 (search_engine_name, new_search_engine_name) == 0) {
    dialog_set_entry_normal_state (dialog->search_engine_name_entry);
    return;
  }

  dialog_set_entry_normal_state (dialog->search_engine_name_entry);
  valid_name = dialog_check_name_entry (new_search_engine_name,
                                        dialog);

  valid_address = dialog_check_address_entry (search_engine_address,
                                              dialog);
  valid_bang = dialog_check_bang_entry (search_engine_bang,
                                        dialog);

  if (!valid_name || !valid_address || !valid_bang)
    return;

  /* Create a new search engine */
  ephy_search_engine_manager_add_engine (manager,
                                         new_search_engine_name,
                                         search_engine_address,
                                         search_engine_bang);

  /* Change the name of the default searche engine too*/
  default_search_engine = ephy_search_engine_manager_get_default_engine (dialog->search_engine_manager);

  if (g_strcmp0(default_search_engine, search_engine_name) == 0)
    ephy_search_engine_manager_set_default_engine (dialog->search_engine_manager,
                                                   new_search_engine_name);

  ephy_search_engine_manager_delete_engine (manager, search_engine_name);

  /* Add new element in the ListBox */
  new_list_box_row = add_list_box_row (dialog,
                                       new_search_engine_name,
                                       -1);
  gtk_container_remove (GTK_CONTAINER (dialog->search_engine_list_box),
                        GTK_WIDGET (list_box_row));
  gtk_list_box_select_row (GTK_LIST_BOX (dialog->search_engine_list_box),
                           GTK_LIST_BOX_ROW (new_list_box_row));
  gtk_list_box_invalidate_sort (GTK_LIST_BOX (dialog->search_engine_list_box));
}

static gboolean
name_entry_on_focus_out_cb (GtkWidget *entry,
                            GdkEvent  *event,
                            gpointer   data)
{
  EphySearchEngineDialog *dialog;
  dialog = EPHY_SEARCH_ENGINE_DIALOG (data);
  name_entry_change_on_event (entry, dialog);
  return FALSE;
}

static void
name_entry_on_activate_cb (GtkWidget *entry,
                           gpointer   data)
{
  EphySearchEngineDialog *dialog;
  dialog = EPHY_SEARCH_ENGINE_DIALOG (data);
  name_entry_change_on_event (entry, dialog);
}

static void
ephy_search_engine_dialog_init (EphySearchEngineDialog *dialog)
{
  EphyEmbedShell *shell;

  shell = ephy_embed_shell_get_default ();
  dialog->search_engine_manager = ephy_embed_shell_get_search_engine_manager (shell);

  gtk_widget_init_template (GTK_WIDGET (dialog));
  gtk_list_box_set_sort_func (GTK_LIST_BOX (dialog->search_engine_list_box),
                              sort_list_box_by_engine_name,
                              NULL,
                              NULL);
  ephy_search_engine_dialog_fill_list_box (dialog);
  gtk_list_box_invalidate_sort (GTK_LIST_BOX (dialog->search_engine_list_box));

  g_signal_connect (dialog->search_engine_address_entry,
                    "focus-out-event",
                    G_CALLBACK (address_entry_on_focus_out_cb),
                    dialog);
  g_signal_connect (dialog->search_engine_address_entry,
                    "activate",
                    G_CALLBACK (address_entry_on_activate_cb),
                    dialog);
  g_signal_connect (dialog->search_engine_bang_entry,
                    "focus-out-event",
                    G_CALLBACK (bang_entry_on_focus_out_cb),
                    dialog);
  g_signal_connect (dialog->search_engine_bang_entry,
                    "activate",
                    G_CALLBACK (bang_entry_on_activate_cb),
                    dialog);
  g_signal_connect (dialog->search_engine_name_entry,
                    "focus-out-event",
                    G_CALLBACK (name_entry_on_focus_out_cb),
                    dialog);
  g_signal_connect (dialog->search_engine_name_entry,
                    "activate",
                    G_CALLBACK (name_entry_on_activate_cb),
                    dialog);
}

EphySearchEngineDialog *
ephy_search_engine_dialog_new (void)
{
  return g_object_new (EPHY_TYPE_SEARCH_ENGINE_DIALOG,
                       "use-header-bar", TRUE,
                       NULL);
}
