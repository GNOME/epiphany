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
  AdwTabPage *page;

  g_assert (EPHY_IS_PAGES_POPOVER (self));
  g_assert (EPHY_IS_PAGE_ROW (row));

  application = g_application_get_default ();
  window = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (application)));
  page = ephy_page_row_get_page (EPHY_PAGE_ROW (row));

  adw_tab_view_set_selected_page (ephy_tab_view_get_tab_view (self->tab_view), page);
  ephy_window_close_pages_view (window);

  gtk_popover_popdown (GTK_POPOVER (self));
}

static GtkWidget *
create_row (AdwTabPage       *page,
            EphyPagesPopover *self)
{
  EphyPageRow *row = ephy_page_row_new (self->tab_view, page);

  ephy_page_row_set_adaptive_mode (row, EPHY_ADAPTIVE_MODE_NORMAL);

  return GTK_WIDGET (row);
}

static void
selected_page_changed_cb (AdwTabView       *tab_view,
                          GParamSpec       *pspec,
                          EphyPagesPopover *self)
{
  AdwTabPage *page = adw_tab_view_get_selected_page (tab_view);
  gint position;
  GtkListBoxRow *row;

  if (!page) {
    gtk_list_box_unselect_all (self->list_box);

    return;
  }

  position = adw_tab_view_get_page_position (tab_view, page);
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

static void
ephy_pages_popover_class_init (EphyPagesPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ephy_pages_popover_dispose;

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
ephy_pages_popover_new (void)
{
  return g_object_new (EPHY_TYPE_PAGES_POPOVER, NULL);
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

  self->model = G_LIST_MODEL (adw_tab_view_get_pages (ephy_tab_view_get_tab_view (tab_view)));

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
