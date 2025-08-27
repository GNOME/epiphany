/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2004 Christian Persch
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

#include "ephy-signal-accumulator.h"

typedef GType (*GetTypeFunc)   (void);

gboolean
ephy_signal_accumulator_object (GSignalInvocationHint *ihint,
                                GValue                *return_accu,
                                const GValue          *handler_return,
                                gpointer               accu_data)
{
  GObject *object;
  GetTypeFunc get_type = (GetTypeFunc)accu_data;

  object = g_value_get_object (handler_return);
  if (object &&
      G_TYPE_CHECK_INSTANCE_TYPE (object, get_type ())) {
    g_value_set_object (return_accu, object);

    return FALSE;
  } else {
    g_assert (!object);
  }

  return TRUE;
}

gboolean
ephy_signal_accumulator_string (GSignalInvocationHint *ihint,
                                GValue                *return_accu,
                                const GValue          *handler_return,
                                gpointer               accu_data)
{
  g_value_copy (handler_return, return_accu);

  return !g_value_get_string (handler_return);
}
