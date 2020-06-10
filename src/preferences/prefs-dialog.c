/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2010, 2017 Igalia S.L.
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
#include "prefs-dialog.h"

#include "ephy-embed-utils.h"
#include "ephy-gui.h"
#include "prefs-general-page.h"
#include "prefs-sync-page.h"

struct _EphyPrefsDialog {
  HdyWindow parent_instance;

  HdyDeck *deck;
  GtkWidget *prefs_pages_view;
  GtkWidget *notebook;

  PrefsGeneralPage *general_page;
  PrefsSyncPage *sync_page;

  GtkStack *data_views_stack;
  GtkWidget *clear_cookies_view;
  GtkWidget *passwords_view;
  GtkWidget *clear_data_view;
};

G_DEFINE_TYPE (EphyPrefsDialog, ephy_prefs_dialog, HDY_TYPE_WINDOW)

static void
on_clear_cookies_row_activated (GtkWidget       *privacy_page,
                                EphyPrefsDialog *prefs_dialog)
{
  gtk_stack_set_visible_child (prefs_dialog->data_views_stack, prefs_dialog->clear_cookies_view);
  hdy_deck_navigate (prefs_dialog->deck, HDY_NAVIGATION_DIRECTION_FORWARD);
}

static void
on_passwords_row_activated (GtkWidget       *privacy_page,
                            EphyPrefsDialog *prefs_dialog)
{
  gtk_stack_set_visible_child (prefs_dialog->data_views_stack, prefs_dialog->passwords_view);
  hdy_deck_navigate (prefs_dialog->deck, HDY_NAVIGATION_DIRECTION_FORWARD);
}

static void
on_clear_data_row_activated (GtkWidget       *privacy_page,
                             EphyPrefsDialog *prefs_dialog)
{
  gtk_stack_set_visible_child (prefs_dialog->data_views_stack, prefs_dialog->clear_data_view);
  hdy_deck_navigate (prefs_dialog->deck, HDY_NAVIGATION_DIRECTION_FORWARD);
}

static void
on_any_data_view_back_button_clicked (GtkWidget       *data_view,
                                      EphyPrefsDialog *prefs_dialog)
{
  hdy_deck_navigate (prefs_dialog->deck, HDY_NAVIGATION_DIRECTION_BACK);
}

static void
ephy_prefs_dialog_class_init (EphyPrefsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyPrefsDialog, deck);
  gtk_widget_class_bind_template_child (widget_class, EphyPrefsDialog, prefs_pages_view);
  gtk_widget_class_bind_template_child (widget_class, EphyPrefsDialog, notebook);
  gtk_widget_class_bind_template_child (widget_class, EphyPrefsDialog, general_page);
  gtk_widget_class_bind_template_child (widget_class, EphyPrefsDialog, sync_page);
  gtk_widget_class_bind_template_child (widget_class, EphyPrefsDialog, data_views_stack);
  gtk_widget_class_bind_template_child (widget_class, EphyPrefsDialog, clear_cookies_view);
  gtk_widget_class_bind_template_child (widget_class, EphyPrefsDialog, passwords_view);
  gtk_widget_class_bind_template_child (widget_class, EphyPrefsDialog, clear_data_view);

  /* Template file callbacks */
  gtk_widget_class_bind_template_callback (widget_class, on_clear_cookies_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_passwords_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_clear_data_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_any_data_view_back_button_clicked);
}

static void
ephy_prefs_dialog_init (EphyPrefsDialog *dialog)
{
  EphyEmbedShellMode mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());

  gtk_widget_init_template (GTK_WIDGET (dialog));
  gtk_window_set_icon_name (GTK_WINDOW (dialog), APPLICATION_ID);

  prefs_general_page_connect_pd_response (dialog->general_page, dialog);

  if (mode == EPHY_EMBED_SHELL_MODE_BROWSER)
    prefs_sync_page_setup (dialog->sync_page);
  else
    gtk_notebook_remove_page (GTK_NOTEBOOK (dialog->notebook), -1);

  ephy_gui_ensure_window_group (GTK_WINDOW (dialog));
}
