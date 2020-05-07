/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019 Alexander Mikhaylenko <exalm7659@gmail.com>
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

#include "ephy-pages-button.h"

/* Copied from GtkInspector code */
#define XFT_DPI_MULTIPLIER (96.0 * PANGO_SCALE)
#define FONT_SIZE_LARGE 8
#define FONT_SIZE_SMALL 6

struct _EphyPagesButton {
  GtkButton parent_instance;

  GtkLabel *pages_label;
  GtkImage *pages_icon;

  int n_pages;
};

G_DEFINE_TYPE (EphyPagesButton, ephy_pages_button, GTK_TYPE_BUTTON)

enum {
  PROP_0,
  PROP_N_PAGES,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/* FIXME: I hope there is a better way to prevent the label from changing scale */
static void
update_label_scale (EphyPagesButton *self,
                    GtkSettings     *settings)
{
  int xft_dpi;
  PangoAttrList *attrs;
  PangoAttribute *scale_attribute;

  g_object_get (settings, "gtk-xft-dpi", &xft_dpi, NULL);

  attrs = gtk_label_get_attributes (self->pages_label);
  scale_attribute = pango_attr_scale_new (XFT_DPI_MULTIPLIER / xft_dpi);

  pango_attr_list_change (attrs, scale_attribute);
}

static void
xft_dpi_changed (GtkSettings     *settings,
                 GParamSpec      *spec,
                 EphyPagesButton *self)
{
  update_label_scale (self, settings);
}

static void
update_icon (EphyPagesButton *self)
{
  gboolean is_overflow;
  double font_size;
  const char *icon_name;
  g_autofree char *label_text = NULL;
  PangoAttrList *attrs;
  PangoAttribute *size_attribute;

  g_assert (self->n_pages >= 0);

  is_overflow = self->n_pages >= 100;
  font_size = self->n_pages >= 10 ? FONT_SIZE_SMALL : FONT_SIZE_LARGE;
  icon_name = is_overflow ? "ephy-tab-overflow-symbolic" : "ephy-tab-counter-symbolic";
  label_text = g_strdup_printf ("%u", (guint)self->n_pages);

  attrs = gtk_label_get_attributes (self->pages_label);
  size_attribute = pango_attr_size_new (font_size * PANGO_SCALE);
  pango_attr_list_change (attrs, size_attribute);

  gtk_widget_set_visible (GTK_WIDGET (self->pages_label), !is_overflow);
  gtk_label_set_text (self->pages_label, label_text);
  gtk_image_set_from_icon_name (self->pages_icon, icon_name, GTK_ICON_SIZE_BUTTON);
}

EphyPagesButton *
ephy_pages_button_new (void)
{
  return g_object_new (EPHY_TYPE_PAGES_BUTTON, NULL);
}

int
ephy_pages_button_get_n_pages (EphyPagesButton *self)
{
  return self->n_pages;
}

void
ephy_pages_button_set_n_pages (EphyPagesButton *self,
                               int              n_pages)
{
  self->n_pages = n_pages;

  update_icon (self);
}

static void
ephy_pages_button_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  EphyPagesButton *self = EPHY_PAGES_BUTTON (object);

  switch (prop_id) {
    case PROP_N_PAGES:
      g_value_set_int (value, ephy_pages_button_get_n_pages (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_pages_button_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  EphyPagesButton *self = EPHY_PAGES_BUTTON (object);

  switch (prop_id) {
    case PROP_N_PAGES:
      ephy_pages_button_set_n_pages (self, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
ephy_pages_button_constructed (GObject *object)
{
  EphyPagesButton *self = EPHY_PAGES_BUTTON (object);
  GtkSettings *settings;

  G_OBJECT_CLASS (ephy_pages_button_parent_class)->constructed (object);

  update_icon (self);

  settings = gtk_settings_get_default ();
  update_label_scale (self, settings);
  g_signal_connect_object (settings, "notify::gtk-xft-dpi",
                           G_CALLBACK (xft_dpi_changed), self, 0);
}

static void
ephy_pages_button_class_init (EphyPagesButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ephy_pages_button_get_property;
  object_class->set_property = ephy_pages_button_set_property;
  object_class->constructed = ephy_pages_button_constructed;

  properties [PROP_N_PAGES] =
    g_param_spec_int ("n-pages",
                      "Number of pages",
                      "The number of pages displayed on the button",
                      0,
                      G_MAXINT,
                      1,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/pages-button.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyPagesButton, pages_label);
  gtk_widget_class_bind_template_child (widget_class, EphyPagesButton, pages_icon);
}

static void
ephy_pages_button_init (EphyPagesButton *self)
{
  self->n_pages = 1;

  gtk_widget_init_template (GTK_WIDGET (self));
}
