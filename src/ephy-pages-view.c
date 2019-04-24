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

#define HANDY_USE_UNSTABLE_API
#include <handy.h>
#include "ephy-notebook.h"
#include "ephy-page-row.h"
#include "ephy-window.h"

struct _EphyPagesView {
  GtkScrolledWindow parent_instance;

  GtkListBox *list_box;

  GListStore *list_store;
  EphyNotebook *notebook;
  EphyAdaptiveMode adaptive_mode;
};

G_DEFINE_TYPE (EphyPagesView, ephy_pages_view, GTK_TYPE_SCROLLED_WINDOW)

static void
drop_notebook (EphyPagesView *self)
{
  self->notebook = NULL;
  g_list_store_remove_all (self->list_store);
}

static void
release_notebook (EphyPagesView *self)
{
  if (self->notebook) {
    g_object_weak_unref (G_OBJECT (self->notebook), (GWeakNotify)drop_notebook, self);
    drop_notebook (self);
  }
}

static GtkWidget *
create_row (gpointer item,
            gpointer user_data)
{
  return GTK_WIDGET (g_object_ref (G_OBJECT (item)));
}

static void
row_activated_cb (EphyPagesView *self,
                  GtkListBoxRow *row)
{
  gint new_page;
  EphyWindow *window;
  GtkWidget *stack;
  GApplication *application;

  g_assert (EPHY_IS_PAGES_VIEW (self));
  g_assert (!row || GTK_IS_LIST_BOX_ROW (row));

  application = g_application_get_default ();
  window = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (application)));
  stack = ephy_window_get_stack (window);

  if (!row)
    return;

  new_page = gtk_list_box_row_get_index (row);

  gtk_notebook_set_current_page (GTK_NOTEBOOK (self->notebook), new_page);
  gtk_stack_set_visible_child_name (GTK_STACK (stack), "content");
}

static void
row_closed_cb (EphyPagesView *self,
               EphyPageRow   *row)
{
  g_assert (EPHY_IS_PAGES_VIEW (self));
  g_assert (EPHY_IS_PAGE_ROW (row));

  gtk_notebook_remove_page (GTK_NOTEBOOK (self->notebook),
                            gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row)));
}

static void
items_changed_cb (EphyPagesView *self,
                  gint           position,
                  gint           removed,
                  gint           added,
                  GMenuModel    *menu_model)
{
  EphyPageRow **items = g_new (EphyPageRow *, added);

  for (int i = 0; i < added; i++) {
    items[i] = ephy_page_row_new (self->notebook, position + i);
    ephy_page_row_set_adaptive_mode (EPHY_PAGE_ROW (items[i]),
                                     self->adaptive_mode);
    g_signal_connect_swapped (items[i], "closed", G_CALLBACK (row_closed_cb), self);
  }

  g_list_store_splice (self->list_store, position, removed, (gpointer) items, added);
}

static void
ephy_pages_view_finalize (GObject *object)
{
  EphyPagesView *self = EPHY_PAGES_VIEW (object);

  g_object_unref (self->list_store);

  G_OBJECT_CLASS (ephy_pages_view_parent_class)->finalize (object);
}

static void
ephy_pages_view_dispose (GObject *object)
{
  EphyPagesView *self = EPHY_PAGES_VIEW (object);

  release_notebook (self);

  G_OBJECT_CLASS (ephy_pages_view_parent_class)->dispose (object);
}

static void
ephy_pages_view_class_init (EphyPagesViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ephy_pages_view_dispose;
  object_class->finalize = ephy_pages_view_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/pages-view.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyPagesView, list_box);
  gtk_widget_class_bind_template_callback (widget_class, row_activated_cb);
}

static void
list_init (EphyPagesView *self)
{
  GtkCssProvider *provider = gtk_css_provider_new ();

  /* This makes the list's background transparent. */
  gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
                                   "list { border-style: none; background-color: transparent; }", -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (self->list_box)),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  g_object_unref (provider);
}

static void
ephy_pages_view_init (EphyPagesView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  list_init (self);

  self->list_store = g_list_store_new (EPHY_TYPE_PAGE_ROW);

  gtk_list_box_set_selection_mode (self->list_box, GTK_SELECTION_NONE);

  ephy_pages_view_set_adaptive_mode (self, EPHY_ADAPTIVE_MODE_NARROW);
  gtk_list_box_bind_model (self->list_box,
                           G_LIST_MODEL (self->list_store),
                           create_row,
                           NULL,
                           NULL);
}

EphyPagesView *
ephy_pages_view_new (void)
{
  return g_object_new (EPHY_TYPE_PAGES_VIEW, NULL);
}

EphyNotebook *
ephy_pages_view_get_notebook (EphyPagesView *self)
{
  g_assert (EPHY_IS_PAGES_VIEW (self));

  return self->notebook;
}

void
ephy_pages_view_set_notebook (EphyPagesView *self,
                              EphyNotebook  *notebook)
{
  GMenu *pages_menu;

  g_assert (EPHY_IS_PAGES_VIEW (self));

  if (self->notebook)
    release_notebook (self);

  if (!notebook)
    return;

  g_object_weak_ref (G_OBJECT (notebook), (GWeakNotify)drop_notebook, self);
  self->notebook = notebook;
  pages_menu = ephy_notebook_get_pages_menu (EPHY_NOTEBOOK (notebook));

  items_changed_cb (self, 0, 0,
                    g_menu_model_get_n_items (G_MENU_MODEL (pages_menu)),
                    G_MENU_MODEL (pages_menu));

  g_signal_connect_object (pages_menu,
                           "items-changed",
                           G_CALLBACK (items_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

void
ephy_pages_view_set_adaptive_mode (EphyPagesView    *self,
                                   EphyAdaptiveMode  adaptive_mode)
{
  GListModel *list_model;

  g_assert (EPHY_IS_PAGES_VIEW (self));

  self->adaptive_mode = adaptive_mode;

  list_model = G_LIST_MODEL (self->list_store);
  for (guint i = 0; i < g_list_model_get_n_items (list_model); i++) {
    EphyPageRow *row = EPHY_PAGE_ROW (g_list_model_get_item (list_model, i));

    ephy_page_row_set_adaptive_mode (row, self->adaptive_mode);
  }

  switch (adaptive_mode) {
  case EPHY_ADAPTIVE_MODE_NORMAL:
    gtk_widget_set_vexpand (GTK_WIDGET (self), FALSE);
    /* This should be enough height in normal mode to fit in 900px hight screen. */
    gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW(self), 700);
    gtk_list_box_set_header_func (self->list_box, NULL, NULL, NULL);

    break;
  case EPHY_ADAPTIVE_MODE_NARROW:
    gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
    gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (self), 0);
    gtk_list_box_set_header_func (self->list_box, hdy_list_box_separator_header, NULL, NULL);

    break;
  }
}
