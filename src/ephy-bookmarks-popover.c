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

#include "ephy-bookmark.h"
#include "ephy-bookmark-row.h"
#include "ephy-bookmarks-popover.h"

#include <glib/gi18n.h>

struct _EphyBookmarksPopover {
  GtkPopover      parent_instance;

  GtkWidget      *bookmarks_list_box;
};

G_DEFINE_TYPE (EphyBookmarksPopover, ephy_bookmarks_popover, GTK_TYPE_POPOVER)

static void
ephy_bookmarks_popover_class_init (EphyBookmarksPopoverClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/bookmarks-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyBookmarksPopover, bookmarks_list_box);
}

static void
ephy_bookmarks_popover_init (EphyBookmarksPopover *self)
{
  EphyBookmark *bookmark;
  GtkWidget *row;

  gtk_widget_init_template (GTK_WIDGET (self));

  bookmark = ephy_bookmark_new (g_strdup ("https://duckduckgo.com"), g_strdup ("Test title"));
  row = ephy_bookmark_row_new (bookmark);
  gtk_list_box_insert (GTK_LIST_BOX (self->bookmarks_list_box), row, -1);
}

EphyBookmarksPopover *
ephy_bookmarks_popover_new (void)
{
  return g_object_new (EPHY_TYPE_BOOKMARKS_POPOVER,
                       NULL);
}
