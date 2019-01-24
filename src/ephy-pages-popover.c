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

#define HANDY_USE_UNSTABLE_API
#include <handy.h>
#include "ephy-notebook.h"
#include "ephy-page-row.h"

struct _EphyPagesPopover {
  GtkPopover parent_instance;

  GtkListBox *list_box;
  GtkScrolledWindow *scrolled_window;

  GListStore *list_store;
  EphyNotebook *notebook;
  EphyAdaptiveMode adaptive_mode;
};

G_DEFINE_TYPE (EphyPagesPopover, ephy_pages_popover, GTK_TYPE_POPOVER)

static void
drop_notebook (EphyPagesPopover *self)
{
  self->notebook = NULL;
  g_list_store_remove_all (self->list_store);
}

static void
release_notebook (EphyPagesPopover *self)
{
  GMenu *pages_menu;

  if (self->notebook) {
    pages_menu = ephy_notebook_get_pages_menu (self->notebook);
    g_signal_handlers_disconnect_by_data (pages_menu, self);
    g_signal_handlers_disconnect_by_data (self->notebook, self);

    g_object_weak_unref (G_OBJECT (self->notebook), (GWeakNotify) drop_notebook, self);
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
row_selected_cb (EphyPagesPopover *self,
                 GtkListBoxRow    *row)
{
  gint current_page;
  gint new_page;

  g_assert (EPHY_IS_PAGES_POPOVER (self));
  g_assert (!row || GTK_IS_LIST_BOX_ROW (row));

  if (!row)
    return;

  current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (self->notebook));
  new_page = gtk_list_box_row_get_index (row);
  if (current_page == new_page)
    return;

  gtk_notebook_set_current_page (GTK_NOTEBOOK (self->notebook), new_page);
}

static void
row_closed_cb (EphyPagesPopover *self,
               EphyPageRow      *row)
{
  g_assert (EPHY_IS_PAGES_POPOVER (self));
  g_assert (EPHY_IS_PAGE_ROW (row));

  gtk_notebook_remove_page (GTK_NOTEBOOK (self->notebook),
                            gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row)));
}

static void
current_page_changed_cb (EphyPagesPopover *self)
{
  GtkListBoxRow *current_row, *new_row;
  gint current_page;

  g_assert (EPHY_IS_PAGES_POPOVER (self));

  current_row = gtk_list_box_get_selected_row (self->list_box);
  current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (self->notebook));
  if (current_row != NULL && gtk_list_box_row_get_index (current_row) == current_page)
    return;

  new_row = gtk_list_box_get_row_at_index (self->list_box, current_page);
  gtk_list_box_select_row (self->list_box, new_row);
}

static void
items_changed_cb (EphyPagesPopover *self,
                  gint              position,
                  gint              removed,
                  gint              added,
                  GMenuModel       *menu_model)
{
  EphyPageRow **items = g_new (EphyPageRow *, added);

  for (int i = 0; i < added; i++) {
    items[i] = ephy_page_row_new (menu_model, position + i);
    ephy_page_row_set_adaptive_mode (EPHY_PAGE_ROW (items[i]),
                                     self->adaptive_mode);
    g_signal_connect_swapped (items[i], "closed", G_CALLBACK (row_closed_cb), self);
  }

  g_list_store_splice (self->list_store, position, removed, (gpointer) items, added);

  current_page_changed_cb (self);
}

static void
ephy_pages_popover_finalize (GObject *object)
{
  EphyPagesPopover *self = EPHY_PAGES_POPOVER (object);

  g_object_unref (self->list_store);

  G_OBJECT_CLASS (ephy_pages_popover_parent_class)->finalize (object);
}

static void
ephy_pages_popover_dispose (GObject *object)
{
  EphyPagesPopover *self = EPHY_PAGES_POPOVER (object);

  release_notebook (self);

  G_OBJECT_CLASS (ephy_pages_popover_parent_class)->dispose (object);
}

static void
ephy_pages_popover_class_init (EphyPagesPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ephy_pages_popover_dispose;
  object_class->finalize = ephy_pages_popover_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/pages-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyPagesPopover, list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyPagesPopover, scrolled_window);
  gtk_widget_class_bind_template_callback (widget_class, row_selected_cb);
}

static void
list_init (EphyPagesPopover *self)
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
ephy_pages_popover_init (EphyPagesPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  list_init (self);

  self->list_store = g_list_store_new (EPHY_TYPE_PAGE_ROW);

  ephy_pages_popover_set_adaptive_mode (self, EPHY_ADAPTIVE_MODE_NORMAL);
  gtk_list_box_bind_model (self->list_box,
                           G_LIST_MODEL (self->list_store),
                           create_row,
                           NULL,
                           NULL);
}

EphyPagesPopover *
ephy_pages_popover_new (GtkWidget *relative_to)
{
  g_assert (!relative_to || GTK_IS_WIDGET (relative_to));

  return g_object_new (EPHY_TYPE_PAGES_POPOVER,
                       "relative-to", relative_to,
                       NULL);
}

EphyNotebook *
ephy_pages_popover_get_notebook (EphyPagesPopover *self)
{
  g_assert (EPHY_IS_PAGES_POPOVER (self));

  return self->notebook;
}

void
ephy_pages_popover_set_notebook (EphyPagesPopover *self,
                                 EphyNotebook     *notebook)
{
  GMenu *pages_menu;

  g_assert (EPHY_IS_PAGES_POPOVER (self));

  if (self->notebook)
    release_notebook (self);

  if (!notebook)
    return;

  g_object_weak_ref (G_OBJECT (notebook), (GWeakNotify) drop_notebook, self);
  self->notebook = notebook;
  pages_menu = ephy_notebook_get_pages_menu (EPHY_NOTEBOOK (notebook));

  items_changed_cb (self, 0, 0,
                    g_menu_model_get_n_items (G_MENU_MODEL (pages_menu)),
                    G_MENU_MODEL (pages_menu));
  current_page_changed_cb (self);

  g_signal_connect_swapped (pages_menu,
                            "items-changed",
                            G_CALLBACK (items_changed_cb),
                            self);
  g_signal_connect_swapped (notebook,
                            "notify::page",
                            G_CALLBACK (current_page_changed_cb),
                            self);
}

void
ephy_pages_popover_set_adaptive_mode (EphyPagesPopover *self,
                                      EphyAdaptiveMode  adaptive_mode)
{
  GListModel *list_model;

  g_assert (EPHY_IS_PAGES_POPOVER (self));

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
    gtk_scrolled_window_set_max_content_height (self->scrolled_window, 700);
    gtk_list_box_set_header_func (self->list_box, NULL, NULL, NULL);

    break;
  case EPHY_ADAPTIVE_MODE_NARROW:
    gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
    /* Sets the max content to 0 and not -1 to ensure the popover doesn't pop out. */
    gtk_scrolled_window_set_max_content_height (self->scrolled_window, 0);
    gtk_list_box_set_header_func (self->list_box, hdy_list_box_separator_header, NULL, NULL);

    break;
  }
}
