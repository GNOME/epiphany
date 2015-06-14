/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2018 Abdullah Alansari
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

#ifndef PREFS_AUTOFILL_DIALOG_H
#define PREFS_AUTOFILL_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PREFS_AUTOFILL_DIALOG (prefs_autofill_dialog_get_type ())

G_DECLARE_FINAL_TYPE (PrefsAutofillDialog, prefs_autofill_dialog, EPHY, PREFS_AUTOFILL_DIALOG, GtkDialog)

PrefsAutofillDialog* prefs_autofill_dialog_new (void);

G_END_DECLS

#endif
