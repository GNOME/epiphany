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

#include "ephy-search-engine-listbox.h"
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

  /* This is only used to be able to rename the old search engine with a new name,
   * and to access the search engine's informations stored in the @manager.
   * It is always a valid name.
   */
  char *saved_name;
  /* This is the name that was previously in the entry. Use this only from on_name_entry_text_changed_cb() */
  char *previous_name;
  EphySearchEngineManager *manager;
};

G_DEFINE_TYPE (EphySearchEngineRow, ephy_search_engine_row, HDY_TYPE_EXPANDER_ROW)

enum {
  PROP_0,
  PROP_SEARCH_ENGINE_NAME,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/***** Mostly public functions *****/

/**
 * ephy_search_engine_row_new:
 *
 * Creates a new #EphySearchEngineRow showing @search_engine_name engine informations.
 *
 * @search_engine_name: the name of the search engine to show.
 * This search engine must already exist in the default search engine manager.
 *
 * Returns: a newly created #EphySearchEngineRow
 */
EphySearchEngineRow *
ephy_search_engine_row_new (const char *search_engine_name)
{
  return g_object_new (EPHY_TYPE_SEARCH_ENGINE_ROW,
                       "search-engine-name", search_engine_name,
                       NULL);
}

static int
sort_search_engine_list_box_cb (EphySearchEngineRow *first_row,
                                EphySearchEngineRow *second_row,
                                gpointer             user_data)
{
  g_autofree char *first_row_name = NULL;
  g_autofree char *second_row_name = NULL;

  /* Place the "add search engine" row at the end.
   * This row isn't an expander row, only a regular row.
   */
  if (!EPHY_IS_SEARCH_ENGINE_ROW (first_row))
    return 1;
  if (!EPHY_IS_SEARCH_ENGINE_ROW (second_row))
    return -1;

  first_row_name = g_utf8_casefold (first_row->saved_name, -1);
  second_row_name = g_utf8_casefold (second_row->saved_name, -1);

  return g_strcmp0 (first_row_name, second_row_name);
}

GtkListBoxSortFunc
ephy_search_engine_row_get_sort_func (void)
{
  return (GtkListBoxSortFunc)sort_search_engine_list_box_cb;
}

/**
 * ephy_search_engine_row_set_can_remove:
 *
 * Sets whether the Remove button of @self is sensitive.
 *
 * @self: an #EphySearchEngineRow
 * @can_remove: whether the user can click the @self's Remove button
 */
void
ephy_search_engine_row_set_can_remove (EphySearchEngineRow *self,
                                       gboolean             can_remove)
{
  gtk_widget_set_sensitive (self->remove_button, can_remove);
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

/**
 * ephy_search_engine_row_set_as_default:
 *
 * Sets this search engine represented by @self as the default engine for
 * the default search engine manager. In practice, it toggles the default engine radio button.
 *
 * @self: an #EphySearchEngineRow
 */
void
ephy_search_engine_row_set_as_default (EphySearchEngineRow *self)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->radio_button), TRUE);
}

/***** Private implementation *****/

static gboolean
search_engine_already_exists (EphySearchEngineRow *searched_row,
                              const char          *engine_name)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (gtk_widget_get_parent (GTK_WIDGET (searched_row))));

  for (; children->next != NULL; children = children->next) {
    EphySearchEngineRow *iterated_row;

    /* As it iterates on the whole list box, this function will run on the "add search engine" row, which isn't an EphySearchEngineRow. */
    if (!EPHY_IS_SEARCH_ENGINE_ROW (children->data))
      continue;

    iterated_row = EPHY_SEARCH_ENGINE_ROW (children->data);

    if (iterated_row == searched_row)
      continue;

    if (g_strcmp0 (iterated_row->saved_name, engine_name) == 0)
      return TRUE;
  }

  return FALSE;
}

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
  const char *engine_from_bang = ephy_search_engine_manager_engine_from_bang (row->manager, bang);

  /* Checks if the bang already exists */
  if (engine_from_bang && g_strcmp0 (engine_from_bang, row->saved_name) != 0) {
    set_entry_as_invalid (bang_entry, _("This shortcut is already used."));
  } else {
    set_entry_as_valid (bang_entry);
    ephy_search_engine_manager_modify_engine (row->manager,
                                              row->saved_name,
                                              ephy_search_engine_manager_get_address (row->manager, row->saved_name),
                                              gtk_entry_get_text (bang_entry));
  }
}

static void
on_address_entry_text_changed_cb (EphySearchEngineRow *row,
                                  GParamSpec          *pspec,
                                  GtkEntry            *address_entry)
{
  const char *validation_message = NULL;

  /* Address in invalid. */
  if (!validate_search_engine_address (gtk_entry_get_text (address_entry), &validation_message)) {
    set_entry_as_invalid (address_entry, validation_message);
  } else { /* Address in valid. */
    set_entry_as_valid (address_entry);
    ephy_search_engine_manager_modify_engine (row->manager,
                                              row->saved_name,
                                              gtk_entry_get_text (address_entry),
                                              ephy_search_engine_manager_get_bang (row->manager,
                                                                                   row->saved_name));
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
  g_autoptr (GError) error = NULL;
  int i = 0, j = 0;

  unicode_str = g_utf8_to_ucs4 (utf8_str, -1, NULL, NULL, &error);
  if (!unicode_str)
    g_error ("%s", error->message);

  for (; unicode_str[i] != 0; ++i) {
    /* If this characters matches, we add it to the final string. */
    if (filter_func (unicode_str[i]))
      filtered_unicode_str[j++] = unicode_str[i];
  }
  final_utf8_str = g_ucs4_to_utf8 (filtered_unicode_str, -1, NULL, NULL, &error);
  if (!final_utf8_str)
    g_error ("%s", error->message);
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
  /* Fit the largest possible size for an UTF-8 character (4 bytes) and one byte for the NUL string terminator */

  /* There's nothing to do if the string is empty. */
  if (g_strcmp0 (search_engine_name, "") == 0)
    return;

  words = g_strsplit (search_engine_name, " ", 0);

  for (; words[i] != NULL; ++i) {
    g_autofree char *uppercase_chars = NULL;
    char *tmp_acronym = NULL;
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
  ephy_search_engine_manager_modify_engine (row->manager,
                                            row->saved_name,
                                            ephy_search_engine_manager_get_address (row->manager, row->saved_name),
                                            gtk_entry_get_text (GTK_ENTRY (row->bang_entry)));
}

static void
on_name_entry_text_changed_cb (EphySearchEngineRow *row,
                               GParamSpec          *pspec,
                               GtkEntry            *name_entry)
{
  EphySearchEngineListBox *search_engine_list_box = EPHY_SEARCH_ENGINE_LIST_BOX (gtk_widget_get_parent (GTK_WIDGET (row)));
  const char *new_name = gtk_entry_get_text (name_entry);

  /* This is an edge case when you copy the whole name then paste it again in
   * place of the whole current name. GtkEntry will record a notify signal even
   * if the name didn't actually change. This could toggle the entry as invalid
   * because the engine would already exist, so don't go any further in this case.
   */
  if (g_strcmp0 (row->previous_name, new_name) == 0)
    return;

  g_free (row->previous_name);
  row->previous_name = g_strdup (new_name);

  hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (row), new_name);

  if (g_strcmp0 (new_name, EMPTY_NEW_SEARCH_ENGINE_NAME) == 0)
    ephy_search_engine_list_box_set_can_add_engine (search_engine_list_box, FALSE);

  /* Name validation. */
  if (g_strcmp0 (new_name, "") == 0) {
    set_entry_as_invalid (name_entry, _("A name is required"));
  } else if (search_engine_already_exists (row, new_name)) {
    set_entry_as_invalid (name_entry, _("This search engine already exists"));
  } else {
    set_entry_as_valid (name_entry);

    /* This allows the user to add new search engine again once it is renamed. */
    if (g_strcmp0 (row->saved_name, EMPTY_NEW_SEARCH_ENGINE_NAME) == 0 &&
        g_strcmp0 (new_name, EMPTY_NEW_SEARCH_ENGINE_NAME) != 0)
      ephy_search_engine_list_box_set_can_add_engine (search_engine_list_box, TRUE);

    update_bang_for_name (row, new_name);

    ephy_search_engine_manager_rename (row->manager,
                                       row->saved_name,
                                       new_name);
    g_free (row->saved_name);
    row->saved_name = g_strdup (new_name);
  }
}

static void
on_radio_button_clicked_cb (EphySearchEngineRow *row,
                            GtkButton           *button)
{
  /* This avoids having some random engines being set as default when adding a new row,
   * since when it default initialize the "active" property to %FALSE on object construction,
   * it records a "clicked" signal
   */
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    ephy_search_engine_manager_set_default_engine (row->manager, row->saved_name);
}

static void
on_remove_button_clicked_cb (EphySearchEngineRow *row,
                             GtkButton           *button)
{
  EphySearchEngineRow *top_row;
  g_autofree char *default_engine = ephy_search_engine_manager_get_default_engine (row->manager);
  GtkListBox *parent_list_box = GTK_LIST_BOX (gtk_widget_get_parent (GTK_WIDGET (row)));

  /* Temporarly ref the row, as we'll remove it from its parent container
   * but will still use some struct members of it.
   */
  g_object_ref (row);

  ephy_search_engine_manager_delete_engine (row->manager,
                                            row->saved_name);

  /* FIXME: this should be fixed in libhandy
   * Unexpand the row before removing it so the styling isn't broken.
   * See the checked-expander-row-previous-sibling style class in HdyExpanderRow documentation.
   */
  hdy_expander_row_set_expanded (HDY_EXPANDER_ROW (row), FALSE);
  if (!search_engine_already_exists (row, row->saved_name))
    ephy_search_engine_list_box_set_can_add_engine (EPHY_SEARCH_ENGINE_LIST_BOX (parent_list_box),
                                                    TRUE);

  gtk_container_remove (GTK_CONTAINER (parent_list_box), GTK_WIDGET (row));

  top_row = EPHY_SEARCH_ENGINE_ROW (gtk_list_box_get_row_at_index (parent_list_box, 0));
  /* Set an other row (the first one) as default search engine to replace this one (if it was the default one). */
  if (g_strcmp0 (default_engine,
                 row->saved_name) == 0)
    ephy_search_engine_row_set_as_default (top_row);

  if (gtk_list_box_get_row_at_index (parent_list_box, 2) == NULL)
    gtk_widget_set_sensitive (top_row->remove_button, FALSE);

  /* Drop the temporary reference */
  g_object_unref (row);
}

static void
ephy_search_engine_row_finalize (GObject *object)
{
  EphySearchEngineRow *self = (EphySearchEngineRow *)object;

  g_free (self->saved_name);
  g_free (self->previous_name);

  G_OBJECT_CLASS (ephy_search_engine_row_parent_class)->finalize (object);
}

static void
ephy_search_engine_row_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  EphySearchEngineRow *self = EPHY_SEARCH_ENGINE_ROW (object);

  switch (prop_id) {
    case PROP_SEARCH_ENGINE_NAME:
      g_free (self->saved_name);
      self->saved_name = g_value_dup_string (value);
      g_free (self->previous_name);
      self->previous_name = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
on_ephy_search_engine_row_constructed (GObject *object)
{
  EphySearchEngineRow *self = EPHY_SEARCH_ENGINE_ROW (object);
  g_autofree char *default_search_engine_name = ephy_search_engine_manager_get_default_engine (self->manager);

  g_assert (self->saved_name != NULL);
  g_assert (g_strcmp0 (self->previous_name, self->saved_name) == 0);

  gtk_entry_set_text (GTK_ENTRY (self->name_entry), self->saved_name);
  hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (self), self->saved_name);

  gtk_entry_set_text (GTK_ENTRY (self->address_entry),
                      ephy_search_engine_manager_get_address (self->manager, self->saved_name));
  gtk_entry_set_text (GTK_ENTRY (self->bang_entry),
                      ephy_search_engine_manager_get_bang (self->manager, self->saved_name));

  /* Tick the radio button if it's the default search engine. */
  if (g_strcmp0 (self->saved_name, default_search_engine_name) == 0)
    ephy_search_engine_row_set_as_default (self);

  g_signal_connect_object (self->name_entry, "notify::text", G_CALLBACK (on_name_entry_text_changed_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->address_entry, "notify::text", G_CALLBACK (on_address_entry_text_changed_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->bang_entry, "notify::text", G_CALLBACK (on_bang_entry_text_changed_cb), self, G_CONNECT_SWAPPED);

  G_OBJECT_CLASS (ephy_search_engine_row_parent_class)->constructed (object);
}

static void
ephy_search_engine_row_class_init (EphySearchEngineRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ephy_search_engine_row_finalize;
  object_class->set_property = ephy_search_engine_row_set_property;
  object_class->constructed = on_ephy_search_engine_row_constructed;

  properties[PROP_SEARCH_ENGINE_NAME] = g_param_spec_string ("search-engine-name",
                                                             "search-engine-name",
                                                             "The name of the search engine",
                                                             NULL,
                                                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/search-engine-row.ui");

  gtk_widget_class_bind_template_child (widget_class, EphySearchEngineRow, radio_button);
  gtk_widget_class_bind_template_child (widget_class, EphySearchEngineRow, name_entry);
  gtk_widget_class_bind_template_child (widget_class, EphySearchEngineRow, address_entry);
  gtk_widget_class_bind_template_child (widget_class, EphySearchEngineRow, bang_entry);
  gtk_widget_class_bind_template_child (widget_class, EphySearchEngineRow, remove_button);

  gtk_widget_class_bind_template_callback (widget_class, on_radio_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_remove_button_clicked_cb);
}

static void
ephy_search_engine_row_init (EphySearchEngineRow *self)
{
  self->manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());

  gtk_widget_init_template (GTK_WIDGET (self));
}
