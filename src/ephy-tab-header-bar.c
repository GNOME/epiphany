/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019 Christopher Davis <christopherdavis@gnome.org>
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

#include "ephy-tab-header-bar.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

struct _EphyTabHeaderBar {
  GtkHeaderBar parent_instance;

  GtkWidget    *back_button;
  GtkWidget    *new_tab_button;
};

G_DEFINE_TYPE(EphyTabHeaderBar, ephy_tab_header_bar, GTK_TYPE_HEADER_BAR)

/**
 * ephy_tab_header_bar_new:
 *
 * Create a new #EphyTabHeaderBar.
 *
 * Returns: (transfer full): a newly created #EphyTabHeaderBar
 */
GtkWidget *
ephy_tab_header_bar_new(void)
{
  return GTK_WIDGET(g_object_new(EPHY_TYPE_TAB_HEADER_BAR, NULL));
}

static void
ephy_tab_header_bar_constructed(GObject *object)
{
  GtkWidget *back_button;
  GtkWidget *new_tab_button;
  EphyTabHeaderBar *self = EPHY_TAB_HEADER_BAR(object);

  G_OBJECT_CLASS(ephy_tab_header_bar_parent_class)->constructed(object);
  
  back_button = GTK_WIDGET (gtk_button_new_from_icon_name ("go-previous-symbolic", GTK_ICON_SIZE_BUTTON));
  new_tab_button = GTK_WIDGET (gtk_button_new_from_icon_name ("tab-new-symbolic", GTK_ICON_SIZE_BUTTON));

  self->back_button = back_button;
  self->new_tab_button = new_tab_button;
  gtk_actionable_set_action_name (GTK_ACTIONABLE (back_button), "win.content");
  gtk_actionable_set_action_name (GTK_ACTIONABLE (new_tab_button), "win.new-tab");


  gtk_header_bar_pack_start (GTK_HEADER_BAR (self), back_button);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (self), new_tab_button);
  
  gtk_header_bar_set_title (GTK_HEADER_BAR (self), _("Tabs"));
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self), TRUE);

  gtk_widget_show_all (GTK_WIDGET (self));
}

static void
ephy_tab_header_bar_class_init(EphyTabHeaderBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->constructed = ephy_tab_header_bar_constructed;
}

static void
ephy_tab_header_bar_init(EphyTabHeaderBar *self)
{
}
