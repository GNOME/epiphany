/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2018 Abdullah Alansari
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

#ifndef EPHY_AUTOFILL_MATCHERS_H
#define EPHY_AUTOFILL_MATCHERS_H

#include <glib.h>
#include <stdbool.h>
#include <stdio.h>

G_BEGIN_DECLS

bool ephy_autofill_matchers_is_firstname (const char *element_key);
bool ephy_autofill_matchers_is_lastname  (const char *element_key);
bool ephy_autofill_matchers_is_fullname  (const char *element_key);
bool ephy_autofill_matchers_is_username  (const char *element_key);
bool ephy_autofill_matchers_is_email     (const char *element_key);
bool ephy_autofill_matchers_is_phone     (const char *element_key);

bool ephy_autofill_matchers_is_street_address (const char *element_key);
bool ephy_autofill_matchers_is_organization   (const char *element_key);
bool ephy_autofill_matchers_is_postal_code    (const char *element_key);
bool ephy_autofill_matchers_is_country        (const char *element_key);
bool ephy_autofill_matchers_is_state          (const char *element_key);
bool ephy_autofill_matchers_is_city           (const char *element_key);

bool ephy_autofill_matchers_is_card_expdate_month (const char *element_key);
bool ephy_autofill_matchers_is_card_expdate_year  (const char *element_key);
bool ephy_autofill_matchers_is_card_expdate       (const char *element_key);
bool ephy_autofill_matchers_is_month              (const char *element_key);
bool ephy_autofill_matchers_is_year               (const char *element_key);

bool ephy_autofill_matchers_is_name_on_card (const char *element_key);
bool ephy_autofill_matchers_is_card_number  (const char *element_key);
bool ephy_autofill_matchers_is_card_type    (const char *element_key);

G_END_DECLS

#endif
