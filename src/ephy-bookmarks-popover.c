/*
 * Copyright (C) 2016 Iulian-Gabriel Radu <iulian.radu67@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ephy-bookmarks-popover.h"

struct _EphyBookmarksPopover {
  GtkPopover      parent_instance;
};

G_DEFINE_TYPE (EphyBookmarksPopover, ephy_bookmarks_popover, GTK_TYPE_POPOVER)

static void
ephy_bookmarks_popover_class_init (EphyBookmarksPopoverClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/bookmarks-popover.ui");
}

static void
ephy_bookmarks_popover_init (EphyBookmarksPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

EphyBookmarksPopover *
ephy_bookmarks_popover_new (void)
{
  return g_object_new (EPHY_TYPE_BOOKMARKS_POPOVER,
                       NULL);
}
