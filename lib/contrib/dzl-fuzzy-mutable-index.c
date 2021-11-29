/* dzl-fuzzy-mutable-index.c
 *
 * Copyright (C) 2014-2017 Christian Hergert <christian@hergert.me>
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
 */

#include "config.h"

#include <ctype.h>
#include <string.h>

#include "dzl-fuzzy-mutable-index.h"

/**
 * SECTION:dzl-fuzzy-mutable-index
 * @title: DzlFuzzyMutableIndex Matching
 * @short_description: DzlFuzzyMutableIndex matching for GLib based programs.
 *
 * #DzlFuzzyMutableIndex provides a fulltext index that focuses around fuzzy
 * matching words. This version of the datastructure is focused around
 * in-memory storage. This makes mutability performance of adding or removing
 * items from the corpus simpler.
 *
 * If you need mostly read-only indexes, you might consider using
 * #DzlFuzzyIndex and #DzlFuzzyIndexBuilder which can create a disk-based file
 * and mmap() a read-only version of the data set.
 *
 * It is a programming error to modify #Fuzzy while holding onto an array
 * of #FuzzyMatch elements. The position of strings within the DzlFuzzyMutableIndexMatch
 * may no longer be valid.
 */

G_DEFINE_BOXED_TYPE (DzlFuzzyMutableIndex, dzl_fuzzy_mutable_index,
                     (GBoxedCopyFunc)dzl_fuzzy_mutable_index_ref,
                     (GBoxedFreeFunc)dzl_fuzzy_mutable_index_unref)

struct _DzlFuzzyMutableIndex
{
  volatile gint   ref_count;
  GByteArray     *heap;
  GArray         *id_to_text_offset;
  GPtrArray      *id_to_value;
  GHashTable     *char_tables;
  GHashTable     *removed;
  guint           in_bulk_insert : 1;
  guint           case_sensitive : 1;
};

#pragma pack(push, 1)
typedef struct
{
#ifdef G_OS_WIN32
  guint32 id;
  guint16 pos;
#else
  guint64 id : 32;
  guint64 pos : 16;
#endif
} DzlFuzzyMutableIndexItem;
#pragma pack(pop)

G_STATIC_ASSERT (sizeof(DzlFuzzyMutableIndexItem) == 6);

typedef struct
{
   DzlFuzzyMutableIndex        *fuzzy;
   GArray      **tables;
   gint         *state;
   guint         n_tables;
   gsize         max_matches;
   const gchar  *needle;
   GHashTable   *matches;
} DzlFuzzyMutableIndexLookup;

static gint
dzl_fuzzy_mutable_index_item_compare (gconstpointer a,
                                      gconstpointer b)
{
  gint ret;

  const DzlFuzzyMutableIndexItem *fa = a;
  const DzlFuzzyMutableIndexItem *fb = b;

  if ((ret = fa->id - fb->id) == 0)
    ret = fa->pos - fb->pos;

  return ret;
}

static gint
dzl_fuzzy_mutable_index_match_compare (gconstpointer a,
                                       gconstpointer b)
{
  const DzlFuzzyMutableIndexMatch *ma = a;
  const DzlFuzzyMutableIndexMatch *mb = b;

  if (ma->score < mb->score) {
    return 1;
  } else if (ma->score > mb->score) {
    return -1;
  }

  return strcmp (ma->key, mb->key);
}

DzlFuzzyMutableIndex *
dzl_fuzzy_mutable_index_ref (DzlFuzzyMutableIndex *fuzzy)
{
  g_return_val_if_fail (fuzzy, NULL);
  g_return_val_if_fail (fuzzy->ref_count > 0, NULL);

  g_atomic_int_inc (&fuzzy->ref_count);

  return fuzzy;
}

/**
 * dzl_fuzzy_mutable_index_new:
 * @case_sensitive: %TRUE if case should be preserved.
 *
 * Create a new #Fuzzy for fuzzy matching strings.
 *
 * Returns: A newly allocated #Fuzzy that should be freed with dzl_fuzzy_mutable_index_unref().
 */
DzlFuzzyMutableIndex *
dzl_fuzzy_mutable_index_new (gboolean case_sensitive)
{
  DzlFuzzyMutableIndex *fuzzy;

  fuzzy = g_slice_new0 (DzlFuzzyMutableIndex);
  fuzzy->ref_count = 1;
  fuzzy->heap = g_byte_array_new ();
  fuzzy->id_to_value = g_ptr_array_new ();
  fuzzy->id_to_text_offset = g_array_new (FALSE, FALSE, sizeof (gsize));
  fuzzy->char_tables = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)g_array_unref);
  fuzzy->case_sensitive = case_sensitive;
  fuzzy->removed = g_hash_table_new (g_direct_hash, g_direct_equal);

  return fuzzy;
}

DzlFuzzyMutableIndex *
dzl_fuzzy_mutable_index_new_with_free_func (gboolean       case_sensitive,
                                            GDestroyNotify free_func)
{
  DzlFuzzyMutableIndex *fuzzy;

  fuzzy = dzl_fuzzy_mutable_index_new (case_sensitive);
  dzl_fuzzy_mutable_index_set_free_func (fuzzy, free_func);

  return fuzzy;
}

void
dzl_fuzzy_mutable_index_set_free_func (DzlFuzzyMutableIndex *fuzzy,
                                       GDestroyNotify        free_func)
{
  g_return_if_fail (fuzzy);

  g_ptr_array_set_free_func (fuzzy->id_to_value, free_func);
}

static gsize
dzl_fuzzy_mutable_index_heap_insert (DzlFuzzyMutableIndex *fuzzy,
                                     const gchar          *text)
{
  gsize ret;

  g_assert (fuzzy != NULL);
  g_assert (text != NULL);

  ret = fuzzy->heap->len;

  g_byte_array_append (fuzzy->heap, (guint8 *)text, strlen (text) + 1);

  return ret;
}

/**
 * dzl_fuzzy_mutable_index_begin_bulk_insert:
 * @fuzzy: (in): A #Fuzzy.
 *
 * Start a bulk insertion. @fuzzy is not ready for searching until
 * dzl_fuzzy_mutable_index_end_bulk_insert() has been called.
 *
 * This allows for inserting large numbers of strings and deferring
 * the final sort until dzl_fuzzy_mutable_index_end_bulk_insert().
 */
void
dzl_fuzzy_mutable_index_begin_bulk_insert (DzlFuzzyMutableIndex *fuzzy)
{
   g_return_if_fail (fuzzy);
   g_return_if_fail (!fuzzy->in_bulk_insert);

   fuzzy->in_bulk_insert = TRUE;
}

/**
 * dzl_fuzzy_mutable_index_end_bulk_insert:
 * @fuzzy: (in): A #Fuzzy.
 *
 * Complete a bulk insert and resort the index.
 */
void
dzl_fuzzy_mutable_index_end_bulk_insert (DzlFuzzyMutableIndex *fuzzy)
{
   GHashTableIter iter;
   gpointer key;
   gpointer value;

   g_return_if_fail(fuzzy);
   g_return_if_fail(fuzzy->in_bulk_insert);

   fuzzy->in_bulk_insert = FALSE;

   g_hash_table_iter_init (&iter, fuzzy->char_tables);

   while (g_hash_table_iter_next (&iter, &key, &value)) {
      GArray *table = value;

      g_array_sort (table, dzl_fuzzy_mutable_index_item_compare);
   }
}

/**
 * dzl_fuzzy_mutable_index_insert:
 * @fuzzy: (in): A #Fuzzy.
 * @key: (in): A UTF-8 encoded string.
 * @value: (in): A value to associate with key.
 *
 * Inserts a string into the fuzzy matcher.
 */
void
dzl_fuzzy_mutable_index_insert (DzlFuzzyMutableIndex *fuzzy,
                                const gchar          *key,
                                gpointer              value)
{
  const gchar *tmp;
  gchar *downcase = NULL;
  gsize offset;
  guint id;

  if (G_UNLIKELY (!key || !*key || (fuzzy->id_to_text_offset->len == G_MAXUINT)))
    return;

  if (!fuzzy->case_sensitive)
    downcase = g_utf8_casefold (key, -1);

  offset = dzl_fuzzy_mutable_index_heap_insert (fuzzy, key);
  id = fuzzy->id_to_text_offset->len;
  g_array_append_val (fuzzy->id_to_text_offset, offset);
  g_ptr_array_add (fuzzy->id_to_value, value);

  if (!fuzzy->case_sensitive)
    key = downcase;

  for (tmp = key; *tmp; tmp = g_utf8_next_char (tmp))
    {
      gunichar ch = g_utf8_get_char (tmp);
      GArray *table;
      DzlFuzzyMutableIndexItem item;

      table = g_hash_table_lookup (fuzzy->char_tables, GINT_TO_POINTER (ch));

      if (G_UNLIKELY (table == NULL))
        {
          table = g_array_new (FALSE, FALSE, sizeof (DzlFuzzyMutableIndexItem));
          g_hash_table_insert (fuzzy->char_tables, GINT_TO_POINTER (ch), table);
        }

      item.id = id;
      item.pos = (guint)(gsize)(tmp - key);

      g_array_append_val (table, item);
    }

  if (G_UNLIKELY (!fuzzy->in_bulk_insert))
    {
      for (tmp = key; *tmp; tmp = g_utf8_next_char (tmp))
        {
          GArray *table;
          gunichar ch;

          ch = g_utf8_get_char (tmp);
          table = g_hash_table_lookup (fuzzy->char_tables, GINT_TO_POINTER (ch));
          g_array_sort (table, dzl_fuzzy_mutable_index_item_compare);
        }
    }

  g_free (downcase);
}

/**
 * dzl_fuzzy_mutable_index_unref:
 * @fuzzy: A #Fuzzy.
 *
 * Decrements the reference count of fuzzy by one. When the reference count
 * reaches zero, the structure will be freed.
 */
void
dzl_fuzzy_mutable_index_unref (DzlFuzzyMutableIndex *fuzzy)
{
  g_return_if_fail (fuzzy);
  g_return_if_fail (fuzzy->ref_count > 0);

  if (G_UNLIKELY (g_atomic_int_dec_and_test (&fuzzy->ref_count)))
    {
      g_byte_array_unref (fuzzy->heap);
      fuzzy->heap = NULL;

      g_array_unref (fuzzy->id_to_text_offset);
      fuzzy->id_to_text_offset = NULL;

      g_ptr_array_unref (fuzzy->id_to_value);
      fuzzy->id_to_value = NULL;

      g_hash_table_unref (fuzzy->char_tables);
      fuzzy->char_tables = NULL;

      g_hash_table_unref (fuzzy->removed);
      fuzzy->removed = NULL;

      g_slice_free (DzlFuzzyMutableIndex, fuzzy);
    }
}

static void
rollback_state_to_pos (GArray *table,
                       gint   *state,
                       guint   id,
                       guint   pos)
{
  g_assert (table != NULL);
  g_assert (state != NULL);
  g_assert (pos > 0);

  while (*state > 0 && (guint) *state <= table->len)
    {
      DzlFuzzyMutableIndexItem *iter;

      (*state)--;

      iter = &g_array_index (table, DzlFuzzyMutableIndexItem, *state);

      if (iter->id > id || (iter->id == id && (guint) *state >= pos))
        continue;

      break;
    }
}

static gboolean
dzl_fuzzy_mutable_index_do_match (DzlFuzzyMutableIndexLookup *lookup,
                                  DzlFuzzyMutableIndexItem   *item,
                                  guint                       table_index,
                                  gint                        score)
{
  gboolean ret = FALSE;
  GArray *table;
  gint *state;

  table = lookup->tables [table_index];
  state = &lookup->state [table_index];

  for (; state [0] < (gint)table->len; state [0]++)
    {
      DzlFuzzyMutableIndexItem *iter;
      gpointer key;
      gint iter_score;

      iter = &g_array_index (table, DzlFuzzyMutableIndexItem, state[0]);

      if ((iter->id < item->id) || ((iter->id == item->id) && (iter->pos <= item->pos)))
        continue;
      else if (iter->id > item->id)
        break;

      iter_score = score + (iter->pos - item->pos - 1);

      if ((table_index + 1) < lookup->n_tables)
        {
          if (dzl_fuzzy_mutable_index_do_match (lookup, iter, table_index + 1, iter_score))
            {
              ret = TRUE;

              /* We already found a match, but we could have a better match
               * further in the word. We need to rollback all of our additional
               * table state to the current position so that we can possibly
               * advance again.
               */
              if ((state[0] + 1) < (gint) table->len &&
                  g_array_index (table, DzlFuzzyMutableIndexItem, state[0] + 1).id == item->id)
                {
                  for (guint i = table_index + 1; i < lookup->n_tables; i++)
                    rollback_state_to_pos (lookup->tables[i], &lookup->state[i], iter->id, iter->pos + 1);
                }
            }
          continue;
        }

      key = GINT_TO_POINTER (iter->id);

      if (!g_hash_table_contains (lookup->matches, key) ||
          (iter_score < GPOINTER_TO_INT (g_hash_table_lookup (lookup->matches, key))))
        g_hash_table_insert (lookup->matches, key, GINT_TO_POINTER (iter_score));

      ret = TRUE;
    }

  return ret;
}

static inline const gchar *
dzl_fuzzy_mutable_index_get_string (DzlFuzzyMutableIndex *fuzzy,
                                    gint                  id)
{
  guint offset = g_array_index (fuzzy->id_to_text_offset, gsize, id);
  return (const gchar *)&fuzzy->heap->data [offset];
}

/**
 * dzl_fuzzy_mutable_index_match:
 * @fuzzy: (in): A #Fuzzy.
 * @needle: (in): The needle to fuzzy search for.
 * @max_matches: (in): The max number of matches to return.
 *
 * DzlFuzzyMutableIndex searches within @fuzzy for strings that fuzzy match @needle.
 * Only up to @max_matches will be returned.
 *
 * TODO: max_matches is not yet respected.
 *
 * Returns: (transfer full) (element-type DzlFuzzyMutableIndexMatch): A newly allocated
 *   #GArray containing #FuzzyMatch elements. This should be freed when
 *   the caller is done with it using g_array_unref().
 *   It is a programming error to keep the structure around longer than
 *   the @fuzzy instance.
 */
GArray *
dzl_fuzzy_mutable_index_match (DzlFuzzyMutableIndex *fuzzy,
                               const gchar          *needle,
                               gsize                 max_matches)
{
  DzlFuzzyMutableIndexLookup lookup = { 0 };
  DzlFuzzyMutableIndexMatch match;
  DzlFuzzyMutableIndexItem *item;
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  const gchar *tmp;
  GArray *matches = NULL;
  GArray *root;
  gchar *downcase = NULL;
  guint i;

  g_return_val_if_fail (fuzzy, NULL);
  g_return_val_if_fail (!fuzzy->in_bulk_insert, NULL);
  g_return_val_if_fail (needle, NULL);

  matches = g_array_new (FALSE, FALSE, sizeof (DzlFuzzyMutableIndexMatch));

  if (!*needle)
    goto cleanup;

  if (!fuzzy->case_sensitive)
    {
      downcase = g_utf8_casefold (needle, -1);
      needle = downcase;
    }

  lookup.fuzzy = fuzzy;
  lookup.n_tables = g_utf8_strlen (needle, -1);
  lookup.state = g_new0 (gint, lookup.n_tables);
  lookup.tables = g_new0 (GArray*, lookup.n_tables);
  lookup.needle = needle;
  lookup.max_matches = max_matches;
  lookup.matches = g_hash_table_new (NULL, NULL);

  for (i = 0, tmp = needle; *tmp; tmp = g_utf8_next_char (tmp))
    {
      gunichar ch;
      GArray *table;

      ch = g_utf8_get_char (tmp);
      table = g_hash_table_lookup (fuzzy->char_tables, GINT_TO_POINTER (ch));

      if (table == NULL)
        goto cleanup;

      lookup.tables [i++] = table;
    }

  g_assert (lookup.n_tables == i);
  g_assert (lookup.tables [0] != NULL);

  root = lookup.tables [0];

  if (G_LIKELY (lookup.n_tables > 1))
    {
      for (i = 0; i < root->len; i++)
        {
          item = &g_array_index (root, DzlFuzzyMutableIndexItem, i);

          if (dzl_fuzzy_mutable_index_do_match (&lookup, item, 1, 0) &&
              i + 1 < root->len &&
              (item + 1)->id == item->id)
            {
              /* We found a match, but we might find another one with a higher
               * score later on for the same item of the corpus.  We need to
               * roll state back to the position we're starting at so that we
               * can match all the same characters again.
               */
              for (guint j = 1; j < lookup.n_tables; j++)
                rollback_state_to_pos (lookup.tables[j], &lookup.state[j], item->id, item->pos + 1);
            }
        }
    }
  else
    {
      guint last_id = G_MAXUINT;

      for (i = 0; i < root->len; i++)
        {
          item = &g_array_index (root, DzlFuzzyMutableIndexItem, i);
          match.id = GPOINTER_TO_INT (item->id);
          if (match.id != last_id)
            {
              match.key = dzl_fuzzy_mutable_index_get_string (fuzzy, item->id);
              match.value = g_ptr_array_index (fuzzy->id_to_value, item->id);
              match.score = 1.0 / (strlen (match.key) + item->pos);
              g_array_append_val (matches, match);
              last_id = match.id;
            }
        }

      goto cleanup;
    }

  g_hash_table_iter_init (&iter, lookup.matches);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      /* Ignore keys that have a tombstone record. */
      if (g_hash_table_contains (fuzzy->removed, key))
        continue;

      match.id = GPOINTER_TO_INT (key);
      match.key = dzl_fuzzy_mutable_index_get_string (fuzzy, match.id);
      match.value = g_ptr_array_index (fuzzy->id_to_value, match.id);

      /* If we got a perfect substring match, then this is 1.0, and avoid
       * perturbing further or we risk non-contiguous (but shorter strings)
       * matching at higher value.
       */
      if (value == NULL)
        match.score = 1.0;
      else
        match.score = 1.0 / (strlen (match.key) + GPOINTER_TO_INT (value));

      g_array_append_val (matches, match);
    }

  /*
   * TODO: We could be more clever here when inserting into the array
   *       only if it is a lower score than the end or < max items.
   */

  if (max_matches != 0)
    {
      g_array_sort (matches, dzl_fuzzy_mutable_index_match_compare);

      if (max_matches && (matches->len > max_matches))
        g_array_set_size (matches, max_matches);
    }

cleanup:
  g_free (downcase);
  g_free (lookup.state);
  g_free (lookup.tables);
  g_clear_pointer (&lookup.matches, g_hash_table_unref);

  return matches;
}

gboolean
dzl_fuzzy_mutable_index_contains (DzlFuzzyMutableIndex *fuzzy,
                                  const gchar          *key)
{
  GArray *ar;
  gboolean ret;

  g_return_val_if_fail (fuzzy != NULL, FALSE);

  ar = dzl_fuzzy_mutable_index_match (fuzzy, key, 1);
  ret = (ar != NULL) && (ar->len > 0);
  g_clear_pointer (&ar, g_array_unref);

  return ret;
}

void
dzl_fuzzy_mutable_index_remove (DzlFuzzyMutableIndex *fuzzy,
                                const gchar          *key)
{
  GArray *ar;

  g_return_if_fail (fuzzy != NULL);

  if (!key || !*key)
    return;

  ar = dzl_fuzzy_mutable_index_match (fuzzy, key, 1);

  if (ar != NULL && ar->len > 0)
    {
      for (guint i = 0; i < ar->len; i++)
        {
          const DzlFuzzyMutableIndexMatch *match = &g_array_index (ar, DzlFuzzyMutableIndexMatch, i);

          if (g_strcmp0 (match->key, key) == 0)
            g_hash_table_insert (fuzzy->removed, GINT_TO_POINTER (match->id), NULL);
        }
    }

  g_clear_pointer (&ar, g_array_unref);
}

gchar *
dzl_fuzzy_highlight (const gchar *str,
                     const gchar *match,
                     gboolean     case_sensitive)
{
  static const gchar *begin = "<b>";
  static const gchar *end = "</b>";
  GString *ret;
  gunichar str_ch;
  gunichar match_ch;
  gboolean element_open = FALSE;

  if (str == NULL || match == NULL)
    return g_strdup (str);

  ret = g_string_new (NULL);

  for (; *str; str = g_utf8_next_char (str))
    {
      str_ch = g_utf8_get_char (str);
      match_ch = g_utf8_get_char (match);

      if (str_ch == '&')
        {
          const gchar *entity_end = strchr (str, ';');

          if (entity_end != NULL)
            {
              gsize len = entity_end - str;

              if (element_open)
                {
                  g_string_append (ret, end);
                  element_open = FALSE;
                }

              g_string_append_len (ret, str, len + 1);
              str += len;

              continue;
            }
        }

      if ((str_ch == match_ch) ||
          (!case_sensitive && g_unichar_tolower (str_ch) == g_unichar_tolower (match_ch)))
        {
          if (!element_open)
            {
              g_string_append (ret, begin);
              element_open = TRUE;
            }

          if (str_ch == '<')
            g_string_append (ret, "&lt;");
          else if (str_ch == '>')
            g_string_append (ret, "&gt;");
          else
            g_string_append_unichar (ret, str_ch);

          /* TODO: We could seek to the next char and append in a batch. */
          match = g_utf8_next_char (match);
        }
      else
        {
          if (element_open)
            {
              g_string_append (ret, end);
              element_open = FALSE;
            }

          if (str_ch == '<')
            g_string_append (ret, "&lt;");
          else if (str_ch == '>')
            g_string_append (ret, "&gt;");
          else
            g_string_append_unichar (ret, str_ch);
        }
    }

  if (element_open)
    g_string_append (ret, end);

  return g_string_free (ret, FALSE);
}
