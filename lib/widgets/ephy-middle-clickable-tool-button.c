/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011 Alexandre Mazari
 *  Copyright © 2011 Igalia S.L.
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

#include "config.h"
#include "ephy-middle-clickable-tool-button.h"

#include "ephy-middle-clickable-button.h"

G_DEFINE_TYPE (EphyMiddleClickableToolButton, ephy_middle_clickable_tool_button, GTK_TYPE_TOOL_BUTTON)

static void
ephy_middle_clickable_tool_button_class_init (EphyMiddleClickableToolButtonClass *class)
{
  GtkToolButtonClass *tool_button_class = GTK_TOOL_BUTTON_CLASS (class);

  tool_button_class->button_type = EPHY_TYPE_MIDDLE_CLICKABLE_BUTTON;
}

static void
ephy_middle_clickable_tool_button_init (EphyMiddleClickableToolButton *class)
{
}

GtkWidget *
ephy_middle_clickable_tool_button_new ()
{
  return gtk_widget_new (EPHY_TYPE_MIDDLE_CLICKABLE_TOOL_BUTTON, NULL);
}
