/* ephy-add-opensearch-engine-button.c
 *
 * Copyright 2021 vanadiae <vanadiae35@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"
#include "ephy-add-opensearch-engine-button.h"

#include "ephy-embed-shell.h"
#include "ephy-opensearch-autodiscovery-link.h"
#include "ephy-opensearch-engine.h"
#include "ephy-search-engine-listbox.h"
#include "ephy-search-engine-manager.h"
#include "ephy-search-engine-row.h"
#include "ephy-shell.h"
#include "prefs-general-page.h"

struct _EphyAddOpensearchEngineButton {
  GtkButton parent_instance;

  /* The **filtered** opensearch_engines model from EphyWebView, which changes when switching tab. */
  GListModel *model;
  GCancellable *cancellable;

  GtkListBox *list_box;
  GtkWidget *popover;
};

G_DEFINE_FINAL_TYPE (EphyAddOpensearchEngineButton, ephy_add_opensearch_engine_button, GTK_TYPE_BUTTON)

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

EphyAddOpensearchEngineButton *
ephy_add_opensearch_engine_button_new (void)
{
  return g_object_new (EPHY_TYPE_ADD_OPENSEARCH_ENGINE_BUTTON, NULL);
}

static void
on_model_items_changed_cb (GListModel                    *model,
                           guint                          position,
                           guint                          removed,
                           guint                          added,
                           EphyAddOpensearchEngineButton *self)
{
  /* Only show the button if there is any search engine that can be added. */
  gtk_widget_set_visible (GTK_WIDGET (self), g_list_model_get_n_items (model) != 0);
}

static void
row_expanded_cb (GObject    *object,
                 GParamSpec *pspec,
                 gpointer    user_data)
{
  EphySearchEngineRow *row = EPHY_SEARCH_ENGINE_ROW (object);

  /* FIXME: for some reason our "mapped"+expanded thing is kinda broken when we're
   * doing it just after starting to show the window, as it scrolls until the row
   * itself is reached but not the bang entry itself.
   */
  /* ephy_search_engine_row_focus_bang_entry (row); */
  g_signal_handlers_disconnect_by_func (row, row_expanded_cb, user_data);
}

static void
row_mapped_cb (GObject  *object,
               gpointer  user_data)
{
  EphySearchEngineRow *row = EPHY_SEARCH_ENGINE_ROW (object);
  g_signal_connect (row, "notify::expanded", G_CALLBACK (row_expanded_cb), NULL);
  /* Now expand the just added engine row and focus it so that the GtkViewport
   * scrolls to it automatically.
   */
  adw_expander_row_set_expanded (ADW_EXPANDER_ROW (row), TRUE);
  g_signal_handlers_disconnect_by_func (row, row_mapped_cb, user_data);
}

static void
scroll_to_engine_in_prefs (EphyAddOpensearchEngineButton *self,
                           EphySearchEngine              *engine)
{
  AdwPreferencesDialog *prefs_dialog = ADW_PREFERENCES_DIALOG (ephy_shell_get_prefs_dialog (ephy_shell_get_default ()));
  PrefsGeneralPage *general_page;
  EphySearchEngineListBox *search_engine_list_box;
  EphySearchEngineRow *row;

  /* Don't rely on the general page being the first one opened: make sure it is
   * actually the visible one.
   */
  general_page = EPHY_PREFS_GENERAL_PAGE (adw_preferences_dialog_get_visible_page (prefs_dialog));
  search_engine_list_box = prefs_general_page_get_search_engine_list_box (general_page);
  row = ephy_search_engine_list_box_find_row_for_engine (search_engine_list_box, engine);
  /* We just added the engine so there must be a corresponding row. */
  g_assert (EPHY_IS_SEARCH_ENGINE_ROW (row));
  g_signal_connect (row, "map", G_CALLBACK (row_mapped_cb), NULL);
  gtk_widget_activate_action (GTK_WIDGET (self), "app.preferences", NULL);
}

typedef struct  {
  EphyAddOpensearchEngineButton *self;
  GtkListBoxRow *row;
} LoadEngine;

static void
on_opensearch_engine_loaded_cb (EphyOpensearchAutodiscoveryLink *autodiscovery_link,
                                GAsyncResult                    *result,
                                gpointer                         user_data)
{
  g_autofree LoadEngine *data = user_data;
  EphyAddOpensearchEngineButton *self = data->self;
  g_autoptr (GError) error = NULL;
  g_autoptr (EphySearchEngine) engine = NULL;

  g_assert (EPHY_IS_OPENSEARCH_AUTODISCOVERY_LINK (autodiscovery_link));

  engine = ephy_opensearch_engine_load_from_link_finish (autodiscovery_link, result, &error);
  if (!engine) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GtkLabel *row_label = GTK_LABEL (gtk_list_box_row_get_child (data->row));
      /* Yes this is a terrible way of showing an error message, but libadwaita's
       * stylesheet is confused if I use a status page and list box inside a GtkStack
       * when the .menu CSS style class is used on the popover. So instead just
       * replace the row's name with the error message, with error styling. This is
       * not ideal at all and not completely immune to models changes (as this is
       * not stored there but manually changed in the widget tree), but that should
       * suffice for the few cases where we fail to load the search engine.
       */
      gtk_widget_add_css_class (GTK_WIDGET (row_label), "error");
      gtk_label_set_text (row_label, error->message);
    }
  } else {
    EphySearchEngineManager *manager =
      ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());
    g_autofree char *bang = ephy_search_engine_build_bang_for_name (ephy_search_engine_get_name (engine));

    /* Pre-fill the bang if it wouldn't conflict with another search engine,
     * giving the hint that it can/should be changed for a more suited one
     * if wanted by focusing the bang entry afterwards.
     */
    if (!ephy_search_engine_manager_has_bang (manager, bang))
      ephy_search_engine_set_bang (engine, bang);

    ephy_search_engine_manager_add_engine (manager, engine);
    ephy_search_engine_manager_save_to_settings (manager);

    /* Now all web view will get notified for this newly added engine, and remove
     * their corresponding autodiscovery link in the models. We do not do it
     * manually here for self->model because that would not remove the link for
     * all the other tabs, so that's why we let the EphyWebViews take care of it.
     */

    scroll_to_engine_in_prefs (self, engine);
  }
}

static void
on_opensearch_row_activated (GtkListBox                    *box,
                             GtkListBoxRow                 *row,
                             EphyAddOpensearchEngineButton *self)
{
  EphyOpensearchAutodiscoveryLink *autodiscovery_link =
    g_object_get_data (G_OBJECT (row), "opensearch_item");
  LoadEngine *data = g_new0 (LoadEngine, 1);
  data->self = self;
  data->row = row;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();
  ephy_opensearch_engine_load_from_link_async (autodiscovery_link,
                                               self->cancellable,
                                               (GAsyncReadyCallback)on_opensearch_engine_loaded_cb,
                                               data);
}

static GtkWidget *
create_opensearch_engine_row (gpointer item,
                              gpointer user_data)
{
  EphyOpensearchAutodiscoveryLink *autodiscovery_link = EPHY_OPENSEARCH_AUTODISCOVERY_LINK (item);
  GtkWidget *row = gtk_list_box_row_new ();
  g_autofree char *escaped_name = g_markup_escape_text (ephy_opensearch_autodiscovery_link_get_name (autodiscovery_link), -1);
  /* TRANSLATORS: %s is the name of the search engine for this row in the popover. */
  g_autofree char *label_str =
    g_strdup_printf (_("Add “%s” as Search Engine"), escaped_name);
  GtkWidget *label = gtk_label_new (label_str);

  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), label);

  /* Of course since models with GtkListBox are more of an afterthought,
   * it's more difficult to get the list model item corresponding to a row
   * in the row-activated signal. So just attach it as GObject data to make
   * things easier.
   */
  g_object_set_data (G_OBJECT (row), "opensearch_item", autodiscovery_link);

  return row;
}

static void
ephy_add_opensearch_engine_button_finalize (GObject *object)
{
  EphyAddOpensearchEngineButton *self = (EphyAddOpensearchEngineButton *)object;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->model);

  G_OBJECT_CLASS (ephy_add_opensearch_engine_button_parent_class)->finalize (object);
}

static void
ephy_add_opensearch_engine_button_dispose (GObject *object)
{
  EphyAddOpensearchEngineButton *self = (EphyAddOpensearchEngineButton *)object;

  gtk_widget_unparent (self->popover);

  G_OBJECT_CLASS (ephy_add_opensearch_engine_button_parent_class)->dispose (object);
}

static void
ephy_add_opensearch_engine_button_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
  EphyAddOpensearchEngineButton *self = EPHY_ADD_OPENSEARCH_ENGINE_BUTTON (object);

  switch (prop_id) {
    case PROP_MODEL:
      ephy_add_opensearch_engine_button_set_model (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
on_clicked (GtkWidget *button,
            gpointer   user_data)
{
  EphyAddOpensearchEngineButton *self = EPHY_ADD_OPENSEARCH_ENGINE_BUTTON (user_data);

  gtk_popover_popup (GTK_POPOVER (self->popover));
}

static void
ephy_add_opensearch_engine_button_class_init (EphyAddOpensearchEngineButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ephy_add_opensearch_engine_button_finalize;
  object_class->dispose = ephy_add_opensearch_engine_button_dispose;
  object_class->set_property = ephy_add_opensearch_engine_button_set_property;

  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                         "model",
                         "The search engines model that the popover displays.",
                         G_TYPE_LIST_MODEL, G_PARAM_STATIC_STRINGS | G_PARAM_WRITABLE);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/add-opensearch-engine-button.ui");

  gtk_widget_class_bind_template_callback (widget_class, on_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_opensearch_row_activated);
  gtk_widget_class_bind_template_child (widget_class, EphyAddOpensearchEngineButton, list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyAddOpensearchEngineButton, popover);
}

static void
ephy_add_opensearch_engine_button_init (EphyAddOpensearchEngineButton *self)
{
  self->cancellable = g_cancellable_new ();
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_parent (self->popover, GTK_WIDGET (self));
}

static void
search_engine_items_changed_cb (GListModel *manager,
                                guint       position,
                                guint       removed,
                                guint       added,
                                gpointer    user_data)
{
  /* Filter out the links corresponding to any matching search engine that was just added or removed. */
  if (added > 0)
    gtk_filter_changed (gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (user_data)), GTK_FILTER_CHANGE_MORE_STRICT);
  if (removed > 0)
    gtk_filter_changed (gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (user_data)), GTK_FILTER_CHANGE_LESS_STRICT);
}

static gboolean
filter_opensearch_links (GObject  *item,
                         gpointer  user_data)
{
  EphySearchEngineManager *manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());
  EphyOpensearchAutodiscoveryLink *autodiscovery_link = EPHY_OPENSEARCH_AUTODISCOVERY_LINK (item);
  guint n_engines = g_list_model_get_n_items (G_LIST_MODEL (manager));

  /* The idea here is to detect when a search engine is added from the opensearch
   * button, and to remove its corresponding autodiscovery link from our model
   * so that it do not show up anymore in the list, for each tab.
   */
  for (guint i = 0; i < n_engines; i++) {
    g_autoptr (EphySearchEngine) engine = EPHY_SEARCH_ENGINE (g_list_model_get_item (G_LIST_MODEL (manager), i));

    /* Filter out any autodiscovery link for which there is a corresponding engine. */
    if (ephy_search_engine_matches_by_autodiscovery_link (engine, autodiscovery_link))
      return FALSE;
  }

  return TRUE;
}

/**
 * ephy_add_opensearch_engine_button_set_model:
 * @self: an #EphyAddOpensearchEngineButton
 * @model: a #GListModel containing #EphyOpensearchAutodiscoveryLink objects
 *   representing each engine that was detected in the web page.
 *
 * Sets the model used to display the discovered OpenSearch engines in @self's list.
 */
void
ephy_add_opensearch_engine_button_set_model (EphyAddOpensearchEngineButton *self,
                                             GListModel                    *model)
{
  EphySearchEngineManager *manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());

  g_assert (EPHY_IS_ADD_OPENSEARCH_ENGINE_BUTTON (self));
  g_assert (G_IS_LIST_MODEL (model));

  /* self->model being NULL happens when setting the model initially */
  if (self->model) {
    /* First, avoid having previous models from other web view
     * still update the visibility of the button when browsing
     * to new pages.
     */
    g_signal_handlers_disconnect_by_func (self->model,
                                          G_CALLBACK (on_model_items_changed_cb),
                                          self);

    g_signal_handlers_disconnect_by_func (manager,
                                          G_CALLBACK (search_engine_items_changed_cb),
                                          self->model);
  }

  g_clear_object (&self->model);
  /* Now setup everything back again with the new model. */
  self->model = G_LIST_MODEL (gtk_filter_list_model_new (g_object_ref (model), GTK_FILTER (gtk_custom_filter_new ((GtkCustomFilterFunc)filter_opensearch_links, NULL, NULL))));
  g_signal_connect_object (manager,
                           "items-changed",
                           G_CALLBACK (search_engine_items_changed_cb),
                           self->model, 0);
  on_model_items_changed_cb (self->model, 0, 0, g_list_model_get_n_items (self->model), self);
  g_signal_connect_object (self->model,
                           "items-changed",
                           G_CALLBACK (on_model_items_changed_cb),
                           self, 0);
  gtk_list_box_bind_model (GTK_LIST_BOX (self->list_box),
                           self->model,
                           create_opensearch_engine_row,
                           NULL,
                           NULL);
}
