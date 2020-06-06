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
  GApplication *application;

  g_assert (EPHY_IS_PAGES_VIEW (self));
  g_assert (!row || GTK_IS_LIST_BOX_ROW (row));

  application = g_application_get_default ();
  window = EPHY_WINDOW (gtk_application_get_active_window (GTK_APPLICATION (application)));

  if (!row)
    return;

  new_page = gtk_list_box_row_get_index (row);

  gtk_notebook_set_current_page (GTK_NOTEBOOK (self->notebook), new_page);
  ephy_window_close_pages_view (window);
}

static void
row_closed_cb (EphyPagesView *self,
               EphyPageRow   *row)
{
  GtkWindow *window;
  GtkWidget *embed;
  EphyEmbedShell *shell;

  g_assert (EPHY_IS_PAGES_VIEW (self));
  g_assert (EPHY_IS_PAGE_ROW (row));

  shell = ephy_embed_shell_get_default ();
  window = gtk_application_get_active_window (GTK_APPLICATION (shell));

  embed = gtk_notebook_get_nth_page (GTK_NOTEBOOK (self->notebook),
                                     gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row)));
  g_signal_emit_by_name (self->notebook,
                         "tab-close-request",
                         embed, window);
}


static void
current_page_changed (EphyPagesView *self)
{
  GtkListBoxRow *current_row, *new_row;
  gint current_page;

  g_assert (EPHY_IS_PAGES_VIEW (self));

  current_row = gtk_list_box_get_selected_row (self->list_box);
  current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (self->notebook));
  if (current_row && gtk_list_box_row_get_index (current_row) == current_page)
    return;

  new_row = gtk_list_box_get_row_at_index (self->list_box, current_page);
  gtk_list_box_select_row (self->list_box, new_row);
}

static void
items_changed_cb (EphyPagesView *self,
                  gint           position,
                  gint           removed,
                  gint           added,
                  GMenuModel    *menu_model)
{
  g_autofree EphyPageRow **items = g_new (EphyPageRow *, added);

  for (int i = 0; i < added; i++) {
    items[i] = ephy_page_row_new (self->notebook, position + i);
    ephy_page_row_set_adaptive_mode (EPHY_PAGE_ROW (items[i]),
                                     EPHY_ADAPTIVE_MODE_NARROW);
    g_signal_connect_swapped (items[i], "closed", G_CALLBACK (row_closed_cb), self);
  }

  g_list_store_splice (self->list_store, position, removed, (gpointer)items, added);

  current_page_changed (self);
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
separator_header (GtkListBoxRow *row,
                  GtkListBoxRow *before,
                  gpointer       user_data)
{
  GtkWidget *header;

  if (before == NULL) {
    gtk_list_box_row_set_header (row, NULL);

    return;
  }

  if (gtk_list_box_row_get_header (row) != NULL)
    return;

  header = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_show (header);
  gtk_list_box_row_set_header (row, header);
}

static void
ephy_pages_view_init (EphyPagesView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->list_box, separator_header, NULL, NULL);

  self->list_store = g_list_store_new (EPHY_TYPE_PAGE_ROW);

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
