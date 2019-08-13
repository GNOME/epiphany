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

#ifndef EPHY_AUTOFILL_JS_H
#define EPHY_AUTOFILL_JS_H

#include "ephy-autofill-field.h"

#include <gio/gio.h>
#include <glib.h>
#include <jsc/jsc.h>
#include <webkit2/webkit-web-extension.h>
#include <JavaScriptCore/JavaScript.h>

G_BEGIN_DECLS

void ephy_autofill_js_change_value (JSCValue   *js_input_element,
                                    const char *value);

void ephy_autofill_js_emit_signal (const char       *object_path,
                                   const char       *interface,
                                   GDBusConnection  *dbus_connection,
                                   unsigned long     page_id,
                                   EphyAutofillField field,
                                   char             *selector,
                                   bool              is_fillable_element,
                                   bool              has_personal_fields,
                                   bool              has_card_fields,
                                   double            x,
                                   double            y,
                                   double            element_width,
                                   double            element_height);

void ephy_autofill_js_get_field_value (EphyAutofillField field, JSCValue *cb);

G_END_DECLS

#endif
