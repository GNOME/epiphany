/* vim: set sw=2 ts=2 sts=2 et: */
/*
 * testephyzoomcontrol.c
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
#include "ephy-zoom-control.h"
#include <gtk/gtk.h>

static void
test_new (void)
{
  EphyZoomControl *control;
  control = g_object_new (EPHY_TYPE_ZOOM_CONTROL, "zoom", 1.0, NULL);

  g_assert (GTK_IS_WIDGET (control));
  g_assert (GTK_IS_TOOL_ITEM (control));
  g_assert (EPHY_IS_ZOOM_CONTROL (control));
}

static void
test_set_zoom_level (void)
{
  EphyZoomControl *control;
  float get = 1.0;
  control = g_object_new (EPHY_TYPE_ZOOM_CONTROL, "zoom", 2.0, NULL);

  ephy_zoom_control_set_zoom_level (control, 4.0);

  g_object_get (control, "zoom", &get, NULL);
  g_assert_cmpfloat (4.0, ==, get);
}

static void
test_get_zoom_level (void)
{
  EphyZoomControl *control;
  float get = 1.0;
  control = g_object_new (EPHY_TYPE_ZOOM_CONTROL, "zoom", 2.0, NULL);

  get = ephy_zoom_control_get_zoom_level (control);

  g_assert_cmpfloat (2.0, ==, get);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func (
    "/lib/widgets/ephy-zoom-control/new",
    test_new);
  g_test_add_func (
    "/lib/widgets/ephy-zoom-control/set_zoom_level",
    test_set_zoom_level);
  g_test_add_func (
    "/lib/widgets/ephy-zoom-control/get_zoom_level",
    test_get_zoom_level);

  return g_test_run ();
}
