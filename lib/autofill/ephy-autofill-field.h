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

/**
 * EphyAutofillField:
 * @EPHY_AUTOFILL_FIELD_UNKNOWN:
 * @EPHY_AUTOFILL_FIELD_FIRSTNAME:
 * @EPHY_AUTOFILL_FIELD_LASTNAME:
 * @EPHY_AUTOFILL_FIELD_FULLNAME:
 * @EPHY_AUTOFILL_FIELD_USERNAME:
 * @EPHY_AUTOFILL_FIELD_EMAIL:
 * @EPHY_AUTOFILL_FIELD_PHONE:
 * @EPHY_AUTOFILL_FIELD_STREET_ADDRESS:
 * @EPHY_AUTOFILL_FIELD_COUNTRY_CODE:
 * @EPHY_AUTOFILL_FIELD_COUNTRY_NAME:
 * @EPHY_AUTOFILL_FIELD_COUNTRY:
 * @EPHY_AUTOFILL_FIELD_ORGANIZATION:
 * @EPHY_AUTOFILL_FIELD_POSTAL_CODE:
 * @EPHY_AUTOFILL_FIELD_STATE:
 * @EPHY_AUTOFILL_FIELD_CITY:
 * @EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH_MM:
 * @EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH_M:
 * @EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH:
 * @EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR_YYYY:
 * @EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR_YY:
 * @EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR:
 * @EPHY_AUTOFILL_FIELD_CARD_EXPDATE:
 * @EPHY_AUTOFILL_FIELD_NAME_ON_CARD:
 * @EPHY_AUTOFILL_FIELD_CARD_NUMBER:
 * @EPHY_AUTOFILL_FIELD_CARD_TYPE_CODE:
 * @EPHY_AUTOFILL_FIELD_CARD_TYPE_NAME:
 * @EPHY_AUTOFILL_FIELD_CARD_TYPE:
 * @EPHY_AUTOFILL_FIELD_PERSONAL:
 * @EPHY_AUTOFILL_FIELD_CARD:
 * @EPHY_AUTOFILL_FIELD_GENERAL:
 * @EPHY_AUTOFILL_FIELD_SPECIFIC:
 *
 * # Description
 * An #EphyAutofillField represents the expected value
 * to be filled in an element which is an HTML input or select element.
 * For example: email, credit card number, etc...
 *
 * # Usage
 * - Matching: as in ephy-autofill-matchers.c.
 * - Storage: as in ephy-autofill-storage.c.
 *
 * # Variants
 * Some fields have more than one variant.
 * There are two variants:
 *  1. Specific: most fields are specific.
 *  2. General: represents a hint to use a default specific value.
 *     There are only 4 General fields:
 *      1. @EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH
 *      2. @EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR
 *      3. @EPHY_AUTOFILL_FIELD_CARD_TYPE
 *      4. @EPHY_AUTOFILL_FIELD_COUNTRY
 *
 * For example:
 *  - @EPHY_AUTOFILL_FIELD_COUNTRY_NAME: specific value
 *  - @EPHY_AUTOFILL_FIELD_COUNTRY_CODE: specific value
 *  - @EPHY_AUTOFILL_FIELD_COUNTRY: general value
 *
 * In this case @EPHY_AUTOFILL_FIELD_COUNTRY is used for matching - as in
 * ephy-autofill-matchers.c. And is also used in ephy_autofill_storage_get()
 * where it returns some default value representing some of the two specific
 * values:
 *  1. @EPHY_AUTOFILL_FIELD_COUNTRY_NAME
 *  2. @EPHY_AUTOFILL_FIELD_COUNTRY_CODE
 * However, specific values are required for example in
 * ephy_autofill_storage_set(). The main reason for this is that only specific
 * values are stored and general values are used to simplify some operations:
 * retrieval, matching, etc...
 * 
 * NOTE: @EphyAutofillField here must be synchronized with the Javascript part
 * in `ephy_autofill.js`.
 **/
typedef enum
{
  EPHY_AUTOFILL_FIELD_UNKNOWN = 0,

  EPHY_AUTOFILL_FIELD_FIRSTNAME = (1 << 0),
  EPHY_AUTOFILL_FIELD_LASTNAME = (1 << 1),
  EPHY_AUTOFILL_FIELD_FULLNAME = (1 << 2),
  EPHY_AUTOFILL_FIELD_USERNAME = (1 << 3),
  EPHY_AUTOFILL_FIELD_EMAIL = (1 << 4),
  EPHY_AUTOFILL_FIELD_PHONE = (1 << 5),

  EPHY_AUTOFILL_FIELD_STREET_ADDRESS = (1 << 6),
  EPHY_AUTOFILL_FIELD_COUNTRY_CODE = (1 << 7),
  EPHY_AUTOFILL_FIELD_COUNTRY_NAME = (1 << 8),
  EPHY_AUTOFILL_FIELD_ORGANIZATION = (1 << 9),
  EPHY_AUTOFILL_FIELD_POSTAL_CODE = (1 << 10),
  EPHY_AUTOFILL_FIELD_COUNTRY = (1 << 11),
  EPHY_AUTOFILL_FIELD_STATE = (1 << 12),
  EPHY_AUTOFILL_FIELD_CITY = (1 << 13),

  EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH_MM = (1 << 14),
  EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH_M = (1 << 15),
  EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH = (1 << 16),

  EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR_YYYY = (1 << 17),
  EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR_YY = (1 << 18),
  EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR = (1 << 19),

  EPHY_AUTOFILL_FIELD_CARD_EXPDATE = (1 << 20),
  EPHY_AUTOFILL_FIELD_NAME_ON_CARD = (1 << 21),
  EPHY_AUTOFILL_FIELD_CARD_NUMBER = (1 << 22),

  EPHY_AUTOFILL_FIELD_CARD_TYPE_CODE = (1 << 23),
  EPHY_AUTOFILL_FIELD_CARD_TYPE_NAME = (1 << 24),
  EPHY_AUTOFILL_FIELD_CARD_TYPE = (1 << 25),

  EPHY_AUTOFILL_FIELD_PERSONAL = (EPHY_AUTOFILL_FIELD_FIRSTNAME |
                                  EPHY_AUTOFILL_FIELD_LASTNAME |
                                  EPHY_AUTOFILL_FIELD_FULLNAME |
                                  EPHY_AUTOFILL_FIELD_USERNAME |
                                  EPHY_AUTOFILL_FIELD_EMAIL |
                                  EPHY_AUTOFILL_FIELD_PHONE |
                                  EPHY_AUTOFILL_FIELD_STREET_ADDRESS |
                                  EPHY_AUTOFILL_FIELD_COUNTRY_CODE |
                                  EPHY_AUTOFILL_FIELD_COUNTRY_NAME |
                                  EPHY_AUTOFILL_FIELD_ORGANIZATION |
                                  EPHY_AUTOFILL_FIELD_POSTAL_CODE |
                                  EPHY_AUTOFILL_FIELD_COUNTRY |
                                  EPHY_AUTOFILL_FIELD_STATE |
                                  EPHY_AUTOFILL_FIELD_CITY),

  EPHY_AUTOFILL_FIELD_CARD = (EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH_MM |
                              EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH_M |
                              EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH |
                              EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR_YYYY |
                              EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR_YY |
                              EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR |
                              EPHY_AUTOFILL_FIELD_CARD_EXPDATE |
                              EPHY_AUTOFILL_FIELD_NAME_ON_CARD |
                              EPHY_AUTOFILL_FIELD_CARD_NUMBER |
                              EPHY_AUTOFILL_FIELD_CARD_TYPE_CODE |
                              EPHY_AUTOFILL_FIELD_CARD_TYPE_NAME |
                              EPHY_AUTOFILL_FIELD_CARD_TYPE),

  EPHY_AUTOFILL_FIELD_GENERAL = (EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH |
                                 EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR |
                                 EPHY_AUTOFILL_FIELD_CARD_TYPE |
                                 EPHY_AUTOFILL_FIELD_COUNTRY),

  EPHY_AUTOFILL_FIELD_SPECIFIC = ~EPHY_AUTOFILL_FIELD_GENERAL
} EphyAutofillField;

G_END_DECLS

