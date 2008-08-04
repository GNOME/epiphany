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

GtkWidget *entry;

static void
test_entry_clear (void)
{
    const char *text;

    ephy_search_entry_clear (EPHY_SEARCH_ENTRY (entry));
    text = gtk_entry_get_text (GTK_ENTRY (ephy_icon_entry_get_entry
                                   (EPHY_ICON_ENTRY (entry))));
    g_assert_cmpstr (text, ==, "");
}

int
main (int argc, char *argv[])
{
    gtk_test_init (&argc, &argv);
    
    entry = ephy_search_entry_new ();
    gtk_entry_set_text (GTK_ENTRY (ephy_icon_entry_get_entry
                                   (EPHY_ICON_ENTRY (entry))), "Test");
    
    g_test_add_func ("/lib/widgets/ephy-search-entry/clear", test_entry_clear);
    
    return g_test_run ();
}
