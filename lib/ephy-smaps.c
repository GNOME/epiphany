/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011 Igalia S.L.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-smaps.h"

#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

G_DEFINE_TYPE (EphySMaps, ephy_smaps, G_TYPE_OBJECT)

struct _EphySMapsPrivate {
  GRegex *header;
  GRegex *detail;
};

typedef struct {
  char *start;
  char *end;
  char *perms;
  char *offset;
  char *major;
  char *minor;
  char *inode;
  char *filename;
  char *size;
  char *rss;
  char *pss;
  char *shared_clean;
  char *shared_dirty;
  char *private_clean;
  char *private_dirty;
} VMA_t;

typedef struct {
  guint shared_clean;
  guint shared_dirty;
  guint private_clean;
  guint private_dirty;
} PermEntry;

static void vma_free (VMA_t* vma)
{
  g_free (vma->start);
  g_free (vma->end);
  g_free (vma->perms);
  g_free (vma->offset);
  g_free (vma->major);
  g_free (vma->minor);
  g_free (vma->inode);
  g_free (vma->filename);
  g_free (vma->size);
  g_free (vma->rss);
  g_free (vma->pss);
  g_free (vma->shared_clean);
  g_free (vma->shared_dirty);
  g_free (vma->private_clean);
  g_free (vma->private_dirty);

  g_slice_free (VMA_t, vma);
}

static void perm_entry_free (PermEntry *entry)
{
  if (entry)
    g_slice_free (PermEntry, entry);
}

static void add_to_perm_entry (GHashTable *hash, VMA_t *entry)
{
  const char *perms = entry->perms;
  PermEntry *value;
  guint number;
  gboolean insert = FALSE;

  value = g_hash_table_lookup (hash, perms);

  if (!value) {
    value = g_slice_new0 (PermEntry);
    insert = TRUE;
  }

  sscanf (entry->shared_clean, "%u", &number);
  value->shared_clean += number;
  sscanf (entry->shared_dirty, "%u", &number);
  value->shared_dirty += number;
  sscanf (entry->private_clean, "%u", &number);
  value->private_clean += number;
  sscanf (entry->private_dirty, "%u", &number);
  value->private_dirty += number;

  if (insert)
    g_hash_table_insert (hash, g_strdup (perms), value);
}

static void add_to_totals (PermEntry *entry, PermEntry *totals)
{
  totals->shared_clean += entry->shared_clean;
  totals->shared_dirty += entry->shared_dirty;
  totals->private_dirty += entry->private_dirty;
  totals->private_dirty += entry->private_dirty;
}

static void print_vma_table (GString *str, GHashTable *hash, const char *caption)
{
  PermEntry *pentry, totals;

  memset (&totals, 0, sizeof (PermEntry));

  g_string_append_printf (str, "<table class=\"memory-table\"><caption>%s</caption><colgroup><colgroup span=\"2\" align=\"center\"><colgroup span=\"2\" align=\"center\"><colgroup><thead><tr><th><th colspan=\"2\">Shared</th><th colspan=\"2\">Private</th><th></tr></thead>", caption);
  g_string_append (str, "<tbody><tr><td></td><td>Clean</td><td>Dirty</td><td>Clean</td><td>Dirty</td><td></td></tr>");
  pentry = g_hash_table_lookup (hash, "r-xp");
  if (pentry) {
    g_string_append_printf (str, "<tbody><tr><td>r-xp</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>Code</td></tr>",
                            pentry->shared_clean, pentry->shared_dirty, pentry->private_clean, pentry->private_dirty);
    add_to_totals (pentry, &totals);
  }
  pentry = g_hash_table_lookup (hash, "rw-p");
  if (pentry) {
    g_string_append_printf (str, "<tbody><tr><td>rw-p</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>Data</td></tr>",
                            pentry->shared_clean, pentry->shared_dirty, pentry->private_clean, pentry->private_dirty);
    add_to_totals (pentry, &totals);
  }
  pentry = g_hash_table_lookup (hash, "r--p");
  if (pentry) {
    g_string_append_printf (str, "<tbody><tr><td>r--p</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>Read-only Data</td></tr>",
                            pentry->shared_clean, pentry->shared_dirty, pentry->private_clean, pentry->private_dirty);
    add_to_totals (pentry, &totals);
  }
  pentry = g_hash_table_lookup (hash, "---p");
  if (pentry) {
    g_string_append_printf (str, "<tbody><tr><td>---p</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td></td></tr>",
                            pentry->shared_clean, pentry->shared_dirty, pentry->private_clean, pentry->private_dirty);
    add_to_totals (pentry, &totals);
  }
  pentry = g_hash_table_lookup (hash, "r--s");
  if (pentry) {
    g_string_append_printf (str, "<tbody><tr><td>r--s</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td></td></tr>",
                            pentry->shared_clean, pentry->shared_dirty, pentry->private_clean, pentry->private_dirty);
    add_to_totals (pentry, &totals);
  }
  g_string_append_printf (str, "<tbody><tr><td>Total:</td><td>%d kB</td><td>%d kB</td><td>%d kB</td><td>%d kB</td><td></td></tr>",
                          totals.shared_clean, totals.shared_dirty, totals.private_clean, totals.private_dirty);
  g_string_append (str, "</table>");
}

char* ephy_smaps_to_html (EphySMaps *smaps)
{
  GFileInputStream *stream;
  GDataInputStream *data_stream;
  GFile *file = g_file_new_for_path ("/proc/self/smaps");
  char *line;
  GString *str;
  GError *error = NULL;
  VMA_t *vma = NULL;
  GHashTable *anon_hash, *mapped_hash;
  GSList *vma_entries = NULL, *p;
  EphySMapsPrivate *priv = smaps->priv;

  stream = g_file_read (file, NULL, &error);
  g_object_unref (file);

  if (error && error->code == G_IO_ERROR_NOT_FOUND ) {
    /* This is not GNU/Linux, do nothing. */
    g_error_free (error);
    return NULL;
  }

  data_stream = g_data_input_stream_new (G_INPUT_STREAM (stream));
  g_object_unref (stream);

  str = g_string_new ("");

  while ((line = g_data_input_stream_read_line (data_stream, NULL, NULL, NULL))) {
    GMatchInfo *match_info = NULL;
    gboolean matched = FALSE;

    g_regex_match (priv->header, line, 0, &match_info);
    if (g_match_info_matches (match_info)) {
      matched = TRUE;

      if (vma)
        vma_entries = g_slist_append (vma_entries, vma);

      vma = g_slice_new0 (VMA_t);

      vma->start = g_match_info_fetch (match_info, 1);
      vma->end = g_match_info_fetch (match_info, 2);
      vma->perms = g_match_info_fetch (match_info, 3);
      vma->offset = g_match_info_fetch (match_info, 4);
      vma->major = g_match_info_fetch (match_info, 5);
      vma->minor = g_match_info_fetch (match_info, 6);
      vma->inode = g_match_info_fetch (match_info, 7);
      vma->filename = g_match_info_fetch (match_info, 8);
    }
    
    g_match_info_free (match_info);

    if (matched)
      goto out;

    g_regex_match (priv->detail, line, 0, &match_info);
    if (g_match_info_matches (match_info)) {
      char *name = g_match_info_fetch (match_info, 1);
      char **size = NULL;

      if (g_str_equal (name, "Size"))
        size = &vma->size;
      else if (g_str_equal (name, "Rss"))
        size = &vma->rss;
      else if (g_str_equal (name, "Pss"))
        size = &vma->pss;
      else if (g_str_equal (name, "Shared_Clean"))
        size = &vma->shared_clean;
      else if (g_str_equal (name, "Shared_Dirty"))
        size = &vma->shared_dirty;
      else if (g_str_equal (name, "Private_Clean"))
        size = &vma->private_clean;
      else if (g_str_equal (name, "Private_Dirty"))
        size = &vma->private_dirty;

      if (size)
        *size = g_match_info_fetch (match_info, 2);

      g_free (name);
    }

    g_match_info_free (match_info);
  out:
    g_free (line);
  }

  if (vma)
    vma_entries = g_slist_append (vma_entries, vma);

  g_object_unref (data_stream);

  /* All the file is parsed now. We have parsed more stuff than what
   * we are going to use, but it might be useful in the future. */
  anon_hash = g_hash_table_new_full (g_str_hash,
                                     g_str_equal,
                                     (GDestroyNotify)g_free,
                                     (GDestroyNotify)perm_entry_free);
  mapped_hash = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       (GDestroyNotify)g_free,
                                       (GDestroyNotify)perm_entry_free);

  for (p = vma_entries; p; p = p->next) {
    VMA_t *entry = (VMA_t*)p->data;

    if (g_strcmp0 (entry->major, "00") && g_strcmp0 (entry->minor, "00"))
      add_to_perm_entry (anon_hash, entry);
    else
      add_to_perm_entry (mapped_hash, entry);

    vma_free (entry);
  }

  g_slist_free (vma_entries);

  /* Create the page. */
  g_string_append (str, "<body>");

  /* Anon table. */
  print_vma_table (str, anon_hash, "Anonymous memory");

  /* Mapped table. */
  print_vma_table (str, mapped_hash, "Mapped memory");
  
  g_string_append (str, "</body>");

  /* Done. */
  g_hash_table_unref (anon_hash);
  g_hash_table_unref (mapped_hash);

  return g_string_free (str, FALSE);
}

static void
ephy_smaps_init (EphySMaps *smaps)
{
  smaps->priv = G_TYPE_INSTANCE_GET_PRIVATE (smaps, EPHY_TYPE_SMAPS, EphySMapsPrivate);

  /* Prepare the regexps for the smaps file. */
  smaps->priv->header = g_regex_new ("^([0-9a-f]+)-([0-9a-f]+) (....) ([0-9a-f]+) (..):(..) (\\d+) *(.*)$",
                                     G_REGEX_OPTIMIZE,
                                     0,
                                     NULL);
  smaps->priv->detail = g_regex_new ("^(.*): +(\\d+) kB", G_REGEX_OPTIMIZE, 0, NULL);
}

static void
ephy_smaps_finalize (GObject *obj)
{
  EphySMapsPrivate *priv = EPHY_SMAPS (obj)->priv;

  g_regex_unref (priv->header);
  g_regex_unref (priv->detail);

  G_OBJECT_CLASS (ephy_smaps_parent_class)->finalize (obj);
}

static void
ephy_smaps_class_init (EphySMapsClass *smaps_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (smaps_class);

  gobject_class->finalize = ephy_smaps_finalize;

  g_type_class_add_private (smaps_class, sizeof (EphySMapsPrivate));
}

EphySMaps *ephy_smaps_new ()
{
  return EPHY_SMAPS (g_object_new (EPHY_TYPE_SMAPS, NULL));
}


