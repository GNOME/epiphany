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

#include "ephy-desktop-utils.h"
#include "ephy-embed.h"
#include "ephy-page-row.h"
#include "ephy-web-view.h"

struct _EphyPageRow {
  GtkPopover parent_instance;

  GtkBox *box;
  GtkImage *icon;
  GtkStack *icon_stack;
  GtkImage *speaker_icon;
  GtkSpinner *spinner;
  GtkLabel *title;
  GtkButton *close_button;

  HdyTabPage *page;
  EphyTabView *tab_view;
};

G_DEFINE_TYPE (EphyPageRow, ephy_page_row, GTK_TYPE_LIST_BOX_ROW)

static void
update_spinner (EphyPageRow *self)
{
  if (gtk_widget_get_mapped (GTK_WIDGET (self)) &&
      hdy_tab_page_get_loading (self->page))
    gtk_spinner_start (self->spinner);
  else
    gtk_spinner_stop (self->spinner);
}

static void
close_clicked_cb (EphyPageRow *self)
{
  hdy_tab_view_close_page (ephy_tab_view_get_tab_view (self->tab_view), self->page);
}

static gboolean
button_release_event (GtkWidget   *widget,
                      GdkEvent    *event,
                      EphyPageRow *self)
{
  GdkEventButton *button_event = (GdkEventButton *)event;

  if (button_event->button == GDK_BUTTON_MIDDLE) {
    hdy_tab_view_close_page (ephy_tab_view_get_tab_view (self->tab_view), self->page);

    return GDK_EVENT_STOP;
  }

  return GDK_EVENT_PROPAGATE;
}

static void
ephy_page_row_class_init (EphyPageRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/page-row.ui");
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, box);
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, icon);
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, icon_stack);
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, speaker_icon);
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, spinner);
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, title);
  gtk_widget_class_bind_template_child (widget_class, EphyPageRow, close_button);
  gtk_widget_class_bind_template_callback (widget_class, update_spinner);
  gtk_widget_class_bind_template_callback (widget_class, close_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, button_release_event);
}

static void
ephy_page_row_init (EphyPageRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static gboolean
loading_to_visible_child (GBinding     *binding,
                          const GValue *input,
                          GValue       *output,
                          EphyPageRow  *self)
{
  if (g_value_get_boolean (input))
    g_value_set_object (output, self->spinner);
  else
    g_value_set_object (output, self->icon);

  return TRUE;
}

static void
update_icon_cb (EphyPageRow *self)
{
  EphyEmbed *embed = EPHY_EMBED (hdy_tab_page_get_child (self->page));
  EphyWebView *view = ephy_embed_get_web_view (embed);
  GIcon *icon = G_ICON (ephy_web_view_get_icon (view));
  const char *uri, *favicon_name;
  HdyTabView *tab_view;

  if (icon) {
    gtk_image_set_from_gicon (self->icon, icon, GTK_ICON_SIZE_MENU);

    return;
  }

  uri = webkit_web_view_get_uri (WEBKIT_WEB_VIEW (view));
  favicon_name = ephy_get_fallback_favicon_name (uri, EPHY_FAVICON_TYPE_SHOW_MISSING_PLACEHOLDER);

  if (favicon_name) {
    g_autoptr (GIcon) fallback_icon = g_themed_icon_new (favicon_name);

    gtk_image_set_from_gicon (self->icon, fallback_icon, GTK_ICON_SIZE_MENU);

    return;
  }

  tab_view = ephy_tab_view_get_tab_view (self->tab_view);

  gtk_image_set_from_gicon (self->icon, hdy_tab_view_get_default_icon (tab_view), GTK_ICON_SIZE_MENU);
}

EphyPageRow *
ephy_page_row_new (EphyTabView *tab_view,
                   HdyTabPage  *page)
{
  EphyPageRow *self;
  GtkWidget *embed = hdy_tab_page_get_child (page);
  EphyWebView *view;

  g_assert (HDY_IS_TAB_PAGE (page));
  g_assert (EPHY_IS_EMBED (embed));

  view = ephy_embed_get_web_view (EPHY_EMBED (embed));

  self = g_object_new (EPHY_TYPE_PAGE_ROW, NULL);
  self->tab_view = tab_view;
  self->page = page;

  g_object_bind_property (page, "title",
                          self->title, "label",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (page, "indicator-icon",
                          self->speaker_icon, "gicon",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (page, "pinned",
                          self->close_button, "visible",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  g_object_bind_property_full (page, "loading",
                               self->icon_stack, "visible-child",
                               G_BINDING_SYNC_CREATE,
                               (GBindingTransformFunc)loading_to_visible_child,
                               NULL,
                               self, NULL);
  g_signal_connect_object (page, "notify::loading",
                           G_CALLBACK (update_spinner), self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (view, "notify::icon",
                           G_CALLBACK (update_icon_cb), self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (view, "notify::uri",
                           G_CALLBACK (update_icon_cb), self,
                           G_CONNECT_SWAPPED);

  update_icon_cb (self);

  return self;
}

void
ephy_page_row_set_adaptive_mode (EphyPageRow      *self,
                                 EphyAdaptiveMode  adaptive_mode)
{
  g_assert (EPHY_IS_PAGE_ROW (self));

  switch (adaptive_mode) {
    case EPHY_ADAPTIVE_MODE_NORMAL:
      gtk_widget_set_margin_start (GTK_WIDGET (self->box), 3);
      gtk_widget_set_margin_end (GTK_WIDGET (self->box), 1);
      gtk_box_set_spacing (self->box, 0);

      break;
    case EPHY_ADAPTIVE_MODE_NARROW:
      gtk_widget_set_margin_start (GTK_WIDGET (self->box), 8);
      gtk_widget_set_margin_end (GTK_WIDGET (self->box), 0);
      gtk_box_set_spacing (self->box, 4);

      break;
  }
}

HdyTabPage *
ephy_page_row_get_page (EphyPageRow *self)
{
  g_assert (EPHY_IS_PAGE_ROW (self));

  return self->page;
}
