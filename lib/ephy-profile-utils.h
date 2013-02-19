/*
 *  Copyright © 2009 Xan López
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

#ifndef EPHY_PROFILE_UTILS_H
#define EPHY_PROFILE_UTILS_H

#define SECRET_API_SUBJECT_TO_CHANGE

#include <glib.h>
#include <libsecret/secret.h>

#define URI_KEY           "uri"
#define FORM_USERNAME_KEY "form_username"
#define FORM_PASSWORD_KEY "form_password"
#define USERNAME_KEY      "username"

#define EPHY_PROFILE_MIGRATION_VERSION 8

#define EPHY_HISTORY_FILE       "ephy-history.db"
#define EPHY_BOOKMARKS_FILE     "ephy-bookmarks.xml"
#define EPHY_BOOKMARKS_FILE_RDF "bookmarks.rdf"

int ephy_profile_utils_get_migration_version (void);

gboolean ephy_profile_utils_set_migration_version (int version);

gboolean ephy_profile_utils_do_migration (const char *profile_directory, int test_to_run, gboolean debug);

void _ephy_profile_utils_store_form_auth_data            (const char *uri,
							  const char *form_username,
							  const char *form_password,
							  const char *username,
							  const char *password,
							  GAsyncReadyCallback callback,
							  gpointer userdata);

gboolean _ephy_profile_utils_store_form_auth_data_finish (GAsyncResult *result,
							  GError **error);

typedef void (*EphyQueryFormDataCallback)                (const char *username, const char *password, gpointer user_data);

void
_ephy_profile_utils_query_form_auth_data                 (const char *uri,
							  const char *form_username,
							  const char *form_password,
							  EphyQueryFormDataCallback callback,
							  gpointer data,
							  GDestroyNotify destroy_data);

const SecretSchema *ephy_profile_get_form_password_schema (void) G_GNUC_CONST;

#define EPHY_FORM_PASSWORD_SCHEMA ephy_profile_get_form_password_schema ()

#endif
