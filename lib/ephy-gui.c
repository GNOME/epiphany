/*
 *  Copyright (C) 2002 Marco Pesenti Gritti
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

#include "ephy-gui.h"
#include "eel-gconf-extensions.h"

#include <ctype.h>
#include <string.h>
#include <libgnome/gnome-i18n.h>
#include <gtk/gtktreemodel.h>

/* Styles for tab labels */
GtkStyle *loading_text_style = NULL;
GtkStyle *new_text_style = NULL;

/**
 * gul_gui_menu_position_under_widget:
 */
void
ephy_gui_menu_position_under_widget (GtkMenu   *menu,
				     gint      *x,
				     gint      *y,
				     gboolean  *push_in,
				     gpointer	user_data)
{
	GtkWidget *w = GTK_WIDGET (user_data);
	gint screen_width, screen_height;
	GtkRequisition requisition;

	gdk_window_get_origin (w->window, x, y);
	*x += w->allocation.x;
	*y += w->allocation.y + w->allocation.height;

	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

	screen_width = gdk_screen_width ();
	screen_height = gdk_screen_height ();

	*x = CLAMP (*x, 0, MAX (0, screen_width - requisition.width));
	*y = CLAMP (*y, 0, MAX (0, screen_height - requisition.height));
	g_print ("result %d\n", *y);
}

/**
 * gul_gui_gtk_radio_button_get: get the active member of a radiobutton
 * group from one of the buttons in the group. This should be in GTK+!
 */
gint
ephy_gui_gtk_radio_button_get (GtkRadioButton *radio_button)
{
	GtkToggleButton *toggle_button;
	gint i, length;
        GSList *list;

	/* get group list */
        list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio_button));
        length = g_slist_length (list);

	/* iterate over list to find active button */
	for (i = 0; list != NULL; i++, list = g_slist_next (list))
	{
		/* get button and text */
		toggle_button = GTK_TOGGLE_BUTTON (list->data);
		if (gtk_toggle_button_get_active (toggle_button))
		{
			break;
		}
	}

	/* check we didn't run off end */
	g_assert (list != NULL);

	/* return index (reverse order!) */
	return (length - 1) - i;
}

/**
 * gul_gui_gtk_radio_button_set: set the active member of a radiobutton
 * group from one of the buttons in the group. This should be in GTK+!
 */
void
ephy_gui_gtk_radio_button_set (GtkRadioButton *radio_button, gint index)
{
	GtkToggleButton *button;
	GSList *list;
	gint length;

	/* get the list */
        list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio_button));

	/* check out the length */
        length = g_slist_length (list);

        /* new buttons are *preppended* to the list, so button added as first
         * has last position in the list */
        index = (length - 1) - index;

	/* find the right button */
        button = GTK_TOGGLE_BUTTON (g_slist_nth_data (list, index));

	/* set it... this will de-activate the others in the group */
	if (gtk_toggle_button_get_active (button) == FALSE)
	{
		gtk_toggle_button_set_active (button, TRUE);
	}
}

gboolean
ephy_gui_confirm_overwrite_file (GtkWidget *parent, const char *filename)
{
	char *question;
	GtkWidget *dialog;
	gboolean res;

	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
	{
		return TRUE;
	}

	question = g_strdup_printf (_("File %s will be overwritten.\n"
				    "If you choose yes, the contents will be lost.\n\n"
				    "Do you want to continue?"), filename);
	dialog = gtk_message_dialog_new (parent ? GTK_WINDOW(parent) : NULL,
			                 GTK_DIALOG_MODAL,
				         GTK_MESSAGE_QUESTION,
				         GTK_BUTTONS_YES_NO,
				         question);
	res = (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES);
	gtk_widget_destroy (dialog);
	g_free (question);

	return res;
}

static guint32
shift_color_component (guchar component, float shift_by)
{
	guint32 result;
	if (shift_by > 1.0) {
		result = component * (2 - shift_by);
	} else {
		result = 0xff - shift_by * (0xff - component);
	}

	return result & 0xff;
}

/**
 * ephy_gui_rgb_shift_color
 * @color: A color.
 * @shift_by: darken or lighten factor.
 * Returns: An darkened or lightened rgb value.
 *
 * Darkens (@shift_by > 1) or lightens (@shift_by < 1)
 * @color.
 */
guint32
ephy_gui_rgb_shift_color (guint32 color, float shift_by)
{
	guint32 result;

	/* shift red by shift_by */
	result = shift_color_component((color & 0x00ff0000) >> 16, shift_by);
	result <<= 8;
	/* shift green by shift_by */
	result |=  shift_color_component((color & 0x0000ff00) >> 8, shift_by);
	result <<= 8;
	/* shift blue by shift_by */
	result |=  shift_color_component((color & 0x000000ff), shift_by);

	/* alpha doesn't change */
	result |= (0xff000000 & color);

	return result;
}

static guint32
rgb16_to_rgb (gushort r, gushort g, gushort b)
{
	guint32 result;

	result = (0xff0000 | (r & 0xff00));
	result <<= 8;
	result |= ((g & 0xff00) | (b >> 8));

	return result;
}

/**
 * ephy_gui_gdk_color_to_rgb
 * @color: A GdkColor style color.
 * Returns: An rgb value.
 *
 * Converts from a GdkColor stlye color to a gdk_rgb one.
 * Alpha gets set to fully opaque
 */
guint32
ephy_gui_gdk_color_to_rgb (const GdkColor *color)
{
	return rgb16_to_rgb (color->red, color->green, color->blue);
}

/**
 * ephy_gui_rgb_to_color
 * @color: a gdk_rgb style value.
 *
 * Converts from a gdk_rgb value style to a GdkColor one.
 * The gdk_rgb color alpha channel is ignored.
 *
 * Return value: A GdkColor structure version of the given RGB color.
 */
GdkColor
ephy_gui_gdk_rgb_to_color (guint32 color)
{
	GdkColor result;

	result.red = ((color >> 16) & 0xFF) * 0x101;
	result.green = ((color >> 8) & 0xFF) * 0x101;
	result.blue = (color & 0xff) * 0x101;
	result.pixel = 0;

	return result;
}
