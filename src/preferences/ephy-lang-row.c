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

  LAST_SIGNAL
};

struct _EphyLangRow {
  GtkListBoxRow parent_instance;

  GtkWidget *dnd_top_revealer;
  GtkWidget *action_row;
  GtkWidget *drag_event_box;
  GtkWidget *delete_button;
  GtkWidget *dnd_bottom_revealer;
  char *code;
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EphyLangRow, ephy_lang_row, GTK_TYPE_LIST_BOX_ROW)

static void
ephy_lang_row_dispose (GObject *object)
{
  EphyLangRow *self = EPHY_LANG_ROW (object);

  g_clear_pointer (&self->code, g_free);

  G_OBJECT_CLASS (ephy_lang_row_parent_class)->dispose (object);
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

  gtk_widget_class_bind_template_child (widget_class, EphyLangRow, dnd_top_revealer);
  gtk_widget_class_bind_template_child (widget_class, EphyLangRow, action_row);
  gtk_widget_class_bind_template_child (widget_class, EphyLangRow, drag_event_box);
  gtk_widget_class_bind_template_child (widget_class, EphyLangRow, delete_button);
  gtk_widget_class_bind_template_child (widget_class, EphyLangRow, dnd_bottom_revealer);

  gtk_widget_class_bind_template_callback (widget_class, on_delete_button_clicked);
}

static void
ephy_lang_row_init (EphyLangRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
ephy_lang_row_new ()
{
  return g_object_new (EPHY_TYPE_LANG_ROW, NULL);
}

void
ephy_lang_row_set_title (EphyLangRow *self,
                         const char  *title)
{
  hdy_preferences_row_set_title (HDY_PREFERENCES_ROW (self->action_row), title);
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

GtkWidget *
ephy_lang_row_get_drag_event_box (EphyLangRow *self)
{
  return self->drag_event_box;
}

void
ephy_lang_row_set_delete_sensitive (EphyLangRow *self,
                                    gboolean     sensitive)
{
  gtk_widget_set_sensitive (self->delete_button, sensitive);
}

GtkWidget *
ephy_lang_row_get_dnd_top_revealer (EphyLangRow *self)
{
  return self->dnd_top_revealer;
}

GtkWidget *
ephy_lang_row_get_dnd_bottom_revealer (EphyLangRow *self)
{
  return self->dnd_bottom_revealer;
}
