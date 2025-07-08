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

#include "config.h"
#include "ephy-lib-type-builtins.h"
#include "ephy-title-widget.h"

G_DEFINE_INTERFACE (EphyTitleWidget, ephy_title_widget, GTK_TYPE_WIDGET);

static void
ephy_title_widget_default_init (EphyTitleWidgetInterface *iface)
{
  g_object_interface_install_property (iface,
                                       g_param_spec_string ("address",
                                                            NULL, NULL,
                                                            "",
                                                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
                                       g_param_spec_enum ("security-level",
                                                          NULL, NULL,
                                                          EPHY_TYPE_SECURITY_LEVEL,
                                                          EPHY_SECURITY_LEVEL_TO_BE_DETERMINED,
                                                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

const char *
ephy_title_widget_get_address (EphyTitleWidget *widget)
{
  EphyTitleWidgetInterface *iface;

  g_assert (EPHY_IS_TITLE_WIDGET (widget));

  iface = EPHY_TITLE_WIDGET_GET_IFACE (widget);

  g_assert (iface->get_address);
  return iface->get_address (widget);
}

void
ephy_title_widget_set_address (EphyTitleWidget *widget,
                               const char      *address)
{
  EphyTitleWidgetInterface *iface;

  g_assert (EPHY_IS_TITLE_WIDGET (widget));

  iface = EPHY_TITLE_WIDGET_GET_IFACE (widget);

  g_assert (iface->set_address);
  iface->set_address (widget, address);
}

EphySecurityLevel
ephy_title_widget_get_security_level (EphyTitleWidget *widget)
{
  EphyTitleWidgetInterface *iface;

  g_assert (EPHY_IS_TITLE_WIDGET (widget));

  iface = EPHY_TITLE_WIDGET_GET_IFACE (widget);

  g_assert (iface->get_security_level);
  return iface->get_security_level (widget);
}

void
ephy_title_widget_set_security_level (EphyTitleWidget   *widget,
                                      EphySecurityLevel  security_level)
{
  EphyTitleWidgetInterface *iface;

  g_assert (EPHY_IS_TITLE_WIDGET (widget));

  iface = EPHY_TITLE_WIDGET_GET_IFACE (widget);

  g_assert (iface->set_security_level);
  iface->set_security_level (widget, security_level);
}
