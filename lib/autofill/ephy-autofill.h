/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2019 Abdullah Alansari
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef EPHY_AUTOFILL_H
#define EPHY_AUTOFILL_H

#include "ephy-autofill-field.h"
#include "ephy-autofill-fill-choice.h"

#include <gio/gio.h>
#include <glib.h>
#include <webkit2/webkit-web-extension.h>

G_BEGIN_DECLS

void
ephy_autofill_web_page_document_loaded (WebKitWebPage *web_page,
                                        GDBusConnection *dbus_connection,
                                        const char *ephy_web_extension_object_path,
                                        const char *ephy_web_extension_interface);

void
ephy_autofill_fill (WebKitWebPage *web_page,
                    const char *selector,
                    EphyAutofillFillChoice fill_choice);

void ephy_autofill_delete (EphyAutofillField field,
                           GAsyncReadyCallback callback,
                           gpointer user_data);

void ephy_autofill_get (EphyAutofillField field,
                        GAsyncReadyCallback callback,
                        gpointer user_data);

void ephy_autofill_set (EphyAutofillField field,
                        const char *value,
                        GAsyncReadyCallback callback,
                        gpointer user_data);

bool  ephy_autofill_delete_finish (GAsyncResult *res);
char *ephy_autofill_get_finish (GAsyncResult *res);
bool  ephy_autofill_set_finish (GAsyncResult *res);

G_END_DECLS

#endif
