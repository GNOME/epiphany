/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019 Purism SPC
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_DATA_DIALOG (ephy_data_dialog_get_type ())

G_DECLARE_DERIVABLE_TYPE (EphyDataDialog, ephy_data_dialog, EPHY, DATA_DIALOG, GtkWindow)

struct _EphyDataDialogClass
{
  GtkWindowClass parent_class;
};

const gchar *ephy_data_dialog_get_clear_all_description (EphyDataDialog *self);
void         ephy_data_dialog_set_clear_all_description (EphyDataDialog *self,
                                                         const gchar    *description);

gboolean ephy_data_dialog_get_has_data (EphyDataDialog *self);
void     ephy_data_dialog_set_has_data (EphyDataDialog *self,
                                        gboolean        has_data);

gboolean ephy_data_dialog_get_has_search_results (EphyDataDialog *self);
void     ephy_data_dialog_set_has_search_results (EphyDataDialog *self,
                                                  gboolean        has_search_results);

gboolean ephy_data_dialog_get_can_clear (EphyDataDialog *self);
void     ephy_data_dialog_set_can_clear (EphyDataDialog *self,
                                         gboolean        can_clear);

const gchar *ephy_data_dialog_get_search_text (EphyDataDialog *self);

G_END_DECLS
