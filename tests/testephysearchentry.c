/* vim: set sw=2 ts=2 sts=2 et: */
/*
 * testephysearchentry.c
 * This file is part of Epiphany
 *
 * Copyright (C) 2008 - Diego Escalante Urrelo
 *
 * Epiphany is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Epiphany is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Epiphany; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */
 
#include "config.h"
#include "ephy-search-entry.h"
#include "ephy-icon-entry.h"
#include <gtk/gtk.h>

static void
test_entry_new (void)
{

  EphySearchEntry *entry;
  entry = EPHY_SEARCH_ENTRY (ephy_search_entry_new ());

  g_assert (GTK_IS_WIDGET (entry));
  g_assert (EPHY_IS_ICON_ENTRY (entry));
}

static void
test_entry_clear (void)
{
  const char *set = "test";
  const char *get = NULL;
  GtkWidget *internal_entry;

  EphySearchEntry *entry;
  entry = EPHY_SEARCH_ENTRY (ephy_search_entry_new ());

  internal_entry = ephy_icon_entry_get_entry (EPHY_ICON_ENTRY (entry));

  gtk_entry_set_text (GTK_ENTRY (internal_entry), set);
  get = gtk_entry_get_text (GTK_ENTRY (internal_entry));

  g_assert_cmpstr (set, ==, get);

  /* At this point, the text in the entry is either 'vanilla' or the
   * contents of 'set' char*
   */
  ephy_search_entry_clear (EPHY_SEARCH_ENTRY (entry));
  get = gtk_entry_get_text (GTK_ENTRY (internal_entry));

  g_assert_cmpstr ("", ==, get);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func (
    "/lib/widgets/ephy-search-entry/new",
    test_entry_new);
  g_test_add_func (
    "/lib/widgets/ephy-search-entry/clear",
    test_entry_clear);

  return g_test_run ();
}
