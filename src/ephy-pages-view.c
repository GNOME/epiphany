/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019 Purism SPC
 *  Copyright © 2019 Adrien Plazas <kekun.plazas@laposte.net>
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

#include "config.h"

#include "ephy-pages-view.h"

#include "ephy-page-row.h"
#include "ephy-window.h"

struct _EphyPagesView {
  GtkBox parent_instance;

  GtkListBox *list_box;

  GListModel *model;
  EphyTabView *tab_view;
};

G_DEFINE_TYPE (EphyPagesView, ephy_pages_view, GTK_TYPE_BOX)

static void
row_activated_cb (EphyPagesView *self,
                  EphyPageRow   *row)
{
  EphyWindow *window;
  GApplication *application;
  HdyTabPage *page;

  g_assert (EPHY_IS_PAGES_VIEW (self));
  g_assert (EPHY_IS_PAGE_ROW (row));

  application = g_application_get_default ();
  window = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (application)));
  page = ephy_page_row_get_page (EPHY_PAGE_ROW (row));

  hdy_tab_view_set_selected_page (ephy_tab_view_get_tab_view (self->tab_view), page);
  ephy_window_close_pages_view (window);
}

static GtkWidget *
create_row (HdyTabPage    *page,
            EphyPagesView *self)
{
  EphyPageRow *row = ephy_page_row_new (self->tab_view, page);

  ephy_page_row_set_adaptive_mode (row, EPHY_ADAPTIVE_MODE_NARROW);

  gtk_widget_show (GTK_WIDGET (row));

  return GTK_WIDGET (row);
}

static void
selected_page_changed_cb (HdyTabView    *tab_view,
                          GParamSpec    *pspec,
                          EphyPagesView *self)
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
ephy_pages_view_dispose (GObject *object)
{
  EphyPagesView *self = EPHY_PAGES_VIEW (object);

  g_clear_weak_pointer (&self->tab_view);

  G_OBJECT_CLASS (ephy_pages_view_parent_class)->dispose (object);
}

static void
ephy_pages_view_class_init (EphyPagesViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ephy_pages_view_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/pages-view.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyPagesView, list_box);
  gtk_widget_class_bind_template_callback (widget_class, row_activated_cb);
}

static void
ephy_pages_view_init (EphyPagesView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

EphyPagesView *
ephy_pages_view_new (void)
{
  return g_object_new (EPHY_TYPE_PAGES_VIEW, NULL);
}

EphyTabView *
ephy_pages_view_get_tab_view (EphyPagesView *self)
{
  g_assert (EPHY_IS_PAGES_VIEW (self));

  return self->tab_view;
}

void
ephy_pages_view_set_tab_view (EphyPagesView *self,
                              EphyTabView   *tab_view)
{
  g_assert (EPHY_IS_PAGES_VIEW (self));

  g_clear_weak_pointer (&self->tab_view);

  if (!tab_view)
    return;

  g_object_add_weak_pointer (G_OBJECT (tab_view), (gpointer *)&self->tab_view);
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
