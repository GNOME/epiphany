/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019 Purism SPC
 *  Copyright © 2019 Adrien Plazas <kekun.plazas@laposte.net>
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

#include "ephy-page-row.h"

enum {
  CLOSED,

  LAST_SIGNAL
};

struct _EphyPageRow {
  GtkPopover parent_instance;

  GtkBox *box;
  GtkLabel *title;
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EphyPageRow, ephy_page_row, GTK_TYPE_LIST_BOX_ROW)

static void
close_clicked_cb (EphyPageRow *self)
{
  g_signal_emit (self, signals[CLOSED], 0);
}

static void
ephy_page_row_class_init (EphyPageRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  signals[CLOSED] =
    g_signal_new ("closed",
                  EPHY_TYPE_PAGE_ROW,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/page-row.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, box);
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, title);
  gtk_widget_class_bind_template_callback (widget_class, close_clicked_cb);
}

static void
ephy_page_row_init (EphyPageRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

EphyPageRow *
ephy_page_row_new (GMenuModel *menu_model,
                   gint        position)
{
  EphyPageRow *self;
  GVariant *label;

  g_assert (menu_model != NULL);
  g_assert (position >= 0);
  g_assert (position < g_menu_model_get_n_items (menu_model));

  self = g_object_new (EPHY_TYPE_PAGE_ROW, NULL);

  label = g_menu_model_get_item_attribute_value (menu_model,
                                                 position,
                                                 G_MENU_ATTRIBUTE_LABEL,
                                                 G_VARIANT_TYPE_STRING);
  gtk_label_set_text (self->title, g_variant_get_string (label, NULL));

  return self;
}

void
ephy_page_row_set_adaptive_mode (EphyPageRow      *self,
                                 EphyAdaptiveMode  adaptive_mode)
{
  g_assert (EPHY_IS_PAGE_ROW (self));

  switch (adaptive_mode) {
  case EPHY_ADAPTIVE_MODE_NORMAL:
    gtk_widget_set_size_request (GTK_WIDGET (self->box), -1, -1);

    break;
  case EPHY_ADAPTIVE_MODE_NARROW:
    gtk_widget_set_size_request (GTK_WIDGET (self->box), -1, 50);

    break;
  }
}
