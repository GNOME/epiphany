/*
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003 Christian Persch
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
 *
 *  $Id$
 */

#ifndef EPHY_PRINT_DIALOG_H
#define EPHY_PRINT_DIALOG_H

#include "ephy-dialog.h"
#include "ephy-embed.h"

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

EphyDialog	*ephy_print_dialog_new		(GtkWidget *parent,
						 EphyEmbed *embed);

EphyDialog	*ephy_print_setup_dialog_new	(void);

EmbedPrintInfo  *ephy_print_get_print_info	(void);

void		 ephy_print_info_free		(EmbedPrintInfo *info);

G_END_DECLS

#endif
