/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003-2004 Christian Persch
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PREFS_DIALOG_H
#define PREFS_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PREFS_DIALOG (prefs_dialog_get_type ())

G_DECLARE_FINAL_TYPE (PrefsDialog, prefs_dialog, EPHY, PREFS_DIALOG, GtkDialog)

G_END_DECLS

#endif
