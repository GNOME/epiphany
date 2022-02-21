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

#include "ephy-search-engine-manager.h"

#include "ephy-embed-shell.h"

struct _EphySearchEngineRow {
  HdyExpanderRow parent_instance;

  /* Widgets */
  GtkWidget *name_entry;
  GtkWidget *address_entry;
  GtkWidget *bang_entry;
  GtkWidget *remove_button;
  GtkWidget *radio_button;

  EphySearchEngine *engine;
  EphySearchEngineManager *manager;
};

G_DEFINE_TYPE (EphySearchEngineRow, ephy_search_engine_row, HDY_TYPE_EXPANDER_ROW)

enum {
  PROP_0,
  PROP_SEARCH_ENGINE,
  PROP_MANAGER,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/***** Mostly public functions *****/

/**
 * ephy_search_engine_row_new:
 * @search_engine: the search engine to show. This search engine must already
 *   exist in @manager.
 * @manager: The search engine manager to which @search_engine belongs.
 *
 * Creates a new #EphySearchEngineRow showing @search_engine informations and
 * allowing to edit them.
 *
 * Returns: a newly created #EphySearchEngineRow
 */
EphySearchEngineRow *
ephy_search_engine_row_new (EphySearchEngine        *engine,
                            EphySearchEngineManager *manager)
{
  return g_object_new (EPHY_TYPE_SEARCH_ENGINE_ROW,
                       "search-engine", engine,
                       "manager", manager,
                       NULL);
}

/**
 * ephy_search_engine_row_set_radio_button_group:
 *
 * Adds @self's radio button to group @radio_button_group.
 *
 * @self: an #EphySearchEngineRow
 * @radio_button_group: the group to add @self's radio button to
 */
void
ephy_search_engine_row_set_radio_button_group (EphySearchEngineRow *self,
                                               GtkRadioButton      *radio_button_group)
{
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (self->radio_button),
                              gtk_radio_button_get_group (radio_button_group));
}

/***** Private implementation *****/

/**
 * validate_search_engine_address:
 *
 * @address:       the address to validate
 * @error_message: filled with a meaningful error message explaining what's wrong with the address. Left unchanged if the address is valid.
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

  uri = g_uri_parse (uri_friendly_pattern_address->str, G_URI_FLAGS_NONE, NULL);
  if (!uri) {
    *error_message = _("Address is not a valid URI");
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
set_entry_as_invalid (GtkEntry   *entry,
                      const char *error_message)
{
  gtk_entry_set_icon_from_icon_name (entry,
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "dialog-warning-symbolic");
  gtk_entry_set_icon_tooltip_text (entry,
                                   GTK_ENTRY_ICON_SECONDARY,
                                   error_message);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (entry)),
                               "error");
}

static void
set_entry_as_valid (GtkEntry *entry)
{
  gtk_entry_set_icon_from_icon_name (entry,
                                     GTK_ENTRY_ICON_SECONDARY,
                                     NULL);
  gtk_entry_set_icon_tooltip_text (entry,
                                   GTK_ENTRY_ICON_SECONDARY,
                                   NULL);
  gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (entry)),
                                  "error");
}

static void
on_bang_entry_text_changed_cb (EphySearchEngineRow *row,
                               GParamSpec          *pspec,
                               GtkEntry            *bang_entry)
{
  const char *bang = gtk_entry_get_text (bang_entry);

  /* Checks if the bang already exists */
  if (g_strcmp0 (bang, ephy_search_engine_get_bang (row->engine)) != 0 &&
      ephy_search_engine_manager_has_bang (row->manager, bang)) {
    set_entry_as_invalid (bang_entry, _("This shortcut is already used."));
  } else if (strchr (bang, ' ') != NULL) {
    set_entry_as_invalid (bang_entry, _("Search shortcuts must not contain any space."));
  } else if (bang[0] != '\0' && /* Empty bangs are allowed if none is wanted. */
             (!g_unichar_ispunct (g_utf8_get_char (bang)) ||
              /* "Punctuation" covers a wide range of symbols, with some
               * of them that obviously don't make sense at all. So make
               * sure those aren't allowed.
               */
              g_utf8_strchr ("(){}[].,", -1, g_utf8_get_char (bang)))) {
    set_entry_as_invalid (bang_entry, _("Search shortcuts should start with a symbol such as !, # or @."));
  } else {
    set_entry_as_valid (bang_entry);
    ephy_search_engine_set_bang (row->engine, bang);
  }
}

static void
on_address_entry_text_changed_cb (EphySearchEngineRow *row,
                                  GParamSpec          *pspec,
                                  GtkEntry            *address_entry)
{
  const char *validation_message = NULL;
  const char *url = gtk_entry_get_text (address_entry);

  /* Address in invalid. */
  if (!validate_search_engine_address (url, &validation_message)) {
    set_entry_as_invalid (address_entry, validation_message);
  } else { /* Address in valid. */
    set_entry_as_valid (address_entry);
    ephy_search_engine_set_url (row->engine, url);
  }
}

typedef gboolean ( *UnicodeStrFilterFunc )(gunichar c);
/**
 * filter_str_with_functor:
 *
 * Filters-out every character that doesn't match @filter.
 *
 * @utf8_str: an UTF-8 string
 * @filter: a function pointer to one of the g_unichar_isX function.
 *
 * Returns: a new UTF-8 string containing only the characters matching @filter.
 */
static char *
filter_str_with_functor (const char           *utf8_str,
                         UnicodeStrFilterFunc  filter_func)
{
  gunichar *filtered_unicode_str = g_new0 (gunichar, strlen (utf8_str) + 1);
  g_autofree gunichar *unicode_str = NULL;
  char *final_utf8_str = NULL;
  int i = 0, j = 0;

  unicode_str = g_utf8_to_ucs4_fast (utf8_str, -1, NULL);

  for (; unicode_str[i] != 0; ++i) {
    /* If this characters matches, we add it to the final string. */
    if (filter_func (unicode_str[i]))
      filtered_unicode_str[j++] = unicode_str[i];
  }
  final_utf8_str = g_ucs4_to_utf8 (filtered_unicode_str, -1, NULL, NULL, NULL);
  /* We already assume it's UTF-8 when using g_utf8_to_ucs4_fast() above, and
   * our processing can't create invalid UTF-8 characters as we are only
   * copying existing and already valid UTF-8 characters. So it's safe to assert.
   */
  g_assert (final_utf8_str);
  /* Would be better to use g_autofree but scan-build complains as it doesn't properly handle the cleanup attribute. */
  g_free (filtered_unicode_str);

  return final_utf8_str;
}

/* This function automatically builds the shortcut string from the search engine
 * name, taking every first character in each word and every uppercase characters.
 * This means name "DuckDuckGo" will set bang to "!ddg" and "duck duck go" will
 * set bang to "!ddg" as well.
 */
static void
update_bang_for_name (EphySearchEngineRow *row,
                      const char          *new_name)
{
  g_autofree char *search_engine_name = g_strstrip (g_strdup (new_name));
  g_auto (GStrv) words = NULL;
  char *word;
  g_autofree char *acronym = g_strdup ("");
  g_autofree char *lowercase_acronym = NULL;
  g_autofree char *final_bang = NULL;
  int i = 0;

  /* There's nothing to do if the string is empty. */
  if (g_strcmp0 (search_engine_name, "") == 0)
    return;

  /* We ignore both the space character and opening parenthesis, as that
   * allows us to get !we as bang with "Wikipedia (en)" as name.
   */
  words = g_strsplit_set (search_engine_name, " (", 0);

  for (; words[i] != NULL; ++i) {
    g_autofree char *uppercase_chars = NULL;
    char *tmp_acronym = NULL;
    /* Fit the largest possible size for an UTF-8 character (4 bytes) and one byte for the NUL string terminator */
    char first_word_char[5] = {0};
    word = words[i];

    /* Ignore empty words. This might happen if there are multiple consecutives spaces between two words. */
    if (strcmp (word, "") == 0)
      continue;

    /* Go to the next character, as we treat the first character of each word separately. */
    uppercase_chars = filter_str_with_functor (g_utf8_find_next_char (word, NULL), g_unichar_isupper);
    /* Keep the first UTF-8 character so that names such as "duck duck go" will produce "ddg". */
    g_utf8_strncpy (first_word_char, word, 1);
    tmp_acronym = g_strconcat (acronym,
                               first_word_char,
                               uppercase_chars, NULL);
    g_free (acronym);
    acronym = tmp_acronym;
  }
  lowercase_acronym = g_utf8_strdown (acronym, -1); /* Bangs are usually lowercase */
  final_bang = g_strconcat ("!", lowercase_acronym, NULL); /* "!" is the prefix for the bang */
  gtk_entry_set_text (GTK_ENTRY (row->bang_entry), final_bang);
  ephy_search_engine_set_bang (row->engine, final_bang);
}

static void
on_name_entry_text_changed_cb (EphySearchEngineRow *row,
                               GParamSpec          *pspec,
                               GtkEntry            *name_entry)
{
  const char *new_name = gtk_entry_get_text (name_entry);

  /* This is an edge case when you copy the whole name then paste it again in
   * place of the whole current name. GtkEntry will record a notify signal even
   * if the name didn't actually change. This could toggle the entry as invalid
   * because the engine would already exist, so don't go any further in this case.
   */
  if (g_strcmp0 (ephy_search_engine_get_name (row->engine), new_name) == 0)
    return;

  /* Name validation. */
  if (g_strcmp0 (new_name, "") == 0) {
    set_entry_as_invalid (name_entry, _("A name is required"));
  } else if (ephy_search_engine_manager_find_engine_by_name (row->manager, new_name)) {
    set_entry_as_invalid (name_entry, _("This search engine already exists"));
  } else {
    set_entry_as_valid (name_entry);

    /* Let's not overwrite any existing bang, as that's likely not what is wanted.
     * For example when I wanted to rename my "wiktionary en" search engine that
     * had the !wte bang, it replaced the bang with !we, which is the one for
     * "Wikipedia (en)". That's just annoying, so only do it when there hasn't
     * been any bang added yet.
     */
    if (g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (row->bang_entry)), "") == 0)
      update_bang_for_name (row, new_name);

    ephy_search_engine_set_name (row->engine, new_name);
  }
}

static void
on_radio_button_active_changed_cb (EphySearchEngineRow *self,
                                   GParamSpec          *pspec,
                                   GtkButton           *button)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)) &&
      /* Avoid infinite loop between this callback and on_default_engine_changed_cb() */
      ephy_search_engine_manager_get_default_engine (self->manager) != self->engine)
    ephy_search_engine_manager_set_default_engine (self->manager, self->engine);
}

static void
on_default_engine_changed_cb (EphySearchEngineManager *manager,
                              GParamSpec              *pspec,
                              EphySearchEngineRow     *self)
{
  if (ephy_search_engine_manager_get_default_engine (manager) == self->engine)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->radio_button), TRUE);
}

static void
on_remove_button_clicked_cb (EphySearchEngineRow *row,
                             GtkButton           *button)
{
  /* FIXME: this should be fixed in libhandy
   * Unexpand the row before removing it so the styling isn't broken.
   * See the checked-expander-row-previous-sibling style class in HdyExpanderRow documentation.
   */
  hdy_expander_row_set_expanded (HDY_EXPANDER_ROW (row), FALSE);

  ephy_search_engine_manager_delete_engine (row->manager, row->engine);
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
    case PROP_MANAGER:
      self->manager = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
on_manager_items_changed_cb (EphySearchEngineManager *manager,
                             guint                    position,
                             guint                    removed,
                             guint                    added,
                             EphySearchEngineRow     *self)
{
  guint n_items = g_list_model_get_n_items (G_LIST_MODEL (manager));

  /* We don't allow removing the engine if it's the last one, as it
   * doesn't make sense at all and just too much relies on having a
   * search engine available.
   */
  gtk_widget_set_sensitive (self->remove_button, n_items > 1);
}

static void
on_ephy_search_engine_row_constructed (GObject *object)
{
  EphySearchEngineRow *self = EPHY_SEARCH_ENGINE_ROW (object);

  g_assert (self->engine != NULL);
  g_assert (self->manager != NULL);

  gtk_entry_set_text (GTK_ENTRY (self->name_entry), ephy_search_engine_get_name (self->engine));

  /* We can't directly bind that in the UI file because there's issues with
   * properties bindings that involve the root widget (the <template> root one).
   */
  g_object_bind_property (self->name_entry, "text",
                          self, "title",
                          G_BINDING_SYNC_CREATE);

  gtk_entry_set_text (GTK_ENTRY (self->address_entry),
                      ephy_search_engine_get_url (self->engine));
  gtk_entry_set_text (GTK_ENTRY (self->bang_entry),
                      ephy_search_engine_get_bang (self->engine));

  g_signal_connect_object (self->name_entry, "notify::text", G_CALLBACK (on_name_entry_text_changed_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->address_entry, "notify::text", G_CALLBACK (on_address_entry_text_changed_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->bang_entry, "notify::text", G_CALLBACK (on_bang_entry_text_changed_cb), self, G_CONNECT_SWAPPED);

  on_manager_items_changed_cb (self->manager, 0, 0, g_list_model_get_n_items (G_LIST_MODEL (self->manager)), self);
  g_signal_connect_object (self->manager, "items-changed", G_CALLBACK (on_manager_items_changed_cb), self, 0);

  on_default_engine_changed_cb (self->manager, NULL, self);
  g_signal_connect_object (self->manager, "notify::default-engine", G_CALLBACK (on_default_engine_changed_cb), self, 0);

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
                                                        "search-engine",
                                                        "The search engine that this row should show and allow to edit.",
                                                        EPHY_TYPE_SEARCH_ENGINE,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_MANAGER] = g_param_spec_object ("manager",
                                                  "manager",
                                                  "The search engine manager that manages @search-engine.",
                                                  EPHY_TYPE_SEARCH_ENGINE_MANAGER,
                                                  G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/search-engine-row.ui");

  gtk_widget_class_bind_template_child (widget_class, EphySearchEngineRow, radio_button);
  gtk_widget_class_bind_template_child (widget_class, EphySearchEngineRow, name_entry);
  gtk_widget_class_bind_template_child (widget_class, EphySearchEngineRow, address_entry);
  gtk_widget_class_bind_template_child (widget_class, EphySearchEngineRow, bang_entry);
  gtk_widget_class_bind_template_child (widget_class, EphySearchEngineRow, remove_button);

  gtk_widget_class_bind_template_callback (widget_class, on_radio_button_active_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_remove_button_clicked_cb);
}

static void
ephy_search_engine_row_init (EphySearchEngineRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
