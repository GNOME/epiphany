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

#ifndef EPHY_AUTOFILL_FILL_CHOICE_H
#define EPHY_AUTOFILL_FILL_CHOICE_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  EPHY_AUTOFILL_FILL_CHOICE_FORM_PERSONAL = (1 << 0),
  EPHY_AUTOFILL_FILL_CHOICE_FORM_ALL = (1 << 1),
  EPHY_AUTOFILL_FILL_CHOICE_ELEMENT = (1 << 2)
} EphyAutofillFillChoice;

G_END_DECLS

#endif
