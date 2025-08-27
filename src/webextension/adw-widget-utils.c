/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.> See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "adw-widget-utils-private.h"

void
adw_widget_compute_expand (GtkWidget *widget,
                           gboolean  *hexpand_p,
                           gboolean  *vexpand_p)
{
  GtkWidget *child;
  gboolean hexpand = FALSE;
  gboolean vexpand = FALSE;

  for (child = gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child)) {
    hexpand = hexpand || gtk_widget_compute_expand (child, GTK_ORIENTATION_HORIZONTAL);
    vexpand = vexpand || gtk_widget_compute_expand (child, GTK_ORIENTATION_VERTICAL);
  }

  *hexpand_p = hexpand;
  *vexpand_p = vexpand;
}

GtkSizeRequestMode
adw_widget_get_request_mode (GtkWidget *widget)
{
  GtkWidget *child;
  int wfh = 0, hfw = 0;

  for (child = gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child)) {
    GtkSizeRequestMode mode = gtk_widget_get_request_mode (child);

    switch (mode) {
      case GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH:
        hfw++;
        break;
      case GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT:
        wfh++;
        break;
      case GTK_SIZE_REQUEST_CONSTANT_SIZE:
      default:
        break;
    }
  }

  if (hfw == 0 && wfh == 0)
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
  else
    return wfh > hfw ?
           GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT :
           GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}
