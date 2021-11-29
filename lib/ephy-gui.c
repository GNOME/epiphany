/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002 Marco Pesenti Gritti
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
#include "ephy-gui.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

GtkWindowGroup *
ephy_gui_ensure_window_group (GtkWindow *window)
{
  GtkWindowGroup *group;

  group = gtk_window_get_group (window);
  if (group == NULL) {
    group = gtk_window_group_new ();
    gtk_window_group_add_window (group, window);
    g_object_unref (group);
  }

  return group;
}

/**
 * ephy_gui_help:
 * @parent: the parent window where help is being called
 * @page: help page to open or %NULL
 *
 * Displays Epiphany's help, opening the page indicated by @page.
 *
 * Note that @parent is used to know the #GdkScreen where to open the help
 * window.
 **/
void
ephy_gui_help (GtkWidget  *parent,
               const char *page)
{
  char *url;

  if (page)
    url = g_strdup_printf ("help:epiphany/%s", page);
  else
    url = g_strdup ("help:epiphany");

  gtk_show_uri (GTK_WINDOW (parent), url, GDK_CURRENT_TIME);

  g_free (url);
}
