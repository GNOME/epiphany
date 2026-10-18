/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2013, 2014 Yosef Or Boczko <yoseforb@gnome.org>
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
#include "ephy-title-box.h"

#include "ephy-lib-type-builtins.h"
#include "ephy-title-widget.h"

typedef enum {
  PROP_ADDRESS = 1,
  PROP_SECURITY_LEVEL,
} EphyTitleBoxProps;

static GParamSpec *props[PROP_SECURITY_LEVEL + 1] = { NULL, };

struct _EphyTitleBox {
  AdwBin parent_instance;

  GtkWidget *security_button;
  GtkWidget *title;
  GtkWidget *subtitle;

  EphySecurityLevel security_level;
};

static void ephy_title_box_title_widget_interface_init (EphyTitleWidgetInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (EphyTitleBox, ephy_title_box, ADW_TYPE_BIN,
                               G_IMPLEMENT_INTERFACE (EPHY_TYPE_TITLE_WIDGET,
                                                      ephy_title_box_title_widget_interface_init))

static const char *
ephy_title_box_title_widget_get_address (EphyTitleWidget *widget)
{
  EphyTitleBox *title_box = EPHY_TITLE_BOX (widget);

  g_assert (title_box);

  return gtk_label_get_text (GTK_LABEL (title_box->subtitle));
}

static void
ephy_title_box_title_widget_set_address (EphyTitleWidget *widget,
                                         const char      *address)
{
  EphyTitleBox *title_box = EPHY_TITLE_BOX (widget);

  g_assert (title_box);

  if (address && *address)
    gtk_label_set_text (GTK_LABEL (title_box->subtitle), address);
}

static EphySecurityLevel
ephy_title_box_title_widget_get_security_level (EphyTitleWidget *widget)
{
  EphyTitleBox *title_box = EPHY_TITLE_BOX (widget);

  g_assert (title_box);

  return title_box->security_level;
}

static void
ephy_title_box_title_widget_set_security_level (EphyTitleWidget   *widget,
                                                EphySecurityLevel  security_level)
{
  EphyTitleBox *title_box = EPHY_TITLE_BOX (widget);
  const char *icon_name;

  g_assert (title_box);

  icon_name = ephy_security_level_to_icon_name (security_level);

  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (title_box->security_button),
                                 icon_name);
  gtk_widget_set_visible (title_box->security_button, !!icon_name);

  title_box->security_level = security_level;
}

static void
ephy_title_box_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  EphyTitleWidget *widget = EPHY_TITLE_WIDGET (object);

  switch ((EphyTitleBoxProps)prop_id) {
    case PROP_ADDRESS:
      ephy_title_widget_set_address (widget, g_value_get_string (value));
      break;
    case PROP_SECURITY_LEVEL:
      ephy_title_widget_set_security_level (widget, g_value_get_enum (value));
      break;
  }
}

static void
ephy_title_box_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  EphyTitleWidget *widget = EPHY_TITLE_WIDGET (object);

  switch ((EphyTitleBoxProps)prop_id) {
    case PROP_ADDRESS:
      g_value_set_string (value, ephy_title_widget_get_address (widget));
      break;
    case PROP_SECURITY_LEVEL:
      g_value_set_enum (value, ephy_title_widget_get_security_level (widget));
      break;
  }
}

static void
ephy_title_box_title_widget_interface_init (EphyTitleWidgetInterface *iface)
{
  iface->get_address = ephy_title_box_title_widget_get_address;
  iface->set_address = ephy_title_box_title_widget_set_address;
  iface->get_security_level = ephy_title_box_title_widget_get_security_level;
  iface->set_security_level = ephy_title_box_title_widget_set_security_level;
}

static void
ephy_title_box_class_init (EphyTitleBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ephy_title_box_get_property;
  object_class->set_property = ephy_title_box_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/title-box.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyTitleBox, title);
  gtk_widget_class_bind_template_child (widget_class, EphyTitleBox, security_button);
  gtk_widget_class_bind_template_child (widget_class, EphyTitleBox, subtitle);

  props[PROP_ADDRESS] = g_param_spec_override ("address",
                                               g_object_interface_find_property (g_type_default_interface_get (EPHY_TYPE_TITLE_WIDGET), "address"));
  props[PROP_SECURITY_LEVEL] = g_param_spec_override ("security-level",
                                                      g_object_interface_find_property (g_type_default_interface_get (EPHY_TYPE_TITLE_WIDGET), "security-level"));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (props), props);
}

static void
ephy_title_box_init (EphyTitleBox *title_box)
{
  gtk_widget_init_template (GTK_WIDGET (title_box));
}

/**
 * ephy_title_box_new:
 *
 * Creates a new #EphyTitleBox.
 *
 * Returns: a new #EphyTitleBox
 **/
EphyTitleBox *
ephy_title_box_new (void)
{
  return g_object_new (EPHY_TYPE_TITLE_BOX, NULL);
}

void
ephy_title_box_reset (EphyTitleBox *self,
                      const char   *title,
                      const char   *address)
{
  gtk_label_set_text (GTK_LABEL (self->title), title);
  ephy_title_box_title_widget_set_address (EPHY_TITLE_WIDGET (self), address);
}
