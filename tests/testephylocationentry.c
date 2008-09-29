/*
 * testephylocationentry.c
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
 
#include "ephy-location-entry.h"
#include <gtk/gtk.h>

static void
test_entry_new (void)
{
	GtkWidget *entry;
	entry = ephy_location_entry_new ();

	g_assert (GTK_IS_WIDGET (entry));
	g_assert (EPHY_IS_LOCATION_ENTRY (entry));
}

static void
test_entry_get_entry (void)
{
	EphyLocationEntry *entry;
	entry = EPHY_LOCATION_ENTRY (ephy_location_entry_new ());

	g_assert (GTK_IS_ENTRY (
				ephy_location_entry_get_entry (entry)));
}

static void
test_entry_set_location (void)
{
	const char *set = "test";
	const char *null;
	const char *get;

	EphyLocationEntry *entry;
	entry = EPHY_LOCATION_ENTRY (ephy_location_entry_new ());

	null = ephy_location_entry_get_location (entry);

	ephy_location_entry_set_location (entry, set, NULL);
	get = ephy_location_entry_get_location (entry);
	g_assert_cmpstr (set, ==, get);
}

/*
 * FIXME: there's an already an assertion to avoid null as the arg, but we
 * should *confirm* that it indeed fails, although I'm not pretty sure it's
 * required to fail if the text is NULL.
static void
test_entry_set_location_null (void)
{
	const char *set = "test";
	const char *get;

	EphyLocationEntry *entry;
	entry = EPHY_LOCATION_ENTRY (ephy_location_entry_new ());

	ephy_location_entry_set_location (entry, NULL, NULL);
	get = ephy_location_entry_get_location (entry);
	g_assert_cmpstr (set, ==, get);
}
*/

static void
test_entry_get_location (void)
{
	const char *set = "test";
	const char *get;

	EphyLocationEntry *entry;
	entry = EPHY_LOCATION_ENTRY (ephy_location_entry_new ());

	ephy_location_entry_set_location (entry, set, NULL);
	get = ephy_location_entry_get_location (entry);
	g_assert_cmpstr (set, ==, get);
}

static void
test_entry_get_location_empty (void)
{
	const char *get;

	EphyLocationEntry *entry;
	entry = EPHY_LOCATION_ENTRY (ephy_location_entry_new ());

	get = ephy_location_entry_get_location (entry);
	g_assert_cmpstr ("", ==, get);
}

int
main (int argc, char *argv[])
{
	gtk_test_init (&argc, &argv);

	g_test_add_func (
		"/lib/widgets/ephy-location-entry/new",
		test_entry_new);
	g_test_add_func (
		"/lib/widgets/ephy-location-entry/get_entry",
		test_entry_get_entry);
	g_test_add_func (
		"/lib/widgets/ephy-location-entry/set_location",
		test_entry_set_location);
	g_test_add_func (
		"/lib/widgets/ephy-location-entry/get_location",
		test_entry_get_location);
	/*
	g_test_add_func (
		"/lib/widgets/ephy-location-entry/set_location_null",
		test_entry_set_location_null);
	*/
	g_test_add_func (
		"/lib/widgets/ephy-location-entry/get_location_empty",
		test_entry_get_location_empty);

	return g_test_run ();
}
