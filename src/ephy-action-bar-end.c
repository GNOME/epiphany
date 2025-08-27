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
#include "ephy-bookmarks-dialog.h"
#include "ephy-bookmark-properties.h"
#include "ephy-browser-action.h"
#include "ephy-browser-action-row.h"
#include "ephy-desktop-utils.h"
#include "ephy-downloads-paintable.h"
#include "ephy-downloads-popover.h"
#include "ephy-embed-container.h"
#include "ephy-shell.h"
#include "ephy-web-extension-manager.h"
#include "ephy-window.h"

#define NEEDS_ATTENTION_ANIMATION_TIMEOUT 2000 /*ms */

struct _EphyActionBarEnd {
  GtkBox parent_instance;

  GtkWidget *bookmarks_button;
  GtkWidget *downloads_revealer;
  GtkWidget *downloads_button;
  GtkWidget *downloads_popover;
  GtkWidget *downloads_icon;
  GtkWidget *overview_button;
  GtkWidget *browser_actions_button;
  GtkWidget *browser_actions_popover;
  GtkWidget *browser_actions_scrolled_window;
  GtkWidget *browser_actions_listbox;
  GtkWidget *browser_actions_stack;
  GtkWidget *browser_actions_popup_view_box;
  GtkWidget *browser_actions_popup_view_label;
  GtkWidget *browser_action_popup_web_view;

  GdkPaintable *downloads_paintable;

  guint downloads_button_attention_timeout_id;
};

G_DEFINE_FINAL_TYPE (EphyActionBarEnd, ephy_action_bar_end, GTK_TYPE_BOX)

static void set_browser_actions (EphyActionBarEnd *action_bar_end,
                                 GListStore       *browser_actions);

static void
add_attention_timeout_cb (EphyActionBarEnd *self)
{
  gtk_widget_remove_css_class (self->downloads_icon, "accent");
  self->downloads_button_attention_timeout_id = 0;
}

static void
add_attention (EphyActionBarEnd *self)
{
  g_clear_handle_id (&self->downloads_button_attention_timeout_id, g_source_remove);

  gtk_widget_add_css_class (self->downloads_icon, "accent");
  self->downloads_button_attention_timeout_id = g_timeout_add_once (NEEDS_ATTENTION_ANIMATION_TIMEOUT,
                                                                    (GSourceOnceFunc)add_attention_timeout_cb,
                                                                    self);
}

static void
download_added_cb (EphyDownloadsManager *manager,
                   EphyDownload         *download,
                   EphyActionBarEnd     *action_bar_end)
{
  if (!action_bar_end->downloads_popover) {
    action_bar_end->downloads_popover = ephy_downloads_popover_new ();
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (action_bar_end->downloads_button),
                                 action_bar_end->downloads_popover);
  }

  add_attention (action_bar_end);
  gtk_revealer_set_reveal_child (GTK_REVEALER (action_bar_end->downloads_revealer), TRUE);
}

static void
download_completed_cb (EphyDownloadsManager *manager,
                       EphyDownload         *download,
                       EphyActionBarEnd     *action_bar_end)
{
  ephy_downloads_paintable_animate_done (EPHY_DOWNLOADS_PAINTABLE (action_bar_end->downloads_paintable));
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
  gdouble fraction = ephy_downloads_manager_get_estimated_progress (manager);

  g_object_set (action_bar_end->downloads_paintable, "progress", fraction, NULL);
}

static void
show_downloads_cb (EphyDownloadsManager *manager,
                   EphyActionBarEnd     *action_bar_end)
{
  if (gtk_widget_get_mapped (GTK_WIDGET (action_bar_end)))
    gtk_menu_button_popup (GTK_MENU_BUTTON (action_bar_end->downloads_button));
}

static void
remove_popup_webview (EphyActionBarEnd *action_bar_end)
{
  if (action_bar_end->browser_action_popup_web_view) {
    gtk_box_remove (GTK_BOX (action_bar_end->browser_actions_popup_view_box),
                    action_bar_end->browser_action_popup_web_view);
    action_bar_end->browser_action_popup_web_view = NULL;
  }
}

static void
show_browser_action_popup (EphyActionBarEnd  *action_bar_end,
                           EphyBrowserAction *action)
{
  EphyWebExtensionManager *manager = ephy_web_extension_manager_get_default ();
  EphyWebExtension *web_extension = ephy_browser_action_get_web_extension (action);
  GtkWidget *popup_view = ephy_web_extension_manager_create_browser_popup (manager, web_extension);

  gtk_box_append (GTK_BOX (action_bar_end->browser_actions_popup_view_box), popup_view);
  action_bar_end->browser_action_popup_web_view = popup_view;

  gtk_label_set_text (GTK_LABEL (action_bar_end->browser_actions_popup_view_label),
                      ephy_browser_action_get_title (action));

  gtk_stack_set_visible_child (GTK_STACK (action_bar_end->browser_actions_stack),
                               action_bar_end->browser_actions_popup_view_box);
}

static void
browser_actions_popup_view_back_clicked_cb (GtkButton        *button,
                                            EphyActionBarEnd *action_bar_end)
{
  gtk_stack_set_visible_child (GTK_STACK (action_bar_end->browser_actions_stack),
                               action_bar_end->browser_actions_scrolled_window);
  remove_popup_webview (action_bar_end);
}

static void
show_browser_action_cb (EphyWebExtensionManager *manager,
                        EphyBrowserAction       *action,
                        EphyActionBarEnd        *action_bar_end)
{
  GtkWindow *parent_window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (action_bar_end)));
  GtkWindow *active_window = gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ()));

  /* There may be multiple action bars that exist. We only want to show the popup in the visible bar the active window. */
  if (parent_window != active_window)
    return;
  if (!gtk_widget_is_visible (action_bar_end->browser_actions_button))
    return;

  remove_popup_webview (action_bar_end);
  gtk_menu_button_popdown (GTK_MENU_BUTTON (action_bar_end->browser_actions_button));
  show_browser_action_popup (action_bar_end, action);
}

static void
browser_actions_row_activated_cb (GtkListBox           *listbox,
                                  EphyBrowserActionRow *row,
                                  EphyActionBarEnd     *action_bar_end)
{
  EphyBrowserAction *action = ephy_browser_action_row_get_browser_action (row);

  /* If it was handled we are done, otherwise we have to show a popup. */
  if (ephy_browser_action_activate (action)) {
    gtk_menu_button_popdown (GTK_MENU_BUTTON (action_bar_end->browser_actions_button));
    return;
  }

  show_browser_action_popup (action_bar_end, action);
}

static void
browser_action_popover_visible_changed_cb (GtkWidget        *popover,
                                           GParamSpec       *pspec,
                                           EphyActionBarEnd *action_bar_end)
{
  if (!gtk_widget_get_visible (popover)) {
    GtkStack *stack = GTK_STACK (action_bar_end->browser_actions_stack);

    /* Reset to default state and destroy any open webview. */
    gtk_stack_set_visible_child (stack, action_bar_end->browser_actions_scrolled_window);
    remove_popup_webview (action_bar_end);
  }
}

static void
on_bookmarks_button (GtkButton *button,
                     gpointer   user_data)
{
  GtkWindow *parent_window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (button)));
  ephy_window_toggle_bookmarks (EPHY_WINDOW (parent_window));
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
                                        downloads_icon);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        overview_button);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        browser_actions_button);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        browser_actions_popover);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        browser_actions_scrolled_window);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        browser_actions_listbox);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        browser_actions_stack);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        browser_actions_popup_view_box);
  gtk_widget_class_bind_template_child (widget_class,
                                        EphyActionBarEnd,
                                        browser_actions_popup_view_label);

  gtk_widget_class_bind_template_callback (widget_class,
                                           browser_actions_popup_view_back_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class,
                                           browser_actions_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class,
                                           on_bookmarks_button);
}

static void
ephy_action_bar_end_init (EphyActionBarEnd *action_bar_end)
{
  GObject *object = G_OBJECT (action_bar_end);
  EphyDownloadsManager *downloads_manager;
  EphyWebExtensionManager *extension_manager;
  EphyEmbedShell *embed_shell;

  gtk_widget_init_template (GTK_WIDGET (action_bar_end));

  /* Downloads */
  embed_shell = ephy_embed_shell_get_default ();
  downloads_manager = ephy_embed_shell_get_downloads_manager (embed_shell);

  gtk_revealer_set_reveal_child (GTK_REVEALER (action_bar_end->downloads_revealer),
                                 !!ephy_downloads_manager_get_downloads (downloads_manager));

  if (ephy_downloads_manager_get_downloads (downloads_manager)) {
    action_bar_end->downloads_popover = ephy_downloads_popover_new ();
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (action_bar_end->downloads_button), action_bar_end->downloads_popover);
  }

  action_bar_end->downloads_paintable = ephy_downloads_paintable_new (action_bar_end->downloads_icon);
  gtk_image_set_from_paintable (GTK_IMAGE (action_bar_end->downloads_icon),
                                action_bar_end->downloads_paintable);

  if (is_desktop_pantheon ()) {
    gtk_button_set_icon_name (GTK_BUTTON (action_bar_end->bookmarks_button),
                              "user-bookmarks");
    gtk_button_set_icon_name (GTK_BUTTON (action_bar_end->overview_button),
                              "view-grid");
  }

  gtk_widget_set_visible (action_bar_end->overview_button,
                          ephy_embed_shell_get_mode (embed_shell) != EPHY_EMBED_SHELL_MODE_APPLICATION);

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
  g_signal_connect_object (downloads_manager, "show-downloads",
                           G_CALLBACK (show_downloads_cb),
                           object, 0);

  extension_manager = ephy_web_extension_manager_get_default ();
  g_signal_connect_object (extension_manager, "show-browser-action",
                           G_CALLBACK (show_browser_action_cb),
                           object, 0);

  set_browser_actions (action_bar_end, ephy_web_extension_manager_get_browser_actions (extension_manager));

  g_signal_connect (action_bar_end->browser_actions_popover, "notify::visible",
                    G_CALLBACK (browser_action_popover_visible_changed_cb),
                    action_bar_end);
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

void
ephy_action_bar_end_show_downloads (EphyActionBarEnd *action_bar_end)
{
  if (gtk_widget_get_visible (action_bar_end->downloads_button))
    gtk_menu_button_popup (GTK_MENU_BUTTON (action_bar_end->downloads_button));
}

GtkWidget *
ephy_action_bar_end_get_downloads_revealer (EphyActionBarEnd *action_bar_end)
{
  return action_bar_end->downloads_revealer;
}

GtkWidget *
create_browser_action_item_widget (EphyBrowserAction *action,
                                   gpointer           user_data)
{
  return ephy_browser_action_row_new (action);
}

static void
browser_actions_items_changed_cb (GListModel       *list,
                                  guint             position,
                                  guint             removed,
                                  guint             added,
                                  EphyActionBarEnd *action_bar_end)
{
  gtk_widget_set_visible (action_bar_end->browser_actions_button, g_list_model_get_n_items (list) != 0);

  /* This handles an edge-case where if an extension is disabled while its popover is open the webview should be destroyed.
   * However in normal usage this shouldn't happen and with the GTK4 port the extension dialog is also modal.
   * So we just always manually close it instead of trying to track which extension popup is open. */
  if (removed)
    gtk_menu_button_popdown (GTK_MENU_BUTTON (action_bar_end->browser_actions_button));
}

static void
set_browser_actions (EphyActionBarEnd *action_bar_end,
                     GListStore       *browser_actions)
{
  gtk_list_box_bind_model (GTK_LIST_BOX (action_bar_end->browser_actions_listbox),
                           G_LIST_MODEL (browser_actions),
                           (GtkListBoxCreateWidgetFunc)create_browser_action_item_widget,
                           NULL, NULL);

  g_signal_connect_object (browser_actions, "items-changed", G_CALLBACK (browser_actions_items_changed_cb),
                           action_bar_end, 0);

  browser_actions_items_changed_cb (G_LIST_MODEL (browser_actions), 0, 0, 0, action_bar_end);
}
