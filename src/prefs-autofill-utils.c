/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2019 Abdullah Alansari
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
#include "prefs-autofill-utils.h"

#include "ephy-autofill.h"

void
prefs_autofill_utils_get_combo_box_text_cb (GObject *source_object,
                                            GAsyncResult *res,
                                            gpointer user_data)
{
  GtkComboBoxText *combo_box = user_data;
  char *autofill_value = ephy_autofill_get_finish (res);

  if (autofill_value != NULL) {
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo_box), autofill_value);
    g_free (autofill_value);
  }
}

void
prefs_autofill_utils_get_entry_cb (GObject *source_object,
                                   GAsyncResult *res,
                                   gpointer user_data)
{
  GtkEntry *entry = user_data;
  char *autofill_value = ephy_autofill_get_finish (res);

  if (autofill_value != NULL) {
    gtk_entry_set_text (entry, autofill_value);
    g_free (autofill_value);
  }
}

const char *
prefs_autofill_utils_get_active_id (GtkComboBoxText *combo_box)
{
  return gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo_box));
}

void
prefs_autofill_utils_set_free_cb (GObject *source_object,
                                  GAsyncResult *res,
                                  gpointer user_data)
{
  char *value = user_data;
  g_free (value);
}
