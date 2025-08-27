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

#include "config.h"
#include "ephy-autofill-storage.h"

#include <libsecret/secret.h>

#define FIELD_KEY "key"

#define FIRSTNAME_KEY "firstname"
#define LASTNAME_KEY  "lastname"
#define FULLNAME_KEY  "fullname"
#define USERNAME_KEY  "username"
#define EMAIL_KEY     "email"
#define PHONE_KEY     "phone"

#define STREET_ADDRESS_KEY "street-address"
#define COUNTRY_CODE_KEY   "country-code"
#define COUNTRY_NAME_KEY   "country-name"
#define ORGANIZATION_KEY   "organization"
#define POSTAL_CODE_KEY    "postal-code"
#define STATE_KEY          "state"
#define CITY_KEY           "city"

#define CARD_EXPDATE_YEAR_YYYY_KEY "credit-card-expdate-year-yyyy"
#define CARD_EXPDATE_MONTH_MM_KEY  "credit-card-expdate-month-mm"
#define CARD_EXPDATE_MONTH_M_KEY   "credit-card-expdate-month-m"
#define CARD_EXPDATE_YEAR_YY_KEY   "credit-card-expdate-year-yy"

#define CARD_TYPE_CODE_KEY "credit-card-type-code"
#define CARD_TYPE_NAME_KEY "credit-card-type-name"
#define CARD_EXPDATE_KEY   "credit-card-expdate"
#define NAME_ON_CARD_KEY   "credit-card-name-on-card"
#define CARD_NUMBER_KEY    "credit-card-number"

#define FIRSTNAME_LABEL "GNOME Web Autofill - Firstname"
#define LASTNAME_LABEL  "GNOME Web Autofill - Lastname"
#define FULLNAME_LABEL  "GNOME Web Autofill - Fullname"
#define USERNAME_LABEL  "GNOME Web Autofill - Username"
#define EMAIL_LABEL     "GNOME Web Autofill - Email"
#define PHONE_LABEL     "GNOME Web Autofill - Phone"

#define STREET_ADDRESS_LABEL "GNOME Web Autofill - Street Address"
#define COUNTRY_CODE_LABEL   "GNOME Web Autofill - Country Code"
#define COUNTRY_NAME_LABEL   "GNOME Web Autofill - Country Name"
#define ORGANIZATION_LABEL   "GNOME Web Autofill - Organization"
#define POSTAL_CODE_LABEL    "GNOME Web Autofill - Postal Code"
#define STATE_LABEL          "GNOME Web Autofill - State"
#define CITY_LABEL           "GNOME Web Autofill - City"

#define CARD_EXPDATE_YEAR_YYYY_LABEL \
        "GNOME Web Autofill - Credit Card Year Expiration Date - YYYY"
#define CARD_EXPDATE_MONTH_MM_LABEL \
        "GNOME Web Autofill - Credit Card Month Expiration Date - MM"
#define CARD_EXPDATE_MONTH_M_LABEL \
        "GNOME Web Autofill - Credit Card Month Expiration Date - M"
#define CARD_EXPDATE_YEAR_YY_LABEL \
        "GNOME Web Autofill - Credit Card Year Expiration Date - YY"

#define CARD_TYPE_CODE_LABEL "GNOME Web Autofill - Credit Card Type Code"
#define CARD_TYPE_NAME_LABEL "GNOME Web Autofill - Credit Card Type Name"
#define CARD_EXPDATE_LABEL   "GNOME Web Autofill - Credit Card Expiration Date"
#define NAME_ON_CARD_LABEL   "GNOME Web Autofill - Credit Card Name on Card"
#define CARD_NUMBER_LABEL    "GNOME Web Autofill - Credit Card Number"

#define EPHY_AUTOFILL_SCHEMA get_schema ()

static const SecretSchema *
get_schema (void)
{
  static const SecretSchema schema = {
    "org.epiphany.autofill", SECRET_SCHEMA_NONE,
    {
      { FIELD_KEY, SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "NULL", 0 }
    }
  };

  return &schema;
}

static void
get_key_and_label_for_field (EphyAutofillField   field,
                             const char        **key,
                             const char        **label)
{
  switch (field) {
    case EPHY_AUTOFILL_FIELD_FIRSTNAME:
      *label = FIRSTNAME_LABEL;
      *key = FIRSTNAME_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_LASTNAME:
      *label = LASTNAME_LABEL;
      *key = LASTNAME_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_FULLNAME:
      *label = FULLNAME_LABEL;
      *key = FULLNAME_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_USERNAME:
      *label = USERNAME_LABEL;
      *key = USERNAME_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_EMAIL:
      *label = EMAIL_LABEL;
      *key = EMAIL_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_PHONE:
      *label = PHONE_LABEL;
      *key = PHONE_KEY;
      break;

    case EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH_MM:
    case EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH:
      *label = CARD_EXPDATE_MONTH_MM_LABEL;
      *key = CARD_EXPDATE_MONTH_MM_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_CARD_EXPDATE_MONTH_M:
      *label = CARD_EXPDATE_MONTH_M_LABEL;
      *key = CARD_EXPDATE_MONTH_M_KEY;
      break;

    case EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR_YYYY:
    case EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR:
      *label = CARD_EXPDATE_YEAR_YYYY_LABEL;
      *key = CARD_EXPDATE_YEAR_YYYY_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_CARD_EXPDATE_YEAR_YY:
      *label = CARD_EXPDATE_YEAR_YY_LABEL;
      *key = CARD_EXPDATE_YEAR_YY_KEY;
      break;

    case EPHY_AUTOFILL_FIELD_CARD_EXPDATE:
      *label = CARD_EXPDATE_LABEL;
      *key = CARD_EXPDATE_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_NAME_ON_CARD:
      *label = NAME_ON_CARD_LABEL;
      *key = NAME_ON_CARD_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_CARD_NUMBER:
      *label = CARD_NUMBER_LABEL;
      *key = CARD_NUMBER_KEY;
      break;

    case EPHY_AUTOFILL_FIELD_CARD_TYPE_NAME:
    case EPHY_AUTOFILL_FIELD_CARD_TYPE:
      *label = CARD_TYPE_NAME_LABEL;
      *key = CARD_TYPE_NAME_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_CARD_TYPE_CODE:
      *label = CARD_TYPE_CODE_LABEL;
      *key = CARD_TYPE_CODE_KEY;
      break;

    case EPHY_AUTOFILL_FIELD_STREET_ADDRESS:
      *label = STREET_ADDRESS_LABEL;
      *key = STREET_ADDRESS_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_COUNTRY_CODE:
      *label = COUNTRY_CODE_LABEL;
      *key = COUNTRY_CODE_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_COUNTRY_NAME:
    case EPHY_AUTOFILL_FIELD_COUNTRY:
      *label = COUNTRY_NAME_LABEL;
      *key = COUNTRY_NAME_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_ORGANIZATION:
      *label = ORGANIZATION_LABEL;
      *key = ORGANIZATION_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_POSTAL_CODE:
      *label = POSTAL_CODE_LABEL;
      *key = POSTAL_CODE_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_STATE:
      *label = STATE_LABEL;
      *key = STATE_KEY;
      break;
    case EPHY_AUTOFILL_FIELD_CITY:
      *label = CITY_LABEL;
      *key = CITY_KEY;
      break;

    case EPHY_AUTOFILL_FIELD_SPECIFIC:
    case EPHY_AUTOFILL_FIELD_GENERAL:
    case EPHY_AUTOFILL_FIELD_PERSONAL:
    case EPHY_AUTOFILL_FIELD_CARD:
    case EPHY_AUTOFILL_FIELD_UNKNOWN:
    default:
      *label = NULL;
      *key = NULL;
  }
}

void
ephy_autofill_storage_delete (EphyAutofillField    field,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  const char *label;
  const char *key;

  if ((field & EPHY_AUTOFILL_FIELD_SPECIFIC) == 0)
    return;

  get_key_and_label_for_field (field, &key, &label);

  if (key) {
    secret_password_clear (EPHY_AUTOFILL_SCHEMA,
                           cancellable,
                           callback,
                           user_data,
                           FIELD_KEY, key,
                           NULL);
  }
}

void
ephy_autofill_storage_get (EphyAutofillField    field,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  const char *label;
  const char *key;

  get_key_and_label_for_field (field, &key, &label);

  if (key) {
    secret_password_lookup (EPHY_AUTOFILL_SCHEMA,
                            cancellable,
                            callback,
                            user_data,
                            FIELD_KEY,
                            key,
                            NULL);
  }
}

void
ephy_autofill_storage_set (EphyAutofillField    field,
                           const char          *value,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  const char *storable_value = (!value) ? "" : value;
  const char *label;
  const char *key;

  get_key_and_label_for_field (field, &key, &label);

  if (label && key) {
    secret_password_store (EPHY_AUTOFILL_SCHEMA,
                           SECRET_COLLECTION_DEFAULT,
                           label,
                           storable_value,
                           cancellable,
                           callback,
                           user_data,
                           FIELD_KEY, key,
                           NULL);
  }
}

gboolean
ephy_autofill_storage_delete_finish (GAsyncResult  *res,
                                     GError       **error)
{
  return secret_password_store_finish (res, error);
}

char *
ephy_autofill_storage_get_finish (GAsyncResult  *res,
                                  GError       **error)
{
  return secret_password_lookup_finish (res, error);
}

gboolean
ephy_autofill_storage_set_finish (GAsyncResult  *res,
                                  GError       **error)
{
  return secret_password_store_finish (res, error);
}
