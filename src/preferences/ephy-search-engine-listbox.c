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

#include <glib/gi18n.h>
#include <gmodule.h>
#include <libsoup/soup.h>
#define HANDY_USE_UNSTABLE_API
#include <handy.h>

#include "embed/ephy-embed-shell.h"
#include "ephy-search-engine-manager.h"

#define EMPTY_NEW_SEARCH_ENGINE_NAME (_("New search engine"))

typedef enum {
  SEARCH_ENGINE_NAME_FIELD = 1 << 0,
  SEARCH_ENGINE_ADDRESS_FIELD = 1 << 1,
  SEARCH_ENGINE_BANG_FIELD = 1 << 2,
  SEARCH_ENGINE_ALL_FIELDS = SEARCH_ENGINE_NAME_FIELD | SEARCH_ENGINE_ADDRESS_FIELD | SEARCH_ENGINE_BANG_FIELD,
} SearchEngineFields;

typedef struct {
  GtkWidget *name_entry;
  GtkWidget *address_entry;
  GtkWidget *bang_entry;
  GtkWidget *remove_button;
  GtkWidget *radio_button;
  /* This is used to be able to rename the old search engine with a new name. It is always a valid and unique name. */
  char *saved_name;
  /* This is the name that was previously in the entry, be it valid/unique or not. */
  char *previous_name;
  EphySearchEngineManager *manager;
  GtkListBox *parent;
  int invalid_fields;
} SearchEngineRowData;

struct _EphySearchEngineListBox {
  GtkListBox parent_instance;

  /* This widget isn't actually showed anywhere. It is just a stable place where we can add more radio buttons without having to bother if the primary radio button gets removed. */
  GtkWidget *radio_buttons_group;

  GtkWidget *row_add_search_engine;
  EphySearchEngineManager *manager;
};

static GtkWidget *add_search_engine_row (EphySearchEngineListBox *list_box,
                                         const char              *saved_name);

G_DEFINE_TYPE (EphySearchEngineListBox, ephy_search_engine_list_box, GTK_TYPE_LIST_BOX)

static void
search_engine_row_data_free (gpointer data)
{
  SearchEngineRowData *row_data = data;

  g_free (row_data->saved_name);
  g_free (row_data->previous_name);
  g_free (row_data);
}

GtkWidget *
ephy_search_engine_list_box_new (void)
{
  return g_object_new (EPHY_TYPE_SEARCH_ENGINE_LIST_BOX, NULL);
}

static void
ephy_search_engine_list_box_finalize (GObject *object)
{
  EphySearchEngineListBox *self = (EphySearchEngineListBox *)object;

  g_clear_pointer (&self->radio_buttons_group, g_object_unref);

  G_OBJECT_CLASS (ephy_search_engine_list_box_parent_class)->finalize (object);
}

static void
ephy_search_engine_list_box_class_init (EphySearchEngineListBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_search_engine_list_box_finalize;
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
  g_autoptr (SoupURI) soup_uri = NULL;
  g_autofree char *path_and_query = NULL;

  if (g_strcmp0 (address, "") == 0) {
    *error_message = _("This field is required");
    return FALSE;
  }

  if (!g_str_has_prefix (address, "http://") && !g_str_has_prefix (address, "https://")) {
    *error_message = _("Address must start with either http:// or https://");
    return FALSE;
  }

  soup_uri = soup_uri_new (address);
  if (!soup_uri) {
    *error_message = _("Address is not a valid URI");
    return FALSE;
  }

  if (!SOUP_URI_VALID_FOR_HTTP (soup_uri) ||
      /* It seems you can dodge the first condition. When we have URI "http:///", without the host part, libsoup fills the host part with "" but SOUP_URI_VALID_FOR_HTTP checks for non-NULL host, not empty host. This line fixes it. */
      g_strcmp0 (soup_uri->host, "") == 0) {
    *error_message = _("Address is not a valid URL. The address should look like https://www.example.com/search?q=%s");
    return FALSE;
  }

  path_and_query = soup_uri_to_string (soup_uri, TRUE);
  if (!strstr (path_and_query, "%s")) {
    *error_message = _("Address must contain the search term represented by %s");
    return FALSE;
  }

  /* If both are different, this means there are at least two occurences of "%s" since one starts searching from the beginning while the other one starts from the end. */
  if (strstr (address, "%s") != g_strrstr (address, "%s")) {
    *error_message = _("Address should not contain the search term several times");
    return FALSE;
  }

  /* The address is valid. */
  return TRUE;
}

static void
set_entry_as_invalid (GtkEntry   *entry,
                      const char *error_message)
{
  gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, "dialog-warning-symbolic");
  gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY, error_message);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (entry)), GTK_STYLE_CLASS_ERROR);
}

static void
set_entry_as_valid (GtkEntry *entry)
{
  gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
  gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
  gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (entry)), GTK_STYLE_CLASS_ERROR);
}

static void
on_bang_entry_text_changed_cb (GObject    *instance,
                               GParamSpec *pspec,
                               gpointer    user_data)
{
  GtkEntry *bang_entry = GTK_ENTRY (instance);
  SearchEngineRowData *row_data = g_object_get_data (user_data, "row-data");
  const char *engine_from_bang = ephy_search_engine_manager_engine_from_bang (row_data->manager, gtk_entry_get_text (bang_entry));

  /* Checks if the bang already exists */
  if (engine_from_bang && !g_str_equal (engine_from_bang, row_data->saved_name)) {
    set_entry_as_invalid (bang_entry, _("This shortcut is already used."));
    row_data->invalid_fields |= SEARCH_ENGINE_BANG_FIELD;
  } else {
    set_entry_as_valid (bang_entry);
    row_data->invalid_fields &= ~SEARCH_ENGINE_BANG_FIELD;
  }
}

static gboolean
on_bang_entry_focus_out_cb (GObject    *instance,
                            GParamSpec *pspec,
                            GObject    *data)
{
  SearchEngineRowData *row_data = g_object_get_data (data, "row-data");

  /* Check if the bang is valid. */
  if ((row_data->invalid_fields & SEARCH_ENGINE_BANG_FIELD) == 0) {
    ephy_search_engine_manager_modify_engine (row_data->manager,
                                              row_data->saved_name,
                                              ephy_search_engine_manager_get_address (row_data->manager, row_data->saved_name),
                                              gtk_entry_get_text (GTK_ENTRY (row_data->bang_entry)));
  }
  return FALSE;
}

static void
on_address_entry_text_changed_cb (GObject    *instance,
                                  GParamSpec *pspec,
                                  gpointer    user_data)
{
  GtkEntry *address_entry = GTK_ENTRY (instance);
  SearchEngineRowData *row_data = g_object_get_data (user_data, "row-data");
  const char *validation_message = NULL;

  /* Address in invalid. */
  if (!validate_search_engine_address (gtk_entry_get_text (address_entry), &validation_message)) {
    set_entry_as_invalid (address_entry, validation_message);
    row_data->invalid_fields |= SEARCH_ENGINE_ADDRESS_FIELD;
  } else { /* Address in valid. */
    set_entry_as_valid (address_entry);
    row_data->invalid_fields &= ~SEARCH_ENGINE_ADDRESS_FIELD;
  }
}

static gboolean
on_address_entry_focus_out_cb (GObject    *instance,
                               GParamSpec *pspec,
                               GObject    *search_engine_row)
{
  SearchEngineRowData *row_data = g_object_get_data (search_engine_row, "row-data");

  /* Check if the address is valid. */
  if ((row_data->invalid_fields & SEARCH_ENGINE_ADDRESS_FIELD) == 0) {
    ephy_search_engine_manager_modify_engine (row_data->manager,
                                              row_data->saved_name,
                                              gtk_entry_get_text (GTK_ENTRY (row_data->address_entry)),
                                              ephy_search_engine_manager_get_bang (row_data->manager,
                                                                                   row_data->saved_name));
  }
  return FALSE;
}

static void
update_bang_for_name (GObject    *search_engine_row,
                      const char *name)
{
  /* This function automatically builds the shortcut string from the search engine name, taking every first character in each word and every uppercase characters. */
  /* This means name "DuckDuckGo" will set bang to "!ddg" and "duck duck go" will set bang to "!ddg" as well. */
  g_autofree char *search_engine_name = g_strstrip (g_strdup (name));
  g_auto (GStrv) words = NULL;
  char *word;
  g_autofree char *acronym = g_strdup ("");
  g_autofree char *lowercase_acronym = NULL;
  g_autofree char *final_bang = NULL;
  SearchEngineRowData *row_data = g_object_get_data (search_engine_row, "row-data");
  int i = 0;
  static char first_word_char[5]; /* Fit the largest possible size for an UTF-8 character (4 bytes) and one byte for the NUL string terminator */

  /* There's nothing to do if the string is empty. */
  if (g_strcmp0 (search_engine_name, "") == 0)
    return;

  words = g_strsplit (search_engine_name, " ", 0);

  for (; words[i] != NULL; ++i) {
    g_autofree char *uppercase_chars = NULL;
    char *tmp_acronym = NULL;
    word = words[i];

    memset (first_word_char, 0, 5); /* Clear the static string. */

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
  gtk_entry_set_text (GTK_ENTRY (row_data->bang_entry), final_bang);
  ephy_search_engine_manager_modify_engine (row_data->manager,
                                            row_data->saved_name,
                                            ephy_search_engine_manager_get_address (row_data->manager, row_data->saved_name),
                                            gtk_entry_get_text (GTK_ENTRY (row_data->bang_entry)));
}

static void
count_engine_in_list_box_cb (GtkWidget *widget,
                             gpointer   user_data)
{
  GPtrArray *data_tuple = user_data;
  SearchEngineRowData *this_row_row_data = g_ptr_array_index (data_tuple, 0);
  SearchEngineRowData *row_data = g_object_get_data (G_OBJECT (widget), "row-data");
  guint *name_count = g_ptr_array_index (data_tuple, 1);
  const char *engine_name = g_ptr_array_index (data_tuple, 2);

  /* As it iterates on the whole list box, this function will run on the "add search engine" row, which doesn't have row-data. */
  if (row_data && row_data != this_row_row_data && g_str_equal (row_data->previous_name, engine_name))
    ++(*name_count);
}

/* This function checks whether there is an other search engine named */
static gboolean
search_engine_already_exists (SearchEngineRowData *this_row_row_data,
                              const char          *const_engine_name)
{
  guint name_count = 0;
  g_autoptr (GPtrArray) data_tuple = g_ptr_array_new ();
  g_autofree char *engine_name = g_strdup (const_engine_name);
  g_ptr_array_add (data_tuple, this_row_row_data);
  g_ptr_array_add (data_tuple, &name_count);
  g_ptr_array_add (data_tuple, engine_name);

  gtk_container_foreach (GTK_CONTAINER (this_row_row_data->parent), count_engine_in_list_box_cb, data_tuple);
  return name_count > 0;
}

static void
on_name_entry_text_changed_cb (GObject    *instance,
                               GParamSpec *pspec,
                               gpointer    user_data)
{
  SearchEngineRowData *row_data = g_object_get_data (user_data, "row-data");
  EphySearchEngineListBox *list_box = EPHY_SEARCH_ENGINE_LIST_BOX (row_data->parent);
  GtkEntry *name_entry = GTK_ENTRY (instance);
  const char *new_name = gtk_entry_get_text (GTK_ENTRY (row_data->name_entry));

  /* This is an edge case when you copy the whole name then paste it again in place of the whole
   * current name. GtkEntry will record a notify signal even if the name didn't actually change. */
  /* This could toggle the entry as invalid because the engine would already exist (saved in the
   * manager), so don't go any further in this case. */
  if (g_strcmp0 (row_data->previous_name, new_name) == 0)
    return;

  /* This allows the user to add new search engine again once it is renamed. */
  if (g_strcmp0 (new_name, EMPTY_NEW_SEARCH_ENGINE_NAME) == 0)
    gtk_widget_set_sensitive (GTK_WIDGET (list_box->row_add_search_engine), FALSE);

  /* Name validation. */
  if (g_strcmp0 (new_name, "") == 0) {
    set_entry_as_invalid (name_entry, _("A name is required"));
    row_data->invalid_fields |= SEARCH_ENGINE_NAME_FIELD;
  } else if (search_engine_already_exists (row_data, new_name)) {
    set_entry_as_invalid (name_entry, _("This search engine already exists"));
    row_data->invalid_fields |= SEARCH_ENGINE_NAME_FIELD;
  } else {
    set_entry_as_valid (name_entry);
    row_data->invalid_fields &= ~SEARCH_ENGINE_NAME_FIELD;

    if (g_strcmp0 (row_data->previous_name, EMPTY_NEW_SEARCH_ENGINE_NAME) == 0 && g_strcmp0 (new_name, EMPTY_NEW_SEARCH_ENGINE_NAME) != 0)
      gtk_widget_set_sensitive (GTK_WIDGET (list_box->row_add_search_engine), TRUE);
    g_free (row_data->previous_name);
    row_data->previous_name = g_strdup (new_name);
  }
}

static gboolean
on_name_entry_focus_out_cb (GObject    *instance,
                            GParamSpec *pspec,
                            GObject    *search_engine_row)
{
  SearchEngineRowData *row_data = g_object_get_data (search_engine_row, "row-data");
  const char *new_name = gtk_entry_get_text (GTK_ENTRY (row_data->name_entry));
  GtkEntry *name_entry = GTK_ENTRY (instance);

  update_bang_for_name (search_engine_row, gtk_entry_get_text (name_entry));

  /* Check if the name is valid before saving. */
  if ((row_data->invalid_fields & SEARCH_ENGINE_NAME_FIELD) == 0) {
    ephy_search_engine_manager_rename (row_data->manager,
                                       row_data->saved_name,
                                       new_name);
    g_free (row_data->saved_name);
    row_data->saved_name = g_strdup (new_name);
  }

  return FALSE;
}

static int
get_list_box_length (GtkWidget *list_box)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (list_box));

  return g_list_length (children);
}

static void
unexpand_other_rows_cb (GObject    *search_engine_row,
                        GParamSpec *pspec,
                        GObject    *list_box)
{
  int expanded_row_index, listbox_length, i;
  GtkListBoxRow *current_row;

  /* Check if it’s of the right type before-hand */
  if (HDY_IS_EXPANDER_ROW (search_engine_row)) {
    /* We only unexpand other rows if this is a notify signal for an expanded row. */
    if (hdy_expander_row_get_expanded (HDY_EXPANDER_ROW (search_engine_row)) == FALSE) {
      return;
    }
  }

  expanded_row_index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (search_engine_row));
  listbox_length = get_list_box_length (GTK_WIDGET (list_box));

  for (i = 0; i < listbox_length; ++i) {
    /* Ignore the row that was just expanded. */
    if (i == expanded_row_index)
      continue;
    current_row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (list_box), i);

    /* Ignore this row if not an expandable row. */
    if (!HDY_IS_EXPANDER_ROW (current_row))
      continue;
    hdy_expander_row_set_expanded (HDY_EXPANDER_ROW (current_row), FALSE);
  }
}

static SearchEngineRowData *
get_first_row_data (GtkListBox *parent_list_box)
{
  SearchEngineRowData *first_row_data = g_object_get_data (G_OBJECT (gtk_list_box_get_row_at_index (parent_list_box, 0)), "row-data");
  return first_row_data;
}

static void
on_remove_button_clicked_cb (GtkButton *button,
                             gpointer  *data)
{
  HdyExpanderRow *search_engine_row = HDY_EXPANDER_ROW (data);
  SearchEngineRowData *row_data = g_object_get_data (G_OBJECT (search_engine_row), "row-data");
  GtkListBox *parent_list_box = row_data->parent; /* Keep its address as row_data will be freed before we use it. */
  g_autofree char *default_search_engine_name = ephy_search_engine_manager_get_default_engine (row_data->manager);
  g_autofree char *saved_name = g_strdup (row_data->saved_name);
  SearchEngineRowData *first_row_data;

  ephy_search_engine_manager_delete_engine (row_data->manager,
                                            row_data->saved_name);
  /* Unexpand the row before removing it so the styling isn't broken. */
  /* See the checked-expander-row-previous-sibling style class in HdyExpanderRow documentation. */
  hdy_expander_row_set_expanded (search_engine_row, FALSE);
  if (!search_engine_already_exists (row_data, saved_name))
    gtk_widget_set_sensitive (EPHY_SEARCH_ENGINE_LIST_BOX (parent_list_box)->row_add_search_engine, TRUE);

  /* This prevents the focus-out signals from being called when you have an entry focused, */
  /* with modified content, and use Alt+letter to activate the remove button. */
  g_signal_handlers_disconnect_by_data (row_data->name_entry, search_engine_row);
  g_signal_handlers_disconnect_by_data (row_data->address_entry, search_engine_row);
  g_signal_handlers_disconnect_by_data (row_data->bang_entry, search_engine_row);

  gtk_container_remove (GTK_CONTAINER (row_data->parent), GTK_WIDGET (search_engine_row));

  /* @row_data is freed now so we must only use parent_list_box rather than row_data->parent. */
  first_row_data = get_first_row_data (parent_list_box);
  /* Set an other row (the first one) as default search engine to replace this one (if it was the default one). */
  if (g_strcmp0 (default_search_engine_name, saved_name) == 0)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (first_row_data->radio_button), TRUE);
  if (get_list_box_length (GTK_WIDGET (parent_list_box)) == 2)
    gtk_widget_set_sensitive (first_row_data->remove_button, FALSE);
}

static void
on_row_radio_button_clicked_cb (GtkButton *button,
                                gpointer   data)
{
  SearchEngineRowData *row_data = data;

  ephy_search_engine_manager_set_default_engine (row_data->manager, row_data->saved_name);
}

static void
on_add_search_engine_row_clicked_cb (GtkListBox    *list_box,
                                     GtkListBoxRow *row,
                                     gpointer       user_data)
{
  EphySearchEngineListBox *search_engine_list_box;
  GtkWidget *search_engine_row;

  g_assert (EPHY_IS_SEARCH_ENGINE_LIST_BOX (list_box));
  search_engine_list_box = EPHY_SEARCH_ENGINE_LIST_BOX (list_box);

  /* Don't allow the user to have many empty search engines. */
  if (get_list_box_length (GTK_WIDGET (list_box)) == 2)
    gtk_widget_set_sensitive (get_first_row_data (list_box)->remove_button, TRUE);
  ephy_search_engine_manager_add_engine (search_engine_list_box->manager,
                                         EMPTY_NEW_SEARCH_ENGINE_NAME,
                                         "",
                                         "");
  search_engine_row = add_search_engine_row (search_engine_list_box, EMPTY_NEW_SEARCH_ENGINE_NAME);
  gtk_list_box_row_changed (GTK_LIST_BOX_ROW (search_engine_row));
  hdy_expander_row_set_expanded (HDY_EXPANDER_ROW (search_engine_row), TRUE);
  /* Only allow one empty search engine to be created. This row will be sensitive again when the empty row is renamed. */
  gtk_widget_set_sensitive (GTK_WIDGET (row), FALSE);
}

/**
 * add_search_engine_row:
 *
 * Adds a new row to @list_box, presenting the different informations of the search engine @saved_name,
 * with the ability to edit them.
 *
 * @list_box: The parent list box where to add the search engine row.
 * @saved_name: The name the search engine currently has. If NULL, the row will allow to add a search engine rather than editing the current one.
 *
 * Returns: (transfer none): the newly created row
 */
static GtkWidget *
add_search_engine_row (EphySearchEngineListBox *list_box,
                       const char              *saved_name)
{
  GtkWidget *search_engine_row, *child_row;
  GtkWidget *grid, *label, *entry;
  GtkWidget *button;
  GtkWidget *radio_button;
  SearchEngineRowData *row_data = g_new0 (SearchEngineRowData, 1);
  g_autofree char *default_search_engine_name = ephy_search_engine_manager_get_default_engine (row_data->manager);
  int grid_vertical_pos = 0;

  search_engine_row = hdy_expander_row_new ();
  g_signal_connect (search_engine_row, "notify::expanded", G_CALLBACK (unexpand_other_rows_cb), list_box);

  /* Child rows have access to their list box so they can remove themselves from the list then sort again the list. */
  row_data->parent = GTK_LIST_BOX (list_box);
  row_data->manager = list_box->manager;
  row_data->saved_name = g_strdup (saved_name);
  row_data->previous_name = g_strdup (saved_name);
  row_data->invalid_fields = SEARCH_ENGINE_ALL_FIELDS;
  g_object_set_data_full (G_OBJECT (search_engine_row), "row-data", row_data, search_engine_row_data_free);

  gtk_list_box_insert (GTK_LIST_BOX (list_box), search_engine_row, -1);

  /* Radio button to set the default search engine. */
  /* Add this radio button to the existing group. */
  radio_button = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (list_box->radio_buttons_group));
  gtk_widget_set_valign (radio_button, GTK_ALIGN_CENTER);
  gtk_widget_set_tooltip_text (radio_button, _("Selects the default search engine"));
  hdy_expander_row_add_prefix (HDY_EXPANDER_ROW (search_engine_row), radio_button);
  g_signal_connect (radio_button, "clicked", G_CALLBACK (on_row_radio_button_clicked_cb), row_data);
  gtk_widget_show (radio_button);
  row_data->radio_button = radio_button;

  /* Tick the radio button if it's the default search engine. */
  if (g_strcmp0 (row_data->saved_name, default_search_engine_name) == 0)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), TRUE);

  child_row = gtk_list_box_row_new ();
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (child_row), FALSE);
  gtk_widget_show (child_row);
  gtk_container_add (GTK_CONTAINER (search_engine_row), child_row);

  grid = gtk_grid_new ();
  g_object_set (grid, "margin", 12, NULL);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_widget_set_can_focus (grid, FALSE);
  gtk_widget_show (grid);
  gtk_container_add (GTK_CONTAINER (child_row), grid);

  /* Name label and entry pair */
  label = gtk_label_new (_("Name"));
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_show (label);
  gtk_grid_attach (GTK_GRID (grid), label, 0, grid_vertical_pos++, 1, 1);

  entry = gtk_entry_new ();
  gtk_grid_attach_next_to (GTK_GRID (grid), entry, label, GTK_POS_RIGHT, 1, 1);
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_entry_set_text (GTK_ENTRY (entry), saved_name);
  gtk_widget_show (entry);
  g_object_bind_property (entry, "text", HDY_EXPANDER_ROW (search_engine_row), "title", G_BINDING_SYNC_CREATE);
  gtk_widget_set_events (entry, GDK_FOCUS_CHANGE_MASK);
  gtk_entry_set_icon_sensitive (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, FALSE);
  row_data->name_entry = entry;
  /* Validate the name when typing. */
  g_signal_connect (entry, "notify::text", G_CALLBACK (on_name_entry_text_changed_cb), search_engine_row);
  /* But only save when the entry loses focus. */
  g_signal_connect (entry, "focus-out-event", G_CALLBACK (on_name_entry_focus_out_cb), search_engine_row);

  /* Address label and entry pair */
  label = gtk_label_new (_("Address"));
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_show (label);
  gtk_grid_attach (GTK_GRID (grid), label, 0, grid_vertical_pos++, 1, 1);

  entry = gtk_entry_new ();
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_entry_set_text (GTK_ENTRY (entry), ephy_search_engine_manager_get_address (list_box->manager, saved_name));
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry), "https://www.example.com/search?q=%s");
  gtk_entry_set_input_purpose (GTK_ENTRY (entry), GTK_INPUT_PURPOSE_URL);
  gtk_entry_set_icon_sensitive (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, FALSE);
  gtk_widget_set_events (entry, GDK_FOCUS_CHANGE_MASK);
  /* Validate the name when typing. */
  g_signal_connect (entry, "notify::text", G_CALLBACK (on_address_entry_text_changed_cb), search_engine_row);
  /* But only save when the entry loses focus. */
  g_signal_connect (entry, "focus-out-event", G_CALLBACK (on_address_entry_focus_out_cb), search_engine_row);
  gtk_widget_show (entry);
  gtk_grid_attach_next_to (GTK_GRID (grid), entry, label, GTK_POS_RIGHT, 1, 1);
  row_data->address_entry = entry;

  /* Bang label and entry pair */
  label = gtk_label_new (_("Shortcut"));
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_show (label);
  gtk_grid_attach (GTK_GRID (grid), label, 0, grid_vertical_pos++, 1, 1);

  entry = gtk_entry_new ();
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry), "!e");
  gtk_entry_set_text (GTK_ENTRY (entry), ephy_search_engine_manager_get_bang (list_box->manager, saved_name));
  gtk_entry_set_icon_sensitive (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, FALSE);
  gtk_widget_set_events (entry, GDK_FOCUS_CHANGE_MASK);
  /* There's not validation needed (this field isn't even required). Save when the entry loses focus. */
  g_signal_connect (entry, "focus-out-event", G_CALLBACK (on_bang_entry_focus_out_cb), search_engine_row);
  g_signal_connect (entry, "notify::text", G_CALLBACK (on_bang_entry_text_changed_cb), search_engine_row);
  gtk_widget_show (entry);
  gtk_grid_attach_next_to (GTK_GRID (grid), entry, label, GTK_POS_RIGHT, 1, 1);
  row_data->bang_entry = entry;

  /* Search engine address insight */
  label = gtk_label_new (_("To determine the search address, perform a search using the search engine that you want to add and replace the search term with %s."));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), GTK_STYLE_CLASS_DIM_LABEL);
  gtk_widget_show (label);
  gtk_grid_attach (GTK_GRID (grid), label, 0, grid_vertical_pos++, 2, 1);

  button = gtk_button_new_with_mnemonic (_("R_emove Search Engine"));
  gtk_style_context_add_class (gtk_widget_get_style_context (button), GTK_STYLE_CLASS_DESTRUCTIVE_ACTION);
  g_signal_connect (GTK_BUTTON (button), "clicked", G_CALLBACK (on_remove_button_clicked_cb), search_engine_row);
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_widget_set_hexpand (button, FALSE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, grid_vertical_pos++, 2, 1);
  gtk_widget_show (button);
  row_data->remove_button = button;

  gtk_widget_show (GTK_WIDGET (search_engine_row));

  return search_engine_row;
}

static int
sort_search_engine_list_box_cb (GtkListBoxRow *first_row,
                                GtkListBoxRow *second_row,
                                gpointer       user_data)
{
  g_autofree char *first_name = NULL;
  g_autofree char *second_name = NULL;
  SearchEngineRowData *first_row_data;
  SearchEngineRowData *second_row_data;

  /* Place the "add search engine" row at the end. This row isn't an expander row, just a regular clickable row. */
  if (!HDY_IS_EXPANDER_ROW (first_row))
    return 1;
  if (!HDY_IS_EXPANDER_ROW (second_row))
    return -1;

  first_row_data = g_object_get_data (G_OBJECT (first_row), "row-data");
  second_row_data = g_object_get_data (G_OBJECT (second_row), "row-data");
  first_name = g_utf8_strdown (first_row_data->saved_name, -1);
  second_name = g_utf8_strdown (second_row_data->saved_name, -1);

  return g_strcmp0 (first_name, second_name);
}

static void
ephy_search_engine_list_box_init (EphySearchEngineListBox *self)
{
  char **names, *name;
  int i = 0, list_box_length;
  GtkWidget *row_add_search_engine, *label;
  SearchEngineRowData *first_row_data;

  self->manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());
  names = ephy_search_engine_manager_get_names (self->manager);

  self->radio_buttons_group = gtk_radio_button_new (NULL);
  /* Ref the radio buttons group and remove the floating reference because we don't hook this widget to any part of the UI (for example gtk_container_add would remove the floating reference and ref it). */
  g_object_ref_sink (self->radio_buttons_group);
  gtk_widget_show (self->radio_buttons_group);

  for (; names[i] != NULL; ++i) {
    name = names[i];
    add_search_engine_row (self, name);
  }
  g_strfreev (names);

  row_add_search_engine = gtk_list_box_row_new ();
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row_add_search_engine), TRUE);
  gtk_widget_set_size_request (row_add_search_engine, -1, 50);
  gtk_list_box_prepend (GTK_LIST_BOX (self), row_add_search_engine);
  g_signal_connect (self, "row-activated", G_CALLBACK (on_add_search_engine_row_clicked_cb), NULL);
  if (ephy_search_engine_manager_engine_exists (self->manager, EMPTY_NEW_SEARCH_ENGINE_NAME))
    gtk_widget_set_sensitive (row_add_search_engine, FALSE);
  gtk_widget_show (row_add_search_engine);
  self->row_add_search_engine = row_add_search_engine;
  label = gtk_label_new_with_mnemonic (_("A_dd Search Engine…"));
  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_widget_show (label);
  gtk_container_add (GTK_CONTAINER (row_add_search_engine), label);

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self), sort_search_engine_list_box_cb, NULL, NULL);
  gtk_list_box_invalidate_sort (GTK_LIST_BOX (self));

  gtk_list_box_set_selection_mode (GTK_LIST_BOX (self), GTK_SELECTION_NONE);

  list_box_length = get_list_box_length (GTK_WIDGET (self));
  /* The list box should have at least one "Add search engine" row and one search engine (the default one). */
  /* Since we don't allow removing the last search engine, this won't abort the application unless the user fiddles with gsettings. */
  g_assert (list_box_length >= 2);
  first_row_data = get_first_row_data (GTK_LIST_BOX (self));
  /* This means there is only one search engine left, so we forbid removing this one. */
  if (list_box_length == 2)
    gtk_widget_set_sensitive (first_row_data->remove_button, FALSE);

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "content");
  gtk_widget_show (GTK_WIDGET (self));
}
