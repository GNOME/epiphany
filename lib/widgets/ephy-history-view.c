/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2012 Igalia S.L.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-history-view.h"

#include "ephy-gui.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

G_DEFINE_TYPE (EphyHistoryView, ephy_history_view, GTK_TYPE_TREE_VIEW)


static gboolean
button_event_modifies_selection (GdkEventButton *event)
{
  return (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) != 0;
}

static gboolean
ephy_history_view_button_press (GtkWidget *treeview,
                                GdkEventButton *event)
{
  GtkTreeSelection *selection;
  GtkTreePath *path = NULL;
  gboolean path_is_selected, call_parent = TRUE;

  if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (treeview)))
    return GTK_WIDGET_CLASS (ephy_history_view_parent_class)->button_press_event (treeview, event);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
                                     event->x,
                                     event->y,
                                     &path,
                                     NULL, NULL, NULL)) {
    path_is_selected = gtk_tree_selection_path_is_selected (selection, path);

    if (!gtk_widget_is_focus (GTK_WIDGET (treeview)))
      gtk_widget_grab_focus (GTK_WIDGET (treeview));

    if (event->button == 3 && path_is_selected)
      call_parent = FALSE;

    if (!button_event_modifies_selection (event) &&
        event->button == 1 && path_is_selected &&
        gtk_tree_selection_count_selected_rows (selection) > 1)
      call_parent = FALSE;

    if (call_parent)
      GTK_WIDGET_CLASS (ephy_history_view_parent_class)->button_press_event (treeview, event);

    if (event->button == 3) {
      gboolean retval;

      g_signal_emit_by_name (treeview, "popup_menu", &retval);
    }
    gtk_tree_path_free (path);
  } else
    gtk_tree_selection_unselect_all (selection);

  return FALSE;
}

static void
ephy_history_view_class_init (EphyHistoryViewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->button_press_event = ephy_history_view_button_press;
}

static void
ephy_history_view_init (EphyHistoryView *self)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
}

void
ephy_history_view_popup (EphyHistoryView *view, GtkWidget *menu)
{
  GdkEvent *event;

  g_return_if_fail (EPHY_IS_HISTORY_VIEW (view));

  event = gtk_get_current_event ();

  if (event) {
    if (event->type == GDK_KEY_PRESS) {
      GdkEventKey *key = (GdkEventKey *) event;

      gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                      ephy_gui_menu_position_tree_selection,
                      view, 0, key->time);
      gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
    } else if (event->type == GDK_BUTTON_PRESS) {
      GdkEventButton *button = (GdkEventButton *) event;

      gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL,
                      NULL, button->button, button->time);
    }

    gdk_event_free (event);
  }
}


