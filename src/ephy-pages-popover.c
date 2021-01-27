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

#include "ephy-pages-popover.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "ephy-page-row.h"
#include "ephy-window.h"

struct _EphyPagesPopover {
  GtkPopover parent_instance;

  GtkListBox *list_box;
  GtkScrolledWindow *scrolled_window;

  GListModel *model;
  EphyTabView *tab_view;
};

G_DEFINE_TYPE (EphyPagesPopover, ephy_pages_popover, GTK_TYPE_POPOVER)

static void
drop_tab_view (EphyPagesPopover *self)
{
  self->tab_view = NULL;
}

static void
release_tab_view (EphyPagesPopover *self)
{
  if (self->tab_view) {
    g_object_weak_unref (G_OBJECT (self->tab_view), (GWeakNotify)drop_tab_view, self);
    drop_tab_view (self);
  }
}

static void
row_activated_cb (EphyPagesPopover *self,
                  EphyPageRow      *row)
{
  EphyWindow *window;
  GApplication *application;
  HdyTabPage *page;

  g_assert (EPHY_IS_PAGES_POPOVER (self));
  g_assert (EPHY_IS_PAGE_ROW (row));

  application = g_application_get_default ();
  window = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (application)));
  page = ephy_page_row_get_page (EPHY_PAGE_ROW (row));

  hdy_tab_view_set_selected_page (ephy_tab_view_get_tab_view (self->tab_view), page);
  ephy_window_close_pages_view (window);

  gtk_popover_popdown (GTK_POPOVER (self));
}

static GtkWidget *
create_row (HdyTabPage       *page,
            EphyPagesPopover *self)
{
  EphyPageRow *row = ephy_page_row_new (self->tab_view, page);

  ephy_page_row_set_adaptive_mode (row, EPHY_ADAPTIVE_MODE_NORMAL);

  gtk_widget_show (GTK_WIDGET (row));

  return GTK_WIDGET (row);
}

static void
selected_page_changed_cb (HdyTabView       *tab_view,
                          GParamSpec       *pspec,
                          EphyPagesPopover *self)
{
  HdyTabPage *page = hdy_tab_view_get_selected_page (tab_view);
  gint position;
  GtkListBoxRow *row;

  if (!page) {
    gtk_list_box_unselect_all (self->list_box);

    return;
  }

  position = hdy_tab_view_get_page_position (tab_view, page);
  row = gtk_list_box_get_row_at_index (self->list_box, position);
  gtk_list_box_select_row (self->list_box, row);
}

static void
ephy_pages_popover_dispose (GObject *object)
{
  EphyPagesPopover *self = EPHY_PAGES_POPOVER (object);

  release_tab_view (self);

  G_OBJECT_CLASS (ephy_pages_popover_parent_class)->dispose (object);
}

#ifdef GDK_WINDOWING_X11
static void
ephy_pages_popover_get_preferred_height (GtkWidget *widget,
                                         gint      *minimum_height,
                                         gint      *natural_height)
{
  EphyPagesPopover *self = EPHY_PAGES_POPOVER (widget);
  int height;

  GTK_WIDGET_CLASS (ephy_pages_popover_parent_class)->get_preferred_height (widget,
                                                                            minimum_height,
                                                                            natural_height);
  /* Ensure that popover won't leave current window */
  height = gtk_widget_get_allocated_height (GTK_WIDGET (self->tab_view));
  gtk_scrolled_window_set_max_content_height (self->scrolled_window, height);
}

static GtkSizeRequestMode
ephy_pages_popover_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}
#endif

static void
ephy_pages_popover_class_init (EphyPagesPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ephy_pages_popover_dispose;

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
    widget_class->get_request_mode = ephy_pages_popover_get_request_mode;
    widget_class->get_preferred_height = ephy_pages_popover_get_preferred_height;
  }
#endif

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/pages-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyPagesPopover, list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyPagesPopover, scrolled_window);
  gtk_widget_class_bind_template_callback (widget_class, row_activated_cb);
}

static void
ephy_pages_popover_init (EphyPagesPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

EphyPagesPopover *
ephy_pages_popover_new (GtkWidget *relative_to)
{
  g_assert (!relative_to || GTK_IS_WIDGET (relative_to));

  return g_object_new (EPHY_TYPE_PAGES_POPOVER,
                       "relative-to", relative_to,
                       NULL);
}

EphyTabView *
ephy_pages_popover_get_tab_view (EphyPagesPopover *self)
{
  g_assert (EPHY_IS_PAGES_POPOVER (self));

  return self->tab_view;
}

void
ephy_pages_popover_set_tab_view (EphyPagesPopover *self,
                                 EphyTabView      *tab_view)
{
  g_assert (EPHY_IS_PAGES_POPOVER (self));

  if (self->tab_view)
    release_tab_view (self);

  if (!tab_view)
    return;

  g_object_weak_ref (G_OBJECT (tab_view), (GWeakNotify)drop_tab_view, self);
  self->tab_view = tab_view;

  self->model = hdy_tab_view_get_pages (ephy_tab_view_get_tab_view (tab_view));

  gtk_list_box_bind_model (self->list_box,
                           self->model,
                           (GtkListBoxCreateWidgetFunc)create_row,
                           self,
                           NULL);

  g_signal_connect_object (ephy_tab_view_get_tab_view (tab_view),
                           "notify::selected-page",
                           G_CALLBACK (selected_page_changed_cb),
                           self,
                           0);
}
