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

#include <glib.h>

G_BEGIN_DECLS

/* NOTE: @EphyAutofillFillChoice here must be synchronized with the Javascript
 * part in `ephy_autofill.js`.
 */
typedef enum
{
  EPHY_AUTOFILL_FILL_CHOICE_FORM_PERSONAL = 0,
  EPHY_AUTOFILL_FILL_CHOICE_FORM_ALL = 1,
  EPHY_AUTOFILL_FILL_CHOICE_ELEMENT = 2
} EphyAutofillFillChoice;

G_END_DECLS

