/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2008 Diego Escalante Urrelo
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
#include "ephy-debug.h"
#include "ephy-file-helpers.h"
#include "ephy-location-entry.h"
#include "ephy-title-widget.h"
#include <glib.h>
#include <gtk/gtk.h>

static void
test_entry_new (void)
{
  GtkWidget *entry;
  entry = ephy_location_entry_new ();

  g_assert_true (GTK_IS_WIDGET (entry));
  g_assert_true (EPHY_IS_LOCATION_ENTRY (entry));
}

static void
test_entry_set_location (void)
{
  const char *set = "test";
  const char *get;

  EphyTitleWidget *widget;
  widget = EPHY_TITLE_WIDGET (ephy_location_entry_new ());

  ephy_title_widget_set_address (widget, set);
  get = ephy_title_widget_get_address (widget);
  g_assert_cmpstr (set, ==, get);
}

static void
test_entry_set_location_null (void)
{
  const char *set = "test";
  const char *get;

  EphyTitleWidget *widget;
  widget = EPHY_TITLE_WIDGET (ephy_location_entry_new ());

  ephy_title_widget_set_address (widget, NULL);
  get = ephy_title_widget_get_address (widget);
  g_assert_cmpstr (set, !=, get);
}

static void
test_entry_get_location (void)
{
  const char *set = "test";
  const char *get;

  EphyTitleWidget *widget;
  widget = EPHY_TITLE_WIDGET (ephy_location_entry_new ());

  ephy_title_widget_set_address (widget, set);
  get = ephy_title_widget_get_address (widget);
  g_assert_cmpstr (set, ==, get);
}

static void
test_entry_get_location_empty (void)
{
  const char *get;

  EphyTitleWidget *widget;
  widget = EPHY_TITLE_WIDGET (ephy_location_entry_new ());

  get = ephy_title_widget_get_address (widget);
  g_assert_cmpstr ("", ==, get);
}

static void
test_entry_can_redo (void)
{
  const char *test = "test";

  EphyLocationEntry *entry;
  entry = EPHY_LOCATION_ENTRY (ephy_location_entry_new ());
  g_assert_cmpint (ephy_location_entry_get_can_redo (entry), ==, FALSE);

  /* Can't redo, in this point we can undo */
  ephy_title_widget_set_address (EPHY_TITLE_WIDGET (entry), test);
  g_assert_cmpint (ephy_location_entry_get_can_redo (entry), ==, FALSE);

  /* Reset should set redo to TRUE */
  ephy_location_entry_reset (entry);
  g_assert_cmpint (ephy_location_entry_get_can_redo (entry), ==, TRUE);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);
  ephy_debug_init ();

  if (!ephy_file_helpers_init (NULL,
                               EPHY_FILE_HELPERS_TESTING_MODE | EPHY_FILE_HELPERS_ENSURE_EXISTS,
                               NULL)) {
    g_debug ("Something wrong happened with ephy_file_helpers_init()");
    return -1;
  }

  g_test_add_func ("/src/ephy-location-entry/new",
                   test_entry_new);
  g_test_add_func ("/src/ephy-location-entry/set_location",
                   test_entry_set_location);
  g_test_add_func ("/src/ephy-location-entry/get_location",
                   test_entry_get_location);
  g_test_add_func ("/src/ephy-location-entry/set_location_null",
                   test_entry_set_location_null);
  g_test_add_func ("/src/ephy-location-entry/get_location_empty",
                   test_entry_get_location_empty);
  g_test_add_func ("/src/ephy-location-entry/can_redo",
                   test_entry_can_redo);

  return g_test_run ();
}
