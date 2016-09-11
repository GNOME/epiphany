/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2005 Christian Persch
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

#include "ephy-action-helper.h"

#define SENSITIVITY_KEY "EphyAction::Sensitivity"

/**
 * ephy_action_change_sensitivity_flags:
 * @action: a #GAction object
 * @flags: arbitrary combination of bit flags, defined by the user
 * @set: %TRUE if @flags should be added to @action
 *
 * This helper function provides an extra layer on top of #GAction to
 * manage its sensitivity. It uses bit @flags defined by the user, like
 * in ephy-window.c, SENS_FLAG_*.
 *
 * Effectively, the @action won't be sensitive until it has no flags
 * set. This means you can stack @flags for different events or
 * conditions at the same time.
 */
void
ephy_action_change_sensitivity_flags (GSimpleAction *action,
                                      guint          flags,
                                      gboolean       set)
{
  static GQuark sensitivity_quark = 0;
  GObject *object = G_OBJECT (action);
  guint value;

  if (G_UNLIKELY (sensitivity_quark == 0)) {
    sensitivity_quark = g_quark_from_static_string (SENSITIVITY_KEY);
  }

  value = GPOINTER_TO_UINT (g_object_get_qdata (object, sensitivity_quark));

  if (set) {
    value |= flags;
  } else {
    value &= ~flags;
  }

  g_object_set_qdata (object, sensitivity_quark, GUINT_TO_POINTER (value));

  g_simple_action_set_enabled (action, value == 0);
}
