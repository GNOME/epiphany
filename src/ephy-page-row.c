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

#include "ephy-embed-utils.h"
#include "ephy-page-row.h"
#include "ephy-web-view.h"

enum {
  CLOSED,

  LAST_SIGNAL
};

struct _EphyPageRow {
  GtkPopover parent_instance;

  GtkBox *box;
  GtkImage *icon;
  GtkStack *icon_stack;
  GtkImage *speaker_icon;
  GtkSpinner *spinner;
  GtkLabel *title;
  GtkButton *close_button;
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EphyPageRow, ephy_page_row, GTK_TYPE_LIST_BOX_ROW)

static void
sync_load_status (EphyWebView *view,
                  GParamSpec  *pspec,
                  EphyPageRow *self)
{
  EphyEmbed *embed;

  g_assert (EPHY_IS_WEB_VIEW (view));
  g_assert (EPHY_IS_PAGE_ROW (self));

  embed = EPHY_GET_EMBED_FROM_EPHY_WEB_VIEW (view);

  g_assert (EPHY_IS_EMBED (embed));

  if (ephy_web_view_is_loading (view) && !ephy_embed_has_load_pending (embed)) {
    gtk_stack_set_visible_child (self->icon_stack, GTK_WIDGET (self->spinner));
    gtk_spinner_start (GTK_SPINNER (self->spinner));
  } else {
    gtk_stack_set_visible_child (self->icon_stack, GTK_WIDGET (self->icon));
    gtk_spinner_stop (GTK_SPINNER (self->spinner));
  }
}

static void
load_changed_cb (EphyWebView     *view,
                 WebKitLoadEvent  load_event,
                 EphyPageRow     *self)
{
  sync_load_status (view, NULL, self);
}

static void
close_clicked_cb (EphyPageRow *self)
{
  g_signal_emit (self, signals[CLOSED], 0);
}

static gboolean
button_release_event (GtkWidget   *widget,
                      GdkEvent    *event,
                      EphyPageRow *self)
{
  GdkEventButton *button_event = (GdkEventButton *)event;

  if (button_event->button == GDK_BUTTON_MIDDLE) {
    g_signal_emit (self, signals[CLOSED], 0);

    return GDK_EVENT_STOP;
  }

  return GDK_EVENT_PROPAGATE;
}

static void
ephy_page_row_class_init (EphyPageRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  signals[CLOSED] =
    g_signal_new ("closed",
                  EPHY_TYPE_PAGE_ROW,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/page-row.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, box);
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, icon);
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, icon_stack);
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, speaker_icon);
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, spinner);
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, title);
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, close_button);
  gtk_widget_class_bind_template_callback (widget_class, close_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, button_release_event);
}

static void
ephy_page_row_init (EphyPageRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
sync_favicon (EphyWebView *view,
              GParamSpec  *pspec,
              EphyPageRow *self)
{
  g_autoptr(GdkPixbuf) pixbuf = NULL;

  if (ephy_web_view_get_icon (view))
    pixbuf = gdk_pixbuf_copy (ephy_web_view_get_icon (view));

  if (pixbuf)
    gtk_image_set_from_pixbuf (self->icon, pixbuf);
  else
    gtk_image_set_from_icon_name (self->icon, "ephy-missing-favicon-symbolic",
                                  GTK_ICON_SIZE_MENU);
}

EphyPageRow *
ephy_page_row_new (EphyNotebook *notebook,
                   gint          position)
{
  EphyPageRow *self;
  GtkWidget *embed;
  GtkWidget *tab_label;
  EphyWebView *view;

  g_assert (notebook != NULL);
  g_assert (position >= 0);

  self = g_object_new (EPHY_TYPE_PAGE_ROW, NULL);

  embed = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), position);

  g_assert (EPHY_IS_EMBED (embed));

  tab_label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebook), embed);
  view = ephy_embed_get_web_view (EPHY_EMBED (embed));

  sync_favicon (view, NULL, self);
  g_signal_connect_object (view, "notify::icon", G_CALLBACK (sync_favicon), self, 0);
  g_object_bind_property (embed, "title", self->title, "label", G_BINDING_SYNC_CREATE);
  g_object_bind_property (embed, "title", self->title, "tooltip-text", G_BINDING_SYNC_CREATE);
  g_object_bind_property (view, "is-playing-audio", self->speaker_icon, "visible", G_BINDING_SYNC_CREATE);
  g_object_bind_property (tab_label, "pinned", self->close_button, "visible", G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  sync_load_status (view, NULL, self);
  g_signal_connect_object (view, "load-changed",
                           G_CALLBACK (load_changed_cb), self, 0);

  return self;
}

void
ephy_page_row_set_adaptive_mode (EphyPageRow      *self,
                                 EphyAdaptiveMode  adaptive_mode)
{
  GtkStyleContext *context;

  g_assert (EPHY_IS_PAGE_ROW (self));

  context = gtk_widget_get_style_context (GTK_WIDGET (self));

  switch (adaptive_mode) {
  case EPHY_ADAPTIVE_MODE_NORMAL:
    gtk_widget_set_size_request (GTK_WIDGET (self->box), -1, -1);
    gtk_widget_set_margin_end (GTK_WIDGET (self->box), 0);
    gtk_widget_set_margin_start (GTK_WIDGET (self->box), 4);
    gtk_box_set_spacing (self->box, 0);

    gtk_style_context_remove_class (context, "narrow");

    break;
  case EPHY_ADAPTIVE_MODE_NARROW:
    gtk_widget_set_size_request (GTK_WIDGET (self->box), -1, 50);
    gtk_widget_set_margin_end (GTK_WIDGET (self->box), 4);
    gtk_widget_set_margin_start (GTK_WIDGET (self->box), 8);
    gtk_box_set_spacing (self->box, 4);

    gtk_style_context_add_class (context, "narrow");

    break;
  }
}
