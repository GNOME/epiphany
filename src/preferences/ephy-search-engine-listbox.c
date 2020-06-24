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

typedef enum
{
  SEARCH_ENGINE_FIELD_NAME = 1 << 0,
  SEARCH_ENGINE_FIELD_ADDRESS = 1 << 1,
  SEARCH_ENGINE_FIELD_ALL = SEARCH_ENGINE_FIELD_NAME | SEARCH_ENGINE_FIELD_ADDRESS,
} SearchEngineFields;

typedef struct
{
  GtkWidget *name_entry, *address_entry, *bang_entry;
  GtkWidget *add_search_engine_button;
  char *previous_name;
  EphySearchEngineManager *manager;
  GtkListBox *parent;
  int invalid_fields;
} SearchEngineRowData;

struct _EphySearchEngineListBox
{
  GtkListBox parent_instance;

  /* This widget isn't actually showed anywhere. It is just a stable place where we can add more radio buttons without having to bother if the primary radio button gets removed. */
  GtkWidget *radio_buttons_group;
  EphySearchEngineManager *manager;
};

static void add_search_engine_row (EphySearchEngineListBox *list_box, const char *previous_name);

G_DEFINE_TYPE (EphySearchEngineListBox, ephy_search_engine_list_box, GTK_TYPE_LIST_BOX)

static void
search_engine_row_data_free (gpointer data)
{
  SearchEngineRowData *row_data = data;

  g_warning ("freeing row_data %s in func %s", row_data->previous_name, __FUNCTION__);
  if (row_data->previous_name)
    g_free (row_data->previous_name);
  g_free (row_data);
}

EphySearchEngineListBox *
ephy_search_engine_list_box_new (void)
{
  EphySearchEngineListBox *list_box;

  list_box = g_object_new (EPHY_TYPE_SEARCH_ENGINE_LIST_BOX, NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (list_box)), "preferences");
  return list_box;
}

static void
ephy_search_engine_list_box_dispose (GObject *object)
{
  /* EphySearchEngineListBox *self = (EphySearchEngineListBox *)object; */

  G_OBJECT_CLASS (ephy_search_engine_list_box_parent_class)->dispose (object);
}

static void
ephy_search_engine_list_box_finalize (GObject *object)
{
  EphySearchEngineListBox *self = (EphySearchEngineListBox *)object;

  g_clear_pointer (&(self->radio_buttons_group), g_object_unref);
  G_OBJECT_CLASS (ephy_search_engine_list_box_parent_class)->finalize (object);
}

static void
ephy_search_engine_list_box_class_init (EphySearchEngineListBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ephy_search_engine_list_box_finalize;
  object_class->dispose = ephy_search_engine_list_box_dispose;
}

/* FIXME: Is there any built-in function like that in glib or the standard library (and works with UTF-8 like this one) ? */
/**
 * filter_str_with_functor:
 *
 * Filters out every character that doesn't match the @filter.
 *
 * @utf8_str: an UTF-8 string
 * @filter: a function pointer to one of the g_unichar_is* function.
 *
 * Returns: a new UTF-8 string containing only the characters matching @filter.
 */
static char *
filter_str_with_functor (const char *utf8_str,
                         gboolean (*filter)(gunichar))
{
  g_autofree gunichar *filtered_unicode_str = g_new0 (gunichar, strlen (utf8_str)+1);
  g_autofree gunichar *unicode_str = NULL;
  char *final_utf8_str = NULL;
  g_autoptr (GError) error = NULL;
  int i = 0, j = 0;

  unicode_str = g_utf8_to_ucs4 (utf8_str, -1, NULL, NULL, &error);
  if (!unicode_str)
    g_error ("%s", error->message);

  for (; unicode_str[i] != 0; ++i) {
    /* If this characters matches, we add it to the final string. */
    if (filter (unicode_str[i]))
      filtered_unicode_str[j++] = unicode_str[i];
  }
  final_utf8_str = g_ucs4_to_utf8 (filtered_unicode_str, -1, NULL, NULL, &error);
  if (!final_utf8_str)
    g_error ("%s", error->message);
  return final_utf8_str;
}

/**
 * validate_search_engine_address:
 *
 * @address: the address to validate
 *
 * Returns: NULL if the address is valid, otherwise a meaningful message explaining what's wrong with it.
 */
static const char *
validate_search_engine_address (const char *address)
{
  g_autoptr (SoupURI) soup_uri = NULL;
  g_autofree char *path_and_query = NULL;

  if (g_strcmp0 (address, "") == 0)
    return _("This field is required");

  if (!g_str_has_prefix (address, "http://") && !g_str_has_prefix (address, "https://"))
    return _("Address must start with either http:// or https://");

  soup_uri = soup_uri_new (address);
  if (!soup_uri)
    return _("Address is not a valid URI");

  if (!SOUP_URI_VALID_FOR_HTTP (soup_uri) ||
      /* It seems you can dodge the first condition. When we have URI "http:///path", without the host part, libsoup fills the host part with "" but SOUP_URI_VALID_FOR_HTTP checks for non-NULL host, not empty host. This line fixes it. */
      g_strcmp0 (soup_uri->host, "") == 0)
    return _("Address is not a valid URL");

  path_and_query = soup_uri_to_string (soup_uri, TRUE);
  if (!strstr (path_and_query, "%s"))
    return _("Address must contain the search term represented by %s");

  /* If both are different, this means there are at least two occurences of "%s" since one starts searching from the beginning while the other one starts from the end. */
  if (strstr (address, "%s") != g_strrstr (address, "%s"))
    return _("Address shouldn't contain the search term several times");

  return NULL;
}

static void
update_search_engine_button_sensitiveness (SearchEngineRowData *row_data,
                                           SearchEngineFields  field,
                                           gboolean            invalid)
{
  /* This is only applicable if this is an "add search engine" row. */
  if (!(row_data->add_search_engine_button))
    return;

  if (invalid) {
    row_data->invalid_fields |= field;
    gtk_widget_set_sensitive (row_data->add_search_engine_button, FALSE);
  } else {
    row_data->invalid_fields &= ~field;
    /* This means every fields are valid. */
    if (row_data->invalid_fields == 0)
      gtk_widget_set_sensitive (row_data->add_search_engine_button, TRUE);
  }
}

static gboolean
on_bang_entry_focus_out_cb (GObject    *instance,
                            GParamSpec *pspec,
                            GObject    *data)
{
  SearchEngineRowData *row_data = g_object_get_data (data, "row-data");
  /* Save only when editing an existing search engine. */
  if (row_data->previous_name)
    ephy_search_engine_manager_modify_engine (row_data->manager,
                                              row_data->previous_name,
                                              ephy_search_engine_manager_get_address (row_data->manager, row_data->previous_name),
                                              gtk_entry_get_text (GTK_ENTRY (row_data->bang_entry)));
  return FALSE;
}

static gboolean
on_address_entry_focus_out_cb (GObject    *instance,
                               GParamSpec *pspec,
                               GObject    *search_engine_row)
{
  GtkEntry *address_entry = GTK_ENTRY (instance);
  SearchEngineRowData *row_data = g_object_get_data (search_engine_row, "row-data");
  const char *address = gtk_entry_get_text (GTK_ENTRY (row_data->address_entry));
  const char *validation_message = validate_search_engine_address (address);

  g_warning ("validation_message=%s", validation_message);
  if (validation_message) { /* Address in invalid. */
    gtk_entry_set_icon_from_icon_name (address_entry, GTK_ENTRY_ICON_SECONDARY, "dialog-warning-symbolic");
    gtk_entry_set_icon_tooltip_text (address_entry, GTK_ENTRY_ICON_SECONDARY, validation_message);
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (address_entry)), GTK_STYLE_CLASS_ERROR);
    update_search_engine_button_sensitiveness (row_data, SEARCH_ENGINE_FIELD_ADDRESS, TRUE);
    g_warning ("address invalid in %s", __FUNCTION__);
  } else { /* Address in valid. */
    gtk_entry_set_icon_from_icon_name (address_entry, GTK_ENTRY_ICON_SECONDARY, NULL);
    gtk_entry_set_icon_tooltip_text (address_entry, GTK_ENTRY_ICON_SECONDARY, NULL);
    gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (address_entry)), GTK_STYLE_CLASS_ERROR);
    update_search_engine_button_sensitiveness (row_data, SEARCH_ENGINE_FIELD_ADDRESS, FALSE);
    /* Check if we are modifying a search engine. */
    if (row_data->previous_name)
      ephy_search_engine_manager_modify_engine (row_data->manager,
                                                row_data->previous_name,
                                                address,
                                                ephy_search_engine_manager_get_bang (row_data->manager, row_data->previous_name));
    g_warning ("address valid in %s", __FUNCTION__);
  }
  return FALSE;
}

/* FIXME: here is a bunch of different characters in UTF-8 土豆沙拉☠☣↓œŒ «@ÉBIngy#/\@ÉyYÉeéÈ to test this (try by separating with spaces as well). */
static void
update_bang_for_name (GObject    *search_engine_row,
                      const char *name)
{
  /* This function automatically builds the shortcut string from the search engine name, taking every first character in each word and every uppercase characters. */
  /* This means name "DuckDuckGo" will set bang to "!ddg" and "duck duck go" will set bang to "!ddg" as well. */
  g_autofree char *search_engine_name = g_strstrip (g_strdup (name));
  char **words, *word;
  g_autofree char *acronym = g_strdup ("");
  g_autofree char *lowercase_acronym = NULL;
  g_autofree char *final_bang = NULL;
  SearchEngineRowData *row_data = g_object_get_data (search_engine_row, "row-data");
  int i = 0;

  /* There's nothing to do if the string is empty. */
  if (g_strcmp0 (search_engine_name, "") == 0)
    return;

  words = g_strsplit (search_engine_name, " ", 0);

  for (; words[i] != NULL ; ++i) {
    g_autofree char *uppercase_chars = NULL;
    g_autofree char *first_word_char = g_new0 (char, 4 + 1); /* Fit the largest possible size for an UTF-8 char and one byte for the NUL string terminator */
    char *tmp_acronym = NULL;
    word = words[i];

    /* Ignore empty words. This might happen if there are multiple consecutives spaces between two words. */
    if (strcmp (word, "") == 0)
      continue;

    uppercase_chars = filter_str_with_functor (g_utf8_find_next_char (word, NULL), /* Add one because we treat the first character of each word separately. */
                                               g_unichar_isupper);
    g_warning ("i=%d uppercase_chars=%s", i, uppercase_chars);
    g_utf8_strncpy (first_word_char, word, 1); /* keep the first UTF-8 character so that names such as "duck duck go" will produce "ddg" */
    g_warning ("\"%s\"", first_word_char);
    tmp_acronym = g_strconcat (acronym,
                               first_word_char,
                               uppercase_chars, NULL);
    g_warning ("tmp_acronym=%s", tmp_acronym);
    g_free (acronym);
    acronym = tmp_acronym;
  }
  g_strfreev (words);

  lowercase_acronym = g_utf8_strdown (acronym, -1); /* Bangs are usually lowercase */
  final_bang = g_strconcat ("!", lowercase_acronym, NULL); /* "!" is the prefix for the bang */
  gtk_entry_set_text (GTK_ENTRY (row_data->bang_entry), final_bang);
  if (row_data->previous_name)
    ephy_search_engine_manager_modify_engine (row_data->manager,
                                              row_data->previous_name,
                                              ephy_search_engine_manager_get_address (row_data->manager, row_data->previous_name),
                                              gtk_entry_get_text (GTK_ENTRY (row_data->bang_entry)));
}

static gboolean
on_name_entry_focus_out_cb (GObject    *instance,
                            GParamSpec *pspec,
                            GObject    *search_engine_row)
{
  SearchEngineRowData *row_data = g_object_get_data (search_engine_row, "row-data");
  const char *new_name = gtk_entry_get_text (GTK_ENTRY (row_data->name_entry));
  GtkEntry *name_entry = GTK_ENTRY (instance);

  /* Don't allow empty name. */
  if (g_strcmp0 (new_name, "") == 0) {
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (name_entry)), GTK_STYLE_CLASS_ERROR);
    update_search_engine_button_sensitiveness (row_data, SEARCH_ENGINE_FIELD_NAME, TRUE);
    gtk_entry_set_icon_from_icon_name (name_entry, GTK_ENTRY_ICON_SECONDARY, "dialog-warning-symbolic");
    gtk_entry_set_icon_tooltip_text (name_entry, GTK_ENTRY_ICON_SECONDARY, _("A name is required"));
  } else {
    gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (name_entry)), GTK_STYLE_CLASS_ERROR);
    update_search_engine_button_sensitiveness (row_data, SEARCH_ENGINE_FIELD_NAME, FALSE);
    gtk_entry_set_icon_from_icon_name (name_entry, GTK_ENTRY_ICON_SECONDARY, NULL);
    gtk_entry_set_icon_tooltip_text (name_entry, GTK_ENTRY_ICON_SECONDARY, NULL);
    /* Only rename when editing an existant search engine. */
    if (row_data->previous_name) {
      ephy_search_engine_manager_rename (row_data->manager,
                                         row_data->previous_name,
                                         new_name);
      g_free (row_data->previous_name);
      row_data->previous_name = g_strdup (new_name);
    }
  }
  update_bang_for_name (search_engine_row, gtk_entry_get_text (name_entry));

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

  /* We only unexpand other rows if this is a notify signal for an expanded row. */
  if (hdy_expander_row_get_expanded (HDY_EXPANDER_ROW (search_engine_row)) == FALSE)
    return;

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

/* Siblings expander rows next to a removed one don't update its style (rounded corners and a large black separator line). */
/* FIXME: This is really not ideal… */
static void
fix_expander_row_styling (GtkWidget *widget,
                          gpointer  data)
{
  g_assert (HDY_IS_EXPANDER_ROW (widget));
  g_warning ("in %s", __FUNCTION__);
  hdy_expander_row_set_expanded (HDY_EXPANDER_ROW (widget), TRUE);
  hdy_expander_row_set_expanded (HDY_EXPANDER_ROW (widget), FALSE);
}

static void
on_add_search_engine_button_clicked_cb (GtkButton *button,
                                        gpointer *data)
{
  GtkListBoxRow *search_engine_row = GTK_LIST_BOX_ROW (data);
  SearchEngineRowData *row_data = g_object_get_data (G_OBJECT (search_engine_row), "row-data");
  EphySearchEngineListBox *search_engine_list_box = EPHY_SEARCH_ENGINE_LIST_BOX (row_data->parent);
  const char *search_engine_name = gtk_entry_get_text (GTK_ENTRY (row_data->name_entry));

  ephy_search_engine_manager_add_engine (row_data->manager,
                                         search_engine_name,
                                         gtk_entry_get_text (GTK_ENTRY (row_data->address_entry)),
                                         gtk_entry_get_text (GTK_ENTRY (row_data->bang_entry)));
  /* Load the newly added search engine. */
  add_search_engine_row (EPHY_SEARCH_ENGINE_LIST_BOX (row_data->parent), search_engine_name);
  /* Clear out the "Add search engine" row. */
  gtk_container_remove (GTK_CONTAINER (row_data->parent), GTK_WIDGET (search_engine_row));
  add_search_engine_row (search_engine_list_box, NULL);

  gtk_list_box_invalidate_sort (GTK_LIST_BOX (search_engine_list_box));
  gtk_container_foreach (GTK_CONTAINER (search_engine_list_box), fix_expander_row_styling, NULL);
}

static void
on_remove_button_clicked_cb (GtkButton *button,
                             gpointer  *data)
{
  GtkListBoxRow *search_engine_row = GTK_LIST_BOX_ROW (data);
  SearchEngineRowData *row_data = g_object_get_data (G_OBJECT (search_engine_row), "row-data");

  ephy_search_engine_manager_delete_engine (row_data->manager,
                                            row_data->previous_name);
  gtk_container_remove (GTK_CONTAINER (row_data->parent), GTK_WIDGET (search_engine_row));
  gtk_container_foreach (GTK_CONTAINER (row_data->parent), fix_expander_row_styling, NULL);
}

static void
on_row_radio_button_clicked_cb (GtkButton *button,
                                gpointer   data)
{
  SearchEngineRowData *row_data = data;
  g_warning ("in %s with name=%s", __FUNCTION__, row_data->previous_name);
  ephy_search_engine_manager_set_default_engine (row_data->manager, row_data->previous_name);
}

/**
 * add_search_engine_row:
 *
 * @list_box: The parent list box where to add the search engine row.
 * @previous_name: The name the search engine currently has. If NULL, the row will allow to add a search engine rather than editing the current one.
 */
static void
add_search_engine_row (EphySearchEngineListBox *list_box,
                       const char              *previous_name)
{
  GtkWidget *search_engine_row, *child_row;
  GtkWidget *grid, *label, *entry;
  GtkWidget *insight_hbox, *image, *button;
  GtkWidget *radio_button;
  SearchEngineRowData *row_data = g_new0 (SearchEngineRowData, 1);

  search_engine_row = hdy_expander_row_new ();
  /* Child rows have access to their list box so they can remove themselves from the list then sort again the list. */
  row_data->parent = GTK_LIST_BOX (list_box);
  row_data->manager = list_box->manager;
  /* When the previous name is NULL, the row adapts its behavior like saving only when clicking the validate button, etc. */
  row_data->previous_name = g_strdup (previous_name);
  row_data->invalid_fields = SEARCH_ENGINE_FIELD_ALL;
  g_object_set_data_full (G_OBJECT (search_engine_row), "row-data", row_data, search_engine_row_data_free);

  if (row_data->previous_name)
    gtk_list_box_prepend (GTK_LIST_BOX (list_box), search_engine_row);
  else
    /* Insert the "Add search engine" row at the end. */
    gtk_list_box_insert (GTK_LIST_BOX (list_box), search_engine_row, -1);

  g_signal_connect (search_engine_row, "notify::expanded", G_CALLBACK (unexpand_other_rows_cb), list_box);

  /* You can't set as default a search engine that doesn't (yet) exists. */
  if (row_data->previous_name) {
    g_autofree char *default_search_engine_name = ephy_search_engine_manager_get_default_engine (row_data->manager);
    /* Add this radio button to the group. */
    radio_button = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (list_box->radio_buttons_group));
    /* Tick the radio button if it's the default search engine. */
    if (g_strcmp0 (row_data->previous_name, default_search_engine_name) == 0)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), TRUE);
    gtk_widget_set_valign (radio_button, GTK_ALIGN_CENTER);
    hdy_expander_row_add_prefix (HDY_EXPANDER_ROW (search_engine_row), radio_button);
    /* FIXME: It seems it is called for the old radio button and for the other radio button that got clicked (but what I want is only the latest event). */
    g_signal_connect (radio_button, "clicked", G_CALLBACK (on_row_radio_button_clicked_cb), row_data);
    gtk_widget_show (radio_button);
  }
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
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  entry = gtk_entry_new ();
  gtk_grid_attach_next_to (GTK_GRID (grid), entry, label, GTK_POS_RIGHT, 1, 1);
  gtk_widget_set_hexpand (entry, TRUE);
  if (previous_name)
    gtk_entry_set_text (GTK_ENTRY (entry), previous_name);
  gtk_widget_show (entry);
  if (previous_name) {
    g_object_bind_property (entry, "text", HDY_EXPANDER_ROW (search_engine_row), "title", G_BINDING_SYNC_CREATE);
  } else {
    hdy_expander_row_set_title (HDY_EXPANDER_ROW (search_engine_row), _("New search engine…"));
  }
  gtk_widget_set_events (entry, GDK_FOCUS_CHANGE_MASK);
  gtk_entry_set_icon_sensitive (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, FALSE);
  row_data->name_entry = entry;
  g_signal_connect (entry, "focus-out-event", G_CALLBACK (on_name_entry_focus_out_cb), search_engine_row);

  /* Address label and entry pair */
  label = gtk_label_new (_("Address"));
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_show (label);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

  entry = gtk_entry_new ();
  gtk_widget_set_hexpand (entry, TRUE);
  if (previous_name)
    gtk_entry_set_text (GTK_ENTRY (entry), ephy_search_engine_manager_get_address (list_box->manager, previous_name));
  /* FIXME: does putting the translator comment before the line actually work for the _() as function argument ? */
  /* TRANSLATORS: You should localize the top-level domain "com" (e.g. "fr" in french, etc.), "example" and "search". */
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry), _("https://www.example.com/search?q=%s"));
  gtk_entry_set_input_purpose (GTK_ENTRY (entry), GTK_INPUT_PURPOSE_URL);
  gtk_entry_set_icon_sensitive (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, FALSE);
  gtk_widget_show (entry);
  gtk_grid_attach_next_to (GTK_GRID (grid), entry, label, GTK_POS_RIGHT, 1, 1);
  gtk_widget_set_events (entry, GDK_FOCUS_CHANGE_MASK);
  g_signal_connect (entry, "focus-out-event", G_CALLBACK (on_address_entry_focus_out_cb), search_engine_row);
  row_data->address_entry = entry;

  /* Bang label and entry pair */
  label = gtk_label_new (_("Shortcut"));
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_show (label);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

  entry = gtk_entry_new ();
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry), "!e");
  if (previous_name)
    gtk_entry_set_text (GTK_ENTRY (entry), ephy_search_engine_manager_get_bang (list_box->manager, previous_name));
  gtk_widget_show (entry);
  gtk_grid_attach_next_to (GTK_GRID (grid), entry, label, GTK_POS_RIGHT, 1, 1);
  row_data->bang_entry = entry;
  gtk_widget_set_events (entry, GDK_FOCUS_CHANGE_MASK);
  g_signal_connect (entry, "focus-out-event", G_CALLBACK (on_bang_entry_focus_out_cb), search_engine_row);

  /* Search engine address insight */
  insight_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_can_focus (insight_hbox, FALSE);
  gtk_widget_show (insight_hbox);
  gtk_grid_attach_next_to (GTK_GRID (grid), insight_hbox, label, GTK_POS_BOTTOM, 2, 1);
  /* gtk_container_add (GTK_CONTAINER (vbox), insight_hbox); */
  image = gtk_image_new_from_icon_name ("dialog-information-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_show (image);
  gtk_container_add (GTK_CONTAINER (insight_hbox), image);

  label = gtk_label_new (_("To determine the search address, perform a search using the search engine that you want to add and replace the search term with %s."));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_widget_show (label);
  gtk_widget_show (insight_hbox);
  gtk_container_add (GTK_CONTAINER (insight_hbox), label);

  if (previous_name) {
    button = gtk_button_new_with_label (_("Remove search engine"));
    gtk_style_context_add_class (gtk_widget_get_style_context (button), GTK_STYLE_CLASS_DESTRUCTIVE_ACTION);
    g_signal_connect (GTK_BUTTON (button), "clicked", G_CALLBACK (on_remove_button_clicked_cb), search_engine_row);
  } else {
    button = gtk_button_new_with_label (_("Add search engine"));
    gtk_style_context_add_class (gtk_widget_get_style_context (button), GTK_STYLE_CLASS_SUGGESTED_ACTION);
    g_signal_connect (GTK_BUTTON (button), "clicked", G_CALLBACK (on_add_search_engine_button_clicked_cb), search_engine_row);
    gtk_widget_set_sensitive (button, FALSE);
    row_data->add_search_engine_button = button;
  }
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_widget_set_hexpand (button, FALSE);
  gtk_grid_attach_next_to (GTK_GRID (grid), button, insight_hbox, GTK_POS_BOTTOM, 2, 1);
  /* gtk_container_add (GTK_CONTAINER (vbox), button); */
  gtk_widget_show (button);

  gtk_widget_show (GTK_WIDGET (search_engine_row));
}

static int
list_box_sort_func (GtkListBoxRow *first_row,
                    GtkListBoxRow *second_row,
                    gpointer       user_data)
{
  g_autofree char *first_name = NULL;
  g_autofree char *second_name = NULL;
  SearchEngineRowData *first_row_data = g_object_get_data (G_OBJECT (first_row), "row-data");
  SearchEngineRowData *second_row_data = g_object_get_data (G_OBJECT (second_row), "row-data");

  /* FIXME: remove those debug lines. This is to understand how the sorting is done by the GtkListBox. */
  /* => I still don't understand how it does it but it works with the two conditions that follow the printing. */
  /* g_warning ("%s", first_row_data->previous_name); */
  /* g_warning ("%s\n========================================", second_row_data->previous_name); */

  /* Place the "add search engine" row at the end. */
  if (!first_row_data->previous_name)
    return 1;
  if (!second_row_data->previous_name)
    return -1;

  first_name = g_utf8_strdown (first_row_data->previous_name, -1);
  second_name = g_utf8_strdown (second_row_data->previous_name, -1);
  return g_strcmp0 (first_name, second_name);
}

static void
ephy_search_engine_list_box_init (EphySearchEngineListBox *self)
{
  char **names, *name;
  int i = 0;

  self->manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());
  names = ephy_search_engine_manager_get_names (self->manager);

  self->radio_buttons_group = gtk_radio_button_new (NULL);
  /* Ref the radio buttons group and remove the floating reference because we don't hook this widget to any part of the UI (for example gtk_container_add would remove the floating reference and ref it). */
  g_object_ref_sink (self->radio_buttons_group);

  gtk_widget_show (self->radio_buttons_group);
  for (; names[i] != NULL; ++i) {
    name = names[i];
    g_warning ("in func %s , i=%d name=%s", __FUNCTION__, i, name);
    add_search_engine_row (self, name);
  }
  g_strfreev (names);
  /* Finally append the "Add search engine" row. */
  add_search_engine_row (self, NULL);
  /* g_warning ("filtered string \"%s\"", filter_str_with_functor ("ÉAÉÉÉÉÂÂ^AaaaââèÆÆæœŒŒÈÈÈkkldsÀÀÀ", g_unichar_isupper)); */
  gtk_list_box_set_sort_func (GTK_LIST_BOX (self), list_box_sort_func, NULL, NULL);
  gtk_list_box_invalidate_sort (GTK_LIST_BOX (self));
  gtk_widget_show (GTK_WIDGET (self));
}
