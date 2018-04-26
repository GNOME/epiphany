/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2018 Purism SPC
 *  Copyright © 2018 Adrien Plazas <kekun.plazas@laposte.net>
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

#include "ephy-action-bar.h"

struct _EphyActionBar {
  GtkRevealer parent_instance;

  EphyActionBarStart *action_bar_start;
  EphyActionBarEnd *action_bar_end;
};

G_DEFINE_TYPE (EphyActionBar, ephy_action_bar, GTK_TYPE_REVEALER)

static void
ephy_action_bar_class_init (EphyActionBarClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/action-bar.ui");

  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBar,
                                        action_bar_start);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBar,
                                        action_bar_end);
}

static void
ephy_action_bar_init (EphyActionBar *action_bar)
{
  /* Ensure the types used by the template have been initialized. */
  EPHY_TYPE_ACTION_BAR_END;
  EPHY_TYPE_ACTION_BAR_START;

  gtk_widget_init_template (GTK_WIDGET (action_bar));
}

EphyActionBar *
ephy_action_bar_new (void)
{
  return g_object_new (EPHY_TYPE_ACTION_BAR,
                       NULL);
}

EphyActionBarStart *
ephy_action_bar_get_action_bar_start (EphyActionBar *action_bar)
{
  return action_bar->action_bar_start;
}

EphyActionBarEnd *
ephy_action_bar_get_action_bar_end (EphyActionBar *action_bar)
{
  return action_bar->action_bar_end;
}

void
ephy_action_bar_set_adaptive_mode (EphyActionBar    *action_bar,
                                   EphyAdaptiveMode  adaptive_mode)
{
  switch (adaptive_mode) {
  case EPHY_ADAPTIVE_MODE_NORMAL:
    gtk_revealer_set_reveal_child (GTK_REVEALER (action_bar), FALSE);

    break;
  case EPHY_ADAPTIVE_MODE_NARROW:
    gtk_revealer_set_reveal_child (GTK_REVEALER (action_bar), TRUE);

    break;
  }
}
