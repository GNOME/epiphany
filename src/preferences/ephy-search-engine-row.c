/* ephy-search-engine-row.c
 *
 * Copyright 2020 vanadiae <vanadiae35@gmail.com>
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


#include "ephy-search-engine-row.h"

#include <glib/gi18n.h>
#include <gmodule.h>

#include "ephy-embed-shell.h"
#include "ephy-search-engine-manager.h"
#include "prefs-search-engine-page.h"

struct _EphySearchEngineRow {
  AdwExpanderRow parent_instance;

  GtkWidget *edit_button;

  EphySearchEngine *engine;
};

G_DEFINE_FINAL_TYPE (EphySearchEngineRow, ephy_search_engine_row, ADW_TYPE_ACTION_ROW)

enum {
  PROP_0,
  PROP_SEARCH_ENGINE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/***** Mostly public functions *****/

/**
 * ephy_search_engine_row_new:
 * @search_engine: the search engine to show. This search engine must already
 *   exist in the search engine manager.
 *
 * Creates a new #EphySearchEngineRow showing @search_engine informations and
 * allowing to edit them.
 *
 * Returns: a newly created #EphySearchEngineRow
 */
EphySearchEngineRow *
ephy_search_engine_row_new (EphySearchEngine *engine)
{
  return g_object_new (EPHY_TYPE_SEARCH_ENGINE_ROW,
                       "search-engine", engine,
                       NULL);
}

/**
 * ephy_search_engine_row_get_engine:
 *
 * Returns: the #EphySearchEngine displayed by this row
 */
EphySearchEngine *
ephy_search_engine_row_get_engine (EphySearchEngineRow *self)
{
  return self->engine;
}

/***** Private implementation *****/

static void
on_edit_button_clicked (GtkButton           *button,
                        EphySearchEngineRow *row)
{
  GtkWidget *prefs_dialog = gtk_widget_get_ancestor (GTK_WIDGET (row), ADW_TYPE_PREFERENCES_DIALOG);
  EphyEmbedShell *embed_shell = ephy_embed_shell_get_default ();
  EphySearchEngineManager *manager = ephy_embed_shell_get_search_engine_manager (embed_shell);
  AdwNavigationPage *page;

  page = ADW_NAVIGATION_PAGE (prefs_search_engine_page_new (row->engine, manager, FALSE));
  adw_preferences_dialog_push_subpage (ADW_PREFERENCES_DIALOG (prefs_dialog), page);
}

static void
ephy_search_engine_row_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  EphySearchEngineRow *self = EPHY_SEARCH_ENGINE_ROW (object);

  switch (prop_id) {
    case PROP_SEARCH_ENGINE:
      self->engine = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
on_ephy_search_engine_row_constructed (GObject *object)
{
  EphySearchEngineRow *self = EPHY_SEARCH_ENGINE_ROW (object);

  g_assert (self->engine);

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self),
                                 ephy_search_engine_get_name (self->engine));

  /* We can't directly bind that in the UI file because there's issues with
   * properties bindings that involve the root widget (the <template> root one).
   */
  g_object_bind_property (self->engine, "name",
                          self, "title",
                          G_BINDING_SYNC_CREATE);

  if (!ephy_search_engine_get_suggestions_url (self->engine))
    adw_action_row_set_subtitle (ADW_ACTION_ROW (self), _("No search suggestions"));

  G_OBJECT_CLASS (ephy_search_engine_row_parent_class)->constructed (object);
}

static void
ephy_search_engine_row_class_init (EphySearchEngineRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ephy_search_engine_row_set_property;
  object_class->constructed = on_ephy_search_engine_row_constructed;

  properties[PROP_SEARCH_ENGINE] = g_param_spec_object ("search-engine",
                                                        NULL, NULL,
                                                        EPHY_TYPE_SEARCH_ENGINE,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/search-engine-row.ui");

  gtk_widget_class_bind_template_callback (widget_class, on_edit_button_clicked);
}

static void
ephy_search_engine_row_init (EphySearchEngineRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
