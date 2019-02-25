/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2018 Purism SPC
 *  Copyright © 2018 Adrien Plazas <kekun.plazas@laposte.net>
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

#include "ephy-action-bar-end.h"

#include "ephy-downloads-popover.h"
#include "ephy-downloads-progress-icon.h"
#include "ephy-shell.h"
#include "ephy-window.h"

#define NEEDS_ATTENTION_ANIMATION_TIMEOUT 2000 /*ms */
#define ANIMATION_X_GROW 30
#define ANIMATION_Y_GROW 30

struct _EphyActionBarEnd {
  GtkBox parent_instance;

  GtkWidget *bookmarks_button;
  GtkWidget *downloads_revealer;
  GtkWidget *downloads_button;
  GtkWidget *downloads_popover;
  GtkWidget *downloads_image;

  guint downloads_button_attention_timeout_id;
};

G_DEFINE_TYPE (EphyActionBarEnd, ephy_action_bar_end, GTK_TYPE_BOX)

static void begin_complete_theatrics (EphyActionBarEnd *self);

static void
remove_downloads_button_attention_style (EphyActionBarEnd *self)
{
  GtkStyleContext *style_context;

  style_context = gtk_widget_get_style_context (self->downloads_button);
  gtk_style_context_remove_class (style_context, "epiphany-downloads-button-needs-attention");
}

static gboolean
on_remove_downloads_button_attention_style_timeout_cb (EphyActionBarEnd *self)
{
  remove_downloads_button_attention_style (self);
  self->downloads_button_attention_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
add_attention (EphyActionBarEnd *self)
{
  GtkStyleContext *style_context;

  style_context = gtk_widget_get_style_context (self->downloads_button);

  g_clear_handle_id (&self->downloads_button_attention_timeout_id, g_source_remove);
  remove_downloads_button_attention_style (self);

  gtk_style_context_add_class (style_context, "epiphany-downloads-button-needs-attention");
  self->downloads_button_attention_timeout_id = g_timeout_add (NEEDS_ATTENTION_ANIMATION_TIMEOUT,
                                                               (GSourceFunc) on_remove_downloads_button_attention_style_timeout_cb,
                                                               self);
}

static void
download_added_cb (EphyDownloadsManager *manager,
                   EphyDownload         *download,
                   EphyActionBarEnd     *action_bar_end)
{
  GtkAllocation rect;
  DzlBoxTheatric *theatric;

  if (!action_bar_end->downloads_popover) {
    action_bar_end->downloads_popover = ephy_downloads_popover_new (action_bar_end->downloads_button);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (action_bar_end->downloads_button),
                                 action_bar_end->downloads_popover);
  }

  add_attention (action_bar_end);
  gtk_revealer_set_reveal_child (GTK_REVEALER (action_bar_end->downloads_revealer), TRUE);
  gtk_widget_queue_draw (action_bar_end->downloads_image);

  if (gtk_widget_is_visible (GTK_WIDGET (action_bar_end))) {
    gtk_widget_get_allocation (GTK_WIDGET (action_bar_end->downloads_button), &rect);
    theatric = g_object_new (DZL_TYPE_BOX_THEATRIC,
                             "alpha", 0.9,
                             "background", "#fdfdfd",
                             "target", action_bar_end->downloads_button,
                             "height", rect.height,
                             "width", rect.width,
                             "x", rect.x,
                             "y", rect.y,
                            NULL);

    dzl_object_animate_full (theatric,
                             DZL_ANIMATION_EASE_IN_CUBIC,
                             250,
                             gtk_widget_get_frame_clock (GTK_WIDGET (action_bar_end->downloads_button)),
                             g_object_unref,
                             theatric,
                             "x", rect.x - ANIMATION_X_GROW,
                             "width", rect.width + (ANIMATION_X_GROW * 2),
                             "y", rect.y - ANIMATION_Y_GROW,
                             "height", rect.height + (ANIMATION_Y_GROW * 2),
                             "alpha", 0.0,
                             NULL);
  }
}

static gboolean
begin_complete_theatrics_from_main (gpointer user_data)
{
  EphyActionBarEnd *self = user_data;
  GtkAllocation rect;

  gtk_widget_get_allocation (GTK_WIDGET (self->downloads_button), &rect);
  if (rect.x != -1 && rect.y != -1)
    begin_complete_theatrics (self);

  return G_SOURCE_REMOVE;
}

static void
begin_complete_theatrics (EphyActionBarEnd *self)
{
  g_autoptr(GIcon) icon = NULL;
  DzlBoxTheatric *theatric;
  GtkAllocation rect;

  gtk_widget_get_allocation (GTK_WIDGET (self->downloads_button), &rect);

  if (rect.x == -1 && rect.y == -1) {
    /* Delay this until our widget has been mapped/realized/displayed */
    gdk_threads_add_idle_full (G_PRIORITY_LOW,
                               begin_complete_theatrics_from_main,
                               g_object_ref (self), g_object_unref);
    return;
  }

  rect.x = 0;
  rect.y = 0;

  icon = g_themed_icon_new ("folder-download-symbolic");

  theatric = g_object_new (DZL_TYPE_BOX_THEATRIC,
                           "alpha", 1.0,
                           "height", rect.height,
                           "icon", icon,
                           "target", self,
                           "width", rect.width,
                           "x", rect.x,
                           "y", rect.y,
                           NULL);

  dzl_object_animate_full (theatric,
                           DZL_ANIMATION_EASE_OUT_CUBIC,
                           750,
                           gtk_widget_get_frame_clock (GTK_WIDGET (self)),
                           g_object_unref,
                           theatric,
                           "x", rect.x - 60,
                           "width", rect.width + 120,
                           "y", rect.y,
                           "height", rect.height + 120,
                           "alpha", 0.0,
                           NULL);
}

static void
download_completed_cb (EphyDownloadsManager *manager,
                       EphyDownload         *download,
                       EphyActionBarEnd     *action_bar_end)
{
  begin_complete_theatrics (action_bar_end);
}

static void
download_removed_cb (EphyDownloadsManager *manager,
                     EphyDownload         *download,
                     EphyActionBarEnd     *action_bar_end)
{
  if (!ephy_downloads_manager_get_downloads (manager))
    gtk_revealer_set_reveal_child (GTK_REVEALER (action_bar_end->downloads_revealer), FALSE);
}

static void
downloads_estimated_progress_cb (EphyDownloadsManager *manager,
                                 EphyActionBarEnd     *action_bar_end)
{
  gtk_widget_queue_draw (gtk_button_get_image (GTK_BUTTON (action_bar_end->downloads_button)));
}

static void
ephy_action_bar_end_class_init (EphyActionBarEndClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/action-bar-end.ui");

  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        bookmarks_button);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        downloads_revealer);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        downloads_button);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        downloads_image);
}

static void
ephy_action_bar_end_init (EphyActionBarEnd *action_bar_end)
{
  GObject *object = G_OBJECT (action_bar_end);
  EphyDownloadsManager *downloads_manager;

  /* Ensure the types used by the template have been initialized. */
  EPHY_TYPE_DOWNLOADS_PROGRESS_ICON;

  gtk_widget_init_template (GTK_WIDGET (action_bar_end));

  /* Downloads */
  downloads_manager = ephy_embed_shell_get_downloads_manager (ephy_embed_shell_get_default ());

  gtk_revealer_set_reveal_child (GTK_REVEALER (action_bar_end->downloads_revealer),
                                 ephy_downloads_manager_get_downloads (downloads_manager) != NULL);

  if (ephy_downloads_manager_get_downloads (downloads_manager)) {
    action_bar_end->downloads_popover = ephy_downloads_popover_new (action_bar_end->downloads_button);
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (action_bar_end->downloads_button), action_bar_end->downloads_popover);
  }

  g_signal_connect_object (downloads_manager, "download-added",
                           G_CALLBACK (download_added_cb),
                           object, 0);
  g_signal_connect_object (downloads_manager, "download-completed",
                           G_CALLBACK (download_completed_cb),
                           object, 0);
  g_signal_connect_object (downloads_manager, "download-removed",
                           G_CALLBACK (download_removed_cb),
                           object, 0);
  g_signal_connect_object (downloads_manager, "estimated-progress-changed",
                           G_CALLBACK (downloads_estimated_progress_cb),
                           object, 0);
}

EphyActionBarEnd *
ephy_action_bar_end_new (void)
{
  return g_object_new (EPHY_TYPE_ACTION_BAR_END,
                       NULL);
}

void
ephy_action_bar_end_set_show_bookmarks_button (EphyActionBarEnd *action_bar_end,
                                               gboolean          show)
{
  gtk_widget_set_visible (action_bar_end->bookmarks_button, show);
}

GtkWidget *
ephy_action_bar_end_get_bookmarks_button (EphyActionBarEnd *action_bar_end)
{
  return action_bar_end->bookmarks_button;
}
