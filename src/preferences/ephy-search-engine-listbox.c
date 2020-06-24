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

struct _EphySearchEngineListBox
{
  GtkListBox parent_instance;

  EphySearchEngineManager *manager;
};

G_DEFINE_TYPE (EphySearchEngineListBox, ephy_search_engine_list_box, GTK_TYPE_LIST_BOX)

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

  /* We save the changes only when the row is destroyed (i.e. when the parent dialog is closed). */
  /* This way we don't have to monitor user typing and do I/O frequently but just once on dialog close. */
  /* ephy_search_engine_manager_modify_engine (self->manager, */
  /*                                            self->old_name, */
  /*                                           gtk_entry_get_text (GTK_ENTRY (self->address_entry)), */
  /*                                           gtk_entry_get_text (GTK_ENTRY (self->bang_entry))); */

  G_OBJECT_CLASS (ephy_search_engine_list_box_parent_class)->dispose (object);
}

static void
ephy_search_engine_list_box_finalize (GObject *object)
{
  /* EphySearchEngineListBox *self = (EphySearchEngineListBox *)object; */

  G_OBJECT_CLASS (ephy_search_engine_list_box_parent_class)->finalize (object);

  /* if (self->old_name) */
  /*   g_free (self->old_name); */
}

static void
ephy_search_engine_list_box_class_init (EphySearchEngineListBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /* properties[PROP_NAME] = g_param_spec_string ("search-engine-name", "The name of the search engine", "Creates a new search engine if NULL, otherwise edit the search engine with this name", NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM) */

  /* g_object_class_install_properties (object_class, N_PROPS, properties); */
  object_class->finalize = ephy_search_engine_list_box_finalize;
  object_class->dispose = ephy_search_engine_list_box_dispose;
}

/* FIXME: Is there any function like that built in glib or the standard library ? */
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
  g_autofree char *stripped_address = g_strstrip (g_strdup (address));

  if (g_strcmp0 (stripped_address, "") == 0)
    return _("This field is required");
  if (!g_str_has_prefix (stripped_address, "http://") && !g_str_has_prefix (stripped_address, "https://"))
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

#define GET_ENTRY_CONTENT(search_engine_row, data_name) (gtk_entry_get_text (GTK_ENTRY (g_object_get_data (G_OBJECT (search_engine_row), data_name))))

static void
save_informations_to_manager (GObject *search_engine_row)
{
  /* Only change the address in the manager when not adding a search engine */
  /* FIXME: this is painful to do. It would be simpler if it had a _set_bang() and _set_address() functions. */
  ephy_search_engine_manager_modify_engine (g_object_get_data (search_engine_row, "search-engine-manager"),
                                            GET_ENTRY_CONTENT (search_engine_row, "name-entry"),
                                            GET_ENTRY_CONTENT (search_engine_row, "address-entry"),
                                            GET_ENTRY_CONTENT (search_engine_row, "bang-entry"));
}
static gboolean
on_address_entry_focus_out_cb (GObject    *instance,
                               GParamSpec *pspec,
                               GObject    *search_engine_row)
{
  GtkEntry *address_entry = GTK_ENTRY (instance);
  g_autofree char *address = g_strstrip (g_strdup (gtk_entry_get_text (address_entry)));
  /* EphySearchEngineListBox *list_box = g_object_get_data (G_OBJECT (search_engine_row), "parent-list-box"); */
  const char *validation_message = validate_search_engine_address (address);

  if (validation_message) {
    gtk_entry_set_icon_from_icon_name (address_entry, GTK_ENTRY_ICON_SECONDARY, "dialog-warning-symbolic");
    gtk_entry_set_icon_tooltip_text (address_entry, GTK_ENTRY_ICON_SECONDARY, validation_message);
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (address_entry)), "error");
  } else {
    gtk_entry_set_icon_from_icon_name (address_entry, GTK_ENTRY_ICON_SECONDARY, NULL);
    gtk_entry_set_icon_tooltip_text (address_entry, GTK_ENTRY_ICON_SECONDARY, NULL);
    gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (address_entry)), "error");
    if (g_object_get_data (search_engine_row, "previous-name"))
      save_informations_to_manager (search_engine_row);
  }
  return FALSE;
}


static gboolean
on_bang_entry_focus_out_cb (GObject    *instance,
                            GParamSpec *pspec,
                            GObject    *data)
{
  /* Save only when editing an existing search engine. */
  if (g_object_get_data (G_OBJECT (data), "previous-name"))
    save_informations_to_manager (data);
  return FALSE;
}

/* FIXME: here is a bunch of different characters in UTF-8 土豆沙拉☠☣↓œŒ «@ÉBIngy#/\@ÉyYÉeéÈ to test this (try by separating with spaces as well). */
static void
update_bang_with_name (GObject    *search_engine_row,
                       const char *name)
{
  /* This function automatically builds the shortcut string from the search engine name, taking every first character in each word and every uppercase characters. */
  /* This means name "DuckDuckGo" will set bang to "!ddg" and "duck duck go" will set bang to "!ddg" as well. */
  g_autofree char *search_engine_name = g_strstrip (g_strdup (name));
  char **words, *word;
  g_autofree char *acronym = g_strdup ("");
  g_autofree char *lowercase_acronym = NULL;
  g_autofree char *final_bang = NULL;
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
  gtk_entry_set_text (GTK_ENTRY (g_object_get_data (search_engine_row, "bang-entry")), final_bang);
  if (g_object_get_data (search_engine_row, "previous-name"))
    save_informations_to_manager (search_engine_row);
}

static void
update_search_engine_name (GObject *search_engine_row)
{
  char *previous_name = g_object_get_data (search_engine_row, "previous-name");
  const char *new_name = GET_ENTRY_CONTENT (search_engine_row, "name-entry");

  ephy_search_engine_manager_rename (g_object_get_data (search_engine_row, "search-engine-manager"),
                                     previous_name,
                                     new_name);
  g_free (previous_name);
  g_object_set_data (search_engine_row, "previous-name", g_strdup (new_name));
}

static gboolean
on_name_entry_focus_out_cb (GObject    *instance,
                            GParamSpec *pspec,
                            GObject    *search_engine_row)
{
  const char *new_name = gtk_entry_get_text (GTK_ENTRY (instance));

  /* When editing a new search engine (if previous_name is NULL), we only rename when clicking the validate button. */
  if (g_object_get_data (search_engine_row, "previous-name"))
    update_search_engine_name (search_engine_row);
  update_bang_with_name (search_engine_row, new_name);

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

  g_warning ("in func %s, expanded_row_index=%d", __FUNCTION__, expanded_row_index);
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

static void
on_remove_button_clicked_cb (GtkButton *button,
                             gpointer  *data)
{
  GObject *search_engine_row = G_OBJECT (data);
  EphySearchEngineListBox *list_box = NULL;
  g_assert (GTK_IS_LIST_BOX_ROW (search_engine_row));
  list_box = g_object_get_data (search_engine_row, "parent-list-box");

  ephy_search_engine_manager_delete_engine (EPHY_SEARCH_ENGINE_MANAGER (g_object_get_data (search_engine_row, "search-engine-manager")),
                                            g_object_get_data (search_engine_row, "previous-name"));
  gtk_container_remove (GTK_CONTAINER (list_box), GTK_WIDGET (search_engine_row));
  /* FIXME: the expander rows before and after the removed one don't update its style (rounded corners and a large black separator line). It might be possible to quickly expand and unexpand the row before so it updates its style. */
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
  GtkWidget *search_engine_row, *child_row, *vbox, *insight_hbox, *grid, *label, *entry, *image, *button;

  search_engine_row = hdy_expander_row_new ();
  g_object_set_data (G_OBJECT (search_engine_row), "search-engine-manager", list_box->manager);
  /* When the previous name is NULL, the HdyExpanderRow adapts its behavior like saving only when clicking the validate button, etc. */
  g_object_set_data (G_OBJECT (search_engine_row), "previous-name", g_strdup (previous_name));
  /* Child rows have access to the parent list box so they can remove themselves from the list and sort again the list. */
  g_object_set_data (G_OBJECT (search_engine_row), "parent-list-box", list_box);
  g_signal_connect (search_engine_row, "notify::expanded", G_CALLBACK (unexpand_other_rows_cb), list_box);
  if (g_object_get_data (G_OBJECT (search_engine_row), "previous-name")) {
    gtk_list_box_prepend (GTK_LIST_BOX (list_box), search_engine_row);
  } else {
    /* Insert the "Add search engine" row at the end. */
    gtk_list_box_insert (GTK_LIST_BOX (list_box), search_engine_row, -1);
  }

  child_row = gtk_list_box_row_new ();
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (child_row), FALSE);
  gtk_widget_show (child_row);
  gtk_container_add (GTK_CONTAINER (search_engine_row), child_row);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_top (vbox, 12);
  gtk_widget_set_margin_bottom (vbox, 12);
  gtk_widget_set_margin_start (vbox, 12);
  gtk_widget_set_margin_end (vbox, 12);
  gtk_widget_set_can_focus (vbox, FALSE);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (child_row), vbox);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_widget_set_can_focus (grid, FALSE);
  gtk_widget_show (grid);
  gtk_container_add (GTK_CONTAINER (vbox), grid);

  /* Name label and entry pair */
  label = gtk_label_new (_("Name"));
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_show (label);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  entry = gtk_entry_new ();
  gtk_widget_set_hexpand (entry, TRUE);
  if (previous_name)
    gtk_entry_set_text (GTK_ENTRY (entry), previous_name);
  gtk_widget_show (entry);
  gtk_grid_attach_next_to (GTK_GRID (grid), entry, label, GTK_POS_RIGHT, 1, 1);
  g_object_bind_property (entry, "text", HDY_EXPANDER_ROW (search_engine_row), "title", G_BINDING_SYNC_CREATE | G_BINDING_DEFAULT);
  gtk_widget_set_events (entry, GDK_FOCUS_CHANGE_MASK);
  g_object_set_data (G_OBJECT (search_engine_row), "name-entry", entry);
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
  g_object_set_data (G_OBJECT (search_engine_row), "address-entry", entry);

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
  g_object_set_data (G_OBJECT (search_engine_row), "bang-entry", entry);
  gtk_widget_set_events (entry, GDK_FOCUS_CHANGE_MASK);
  g_signal_connect (entry, "focus-out-event", G_CALLBACK (on_bang_entry_focus_out_cb), search_engine_row);

  /* Search engine address insight */
  insight_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_can_focus (insight_hbox, FALSE);
  gtk_widget_show (insight_hbox);
  gtk_container_add (GTK_CONTAINER (vbox), insight_hbox);
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
  }
  gtk_widget_set_halign (button, GTK_ALIGN_END);
  gtk_widget_set_hexpand (button, FALSE);
  gtk_container_add (GTK_CONTAINER (vbox), button);
  gtk_widget_show (button);

  gtk_widget_show (GTK_WIDGET (search_engine_row));
  gtk_list_box_invalidate_sort (GTK_LIST_BOX (list_box));
}

static int
list_box_sort_func (GtkListBoxRow *first_row,
                    GtkListBoxRow *second_row,
                    gpointer       user_data)
{
  g_autofree char *first_name = NULL;
  g_autofree char *second_name = NULL;
  /* Place the "add search engine" row at the end. */
  if (!g_object_get_data (G_OBJECT (second_row), "previous-name"))
    return -1;
  first_name = g_utf8_strdown (g_object_get_data (G_OBJECT (first_row), "previous-name"), -1);
  second_name = g_utf8_strdown (g_object_get_data (G_OBJECT (second_row), "previous-name"), -1);
  return g_strcmp0 (first_name, second_name);
}

static void
ephy_search_engine_list_box_init (EphySearchEngineListBox *self)
{
  char **names, *name;
  int i = 0;

  self->manager = ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ());
  names = ephy_search_engine_manager_get_names (self->manager);

  for (; names[i] != NULL; ++i) {
    name = names[i];
    g_warning ("in func %s , i=%d name=%s", __FUNCTION__, i, name);
    add_search_engine_row (self, name);
  }
  g_strfreev (names);
  /* Finally append the "Add search engine" row. */
  add_search_engine_row (self, NULL);
  g_warning ("filtered string \"%s\"", filter_str_with_functor ("ÉAÉÉÉÉÂÂ^AaaaââèÆÆæœŒŒÈÈÈkkldsÀÀÀ", g_unichar_isupper));
  gtk_list_box_set_sort_func (GTK_LIST_BOX (self), list_box_sort_func, NULL, NULL);
  gtk_widget_show (GTK_WIDGET (self));
}
