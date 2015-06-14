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

#include "config.h"
#include "ephy-autofill-matchers.h"

#include <glib-object.h>

#define FIRSTNAME_REGEX get_regex (FIRSTNAME_PATTERN)
#define LASTNAME_REGEX  get_regex (LASTNAME_PATTERN)
#define FULLNAME_REGEX  get_regex (FULLNAME_PATTERN)
#define USERNAME_REGEX  get_regex (USERNAME_PATTERN)
#define EMAIL_REGEX     get_regex (EMAIL_PATTERN)
#define PHONE_REGEX     get_regex (PHONE_PATTERN)

#define STREET_ADDRESS_REGEX get_regex (STREET_ADDRESS_PATTERN)
#define ORGANIZATION_REGEX   get_regex (ORGANIZATION_PATTERN)
#define POSTAL_CODE_REGEX    get_regex (POSTAL_CODE_PATTERN)
#define COUNTRY_REGEX        get_regex (COUNTRY_PATTERN)
#define STATE_REGEX          get_regex (STATE_PATTERN)
#define CITY_REGEX           get_regex (CITY_PATTERN)

#define CARD_EXPDATE_MONTH_REGEX get_regex (CARD_EXPDATE_MONTH_PATTERN)
#define CARD_EXPDATE_YEAR_REGEX  get_regex (CARD_EXPDATE_YEAR_PATTERN)
#define CARD_EXPDATE_REGEX       get_regex (CARD_EXPDATE_PATTERN)
#define NAME_ON_CARD_REGEX       get_regex (NAME_ON_CARD_PATTERN)
#define CARD_NUMBER_REGEX        get_regex (CARD_NUMBER_PATTERN)
#define CARD_TYPE_REGEX          get_regex (CARD_TYPE_PATTERN)


/* Regexes below are inspired by Chromium Autofill implementation.
 * You can find them here:
 * https://chromium.googlesource.com/chromium/src.git/+/master/components/autofill/core/browser/autofill_regex_constants.cc
 */
static const char FIRSTNAME_PATTERN[] = "first.*name|initials|fname|first$|given.*name";
static const char LASTNAME_PATTERN[] = "last.*name|lname|surname|last$|secondname|family.*name";
static const char FULLNAME_PATTERN[] =
  "^name|full.?name|your.?name|customer.?name|bill.?name|ship.?name"
  "|name.*first.*last|firstandlastname";
static const char USERNAME_PATTERN[] = "user.?name|nick.?name";
static const char EMAIL_PATTERN[] = "e.?mail";
static const char PHONE_PATTERN[] = "phone|mobile";

static const char STREET_ADDRESS_PATTERN[] =
  "address.*line|address1|addr1|street"
  "|(shipping|billing)address$"
  "|house.?name"
  "|address|line";
static const char ORGANIZATION_PATTERN[] = "company|business|organization|organisation";
static const char POSTAL_CODE_PATTERN[] = "zip|postal|post.*code|pcode|pin.?code";
static const char COUNTRY_PATTERN[] = "country|countries|location";
static const char STATE_PATTERN[] = "province|region|(?<!united )state|county|region|province|county|principality";
static const char CITY_PATTERN[] = "city|town|suburb";

static const char CARD_EXPDATE_MONTH_PATTERN[] = "expir|exp.*mo|exp.*date|ccmonth|cardmonth";
static const char CARD_EXPDATE_YEAR_PATTERN[] = "exp|^/|year";
static const char CARD_EXPDATE_PATTERN[] = "expir|exp.*date";
static const char NAME_ON_CARD_PATTERN[] =
  "card.?(holder|owner)|name.*\\bon\\b.*card"
  "|(card|cc).?name|cc.?full.?name";
static const char CARD_NUMBER_PATTERN[] = "(card|cc|acct).?(number|#|no|num)";
static const char CARD_TYPE_PATTERN[] = "debit.*card|(card|cc).?type";

static GHashTable *regexes = NULL;

static const GRegex *
get_regex (const char *pattern)
{
  GRegex *regex;

  if (regexes == NULL)
    regexes = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  regex = g_hash_table_lookup (regexes, pattern);

  if (regex == NULL) {
    regex = g_regex_new (pattern, G_REGEX_CASELESS | G_REGEX_OPTIMIZE, 0, NULL);
    g_hash_table_insert (regexes, (char*)pattern, regex);
  }

  return (const GRegex*)regex;
}

static bool
matches (const GRegex *regex,
         const char *string)
{
  return g_regex_match (regex, (const char*)string, 0, NULL);
}

bool
ephy_autofill_matchers_is_firstname (const char *element_key)
{
  return matches (FIRSTNAME_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_lastname (const char *element_key)
{
  return matches (LASTNAME_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_fullname (const char *element_key)
{
  return matches (FULLNAME_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_username (const char *element_key)
{
  return matches (USERNAME_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_email (const char *element_key)
{
  return matches (EMAIL_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_phone (const char *element_key)
{
  return matches (PHONE_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_street_address (const char *element_key)
{
  return matches (STREET_ADDRESS_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_organization (const char *element_key)
{
  return matches (ORGANIZATION_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_postal_code (const char *element_key)
{
  return matches (POSTAL_CODE_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_country (const char *element_key)
{
  return matches (COUNTRY_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_state (const char *element_key)
{
  return matches (STATE_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_city (const char *element_key)
{
  return matches (CITY_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_card_expdate_month (const char *element_key)
{
  return matches (CARD_EXPDATE_MONTH_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_card_expdate_year (const char *element_key)
{
  return matches (CARD_EXPDATE_YEAR_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_month (const char *element_key)
{
  return matches (get_regex ("month"), element_key);
}

bool
ephy_autofill_matchers_is_year (const char *element_key)
{
  return matches (get_regex ("year"), element_key);
}

bool
ephy_autofill_matchers_is_card_expdate (const char *element_key)
{
  return matches (CARD_EXPDATE_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_name_on_card (const char *element_key)
{
  return matches (NAME_ON_CARD_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_card_number (const char *element_key)
{
  return matches (CARD_NUMBER_REGEX, element_key);
}

bool
ephy_autofill_matchers_is_card_type (const char *element_key)
{
  return matches (CARD_TYPE_REGEX, element_key);
}
