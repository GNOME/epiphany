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

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

void ephy_pu_set_config_from_editable		(GtkWidget *editable,
						 const char *config_name);

void ephy_pu_set_config_from_optionmenu		(GtkWidget *optionmenu,
						 const char *config_name);

void ephy_pu_set_config_from_radiobuttongroup	(GtkWidget *radiobutton,
						 const char *config_name);

void ephy_pu_set_config_from_spin_button	(GtkWidget *spinbutton,
						 const char *config_name);

void ephy_pu_set_config_from_togglebutton	(GtkWidget *togglebutton,
						 const char *config_name);

void ephy_pu_set_config_from_color		(GtkWidget *colorpicker,
						 const char *config_name);

void ephy_pu_set_editable_from_config		(GtkWidget *editable,
						 const char *config_name);

void ephy_pu_set_optionmenu_from_config		(GtkWidget *optionmenu,
						 const char *config_name);

void ephy_pu_set_radiobuttongroup_from_config	(GtkWidget *radiobutton,
						 const char *config_name);

void ephy_pu_set_togglebutton_from_config	(GtkWidget *togglebutton,
						 const char *config_name);

void ephy_pu_set_spin_button_from_config	(GtkWidget *spinbutton,
						 const char *config_name);

void ephy_pu_set_color_from_config		(GtkWidget *colorpicker,
						 const char *config_name);

int  ephy_pu_get_int_from_optionmenu		(GtkWidget *optionmenu);

G_END_DECLS
