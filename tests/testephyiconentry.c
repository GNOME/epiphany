/* vim: set sw=2 ts=2 sts=2 et: */
/*
 * testephyiconentry.c
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
#include "ephy-icon-entry.h"
#include <gtk/gtk.h>

static void
test_entry_new (void)
{
  GtkWidget *entry;

  entry = ephy_icon_entry_new ();

  g_assert (GTK_IS_WIDGET (entry));
  g_assert (EPHY_IS_ICON_ENTRY (entry));
}

static void
test_entry_pack_widget (void)
{
  GtkWidget *entry;

  GtkWidget *first;
  GtkWidget *last;

  GList *hbox = NULL;
  GList *list = NULL;
  GList *l = NULL;

  entry = ephy_icon_entry_new ();

  first = gtk_button_new ();
  last = gtk_entry_new ();

  /* Add a widget to the start */
  ephy_icon_entry_pack_widget (EPHY_ICON_ENTRY (entry), first, TRUE);
  /* Add a widget to the end */
  ephy_icon_entry_pack_widget (EPHY_ICON_ENTRY (entry), last, FALSE);

  /* The first children is an hbox */
  hbox = gtk_container_get_children (GTK_CONTAINER (entry));
  list = gtk_container_get_children (GTK_CONTAINER (hbox->data));
  g_list_free (hbox);

  g_assert (list);

  /* Remember inside the hbox there's a GtkEntry + our widgets */
  g_assert_cmpuint (g_list_length (list), ==, 3);

  /* Get the first one */
  l = g_list_nth (list, 0);
  g_assert (l);
  g_assert (GTK_IS_BUTTON (l->data));

  /* Get the last one */
  l = g_list_nth (list, 2);
  g_assert (l);
  g_assert (GTK_IS_ENTRY (l->data));

  g_list_free (list);
  g_list_free (l);
}

static void
test_entry_get_entry (void)
{
  GtkWidget *internal;

  GtkWidget *entry;

  entry = ephy_icon_entry_new ();
  internal = ephy_icon_entry_get_entry (EPHY_ICON_ENTRY (entry));

  g_assert (GTK_IS_ENTRY (internal));
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func (
    "/lib/widgets/ephy-icon-entry/new",
    test_entry_new);
  /* FIXME: If we invert this two add_funcs, it will NOT segfault, weird!
   * memory leak anyone?
   */
  g_test_add_func (
    "/lib/widgets/ephy-icon-entry/pack_widget",
    test_entry_pack_widget);
  g_test_add_func (
    "/lib/widgets/ephy-icon-entry/get_entry",
    test_entry_get_entry);

  return g_test_run ();
}
