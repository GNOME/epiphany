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
  AdwBin parent_instance;

  GtkWidget *list;

  /* This widget isn't actually showed anywhere. It is just a stable place where
   * we can add more radio buttons without having to bother if the primary radio
   * button gets removed.
   */
  GtkWidget *radio_buttons_group;

  EphySearchEngineManager *manager;

  /* Used as a flag to avoid expanding the newly created row (which is our
   * default behaviour). It'll only be set to TRUE after the model has been
   * bound to the list box. This avoids having to iterate the list box to
   * unexpand all rows after having expanded them all.
   */
  gboolean is_model_initially_loaded;
};

G_DEFINE_FINAL_TYPE (EphySearchEngineListBox, ephy_search_engine_list_box, ADW_TYPE_BIN)

GtkWidget *
ephy_search_engine_list_box_new (void)
{
  return g_object_new (EPHY_TYPE_SEARCH_ENGINE_LIST_BOX, NULL);
}

/* This signal unexpands all other rows of the list box except the row
 * that just got expanded.
 */
static void
on_row_expand_state_changed_cb (AdwExpanderRow          *expanded_row,
                                GParamSpec              *pspec,
                                EphySearchEngineListBox *self)
{
  GtkListBoxRow *row;
  int i = 0;

  /* We only unexpand other rows if this is a notify signal for an expanded row. */
  if (!adw_expander_row_get_expanded (expanded_row))
    return;

  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->list), i++))) {
    /* Ignore the row that was just expanded. */
    if (!ADW_IS_EXPANDER_ROW (row) || ADW_EXPANDER_ROW (row) == expanded_row)
      continue;

    adw_expander_row_set_expanded (ADW_EXPANDER_ROW (row), FALSE);
  }
}

static void
row_expanded_cb (GObject    *object,
                 GParamSpec *pspec,
                 gpointer    user_data)
{
  EphySearchEngineRow *row = EPHY_SEARCH_ENGINE_ROW (object);

  ephy_search_engine_row_focus_name_entry (row);
  g_signal_handlers_disconnect_by_func (object, row_expanded_cb, user_data);
}

static void
row_mapped_cb (GObject  *row,
               gpointer  user_data)
{
  g_signal_connect (row, "notify::expanded", G_CALLBACK (row_expanded_cb), NULL);
  /* This will also unexpand all other rows, to make the new one stand out,
   * in on_row_expand_state_changed_cb().
   */
  adw_expander_row_set_expanded (ADW_EXPANDER_ROW (row), TRUE);
  g_signal_handlers_disconnect_by_func (row, row_mapped_cb, user_data);
}

#define EMPTY_NEW_SEARCH_ENGINE_NAME (_("New search engine"))

static void
on_add_button_activated (AdwButtonRow *button,
                         gpointer      user_data)
{
  EphySearchEngineListBox *self = EPHY_SEARCH_ENGINE_LIST_BOX (user_data);
  g_autoptr (EphySearchEngine) empty_engine = NULL;

  empty_engine = g_object_new (EPHY_TYPE_SEARCH_ENGINE,
                               "name", EMPTY_NEW_SEARCH_ENGINE_NAME,
                               "url", "https://www.example.com/search?q=%s",
                               NULL);
  ephy_search_engine_manager_add_engine (self->manager, empty_engine);
}

static GtkWidget *
list_box_create_row_func (gpointer item,
                          gpointer user_data)
{
  EphySearchEngineListBox *self = EPHY_SEARCH_ENGINE_LIST_BOX (user_data);
  GtkWidget *action_row;

  if (EPHY_IS_SEARCH_ENGINE (item)) {
    EphySearchEngine *engine = EPHY_SEARCH_ENGINE (item);
    EphySearchEngineRow *row = ephy_search_engine_row_new (engine, self->manager);

    g_signal_connect (row,
                      "notify::expanded",
                      G_CALLBACK (on_row_expand_state_changed_cb),
                      self);

    /* This check ensures we don't try expanding all rows when we initially bind
     * the model to the list box.
     */
    if (self->is_model_initially_loaded) {
      /* We need to wait until the widget is mapped (i.e. ready to be drawn),
       * to connect to the "expanded" signal because the animation will not start
       * before the widget is mapped (or rather, finish instantaneously and
       * focusing won't work).
        */
      g_signal_connect (row, "map", G_CALLBACK (row_mapped_cb), NULL);
    }

    return GTK_WIDGET (row);
  }

  action_row = adw_button_row_new ();
  g_signal_connect (G_OBJECT (action_row), "activated", G_CALLBACK (on_add_button_activated), self);
  adw_button_row_set_start_icon_name (ADW_BUTTON_ROW (action_row), "list-add-symbolic");
  adw_preferences_row_set_use_underline (ADW_PREFERENCES_ROW (action_row), true);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (action_row), _("A_dd Search Engine"));

  return action_row;
}

static void
ephy_search_engine_list_box_finalize (GObject *object)
{
  EphySearchEngineListBox *self = (EphySearchEngineListBox *)object;

  g_clear_object (&self->radio_buttons_group);

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
}

static void
ephy_search_engine_list_box_init (EphySearchEngineListBox *self)
{
  GtkFlattenListModel *flatten_list;
  GListStore *store;
  GListStore *store_b;

  self->manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->radio_buttons_group = gtk_check_button_new ();
  /* Ref the radio buttons group and remove the floating reference because we
   * don't hook this widget to any part of the UI (for example gtk_container_add
   * would remove the floating reference and ref it).
   */
  g_object_ref_sink (self->radio_buttons_group);

  self->is_model_initially_loaded = FALSE;

  store = g_list_store_new (G_TYPE_LIST_MODEL);
  g_list_store_append (store, G_LIST_MODEL (self->manager));

  store_b = g_list_store_new (ADW_TYPE_ACTION_ROW);
  g_list_store_append (store_b, adw_action_row_new ());
  g_list_store_append (store, G_LIST_MODEL (store_b));

  flatten_list = gtk_flatten_list_model_new (G_LIST_MODEL (store));

  gtk_list_box_bind_model (GTK_LIST_BOX (self->list),
                           G_LIST_MODEL (flatten_list),
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
}

/**
 * ephy_search_engine_listbox_find_row_for_engine:
 *
 * Returns: (transfer none) (nullable): the #EphySearchEngineRow that currently display @engine
 * in @self, or %NULL if not found.
 */
EphySearchEngineRow *
ephy_search_engine_list_box_find_row_for_engine (EphySearchEngineListBox *self,
                                                 EphySearchEngine        *engine)
{
  GtkListBoxRow *row;
  guint i = 0;
  while ((row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->list), i++))) {
    if (ephy_search_engine_row_get_engine (EPHY_SEARCH_ENGINE_ROW (row)) == engine) {
      return EPHY_SEARCH_ENGINE_ROW (row);
    }
  }
  return NULL;
}
