/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2020 Epiphany Developers
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

#include "ephy-search-engine-listbox.h"

#include "ephy-search-engine-row.h"
#include "embed/ephy-embed-shell.h"
#include "ephy-search-engine-manager.h"

struct _EphySearchEngineListBox {
  GtkListBox parent_instance;

  /* This widget isn't actually showed anywhere. It is just a stable place where we can add more radio buttons without having to bother if the primary radio button gets removed. */
  GtkWidget *radio_buttons_group;

  GtkWidget *add_search_engine_row;
  EphySearchEngineManager *manager;
};

G_DEFINE_TYPE (EphySearchEngineListBox, ephy_search_engine_list_box, GTK_TYPE_LIST_BOX)

GtkWidget *
ephy_search_engine_list_box_new (void)
{
  return g_object_new (EPHY_TYPE_SEARCH_ENGINE_LIST_BOX, NULL);
}

/**
 * ephy_search_engine_listbox_set_can_add_engine:
 *
 * Sets whether the "Add search engine" row of @self is sensitive.
 *
 * @self: a #EphySearchEngineListBox
 * @can_add_engine: whether the user can add new search engines to @self
 */
void
ephy_search_engine_list_box_set_can_add_engine (EphySearchEngineListBox *self,
                                                gboolean                 can_add_engine)
{
  gtk_widget_set_sensitive (self->add_search_engine_row, can_add_engine);
}

/***** Private *****/

static void
on_row_expand_state_changed_cb (EphySearchEngineRow     *expanded_row,
                                GParamSpec              *pspec,
                                EphySearchEngineListBox *parent_list_box)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (parent_list_box));

  /* We only unexpand other rows if this is a notify signal for an expanded row. */
  if (!hdy_expander_row_get_expanded (HDY_EXPANDER_ROW (expanded_row)))
    return;

  for (; children->next != NULL; children = children->next) {
    EphySearchEngineRow *iterated_row = children->data;

    /* Ignore this row if not a search engine row ("add search engine" row). */
    if (!EPHY_IS_SEARCH_ENGINE_ROW (iterated_row))
      continue;

    /* Ignore the row that was just expanded. */
    if (iterated_row == expanded_row)
      continue;

    hdy_expander_row_set_expanded (HDY_EXPANDER_ROW (iterated_row), FALSE);
  }
}

/**
 * append_search_engine_row:
 *
 * Creates a new row showing search engine @engine_name, and adds
 * it to @search_engine_list_box.
 *
 * @search_engine_list_box: an #EphySearchEngineListBox
 * @engine_name: the name of an already existing engine in @search_engine_list_box->manager which will be presented as a new row
 *
 * Returns: the newly added row.
 */
static EphySearchEngineRow *
append_search_engine_row (EphySearchEngineListBox *list_box,
                          const char              *engine_name)
{
  EphySearchEngineRow *new_row = ephy_search_engine_row_new (engine_name);

  gtk_list_box_prepend (GTK_LIST_BOX (list_box),
                        GTK_WIDGET (new_row));
  ephy_search_engine_row_set_radio_button_group (new_row,
                                                 GTK_RADIO_BUTTON (list_box->radio_buttons_group));
  g_signal_connect (new_row,
                    "notify::expanded",
                    G_CALLBACK (on_row_expand_state_changed_cb),
                    list_box);

  return new_row;
}

static void
on_add_search_engine_row_clicked_cb (EphySearchEngineListBox *search_engine_list_box,
                                     GtkListBoxRow           *add_search_engine_row,
                                     gpointer                 user_data)
{
  GtkWidget *search_engine_row;

  g_assert (add_search_engine_row == GTK_LIST_BOX_ROW (search_engine_list_box->add_search_engine_row));

  /* Allow to remove the row if it was alone */
  if (gtk_list_box_get_row_at_index (GTK_LIST_BOX (search_engine_list_box), 2) == NULL)
    ephy_search_engine_row_set_can_remove (EPHY_SEARCH_ENGINE_ROW (gtk_list_box_get_row_at_index (GTK_LIST_BOX (search_engine_list_box), 0)),
                                           TRUE);
  ephy_search_engine_manager_add_engine (search_engine_list_box->manager,
                                         EMPTY_NEW_SEARCH_ENGINE_NAME,
                                         "",
                                         "");
  search_engine_row = GTK_WIDGET (append_search_engine_row (search_engine_list_box, EMPTY_NEW_SEARCH_ENGINE_NAME));
  hdy_expander_row_set_expanded (HDY_EXPANDER_ROW (search_engine_row), TRUE);
  /* Only allow one empty search engine to be created. This row will be sensitive again when the empty row is renamed. */
  gtk_widget_set_sensitive (GTK_WIDGET (add_search_engine_row), FALSE);
}

static void
ephy_search_engine_list_box_finalize (GObject *object)
{
  EphySearchEngineListBox *self = (EphySearchEngineListBox *)object;

  g_clear_pointer (&self->radio_buttons_group, g_object_unref);

  G_OBJECT_CLASS (ephy_search_engine_list_box_parent_class)->finalize (object);
}

static void
populate_search_engine_list_box (EphySearchEngineListBox *self)
{
  g_auto (GStrv) engine_names = ephy_search_engine_manager_get_names (self->manager);
  g_autofree char *default_engine = ephy_search_engine_manager_get_default_engine (self->manager);

  for (guint i = 0; engine_names[i] != NULL; ++i) {
    EphySearchEngineRow *row = append_search_engine_row (self, engine_names[i]);
    if (g_strcmp0 (engine_names[i], default_engine) == 0)
      ephy_search_engine_row_set_as_default (row);
  }

  if (ephy_search_engine_manager_engine_exists (self->manager, EMPTY_NEW_SEARCH_ENGINE_NAME))
    gtk_widget_set_sensitive (self->add_search_engine_row, FALSE);
}

static void
ephy_search_engine_list_box_class_init (EphySearchEngineListBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ephy_search_engine_list_box_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/search-engine-listbox.ui");

  gtk_widget_class_bind_template_child (widget_class, EphySearchEngineListBox, add_search_engine_row);
  gtk_widget_class_bind_template_callback (widget_class, on_add_search_engine_row_clicked_cb);
}

static void
ephy_search_engine_list_box_init (EphySearchEngineListBox *self)
{
  self->manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->radio_buttons_group = gtk_radio_button_new (NULL);
  /* Ref the radio buttons group and remove the floating reference because we
   * don't hook this widget to any part of the UI (for example gtk_container_add
   * would remove the floating reference and ref it).
   */
  g_object_ref_sink (self->radio_buttons_group);

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self), ephy_search_engine_row_get_sort_func (), NULL, NULL);
  gtk_list_box_invalidate_sort (GTK_LIST_BOX (self));

  populate_search_engine_list_box (self);

  /* The list box should have at least one "Add search engine" row and one search engine (the default one). */
  if (gtk_list_box_get_row_at_index (GTK_LIST_BOX (self), 2) == NULL)
    ephy_search_engine_row_set_can_remove (EPHY_SEARCH_ENGINE_ROW (gtk_list_box_get_row_at_index (GTK_LIST_BOX (self), 0)), FALSE);
}
