/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef EPHY_EMBED_UTILS_H
#define EPHY_EMBED_UTILS_H

#include "ephy-embed-persist.h"

#include <gtk/gtkwidget.h>
#include <bonobo/bonobo-ui-component.h>

G_BEGIN_DECLS

void ephy_embed_utils_save			(GtkWidget *window,
						 const char *title,
						 const char *default_dir_pref,
						 gboolean ask_dest,
						 EphyEmbedPersist *persist);

void ephy_embed_utils_build_encodings_submenu   (BonoboUIComponent *ui_component,
						 const char *path,
						 BonoboUIVerbFn fn,
						 gpointer view);

void ephy_embed_utils_nohandler_dialog_run      (GtkWidget *parent);

G_END_DECLS

#endif
