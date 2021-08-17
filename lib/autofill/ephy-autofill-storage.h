/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2019 Abdullah Alansari
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

#include "ephy-autofill-field.h"

#include <gio/gio.h>

G_BEGIN_DECLS

void ephy_autofill_storage_delete (EphyAutofillField    field,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data);

gboolean  ephy_autofill_storage_delete_finish (GAsyncResult  *res,
                                               GError       **error);

void ephy_autofill_storage_get (EphyAutofillField    field,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data);

char *ephy_autofill_storage_get_finish (GAsyncResult  *res,
                                        GError       **error);

void ephy_autofill_storage_set (EphyAutofillField    field,
                                const char          *value,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data);

gboolean  ephy_autofill_storage_set_finish (GAsyncResult  *res,
                                            GError       **error);

G_END_DECLS

