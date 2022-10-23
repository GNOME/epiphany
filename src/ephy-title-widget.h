/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Igalia S.L.
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

#include "ephy-security-levels.h"

G_BEGIN_DECLS

#define EPHY_TYPE_TITLE_WIDGET (ephy_title_widget_get_type ())

G_DECLARE_INTERFACE (EphyTitleWidget, ephy_title_widget, EPHY, TITLE_WIDGET, GtkWidget)

struct _EphyTitleWidgetInterface
{
  GTypeInterface parent_iface;

  const char       *(*get_address)        (EphyTitleWidget  *widget);
  void              (*set_address)        (EphyTitleWidget  *widget,
                                           const char       *address);
  EphySecurityLevel (*get_security_level) (EphyTitleWidget  *widget);
  void              (*set_security_level) (EphyTitleWidget  *widget,
                                           EphySecurityLevel security_level);
};

const char       *ephy_title_widget_get_address        (EphyTitleWidget *widget);

void              ephy_title_widget_set_address        (EphyTitleWidget *widget,
                                                        const char      *address);

EphySecurityLevel ephy_title_widget_get_security_level (EphyTitleWidget *widget);

void              ephy_title_widget_set_security_level (EphyTitleWidget   *widget,
                                                        EphySecurityLevel  security_level);

G_END_DECLS
