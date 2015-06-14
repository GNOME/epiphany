/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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

#ifndef PREFS_AUTOFILL_UTILS_H
#define PREFS_AUTOFILL_UTILS_H

#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void prefs_autofill_utils_get_combo_box_text_cb (GObject *source_object,
                                                 GAsyncResult *res,
                                                 gpointer user_data);

void prefs_autofill_utils_set_free_cb (GObject *source_object,
                                       GAsyncResult *res,
                                       gpointer user_data);

void prefs_autofill_utils_get_entry_cb (GObject *source_object,
                                        GAsyncResult *res,
                                        gpointer user_data);

const char *prefs_autofill_utils_get_active_id (GtkComboBoxText *combo_box_text);

G_END_DECLS

#endif
