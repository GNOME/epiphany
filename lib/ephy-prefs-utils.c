/*
 *  Copyright (C) 2000 Marco Pesenti Gritti
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

#include "ephy-prefs-utils.h"
#include "ephy-gui.h"
#include "eel-gconf-extensions.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtklist.h>
#include <libgnomeui/gnome-color-picker.h>

void
ephy_pu_set_config_from_editable (GtkWidget *editable, const char *config_name)
{
	GConfValue *gcvalue = eel_gconf_get_value (config_name);
	GConfValueType value_type;
	char *value;
	gint ivalue;
	gfloat fvalue;

	if (gcvalue == NULL) {
		/* ugly hack around what appears to be a gconf bug
		 * it returns a NULL GConfValue for a valid string pref
		 * which is "" by default */
		value_type = GCONF_VALUE_STRING;
	} else {
		value_type = gcvalue->type;
		gconf_value_free (gcvalue);
	}

	/* get all the text into a new string */
	value = gtk_editable_get_chars (GTK_EDITABLE(editable), 0, -1);

	switch (value_type) {
	case GCONF_VALUE_STRING:
		eel_gconf_set_string (config_name,
				      value);
		break;
	/* FIXME : handle possible errors in the input for int and float */
	case GCONF_VALUE_INT:
		ivalue = atoi (value);
		eel_gconf_set_integer (config_name, ivalue);
		break;
	case GCONF_VALUE_FLOAT:
		fvalue = strtod (value, (char**)NULL);
		eel_gconf_set_float (config_name, fvalue);
		break;
	default:
		break;
	}

	/* free the allocated strings */
	g_free (value);
}

void
ephy_pu_set_config_from_optionmenu (GtkWidget *optionmenu, const char *config_name)
{
	int index = ephy_pu_get_int_from_optionmenu (optionmenu);

	eel_gconf_set_integer (config_name, index);
}

void
ephy_pu_set_config_from_radiobuttongroup (GtkWidget *radiobutton, const char *config_name)
{
	gint index;

	/* get value from radio button group */
	index = ephy_gui_gtk_radio_button_get (GTK_RADIO_BUTTON (radiobutton));

	eel_gconf_set_integer (config_name, index);
}

void
ephy_pu_set_config_from_spin_button (GtkWidget *spinbutton, const char *config_name)
{
	gdouble value;
	gboolean use_int;

	/* read the value as an integer */
	value = gtk_spin_button_get_value (GTK_SPIN_BUTTON(spinbutton));

	use_int = (gtk_spin_button_get_digits (GTK_SPIN_BUTTON(spinbutton)) == 0);

	if (use_int)
	{
		eel_gconf_set_integer (config_name, value);
	}
	else
	{
		eel_gconf_set_float (config_name, value);
	}
}

void
ephy_pu_set_config_from_togglebutton (GtkWidget *togglebutton, const char *config_name)
{
	gboolean value;

	/* read the value */
	value = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(togglebutton));

	eel_gconf_set_boolean (config_name, value);
}

void
ephy_pu_set_config_from_color (GtkWidget *colorpicker, const char *config_name)
{
	guint8 r, g, b, a;
	gchar color_string[9];

	/* get color values from color picker */
	gnome_color_picker_get_i8 (GNOME_COLOR_PICKER (colorpicker),
				   &r, &g, &b, &a);

	/* write into string (bounded size) */
	snprintf (color_string, 9, "#%02X%02X%02X", r, g, b);

	/* set the configuration value */
	eel_gconf_set_string (config_name, color_string);
}

void
ephy_pu_set_editable_from_config (GtkWidget *editable, const char *config_name)
{
	GConfValue *gcvalue = eel_gconf_get_value (config_name);
	GConfValueType value_type;
	gchar *value;

	if (gcvalue == NULL)
	{
		/* ugly hack around what appears to be a gconf bug
		 * it returns a NULL GConfValue for a valid string pref
		 * which is "" by default */
		value_type = GCONF_VALUE_STRING;
	}
	else
	{
		value_type = gcvalue->type;
		gconf_value_free (gcvalue);
	}

	switch (value_type)
	{
	case GCONF_VALUE_STRING:
		value = eel_gconf_get_string (config_name);
		break;
	case GCONF_VALUE_INT:
		value = g_strdup_printf ("%d",eel_gconf_get_integer (config_name));
		break;
	case GCONF_VALUE_FLOAT:
		value = g_strdup_printf ("%.2f",eel_gconf_get_float (config_name));
		break;
	default:
		value = NULL;
	}

	/* set this string value in the widget */
	if (value)
	{
		gtk_entry_set_text(GTK_ENTRY(editable), value);
	}

	/* free the allocated string */
	g_free (value);
}

void
ephy_pu_set_optionmenu_from_config (GtkWidget *optionmenu, const char *config_name)
{
	gint index;

	/* get the current value from the configuration space */
	index = eel_gconf_get_integer (config_name);

	/* set this option value in the widget */
	gtk_option_menu_set_history (GTK_OPTION_MENU (optionmenu), index);
}

void
ephy_pu_set_radiobuttongroup_from_config (GtkWidget *radiobutton, const char *config_name)
{
	gint index;

        /* get the current value from the configuration space */
        index = eel_gconf_get_integer (config_name);

	/* set it (finds the group for us) */
	ephy_gui_gtk_radio_button_set (GTK_RADIO_BUTTON (radiobutton), index);
}

void
ephy_pu_set_spin_button_from_config (GtkWidget *spinbutton, const char *config_name)
{
	gdouble value;
	gint use_int;

	use_int = (gtk_spin_button_get_digits (GTK_SPIN_BUTTON(spinbutton)) == 0);

	if (use_int)
	{
		/* get the current value from the configuration space */
		value = eel_gconf_get_integer (config_name);
	}
	else
	{
		/* get the current value from the configuration space */
		value = eel_gconf_get_float (config_name);
	}

	/* set this option value in the widget */
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbutton), value);
}

void
ephy_pu_set_togglebutton_from_config (GtkWidget *togglebutton, const char *config_name)
{
	gboolean value;

	/* get the current value from the configuration space */
	value = eel_gconf_get_boolean (config_name);

	/* set this option value in the widget */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (togglebutton), value);
}

void
ephy_pu_set_color_from_config (GtkWidget *colorpicker, const char *config_name)
{
	gchar *color_string;
	guint r, g, b;

	/* get the string from config */
	color_string = eel_gconf_get_string (config_name);

	if (color_string)
	{
		/* parse it and setup the color picker */
		sscanf (color_string, "#%2X%2X%2X", &r, &g, &b);
		gnome_color_picker_set_i8 (GNOME_COLOR_PICKER (colorpicker),
					   r, g, b, 0);
		/* free the string */
		g_free (color_string);
	}
}

int
ephy_pu_get_int_from_optionmenu (GtkWidget *optionmenu)
{
        GtkWidget *menu;
        GList *list;
        gpointer item;
        gint index;

        /* extract the selection */
        menu = GTK_OPTION_MENU(optionmenu)->menu;
        list = GTK_MENU_SHELL(menu)->children;
        item = gtk_menu_get_active (GTK_MENU(menu));
        index = g_list_index (list, item);

        return index;
}
