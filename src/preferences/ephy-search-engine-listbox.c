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

#define EMPTY_NEW_SEARCH_ENGINE_NAME (_("New search engine"))


/* Goes with _EphyAddEngineButtonMergedModel. Used as a way of detecting if this
 * is the list model item where we should be creating the "Add search engine" row
 * instead of the normal EphySearchEngineRow.
 */
#define EPHY_TYPE_ADD_SEARCH_ENGINE_ROW_ITEM (ephy_add_search_engine_row_item_get_type ())

G_DECLARE_FINAL_TYPE (EphyAddSearchEngineRowItem, ephy_add_search_engine_row_item, EPHY, ADD_SEARCH_ENGINE_ROW_ITEM, GObject)

struct _EphyAddSearchEngineRowItem {
  GObject parent_instance;
};

G_DEFINE_TYPE (EphyAddSearchEngineRowItem, ephy_add_search_engine_row_item, G_TYPE_OBJECT)

static void ephy_add_search_engine_row_item_class_init (EphyAddSearchEngineRowItemClass *klass) {}

static void ephy_add_search_engine_row_item_init (EphyAddSearchEngineRowItem *self) {}

/* This model is only needed because we want to use gtk_list_box_bind_model()
 * while having a "Add search engine" row. In GTK4 we could get our way out
 * using GtkFlattenListModel and 1-item GListStore for the outer and inner (i.e.
 * the list model that only contains one item to indicate "this is an "Add search engine" row")
 * required list models. But we're not GTK4 yet, so we need to proxy all list model
 * calls appropriately to the EphySearchEngineManager one ourselves.
 */
#define EPHY_TYPE_ADD_ENGINE_BUTTON_MERGED_MODEL (ephy_add_engine_button_merged_model_get_type ())

G_DECLARE_FINAL_TYPE (EphyAddEngineButtonMergedModel, ephy_add_engine_button_merged_model, EPHY, ADD_ENGINE_BUTTON_MERGED_MODEL, GObject)

struct _EphyAddEngineButtonMergedModel {
  GObject parent_instance;

  GListModel *model;
  EphyAddSearchEngineRowItem *add_engine_row_item;
};

static void list_model_iface_init (GListModelInterface *iface,
                                   gpointer             iface_data);

G_DEFINE_TYPE_WITH_CODE (EphyAddEngineButtonMergedModel, ephy_add_engine_button_merged_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
inner_model_items_changed_cb (GListModel *list,
                              guint       position,
                              guint       removed,
                              guint       added,
                              gpointer    user_data)
{
  EphyAddEngineButtonMergedModel *self = user_data;

  /* Since we place our custom item at the end of the list model, we can pass
   * items-changed informations unchanged.
   */
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

static GType
list_model_get_item_type_func (GListModel *list)
{
  /* Yes, we're doing this so that EPHY_IS_SEARCH_ENGINE() and
   * EPHY_IS_ADD_SEARCH_ENGINE_ROW_ITEM() will work later.
   */
  return G_TYPE_OBJECT;
}

static guint
list_model_get_n_items_func (GListModel *list)
{
  EphyAddEngineButtonMergedModel *self = (gpointer)list;

  /* +1 for the "add search engine" row placeholder object. */
  return g_list_model_get_n_items (self->model) + 1;
}

static gpointer
list_model_get_item_func (GListModel *list,
                          guint       position)
{
  EphyAddEngineButtonMergedModel *self = (gpointer)list;

  gpointer item = g_list_model_get_item (self->model, position);

  if (item)
    return item;
  else {
    guint n_items = g_list_model_get_n_items (self->model);

    /* Normally, n_items is the length of the list model, so it can't have
     * an associated item in the list model. So when we reach that case,
     * return our "add search engine" row item. Else, we're out of the
     * n_items + 1 range and so we must return NULL to indicate the end
     * of the list model.
     */
    if (position == n_items)
      return g_object_ref (self->add_engine_row_item);
    else
      return NULL;
  }
}

static void
list_model_iface_init (GListModelInterface *iface,
                       gpointer             iface_data)
{
  iface->get_item_type = list_model_get_item_type_func;
  iface->get_n_items = list_model_get_n_items_func;
  iface->get_item = list_model_get_item_func;
}

static void
ephy_add_engine_button_merged_model_finalize (GObject *object)
{
  EphyAddEngineButtonMergedModel *self = (gpointer)object;

  g_clear_object (&self->add_engine_row_item);
  self->model = NULL;

  G_OBJECT_CLASS (ephy_add_engine_button_merged_model_parent_class)->finalize (object);
}

static void
ephy_add_engine_button_merged_model_class_init (EphyAddEngineButtonMergedModelClass *klass)
{
  GObjectClass *o_class = G_OBJECT_CLASS (klass);

  o_class->finalize = ephy_add_engine_button_merged_model_finalize;
}

static void
ephy_add_engine_button_merged_model_init (EphyAddEngineButtonMergedModel *self)
{
  self->model = G_LIST_MODEL (ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ()));
  self->add_engine_row_item = g_object_new (EPHY_TYPE_ADD_SEARCH_ENGINE_ROW_ITEM, NULL);

  g_signal_connect_object (self->model, "items-changed", G_CALLBACK (inner_model_items_changed_cb), self, 0);
}

struct _EphySearchEngineListBox {
  GtkBin parent_instance;

  GtkWidget *list;

  /* This widget isn't actually showed anywhere. It is just a stable place where we can add more radio buttons without having to bother if the primary radio button gets removed. */
  GtkWidget *radio_buttons_group;
  GtkWidget *add_search_engine_row;

  EphySearchEngine *empty_new_search_engine;
  EphySearchEngineManager *manager;

  EphyAddEngineButtonMergedModel *wrapper_model;

  /* Used as a flag to avoid expanding the newly created row (which is our
   * default behaviour). It'll only be set to TRUE after the model has been
   * bound to the list box. This avoids having to iterate the list box to
   * unexpand all rows after having expanded them all.
   */
  gboolean is_model_initially_loaded;
};

G_DEFINE_TYPE (EphySearchEngineListBox, ephy_search_engine_list_box, GTK_TYPE_BIN)

GtkWidget *
ephy_search_engine_list_box_new (void)
{
  return g_object_new (EPHY_TYPE_SEARCH_ENGINE_LIST_BOX, NULL);
}

static void
on_search_engine_name_changed_cb (EphySearchEngine        *engine,
                                  GParamSpec              *pspec,
                                  EphySearchEngineListBox *self)
{
  const char *name = ephy_search_engine_get_name (engine);

  /* If that's the empty search engine, then we keep it internally marked
   * as "this was the empty search engine", since we don't have a way
   * of knowing the previous name in notify:: callbacks. That won't be an
   * issue even if someone tries naming another search engine with the same
   * EMPTY_NEW_SEARCH_ENGINE_NAME, as the row entry's validation prevents
   * this from happening (it checks if there's already a search engine with
   * that name before setting this particular engine's name in the manager).
   */
  if (g_strcmp0 (name, EMPTY_NEW_SEARCH_ENGINE_NAME) == 0) {
    self->empty_new_search_engine = engine;
    gtk_widget_set_sensitive (self->add_search_engine_row, FALSE);
  }
  /* This search engine was the only "new empty" one, and it is no longer
   * the "new empty" search engine, so allow adding a new search engine again.
   */
  else if (engine == self->empty_new_search_engine &&
           g_strcmp0 (name, EMPTY_NEW_SEARCH_ENGINE_NAME) != 0) {
    self->empty_new_search_engine = NULL;
    gtk_widget_set_sensitive (self->add_search_engine_row, TRUE);
  }
}

static void
on_list_box_manager_items_changed_cb (GListModel *list,
                                      guint       position,
                                      guint       removed,
                                      guint       added,
                                      gpointer    user_data)
{
  EphySearchEngineListBox *self = EPHY_SEARCH_ENGINE_LIST_BOX (user_data);
  EphySearchEngineManager *manager = EPHY_SEARCH_ENGINE_MANAGER (list);

  /* This callback is mostly only called when a search engine has been removed
   * (potentially the new empty one), when clicking the Add Search Engine button
   * (in which case we'll want to make the button insensitive), or when initially
   * loading all the search engines. In all those cases, we check if we have the
   * "empty new search engine" and update the Add Search Engine's button sensitivity
   * based on it.
   */
  self->empty_new_search_engine = ephy_search_engine_manager_find_engine_by_name (manager, EMPTY_NEW_SEARCH_ENGINE_NAME);

  gtk_widget_set_sensitive (self->add_search_engine_row,
                            self->empty_new_search_engine == NULL);
}

/* This signal unexpands all other rows of the list box except the row
 * that just got expanded.
 */
static void
on_row_expand_state_changed_cb (EphySearchEngineRow     *expanded_row,
                                GParamSpec              *pspec,
                                EphySearchEngineListBox *self)
{
  GtkListBoxRow *row;
  int i = 0;

  /* We only unexpand other rows if this is a notify signal for an expanded row. */
  if (!hdy_expander_row_get_expanded (HDY_EXPANDER_ROW (expanded_row)))
    return;

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->list), i++))) {
    /* Ignore this row if not a search engine row ("add search engine" row). */
    if (!EPHY_IS_SEARCH_ENGINE_ROW (row))
      continue;

    /* Ignore the row that was just expanded. */
    if (row == GTK_LIST_BOX_ROW (expanded_row))
      continue;

    hdy_expander_row_set_expanded (HDY_EXPANDER_ROW (row), FALSE);
  }
}

static void
on_add_search_engine_row_clicked_cb (EphySearchEngineListBox *self,
                                     GtkListBoxRow           *clicked_row)
{
  g_autoptr (EphySearchEngine) empty_engine = NULL;

  /* Sanity check. Expander rows aren't supposed to be activable=True. */
  g_assert ((gpointer)clicked_row == (gpointer)self->add_search_engine_row);

  empty_engine = g_object_new (EPHY_TYPE_SEARCH_ENGINE,
                               "name", EMPTY_NEW_SEARCH_ENGINE_NAME,
                               "url", "https://www.example.com/search?q=%s",
                               NULL);
  ephy_search_engine_manager_add_engine (self->manager, empty_engine);

  /* In on_search_engine_name_changed_cb above, we set the Add search engine
   * row's sensitivity based on whether there is an engine named EMPTY_NEW_SEARCH_ENGINE_NAME.
   */
}

static GtkWidget *
create_add_search_engine_row ()
{
  GtkWidget *row = gtk_list_box_row_new ();
  GtkWidget *label = gtk_label_new_with_mnemonic (_("A_dd Search Engine…"));

  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), true);
  gtk_widget_set_size_request (row, -1, 50);
  gtk_widget_show (row);

  gtk_widget_show (label);
  gtk_container_add (GTK_CONTAINER (row), label);

  return row;
}

static GtkWidget *
create_search_engine_row (EphySearchEngine        *engine,
                          EphySearchEngineListBox *self)
{
  EphySearchEngineRow *row = ephy_search_engine_row_new (engine, self->manager);

  g_signal_connect_object (engine, "notify::name", G_CALLBACK (on_search_engine_name_changed_cb), self, 0);

  ephy_search_engine_row_set_radio_button_group (row,
                                                 GTK_RADIO_BUTTON (self->radio_buttons_group));
  g_signal_connect (row,
                    "notify::expanded",
                    G_CALLBACK (on_row_expand_state_changed_cb),
                    self);

  /* This check ensures we don't try expanding all rows when we initially bind
   * the model to the list box.
   */
  if (self->is_model_initially_loaded) {
    /* This will also unexpand all other rows, to make the new one stand out,
     * in on_row_expand_state_changed_cb().
     */
    hdy_expander_row_set_expanded (HDY_EXPANDER_ROW (row), TRUE);
  }

  return GTK_WIDGET (row);
}

static GtkWidget *
list_box_create_row_func (gpointer item,
                          gpointer user_data)
{
  EphySearchEngineListBox *self = EPHY_SEARCH_ENGINE_LIST_BOX (user_data);

  g_assert (item != NULL);

  if (EPHY_IS_SEARCH_ENGINE (item)) {
    EphySearchEngine *engine = item;
    return create_search_engine_row (engine, self);
  } else if (EPHY_IS_ADD_SEARCH_ENGINE_ROW_ITEM (item)) {
    self->add_search_engine_row = create_add_search_engine_row ();
    return self->add_search_engine_row;
  } else {
    g_assert_not_reached ();
  }
}

static void
ephy_search_engine_list_box_finalize (GObject *object)
{
  EphySearchEngineListBox *self = (EphySearchEngineListBox *)object;

  g_clear_object (&self->radio_buttons_group);
  g_clear_object (&self->wrapper_model);

  G_OBJECT_CLASS (ephy_search_engine_list_box_parent_class)->finalize (object);
}

static void
ephy_search_engine_list_box_class_init (EphySearchEngineListBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ephy_search_engine_list_box_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/search-engine-listbox.ui");

  gtk_widget_class_bind_template_child (widget_class, EphySearchEngineListBox, list);
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

  self->wrapper_model = g_object_new (EPHY_TYPE_ADD_ENGINE_BUTTON_MERGED_MODEL, NULL);
  self->is_model_initially_loaded = FALSE;
  gtk_list_box_bind_model (GTK_LIST_BOX (self->list),
                           G_LIST_MODEL (self->wrapper_model),
                           (GtkListBoxCreateWidgetFunc)list_box_create_row_func,
                           self, NULL);
  self->is_model_initially_loaded = TRUE;

  /* When the row's radio button gets parented all the way up to the window,
   * it seems like GTK sets one of the radio button in the group as clicked,
   * but messes things up somewhere. Whatever we do to click or not click this
   * particular radio button when creating our row widget depending on whether
   * it is the default engine, all the rows end up not "ticked". To circumvent
   * this, just trick the manager into sending a dummy notify:: signal so that
   * the row which matches the default engine updates its own radio button state.
   * This is the cleanest way I found to workaround the issue.
   */
  ephy_search_engine_manager_set_default_engine (self->manager,
                                                 ephy_search_engine_manager_get_default_engine (self->manager));

  on_list_box_manager_items_changed_cb (G_LIST_MODEL (self->manager),
                                        0,
                                        0,
                                        g_list_model_get_n_items (G_LIST_MODEL (self->manager)),
                                        self);
  g_signal_connect_object (self->manager, "items-changed", G_CALLBACK (on_list_box_manager_items_changed_cb), self, 0);
}
