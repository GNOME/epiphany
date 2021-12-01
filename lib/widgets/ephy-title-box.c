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

enum {
  PROP_0,
  PROP_ADDRESS,
  PROP_SECURITY_LEVEL,
  LAST_PROP
};

struct _EphyTitleBox {
  GtkEventBox parent_instance;

  GtkWidget *lock_image;
  GtkWidget *title;
  GtkWidget *subtitle;

  EphySecurityLevel security_level;
};

static void ephy_title_box_title_widget_interface_init (EphyTitleWidgetInterface *iface);

G_DEFINE_TYPE_WITH_CODE (EphyTitleBox, ephy_title_box, GTK_TYPE_EVENT_BOX,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_TITLE_WIDGET,
                                                ephy_title_box_title_widget_interface_init))

static gboolean
ephy_title_box_button_press_event (GtkWidget      *widget,
                                   GdkEventButton *event)
{
  EphyTitleBox *title_box = EPHY_TITLE_BOX (widget);
  GtkAllocation lock_allocation;

  if (event->button != GDK_BUTTON_PRIMARY)
    return GDK_EVENT_PROPAGATE;

  gtk_widget_get_allocation (title_box->lock_image, &lock_allocation);

  if (event->x >= lock_allocation.x &&
      event->x < lock_allocation.x + lock_allocation.width &&
      event->y >= lock_allocation.y &&
      event->y < lock_allocation.y + lock_allocation.height) {
    g_signal_emit_by_name (title_box, "lock-clicked", (GdkRectangle *)&lock_allocation);
    return GDK_EVENT_STOP;
  }

  return GDK_EVENT_PROPAGATE;
}

static void
ephy_title_box_constructed (GObject *object)
{
  EphyTitleBox *title_box = EPHY_TITLE_BOX (object);
  GtkStyleContext *context;
  GtkWidget *vbox;
  GtkWidget *hbox;

  G_OBJECT_CLASS (ephy_title_box_parent_class)->constructed (object);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_valign (vbox, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (title_box), vbox);

  title_box->title = gtk_label_new (NULL);
  context = gtk_widget_get_style_context (title_box->title);
  gtk_style_context_add_class (context, "title");
  gtk_label_set_line_wrap (GTK_LABEL (title_box->title), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (title_box->title), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (title_box->title), PANGO_ELLIPSIZE_END);
  gtk_label_set_text (GTK_LABEL (title_box->title), g_get_application_name ());
  gtk_box_pack_start (GTK_BOX (vbox), title_box->title, FALSE, TRUE, 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  context = gtk_widget_get_style_context (hbox);
  gtk_style_context_add_class (context, "subtitle");
  gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (hbox, GTK_ALIGN_BASELINE);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

  title_box->lock_image = gtk_image_new ();
  g_object_set (title_box->lock_image, "icon-size", GTK_ICON_SIZE_MENU, NULL);
  gtk_widget_set_valign (title_box->lock_image, GTK_ALIGN_BASELINE);
  gtk_box_pack_start (GTK_BOX (hbox), title_box->lock_image, FALSE, TRUE, 0);

  title_box->subtitle = gtk_label_new (NULL);
  gtk_widget_set_valign (title_box->subtitle, GTK_ALIGN_BASELINE);
  gtk_label_set_line_wrap (GTK_LABEL (title_box->subtitle), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (title_box->subtitle), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (title_box->subtitle), PANGO_ELLIPSIZE_END);
  gtk_label_set_selectable (GTK_LABEL (title_box->subtitle), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), title_box->subtitle, FALSE, TRUE, 0);

  gtk_widget_add_events (GTK_WIDGET (title_box), GDK_BUTTON_PRESS_MASK);
  gtk_widget_show_all (GTK_WIDGET (title_box));
}

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

  g_object_set (title_box->lock_image,
                "icon-name", icon_name,
                NULL);

  gtk_widget_set_visible (title_box->lock_image, icon_name != NULL);

  title_box->security_level = security_level;
}

static void
ephy_title_box_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  EphyTitleWidget *widget = EPHY_TITLE_WIDGET (object);

  switch (prop_id) {
    case PROP_ADDRESS:
      ephy_title_widget_set_address (widget, g_value_get_string (value));
      break;
    case PROP_SECURITY_LEVEL:
      ephy_title_widget_set_security_level (widget, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_title_box_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  EphyTitleWidget *widget = EPHY_TITLE_WIDGET (object);

  switch (prop_id) {
    case PROP_ADDRESS:
      g_value_set_string (value, ephy_title_widget_get_address (widget));
      break;
    case PROP_SECURITY_LEVEL:
      g_value_set_enum (value, ephy_title_widget_get_security_level (widget));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
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

  object_class->constructed = ephy_title_box_constructed;
  object_class->get_property = ephy_title_box_get_property;
  object_class->set_property = ephy_title_box_set_property;
  widget_class->button_press_event = ephy_title_box_button_press_event;

  g_object_class_override_property (object_class, PROP_ADDRESS, "address");
  g_object_class_override_property (object_class, PROP_SECURITY_LEVEL, "security-level");
}

static void
ephy_title_box_init (EphyTitleBox *title_box)
{
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
