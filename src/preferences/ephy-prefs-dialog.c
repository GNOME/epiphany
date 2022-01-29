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

#include "clear-data-view.h"
#include "ephy-data-view.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-gui.h"
#include "ephy-prefs-dialog.h"
#include "ephy-search-engine-manager.h"
#include "passwords-view.h"
#include "prefs-general-page.h"

struct _EphyPrefsDialog {
  HdyPreferencesWindow parent_instance;

  PrefsGeneralPage *general_page;

  GtkWidget *active_data_view;
};

G_DEFINE_TYPE (EphyPrefsDialog, ephy_prefs_dialog, HDY_TYPE_PREFERENCES_WINDOW)

static gboolean
on_key_press_event (EphyPrefsDialog *prefs_dialog,
                    GdkEvent        *event,
                    gpointer         user_data)
{
  EphyDataView *active_data_view = EPHY_DATA_VIEW (prefs_dialog->active_data_view);

  /* If the user is currently viewing one the data views,
   * then we want to redirect any key events there */
  if (active_data_view)
    return ephy_data_view_handle_event (active_data_view, event);

  return GDK_EVENT_PROPAGATE;
}

static void
on_delete_event (EphyPrefsDialog *prefs_dialog)
{
  prefs_general_page_on_pd_delete_event (prefs_dialog->general_page);
  gtk_widget_destroy (GTK_WIDGET (prefs_dialog));

  /* To avoid any unnecessary IO when typing changes in the search engine
   * list row's entries, only save when closing the prefs dialog.
   */
  ephy_search_engine_manager_save_to_settings (ephy_embed_shell_get_search_engine_manager (ephy_embed_shell_get_default ()));
}

static void
on_any_data_view_back_button_clicked (GtkWidget       *data_view,
                                      EphyPrefsDialog *prefs_dialog)
{
  hdy_preferences_window_close_subpage (HDY_PREFERENCES_WINDOW (prefs_dialog));

  prefs_dialog->active_data_view = NULL;
}

static void
present_data_view (EphyPrefsDialog *prefs_dialog,
                   GtkWidget       *presented_view)
{
  g_signal_connect_object (presented_view, "back-button-clicked",
                           G_CALLBACK (on_any_data_view_back_button_clicked),
                           prefs_dialog, 0);

  hdy_preferences_window_present_subpage (HDY_PREFERENCES_WINDOW (prefs_dialog),
                                          presented_view);

  prefs_dialog->active_data_view = presented_view;
}

static void
on_passwords_row_activated (GtkWidget       *privacy_page,
                            EphyPrefsDialog *prefs_dialog)
{
  GtkWidget *view = g_object_new (EPHY_TYPE_PASSWORDS_VIEW,
                                  "visible", TRUE,
                                  NULL);

  present_data_view (prefs_dialog, view);
}

static void
on_clear_data_row_activated (GtkWidget       *privacy_page,
                             EphyPrefsDialog *prefs_dialog)
{
  GtkWidget *view = g_object_new (EPHY_TYPE_CLEAR_DATA_VIEW,
                                  "visible", TRUE,
                                  NULL);

  present_data_view (prefs_dialog, view);
}

static void
ephy_prefs_dialog_class_init (EphyPrefsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyPrefsDialog, general_page);

  /* Template file callbacks */
  gtk_widget_class_bind_template_callback (widget_class, on_key_press_event);
  gtk_widget_class_bind_template_callback (widget_class, on_delete_event);
  gtk_widget_class_bind_template_callback (widget_class, on_passwords_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_clear_data_row_activated);
}

static void
ephy_prefs_dialog_init (EphyPrefsDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));
  gtk_window_set_icon_name (GTK_WINDOW (dialog), APPLICATION_ID);

  ephy_gui_ensure_window_group (GTK_WINDOW (dialog));
}
