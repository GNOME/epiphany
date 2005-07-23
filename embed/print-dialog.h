/*
 *  Copyright (C) 2002 Jorn Baayen
 *  Copyright (C) 2003 Christian Persch
 *  Copyright (C) 2005 Juerg Billeter
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

/* for gnome_print_job_set_file */
#define GNOME_PRINT_UNSTABLE_API
#include <libgnomeprint/gnome-print-config.h>
#include <libgnomeprintui/gnome-print-dialog.h>

G_BEGIN_DECLS

typedef struct _EmbedPrintInfo
{
	GnomePrintConfig *config;
	
	char *tempfile;
	guint print_idle_id;
	gulong cancel_print_id;

	GnomePrintDialogRangeFlags range;
	int from_page;
	int to_page;
	int frame_type;
	gboolean print_color;

	/*
	 * &T - title
	 * &U - Document URL
	 * &D - Date/Time
	 * &P - Page Number
	 * &PT - Page Number with total Number of Pages (example: 1 of 34)
	 *
	 * So, if headerLeftStr = "&T" the title and the document URL
	 * will be printed out on the top left-hand side of each page.
	 */
	char *header_left_string;
	char *header_center_string;
	char *header_right_string;
	char *footer_left_string;
	char *footer_center_string;
	char *footer_right_string;
}
EmbedPrintInfo;

GtkWidget	*ephy_print_dialog_new		(GtkWidget *parent,
						 EmbedPrintInfo *info);

EphyDialog	*ephy_print_setup_dialog_new	(void);

EmbedPrintInfo  *ephy_print_get_print_info	(void);

void		 ephy_print_info_free		(EmbedPrintInfo *info);

void		 ephy_print_do_print_and_free	(EmbedPrintInfo *info);

gboolean	 ephy_print_verify_postscript	(GnomePrintDialog *print_dialog);

G_END_DECLS

#endif
