/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2003 Christian Persch
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

#include <glib.h>

#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-zoom.h"

#define NUM_ZOOM_STEPS 14

static float
  zoom_steps[NUM_ZOOM_STEPS] = {
  0.30f,
  0.50f,
  0.67f,
  0.80f,
  0.90f,
  1.00f,
  1.10f,
  1.20f,
  1.33f,
  1.50f,
  1.70f,
  2.00f,
  2.40f,
  3.00f
};

float
ephy_zoom_get_changed_zoom_level (float level,
                                  int   steps)
{
  float new_level;
  gint i;

  for (i = 0; i < NUM_ZOOM_STEPS; i++) {
    if (zoom_steps[i] == level)
      break;
  }

  if (i == NUM_ZOOM_STEPS) {
    /* No exact step found, try to find the nearest value */
    for (i = 0; i < NUM_ZOOM_STEPS - 1; i++) {
      if (zoom_steps[i] < level && zoom_steps[i + 1] > level)
        break;
    }
  }

  if (i == NUM_ZOOM_STEPS) {
    /* Still no match? Return default */
    return g_settings_get_double (EPHY_SETTINGS_WEB, EPHY_PREFS_WEB_DEFAULT_ZOOM_LEVEL);
  }

  if (steps == -1 && i > 0) {
    new_level = zoom_steps[i - 1];
  } else if (steps == 1 && i < NUM_ZOOM_STEPS - 1) {
    new_level = zoom_steps[i + 1];
  } else {
    /* Ensure that we have a consistent value */
    new_level = level;
  }
  return new_level;
}

int
ephy_zoom_get_index (gdouble value)
{
  for (int idx = 0; idx < NUM_ZOOM_STEPS; idx++) {
    if (zoom_steps[idx] == value)
      return idx;
  }

  return 5;
}

gdouble
ephy_zoom_get_value (int index)
{
  if (index < 0 || index >= NUM_ZOOM_STEPS)
    return 100;

  return zoom_steps[index];
}
