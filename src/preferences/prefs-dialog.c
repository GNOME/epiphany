/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 200-2003 Marco Pesenti Gritti
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

#include <gtk/gtk.h>

struct _PrefsDialog {
  GtkDialog parent_instance;

  GtkWidget *notebook;

  PrefsGeneralPage *general_page;
  PrefsSyncPage *sync_page;
};

G_DEFINE_TYPE (PrefsDialog, prefs_dialog, GTK_TYPE_DIALOG)

static void
prefs_dialog_class_init (PrefsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/epiphany/gtk/prefs-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, notebook);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, general_page);
  gtk_widget_class_bind_template_child (widget_class, PrefsDialog, sync_page);
}

static void
prefs_dialog_init (PrefsDialog *dialog)
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
