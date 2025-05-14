/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2020 Andrei Lisita
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

#include "ephy-lang-row.h"

enum {
  DELETE_BUTTON_CLICKED,
  MOVE_ROW,

  LAST_SIGNAL
};

struct _EphyLangRow {
  AdwActionRow parent_instance;

  GtkWidget *drag_handle;
  GtkWidget *delete_button;
  GtkWidget *move_menu_button;

  char *code;
};

static guint signals[LAST_SIGNAL];

G_DEFINE_FINAL_TYPE (EphyLangRow, ephy_lang_row, ADW_TYPE_ACTION_ROW)

static void
ephy_lang_row_dispose (GObject *object)
{
  EphyLangRow *self = EPHY_LANG_ROW (object);

  g_clear_pointer (&self->code, g_free);

  G_OBJECT_CLASS (ephy_lang_row_parent_class)->dispose (object);
}

static void
move_up_cb (GtkWidget  *self,
            const char *action_name,
            GVariant   *parameter)
{
  GtkListBox *list_box = GTK_LIST_BOX (gtk_widget_get_parent (self));
  int index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (self)) - 1;
  GtkListBoxRow *prev_row = gtk_list_box_get_row_at_index (list_box, index);

  if (!prev_row)
    return;

  g_signal_emit (ADW_ACTION_ROW (self), signals[MOVE_ROW], 0, ADW_ACTION_ROW (prev_row));
}

static void
move_down_cb (GtkWidget  *self,
              const char *action_name,
              GVariant   *parameter)
{
  GtkListBox *list_box = GTK_LIST_BOX (gtk_widget_get_parent (self));
  int index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (self)) + 1;
  GtkListBoxRow *next_row = gtk_list_box_get_row_at_index (list_box, index);

  if (!next_row)
    return;

  g_signal_emit (ADW_ACTION_ROW (self), signals[MOVE_ROW], 0, ADW_ACTION_ROW (next_row));
}

static GdkContentProvider *
drag_prepare_cb (EphyLangRow *self,
                 double       x,
                 double       y)
{
  return gdk_content_provider_new_typed (EPHY_TYPE_LANG_ROW, self);
}

static void
drag_begin_cb (EphyLangRow *self,
               GdkDrag     *drag)
{
  GtkWidget *drag_list;
  GtkWidget *drag_row;
  GtkWidget *drag_icon;
  const char *title;
  int width, height;

  width = gtk_widget_get_width (GTK_WIDGET (self));
  height = gtk_widget_get_height (GTK_WIDGET (self));

  drag_list = gtk_list_box_new ();
  gtk_widget_set_size_request (drag_list, width, height);
  gtk_widget_add_css_class (drag_list, "boxed-list");

  title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (self));

  drag_row = ephy_lang_row_new ();
  ephy_lang_row_set_code (EPHY_LANG_ROW (drag_row), self->code);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (drag_row), title);

  gtk_list_box_append (GTK_LIST_BOX (drag_list), drag_row);

  drag_icon = gtk_drag_icon_get_for_drag (drag);
  gtk_widget_add_css_class (drag_icon, "boxed-list");
  gtk_drag_icon_set_child (GTK_DRAG_ICON (drag_icon), drag_list);
}

static gboolean
drop_cb (EphyLangRow  *self,
         const GValue *value,
         double        x,
         double        y)
{
  EphyLangRow *source;

  if (!G_VALUE_HOLDS (value, EPHY_TYPE_LANG_ROW))
    return FALSE;

  source = g_value_get_object (value);

  g_signal_emit (source, signals[MOVE_ROW], 0, self);

  return TRUE;
}

static void
on_delete_button_clicked (GtkWidget   *button,
                          EphyLangRow *self)
{
  g_signal_emit (self, signals[DELETE_BUTTON_CLICKED], 0);
}

static void
ephy_lang_row_class_init (EphyLangRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ephy_lang_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/lang-row.ui");

  signals[DELETE_BUTTON_CLICKED] =
    g_signal_new ("delete-button-clicked",
                  EPHY_TYPE_LANG_ROW,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[MOVE_ROW] =
    g_signal_new ("move-row",
                  EPHY_TYPE_LANG_ROW,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1, EPHY_TYPE_LANG_ROW);

  gtk_widget_class_install_action (widget_class, "row.move-up", NULL, move_up_cb);
  gtk_widget_class_install_action (widget_class, "row.move-down", NULL, move_down_cb);

  gtk_widget_class_bind_template_child (widget_class, EphyLangRow, drag_handle);
  gtk_widget_class_bind_template_child (widget_class, EphyLangRow, delete_button);
  gtk_widget_class_bind_template_child (widget_class, EphyLangRow, move_menu_button);

  gtk_widget_class_bind_template_callback (widget_class, drag_prepare_cb);
  gtk_widget_class_bind_template_callback (widget_class, drag_begin_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_delete_button_clicked);
}

static void
ephy_lang_row_init (EphyLangRow *self)
{
  GtkDropTarget *target;

  gtk_widget_init_template (GTK_WIDGET (self));

  target = gtk_drop_target_new (EPHY_TYPE_LANG_ROW, GDK_ACTION_MOVE);
  gtk_drop_target_set_preload (target, TRUE);
  g_signal_connect_swapped (target, "drop", G_CALLBACK (drop_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (target));
}

GtkWidget *
ephy_lang_row_new ()
{
  return g_object_new (EPHY_TYPE_LANG_ROW, NULL);
}

void
ephy_lang_row_set_code (EphyLangRow *self,
                        const char  *code)
{
  if (self->code)
    g_free (self->code);

  self->code = g_strdup (code);
}

const char *
ephy_lang_row_get_code (EphyLangRow *self)
{
  return self->code;
}

void
ephy_lang_row_set_delete_sensitive (EphyLangRow *self,
                                    gboolean     sensitive)
{
  gtk_widget_set_sensitive (self->delete_button, sensitive);
}

void
ephy_lang_row_set_movable (EphyLangRow *self,
                           gboolean     movable)
{
  gtk_widget_set_sensitive (self->drag_handle, movable);
  gtk_widget_set_sensitive (self->move_menu_button, movable);
}
