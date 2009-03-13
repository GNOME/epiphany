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

#include "config.h"

#include "ephy-file-helpers.h"
#include "ephy-profile-migration.h"

#include <glib/gi18n.h>
#include <libsoup/soup-gnome.h>

/*
 * What to do to add new migration steps:
 *  - Bump PROFILE_MIGRATION_VERSION
 *  - Add your function at the end of the 'migrators' array
 */

#define PROFILE_MIGRATION_VERSION 1

typedef void (*EphyProfileMigrator) (void);

static void
migrate_cookies ()
{
  const char *cookies_file_sqlite = "cookies.sqlite";
  const char *cookies_file_txt = "cookies.txt";
  char *src_sqlite = NULL, *src_txt = NULL, *dest = NULL;

  dest = g_build_filename (ephy_dot_dir (), cookies_file_sqlite, NULL);
  /* If we already have a cookies.sqlite file, do nothing */
  if (g_file_test (dest, G_FILE_TEST_EXISTS))
    goto out;

  src_sqlite = g_build_filename (ephy_dot_dir (), "mozilla",
                                 "epiphany", cookies_file_sqlite, NULL);
  src_txt = g_build_filename (ephy_dot_dir (), "mozilla",
                              "epiphany", cookies_file_txt, NULL);

  /* First check if we have a cookies.sqlite file in Mozilla */
  if (g_file_test (src_sqlite, G_FILE_TEST_EXISTS)) {
    GFile *gsrc, *gdest;

    /* Copy the file */
    gsrc = g_file_new_for_path (src_sqlite);
    gdest = g_file_new_for_path (dest);

    if (!g_file_copy (gsrc, gdest, 0, NULL, NULL, NULL, NULL))
      g_warning (_("Failed to copy cookies file from Mozilla."));

    g_object_unref (gsrc);
    g_object_unref (gdest);
  } else if (g_file_test (src_txt, G_FILE_TEST_EXISTS)) {
    /* Create a SoupCookieJarSQLite with the contents of the txt file */
    GSList *cookies, *p;
    SoupCookieJar *txt, *sqlite;

    txt = soup_cookie_jar_text_new (src_txt, TRUE);
    sqlite = soup_cookie_jar_sqlite_new (dest, FALSE);
    cookies = soup_cookie_jar_all_cookies (txt);

    for (p = cookies; p; p = p->next) {
      SoupCookie *cookie = (SoupCookie*)p->data;
      /* Cookie is stolen, so we won't free it */
      soup_cookie_jar_add_cookie (sqlite, cookie);
    }

    g_slist_free (cookies);
    g_object_unref (txt);
    g_object_unref (sqlite);
  }
  
 out:
  g_free (src_sqlite);
  g_free (src_txt);
  g_free (dest);
}

const EphyProfileMigrator migrators[] = {
  migrate_cookies
};

#define PROFILE_MIGRATION_FILE ".migrated"

void
_ephy_profile_migrate ()
{
  int latest, i;
  char *migrated_file, *contents;

  /* Figure out the latest migration that occured */
  migrated_file = g_build_filename (ephy_dot_dir (),
                                    PROFILE_MIGRATION_FILE,
                                    NULL);
  if (g_file_test (migrated_file, G_FILE_TEST_EXISTS)) {
    gsize size;
    int result;

    g_file_get_contents (migrated_file, &contents, &size, NULL);
    result = sscanf(contents, "%d", &latest);
    g_free (contents);

    if (result != 1) {
      g_warning (_("Failed to read latest migration marker, aborting profile migration."));
      return;
    }
  } else
    /* Never migrated */
    latest = 0;
  
  for (i = latest; i < PROFILE_MIGRATION_VERSION; i++) {
    EphyProfileMigrator m = migrators[i];
    m();
  }

  /* Write down the latest migration */
  contents = g_strdup_printf ("%d", PROFILE_MIGRATION_VERSION);
  g_file_set_contents (migrated_file, contents, -1, NULL);
  g_free (contents);
  g_free (migrated_file);
}

