/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2020 vanadiae <vanadiae35@gmail.com>
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
#include "prefs-search-engine-page.h"

#include "ephy-search-engine.h"
#include "ephy-search-engine-manager.h"

struct _PrefsSearchEnginePage {
  AdwNavigationPage parent_instance;

  /* Widgets */
  GtkWidget *name_row;
  GtkWidget *address_row;
  GtkWidget *shortcut_row;
  GtkWidget *button_row;

  EphySearchEngine *engine;
  EphySearchEngineManager *manager;
  gboolean engine_is_new;
};

G_DEFINE_FINAL_TYPE (PrefsSearchEnginePage, prefs_search_engine_page, ADW_TYPE_NAVIGATION_PAGE)

enum {
  PROP_0,
  PROP_SEARCH_ENGINE,
  PROP_MANAGER,
  PROP_ENGINE_IS_NEW,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
set_row_as_invalid (AdwEntryRow *row,
                    const char  *error_message)
{
  gtk_widget_set_tooltip_text (GTK_WIDGET (row), error_message);
  gtk_widget_add_css_class (GTK_WIDGET (row), "error");
  gtk_accessible_update_state (GTK_ACCESSIBLE (row),
                               GTK_ACCESSIBLE_STATE_INVALID,
                               GTK_ACCESSIBLE_INVALID_TRUE,
                               -1);
}

static void
set_row_as_valid (AdwEntryRow *row)
{
  gtk_widget_set_tooltip_text (GTK_WIDGET (row), NULL);
  gtk_widget_remove_css_class (GTK_WIDGET (row), "error");
  gtk_accessible_reset_state (GTK_ACCESSIBLE (row),
                              GTK_ACCESSIBLE_STATE_INVALID);
}

/**
 * validate_search_engine_address:
 *
 * @address:       the address to validate.
 * @error_message: filled with a meaningful error message explaining what's wrong with the address.
 *                 Left unchanged if the address is valid.
 *
 * Returns: %TRUE if the address is valid, %FALSE otherwise.
 */
static gboolean
validate_search_engine_address (const char  *address,
                                const char **error_message)
{
  g_autoptr (GUri) uri = NULL;
  g_autoptr (GString) uri_friendly_pattern_address = NULL;
  guint search_terms_count = 0;

  if (g_strcmp0 (address, "") == 0) {
    *error_message = _("This field is required");
    return FALSE;
  }

  if (!g_str_has_prefix (address, "http://") && !g_str_has_prefix (address, "https://")) {
    *error_message = _("Address must start with either http:// or https://");
    return FALSE;
  }

  uri_friendly_pattern_address = g_string_new (address);
  /* As %s is not correctly percent-encoded, g_uri_parse() will fail here if it
   * is in the address. So workaround this by replacing the user-facing %s with
   * a percent-encoded %s.
   */
  search_terms_count = g_string_replace (uri_friendly_pattern_address,
                                         "%s", "%25s", 0);
  if (search_terms_count == 0) {
    *error_message = _("Address must contain the search term represented by %s");
    return FALSE;
  } else if (search_terms_count > 1) {
    *error_message = _("Address should not contain the search term several times");
    return FALSE;
  }

  uri = g_uri_parse (uri_friendly_pattern_address->str, G_URI_FLAGS_PARSE_RELAXED, NULL);
  if (!uri) {
    *error_message = _("Address is not a valid URL");
    return FALSE;
  }

  if (!g_uri_get_host (uri) || g_strcmp0 (g_uri_get_host (uri), "") == 0) {
    *error_message = _("Address is not a valid URL. The address should look like https://www.example.com/search?q=%s");
    return FALSE;
  }

  /* The address is valid. */
  return TRUE;
}

static void
on_name_row_text_changed (PrefsSearchEnginePage *self,
                          GParamSpec            *pspec,
                          AdwEntryRow           *name_row)
{
  const char *new_name = gtk_editable_get_text (GTK_EDITABLE (name_row));

  /* This is an edge case when you copy the whole name then paste it again in
   * place of the whole current name. GtkEntry will record a notify signal even
   * if the name didn't actually change. This could toggle the entry as invalid
   * because the engine would already exist, so don't go any further in this case.
   */
  if (g_strcmp0 (ephy_search_engine_get_name (self->engine), new_name) == 0)
    return;

  /* Name validation. */
  if (g_strcmp0 (new_name, "") == 0) {
    set_row_as_invalid (name_row, _("A name is required"));
  } else if (ephy_search_engine_manager_find_engine_by_name (self->manager, new_name)) {
    set_row_as_invalid (name_row, _("This search engine already exists"));
  } else {
    set_row_as_valid (name_row);

    /* Let's not overwrite any existing bang, as that's likely not what is wanted.
     * For example when I wanted to rename my "wiktionary en" search engine that
     * had the !wte bang, it replaced the bang with !we, which is the one for
     * "Wikipedia (en)". That's just annoying, so only do it when there hasn't
     * been any bang added yet.
     */
    if (g_strcmp0 (gtk_editable_get_text (GTK_EDITABLE (self->shortcut_row)), "") == 0) {
      g_autofree char *new_bang = ephy_search_engine_build_bang_for_name (new_name);

      gtk_editable_set_text (GTK_EDITABLE (self->shortcut_row), new_bang);
      ephy_search_engine_set_bang (self->engine, new_bang);
    }

    ephy_search_engine_set_name (self->engine, new_name);

    if (!self->engine_is_new)
      ephy_search_engine_manager_save_to_settings (self->manager);
  }
}

static void
on_address_row_text_changed (PrefsSearchEnginePage *self,
                             GParamSpec            *pspec,
                             AdwEntryRow           *address_row)
{
  const char *validation_message = NULL;
  const char *url = gtk_editable_get_text (GTK_EDITABLE (address_row));

  /* Address in invalid. */
  if (!validate_search_engine_address (url, &validation_message)) {
    set_row_as_invalid (address_row, validation_message);
  } else { /* Address in valid. */
    set_row_as_valid (address_row);
    ephy_search_engine_set_url (self->engine, url);

    if (!self->engine_is_new)
      ephy_search_engine_manager_save_to_settings (self->manager);
  }

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (address_row),
                                 *url == '\0' ? _("Address (with search term as %s)") : _("Address"));
}

static void
on_shortcut_row_text_changed (PrefsSearchEnginePage *self,
                              GParamSpec            *pspec,
                              AdwEntryRow           *shortcut_row)
{
  const char *bang = gtk_editable_get_text (GTK_EDITABLE (shortcut_row));

  /* Checks if the bang already exists */
  if (g_strcmp0 (bang, ephy_search_engine_get_bang (self->engine)) != 0 &&
      ephy_search_engine_manager_has_bang (self->manager, bang)) {
    set_row_as_invalid (shortcut_row, _("This shortcut is already used."));
  } else if (strchr (bang, ' ')) {
    set_row_as_invalid (shortcut_row, _("Search shortcuts must not contain any space."));
  } else if (bang[0] != '\0' && /* Empty bangs are allowed if none is wanted. */
             (!g_unichar_ispunct (g_utf8_get_char (bang)) ||
              /* "Punctuation" covers a wide range of symbols, with some
               * of them that obviously don't make sense at all. So make
               * sure those aren't allowed.
               */
              g_utf8_strchr ("(){}[].,", -1, g_utf8_get_char (bang)))) {
    set_row_as_invalid (shortcut_row, _("Search shortcuts should start with a symbol such as !, # or @."));
  } else {
    set_row_as_valid (shortcut_row);
    ephy_search_engine_set_bang (self->engine, bang);

    if (!self->engine_is_new)
      ephy_search_engine_manager_save_to_settings (self->manager);
  }

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (shortcut_row),
                                 *bang == '\0' ? _("Shortcut (for example !ddg)") : _("Shortcut"));
}

static void
on_manager_items_changed (EphySearchEngineManager *manager,
                          guint                    position,
                          guint                    removed,
                          guint                    added,
                          PrefsSearchEnginePage   *self)
{
  guint n_items = g_list_model_get_n_items (G_LIST_MODEL (manager));

  /* We don't allow removing the engine if it's the last one, as it
   * doesn't make sense at all and just too much relies on having a
   * search engine available.
   */
  if (!self->engine_is_new)
    gtk_widget_set_sensitive (self->button_row, n_items > 1);
}

static void
on_button_row_activated (AdwButtonRow          *button_row,
                         PrefsSearchEnginePage *self)
{
  GtkWidget *prefs_dialog = gtk_widget_get_ancestor (GTK_WIDGET (self), ADW_TYPE_PREFERENCES_DIALOG);

  if (!self->engine_is_new) {
    ephy_search_engine_manager_delete_engine (self->manager, self->engine);
    self->engine = NULL;
  } else {
    ephy_search_engine_manager_add_engine (self->manager, self->engine);
  }

  ephy_search_engine_manager_save_to_settings (self->manager);
  adw_preferences_dialog_pop_subpage (ADW_PREFERENCES_DIALOG (prefs_dialog));
}

static void
prefs_search_engine_page_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  PrefsSearchEnginePage *self = EPHY_PREFS_SEARCH_ENGINE_PAGE (object);

  switch (prop_id) {
    case PROP_SEARCH_ENGINE:
      self->engine = g_value_get_object (value);
      break;
    case PROP_MANAGER:
      self->manager = g_value_dup_object (value);
      break;
    case PROP_ENGINE_IS_NEW:
      self->engine_is_new = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
prefs_search_engine_page_constructed (GObject *object)
{
  PrefsSearchEnginePage *self = EPHY_PREFS_SEARCH_ENGINE_PAGE (object);

  g_assert (self->engine);
  g_assert (self->manager);

  gtk_editable_set_text (GTK_EDITABLE (self->name_row),
                         ephy_search_engine_get_name (self->engine));
  gtk_editable_set_text (GTK_EDITABLE (self->address_row),
                         ephy_search_engine_get_url (self->engine));
  gtk_editable_set_text (GTK_EDITABLE (self->shortcut_row),
                         ephy_search_engine_get_bang (self->engine));

  gtk_accessible_update_property (GTK_ACCESSIBLE (self->name_row),
                                  GTK_ACCESSIBLE_PROPERTY_REQUIRED, TRUE,
                                  -1);
  gtk_accessible_update_property (GTK_ACCESSIBLE (self->address_row),
                                  GTK_ACCESSIBLE_PROPERTY_REQUIRED, TRUE,
                                  -1);

  if (self->engine_is_new) {
    adw_navigation_page_set_title (ADW_NAVIGATION_PAGE (self), _("Add Search Engine"));

    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->button_row), _("Add Search Engine"));
    gtk_widget_add_css_class (self->button_row, "suggested-action");
  } else {
    adw_navigation_page_set_title (ADW_NAVIGATION_PAGE (self), _("Edit Search Engine"));

    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->button_row), _("Remove Search Engine"));
    gtk_widget_add_css_class (self->button_row, "destructive-action");
  }

  g_signal_connect_object (self->name_row, "notify::text", G_CALLBACK (on_name_row_text_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->address_row, "notify::text", G_CALLBACK (on_address_row_text_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->shortcut_row, "notify::text", G_CALLBACK (on_shortcut_row_text_changed), self, G_CONNECT_SWAPPED);

  on_manager_items_changed (self->manager, 0, 0, g_list_model_get_n_items (G_LIST_MODEL (self->manager)), self);
  g_signal_connect_object (self->manager, "items-changed", G_CALLBACK (on_manager_items_changed), self, 0);

  G_OBJECT_CLASS (prefs_search_engine_page_parent_class)->constructed (object);
}

static void
prefs_search_engine_page_finalize (GObject *object)
{
  PrefsSearchEnginePage *self = EPHY_PREFS_SEARCH_ENGINE_PAGE (object);

  g_clear_object (&self->engine);

  G_OBJECT_CLASS (prefs_search_engine_page_parent_class)->finalize (object);
}

static void
prefs_search_engine_page_class_init (PrefsSearchEnginePageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = prefs_search_engine_page_set_property;
  object_class->constructed = prefs_search_engine_page_constructed;
  object_class->finalize = prefs_search_engine_page_finalize;

  properties[PROP_SEARCH_ENGINE] =
    g_param_spec_object ("search-engine", NULL, NULL, EPHY_TYPE_SEARCH_ENGINE,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_MANAGER] =
    g_param_spec_object ("manager", NULL, NULL, EPHY_TYPE_SEARCH_ENGINE_MANAGER,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_ENGINE_IS_NEW] =
    g_param_spec_boolean ("engine-is-new", NULL, NULL, FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-search-engine-page.ui");

  gtk_widget_class_bind_template_child (widget_class, PrefsSearchEnginePage, name_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsSearchEnginePage, address_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsSearchEnginePage, shortcut_row);
  gtk_widget_class_bind_template_child (widget_class, PrefsSearchEnginePage, button_row);

  gtk_widget_class_bind_template_callback (widget_class, on_button_row_activated);
}

static void
prefs_search_engine_page_init (PrefsSearchEnginePage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

PrefsSearchEnginePage *
prefs_search_engine_page_new (EphySearchEngine        *engine,
                              EphySearchEngineManager *manager,
                              gboolean                 engine_is_new)
{
  return g_object_new (PREFS_SEARCH_ENGINE_PAGE,
                       "search-engine", engine,
                       "manager", manager,
                       "engine-is-new", engine_is_new,
                       NULL);
}
