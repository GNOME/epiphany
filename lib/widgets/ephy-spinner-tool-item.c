/* 
 *  Copyright © 2006 Christian Persch
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef COMPILING_TESTSPINNER
#include "config.h"
#endif

#include "ephy-spinner-tool-item.h"
#include "ephy-spinner.h"

G_DEFINE_TYPE (EphySpinnerToolItem, ephy_spinner_tool_item, GTK_TYPE_TOOL_ITEM)

static void
ephy_spinner_tool_item_init (EphySpinnerToolItem *item)
{
	GtkWidget *spinner;

	spinner = ephy_spinner_new ();
	gtk_container_add (GTK_CONTAINER (item), spinner);
	gtk_widget_show (spinner);
}

static void
ephy_spinner_tool_item_toolbar_reconfigured (GtkToolItem *tool_item)
{
	EphySpinner *spinner;
	GtkToolbarStyle style;
	GtkIconSize spinner_size;

	spinner = EPHY_SPINNER (gtk_bin_get_child (GTK_BIN (tool_item)));
	g_return_if_fail (spinner);

	style = gtk_tool_item_get_toolbar_style (tool_item);

	/* FIXME: be smarter by taking the toolbar icon size (gtk_toolbar_get_icon_size) into account! */

	if (style == GTK_TOOLBAR_BOTH)
	{
		spinner_size = GTK_ICON_SIZE_DIALOG;
	}
	else
	{
		spinner_size = GTK_ICON_SIZE_LARGE_TOOLBAR;
	}

	ephy_spinner_set_size (spinner, spinner_size);

	if (GTK_TOOL_ITEM_CLASS (ephy_spinner_tool_item_parent_class)->toolbar_reconfigured)
		GTK_TOOL_ITEM_CLASS (ephy_spinner_tool_item_parent_class)->toolbar_reconfigured (tool_item);
}

static void
ephy_spinner_tool_item_class_init (EphySpinnerToolItemClass *klass)
{
	GtkToolItemClass *tool_item_class = GTK_TOOL_ITEM_CLASS (klass);

	tool_item_class->toolbar_reconfigured = ephy_spinner_tool_item_toolbar_reconfigured;
}

/*
 * ephy_spinner_tool_item_new:
 *
 * Create a new #EphySpinnerToolItem. The spinner is a widget
 * that gives the user feedback about network status with
 * an animated image.
 *
 * Return Value: the spinner tool item
 **/
GtkToolItem *
ephy_spinner_tool_item_new (void)
{
	return GTK_TOOL_ITEM (g_object_new (EPHY_TYPE_SPINNER_TOOL_ITEM, NULL));
}

/*
 * ephy_spinner_tool_item_set_spinning:
 *
 * Start or stop the spinner.
 **/
void
ephy_spinner_tool_item_set_spinning (EphySpinnerToolItem *item,
				     gboolean spinning)
{
	EphySpinner *spinner;

	spinner = EPHY_SPINNER (gtk_bin_get_child (GTK_BIN (item)));
	g_return_if_fail (spinner);

	if (spinning)
	{
		ephy_spinner_start (spinner);
	}
	else
	{
		ephy_spinner_stop (spinner);
	}
}
