/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2008  Red Hat, Inc,
 *           2007  William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by : William Jon McCann <mccann@jhu.edu>
 *              Ray Strode <rstrode@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <locale.h>
#include <langinfo.h>
#include <sys/stat.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "gnome-languages.h"

#include <langinfo.h>
#ifndef __LC_LAST
#define __LC_LAST       13
#endif

#define ISO_CODES_DATADIR ISO_CODES_PREFIX "/share/xml/iso-codes"
#define ISO_CODES_LOCALESDIR ISO_CODES_PREFIX "/share/locale"

#if 0
#include "default-input-sources.h"
#endif

typedef struct _GnomeLocale {
        char *id;
        char *name;
        char *language_code;
        char *territory_code;
        char *codeset;
        char *modifier;
} GnomeLocale;

static GHashTable *gnome_languages_map;
static GHashTable *gnome_territories_map;
static GHashTable *gnome_available_locales_map;
static GHashTable *gnome_language_count_map;
static GHashTable *gnome_territory_count_map;

static char * construct_language_name (const char *language,
                                       const char *territory,
                                       const char *codeset,
                                       const char *modifier);

static gboolean language_name_is_valid (const char *language_name);

static void
gnome_locale_free (GnomeLocale *locale)
{
        if (locale == NULL) {
                return;
        }

        g_free (locale->id);
        g_free (locale->name);
        g_free (locale->codeset);
        g_free (locale->modifier);
        g_free (locale->language_code);
        g_free (locale->territory_code);
        g_free (locale);
}

static char *
normalize_codeset (const char *codeset)
{
        if (codeset == NULL)
                return NULL;

        if (g_str_equal (codeset, "UTF-8") ||
            g_str_equal (codeset, "utf8"))
                return g_strdup ("UTF-8");

        return g_strdup (codeset);
}

/**
 * gnome_parse_locale:
 * @locale: a locale string
 * @language_codep: (out) (allow-none) (transfer full): location to
 * store the language code, or %NULL
 * @country_codep: (out) (allow-none) (transfer full): location to
 * store the country code, or %NULL
 * @codesetp: (out) (allow-none) (transfer full): location to
 * store the codeset, or %NULL
 * @modifierp: (out) (allow-none) (transfer full): location to
 * store the modifier, or %NULL
 *
 * Extracts the various components of a locale string of the form
 * [language[_country][.codeset][@modifier]]. See
 * http://en.wikipedia.org/wiki/Locale.
 *
 * Return value: %TRUE if parsing was successful.
 *
 * Since: 3.8
 */
gboolean
gnome_parse_locale (const char *locale,
                    char      **language_codep,
                    char      **country_codep,
                    char      **codesetp,
                    char      **modifierp)
{
        static GRegex *re = NULL;
        GMatchInfo *match_info;
        gboolean    res;
        gboolean    retval;

        match_info = NULL;
        retval = FALSE;

        if (re == NULL) {
                GError *error = NULL;
                re = g_regex_new ("^(?P<language>[^_.@[:space:]]+)"
                                  "(_(?P<territory>[[:upper:]]+))?"
                                  "(\\.(?P<codeset>[-_0-9a-zA-Z]+))?"
                                  "(@(?P<modifier>[[:ascii:]]+))?$",
                                  0, 0, &error);
                if (re == NULL) {
                        g_warning ("%s", error->message);
                        g_error_free (error);
                        goto out;
                }
        }

        if (!g_regex_match (re, locale, 0, &match_info) ||
            g_match_info_is_partial_match (match_info)) {
                g_warning ("locale '%s' isn't valid", locale);
                goto out;
        }

        res = g_match_info_matches (match_info);
        if (! res) {
                g_warning ("Unable to parse locale: %s", locale);
                goto out;
        }

        retval = TRUE;

        if (language_codep != NULL) {
                *language_codep = g_match_info_fetch_named (match_info, "language");
        }

        if (country_codep != NULL) {
                *country_codep = g_match_info_fetch_named (match_info, "territory");

                if (*country_codep != NULL &&
                    *country_codep[0] == '\0') {
                        g_free (*country_codep);
                        *country_codep = NULL;
                }
        }

        if (codesetp != NULL) {
                *codesetp = g_match_info_fetch_named (match_info, "codeset");

                if (*codesetp != NULL &&
                    *codesetp[0] == '\0') {
                        g_free (*codesetp);
                        *codesetp = NULL;
                }
        }

        if (modifierp != NULL) {
                *modifierp = g_match_info_fetch_named (match_info, "modifier");

                if (*modifierp != NULL &&
                    *modifierp[0] == '\0') {
                        g_free (*modifierp);
                        *modifierp = NULL;
                }
        }

        if (codesetp != NULL && *codesetp != NULL) {
                g_autofree gchar *normalized_codeset = NULL;
                g_autofree gchar *normalized_name = NULL;

                normalized_codeset = normalize_codeset (*codesetp);
                normalized_name = construct_language_name (language_codep ? *language_codep : NULL,
                                                           country_codep ? *country_codep : NULL,
                                                           normalized_codeset,
                                                           modifierp ? *modifierp : NULL);

                if (language_name_is_valid (normalized_name)) {
                        g_free (*codesetp);
                        *codesetp = g_steal_pointer (&normalized_codeset);
                }
        }

 out:
        g_match_info_free (match_info);

        return retval;
}

static char *
construct_language_name (const char *language,
                         const char *territory,
                         const char *codeset,
                         const char *modifier)
{
        char *name;

        g_assert (language != NULL && language[0] != 0);
        g_assert (territory == NULL || territory[0] != 0);
        g_assert (codeset == NULL || codeset[0] != 0);
        g_assert (modifier == NULL || modifier[0] != 0);

        name = g_strdup_printf ("%s%s%s%s%s%s%s",
                                language,
                                territory != NULL? "_" : "",
                                territory != NULL? territory : "",
                                codeset != NULL? "." : "",
                                codeset != NULL? codeset : "",
                                modifier != NULL? "@" : "",
                                modifier != NULL? modifier : "");

        return name;
}

/**
 * gnome_normalize_locale:
 * @locale: a locale string
 *
 * Gets the normalized locale string in the form
 * [language[_country][.codeset][@modifier]] for @name.
 *
 * Return value: (transfer full): normalized locale string. Caller
 * takes ownership.
 *
 * Since: 3.8
 */
char *
gnome_normalize_locale (const char *locale)
{
        char *normalized_name;
        gboolean valid;
        g_autofree char *language_code = NULL;
        g_autofree char *territory_code = NULL;
        g_autofree char *codeset = NULL;
        g_autofree char *modifier = NULL;

        if (locale[0] == '\0') {
                return NULL;
        }

        valid = gnome_parse_locale (locale,
                                    &language_code,
                                    &territory_code,
                                    &codeset, &modifier);
        if (!valid)
                return NULL;

        normalized_name = construct_language_name (language_code,
                                                   territory_code,
                                                   codeset, modifier);
        return normalized_name;
}

static gboolean
language_name_is_valid (const char *language_name)
{
        locale_t locale;

        locale = newlocale (LC_MESSAGES_MASK, language_name, (locale_t) 0);
        if (locale != (locale_t) 0) {
                freelocale (locale);
                return TRUE;
        }

        return FALSE;
}

static void
language_name_get_codeset_details (const char  *language_name,
                                   char       **pcodeset,
                                   gboolean    *is_utf8)
{
        locale_t locale;
        locale_t old_locale;
        const char *codeset = NULL;

        locale = newlocale (LC_CTYPE_MASK, language_name, (locale_t) 0);
        if (locale == (locale_t) 0)
                return;

        old_locale = uselocale (locale);

        codeset = nl_langinfo (CODESET);

        if (pcodeset != NULL) {
                *pcodeset = g_strdup (codeset);
        }

        if (is_utf8 != NULL) {
                g_autofree char *normalized_codeset = normalize_codeset (codeset);

                *is_utf8 = strcmp (normalized_codeset, "UTF-8") == 0;
        }

        uselocale (old_locale);
        freelocale (locale);
}

/**
 * gnome_language_has_translations:
 * @code: an ISO 639 code string
 *
 * Returns %TRUE if there are translations for language @code.
 *
 * Return value: %TRUE if there are translations for language @code.
 *
 * Since: 3.8
 */
gboolean
gnome_language_has_translations (const char *code)
{
        GDir        *dir;
        const char  *name;
        gboolean     has_translations;
        g_autofree char *path = NULL;

        path = g_build_filename (LOCALEDIR, code, "LC_MESSAGES", NULL);

        has_translations = FALSE;
        dir = g_dir_open (path, 0, NULL);

        if (dir == NULL) {
                goto out;
        }

        do {
                name = g_dir_read_name (dir);

                if (name == NULL) {
                        break;
                }

                if (g_str_has_suffix (name, ".mo")) {
                        has_translations = TRUE;
                        break;
                }
        } while (name != NULL);
        g_dir_close (dir);

 out:
        return has_translations;
}

static gboolean
add_locale (const char *language_name,
            gboolean    utf8_only)
{
        GnomeLocale *locale;
        GnomeLocale *old_locale;
        g_autofree char *name = NULL;
        gboolean   is_utf8 = FALSE;
        gboolean   valid = FALSE;

        g_return_val_if_fail (language_name != NULL, FALSE);
        g_return_val_if_fail (*language_name != '\0', FALSE);

        language_name_get_codeset_details (language_name, NULL, &is_utf8);

        if (is_utf8) {
                name = g_strdup (language_name);
        } else if (utf8_only) {

                if (strchr (language_name, '.'))
                        return FALSE;

                /* If the locale name has no dot, assume that its
                 * encoding part is missing and try again after adding
                 * ".UTF-8". This catches locale names like "de_DE".
                 */
                name = g_strdup_printf ("%s.UTF-8", language_name);

                language_name_get_codeset_details (name, NULL, &is_utf8);
                if (!is_utf8)
                        return FALSE;
        } else {
                name = g_strdup (language_name);
        }

        if (!language_name_is_valid (name)) {
                g_debug ("Ignoring '%s' as a locale, since it's invalid", name);
                return FALSE;
        }

        locale = g_new0 (GnomeLocale, 1);
        valid = gnome_parse_locale (name,
                                    &locale->language_code,
                                    &locale->territory_code,
                                    &locale->codeset,
                                    &locale->modifier);
        if (!valid) {
                gnome_locale_free (locale);
                return FALSE;
        }

        locale->id = construct_language_name (locale->language_code, locale->territory_code,
                                              NULL, locale->modifier);
        locale->name = construct_language_name (locale->language_code, locale->territory_code,
                                                locale->codeset, locale->modifier);

        if (!gnome_language_has_translations (locale->name) &&
            !gnome_language_has_translations (locale->id) &&
            !gnome_language_has_translations (locale->language_code) &&
            utf8_only) {
                g_debug ("Ignoring '%s' as a locale, since it lacks translations", locale->name);
                gnome_locale_free (locale);
                return FALSE;
        }

        if (!utf8_only) {
                g_free (locale->id);
                locale->id = g_strdup (locale->name);
        }

        old_locale = g_hash_table_lookup (gnome_available_locales_map, locale->id);
        if (old_locale != NULL) {
                if (strlen (old_locale->name) > strlen (locale->name)) {
                        gnome_locale_free (locale);
                        return FALSE;
                }
        }

        g_hash_table_insert (gnome_available_locales_map, g_strdup (locale->id), locale);

        return TRUE;
}

static int
select_dirs (const struct dirent *dirent)
{
        int result = 0;

        if (strcmp (dirent->d_name, ".") != 0 && strcmp (dirent->d_name, "..") != 0) {
                mode_t mode = 0;

#ifdef _DIRENT_HAVE_D_TYPE
                if (dirent->d_type != DT_UNKNOWN && dirent->d_type != DT_LNK) {
                        mode = DTTOIF (dirent->d_type);
                } else
#endif
                        {
                                struct stat st;
                                g_autofree char *path = NULL;

                                path = g_build_filename (LIBLOCALEDIR, dirent->d_name, NULL);
                                if (g_stat (path, &st) == 0) {
                                        mode = st.st_mode;
                                }
                        }

                result = S_ISDIR (mode);
        }

        return result;
}

static gboolean
collect_locales_from_directory (void)
{
        gboolean found_locales = FALSE;
        struct dirent **dirents;
        int             ndirents;
        int             cnt;

        ndirents = scandir (LIBLOCALEDIR, &dirents, select_dirs, alphasort);

        for (cnt = 0; cnt < ndirents; ++cnt) {
                if (add_locale (dirents[cnt]->d_name, TRUE))
                        found_locales = TRUE;
        }

        if (ndirents > 0) {
                free (dirents);
        }
        return found_locales;
}

static gboolean
collect_locales_from_localebin (void)
{
        gboolean found_locales = FALSE;
        const gchar *argv[] = { "locale", "-a", NULL };
        gchar **linep;
        g_auto (GStrv) lines = NULL;
        g_autofree gchar *output = NULL;

        if (g_spawn_sync (NULL, (gchar **) argv, NULL,
                          G_SPAWN_SEARCH_PATH|G_SPAWN_STDERR_TO_DEV_NULL,
                          NULL, NULL, &output, NULL, NULL, NULL) == FALSE)
                return FALSE;

        g_return_val_if_fail (output != NULL, FALSE);

        lines = g_strsplit (output, "\n", 0);
        if (lines) {
                linep = lines;
                while (*linep) {
                        if (*linep[0] && add_locale (*linep, TRUE))
                                found_locales = TRUE;
                        linep++;
                }
        }

        return found_locales;
}

static void
count_languages_and_territories (void)
{
	gpointer value;
	GHashTableIter iter;

	gnome_language_count_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	gnome_territory_count_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

        g_hash_table_iter_init (&iter, gnome_available_locales_map);
        while (g_hash_table_iter_next (&iter, NULL, &value)) {
                GnomeLocale *locale;

                locale = (GnomeLocale *) value;

		if (locale->language_code != NULL) {
			int count;

			count = GPOINTER_TO_INT (g_hash_table_lookup (gnome_language_count_map, locale->language_code));
			count++;
			g_hash_table_insert (gnome_language_count_map, g_strdup (locale->language_code), GINT_TO_POINTER (count));
		}

		if (locale->territory_code != NULL) {
			int count;

			count = GPOINTER_TO_INT (g_hash_table_lookup (gnome_territory_count_map, locale->territory_code));
			count++;
			g_hash_table_insert (gnome_territory_count_map, g_strdup (locale->territory_code), GINT_TO_POINTER (count));
		}
        }
}

static void
collect_locales (void)
{
        gboolean found_localebin_locales = FALSE;
        gboolean found_dir_locales = FALSE;

        if (gnome_available_locales_map == NULL) {
                gnome_available_locales_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) gnome_locale_free);
        }

        found_localebin_locales = collect_locales_from_localebin ();

        found_dir_locales = collect_locales_from_directory ();

        if (!(found_localebin_locales || found_dir_locales)) {
                g_warning ("Could not read list of available locales from libc, "
                           "guessing possible locales from available translations, "
                           "but list may be incomplete!");
        }

	count_languages_and_territories ();
}

static gint
get_language_count (const char *language)
{
        if (gnome_language_count_map == NULL) {
                collect_locales ();
        }

	return GPOINTER_TO_INT (g_hash_table_lookup (gnome_language_count_map, language));
}

static gboolean
is_unique_language (const char *language)
{
        return get_language_count (language) == 1;
}

static gint
get_territory_count (const char *territory)
{
        if (gnome_territory_count_map == NULL) {
                collect_locales ();
        }

	return GPOINTER_TO_INT (g_hash_table_lookup (gnome_territory_count_map, territory));
}

static gboolean
is_unique_territory (const char *territory)
{
        return get_territory_count (territory) == 1;
}

static gboolean
is_fallback_language (const char *code)
{
        const char *fallback_language_names[] = { "C", "POSIX", NULL };
        int i;

        for (i = 0; fallback_language_names[i] != NULL; i++) {
                if (strcmp (code, fallback_language_names[i]) == 0) {
                        return TRUE;
                }
        }

        return FALSE;
}

static const char *
get_language (const char *code)
{
        const char *name;
        int         len;

        g_assert (code != NULL);

        if (is_fallback_language (code)) {
                return "Unspecified";
        }

        len = strlen (code);
        if (len != 2 && len != 3) {
                return NULL;
        }

        name = (const char *) g_hash_table_lookup (gnome_languages_map, code);

        return name;
}

static char *
get_first_item_in_semicolon_list (const char *list)
{
        char **items;
        char  *item;

        /* Some entries in iso codes have multiple values, separated
         * by semicolons.  Not really sure which one to pick, so
         * we just arbitrarily pick the first one.
         */
        items = g_strsplit (list, "; ", 2);

        item = g_strdup (items[0]);
        g_strfreev (items);

        return item;
}

static char *
capitalize_utf8_string (const char *str)
{
        char first[8] = { 0 };

        if (!str)
                return NULL;

        g_unichar_to_utf8 (g_unichar_totitle (g_utf8_get_char (str)), first);

        return g_strconcat (first, g_utf8_offset_to_pointer (str, 1), NULL);
}

static char *
get_translated_language (const char *code,
                         const char *locale)
{
        const char *language;
        char *name;

        language = get_language (code);

        name = NULL;
        if (language != NULL) {
                const char *translated_name;
                locale_t loc = (locale_t) 0;
                locale_t old_locale = (locale_t) 0;

                if (locale != NULL) {
                        loc = newlocale (LC_MESSAGES_MASK, locale, (locale_t) 0);
                        if (loc == (locale_t) 0)
                                return NULL;
                        old_locale = uselocale (loc);
                }

                if (is_fallback_language (code)) {
                        name = g_strdup (_("Unspecified"));
                } else {
                        g_autofree char *tmp = NULL;
                        translated_name = dgettext ("iso_639", language);
                        tmp = get_first_item_in_semicolon_list (translated_name);
                        name = capitalize_utf8_string (tmp);
                }

                if (locale != NULL) {
                        uselocale (old_locale);
                        freelocale (loc);
                }
        }

        return name;
}

static const char *
get_territory (const char *code)
{
        const char *name;
        int         len;

        g_assert (code != NULL);

        len = strlen (code);
        if (len != 2 && len != 3) {
                return NULL;
        }

        name = (const char *) g_hash_table_lookup (gnome_territories_map, code);

        return name;
}

static char *
get_translated_territory (const char *code,
                          const char *locale)
{
        const char *territory;
        char       *name;

        territory = get_territory (code);

        name = NULL;
        if (territory != NULL) {
                const char *translated_territory;
                locale_t loc = NULL;
                locale_t old_locale = NULL;
                g_autofree char *tmp = NULL;

                if (locale != NULL) {
                        loc = newlocale (LC_MESSAGES_MASK, locale, (locale_t) 0);
                        if (loc == (locale_t) 0)
                                return NULL;
                        old_locale = uselocale (loc);
                }

                translated_territory = dgettext ("iso_3166", territory);
                tmp = get_first_item_in_semicolon_list (translated_territory);
                name = capitalize_utf8_string (tmp);

                if (locale != NULL) {
                        uselocale (old_locale);
                        freelocale (loc);
                }
        }

        return name;
}

static void
languages_parse_start_tag (GMarkupParseContext      *ctx,
                           const char               *element_name,
                           const char              **attr_names,
                           const char              **attr_values,
                           gpointer                  user_data,
                           GError                  **error)
{
        const char *ccode_longB;
        const char *ccode_longT;
        const char *ccode;
        const char *ccode_id;
        const char *lang_name;

        if (! (g_str_equal (element_name, "iso_639_entry") || g_str_equal (element_name, "iso_639_3_entry"))
            || attr_names == NULL || attr_values == NULL) {
                return;
        }

        ccode = NULL;
        ccode_longB = NULL;
        ccode_longT = NULL;
        ccode_id = NULL;
        lang_name = NULL;

        while (*attr_names && *attr_values) {
                if (g_str_equal (*attr_names, "iso_639_1_code")) {
                        /* skip if empty */
                        if (**attr_values) {
                                if (strlen (*attr_values) != 2) {
                                        return;
                                }
                                ccode = *attr_values;
                        }
                } else if (g_str_equal (*attr_names, "iso_639_2B_code")) {
                        /* skip if empty */
                        if (**attr_values) {
                                if (strlen (*attr_values) != 3) {
                                        return;
                                }
                                ccode_longB = *attr_values;
                        }
                } else if (g_str_equal (*attr_names, "iso_639_2T_code")) {
                        /* skip if empty */
                        if (**attr_values) {
                                if (strlen (*attr_values) != 3) {
                                        return;
                                }
                                ccode_longT = *attr_values;
                        }
                } else if (g_str_equal (*attr_names, "id")) {
                        /* skip if empty */
                        if (**attr_values) {
                                if (strlen (*attr_values) != 2 &&
                                    strlen (*attr_values) != 3) {
                                        return;
                                }
                                ccode_id = *attr_values;
                        }
                } else if (g_str_equal (*attr_names, "name")) {
                        lang_name = *attr_values;
                }

                ++attr_names;
                ++attr_values;
        }

        if (lang_name == NULL) {
                return;
        }

        if (ccode != NULL) {
                g_hash_table_insert (gnome_languages_map,
                                     g_strdup (ccode),
                                     g_strdup (lang_name));
        }
        if (ccode_longB != NULL) {
                g_hash_table_insert (gnome_languages_map,
                                     g_strdup (ccode_longB),
                                     g_strdup (lang_name));
        }
        if (ccode_longT != NULL) {
                g_hash_table_insert (gnome_languages_map,
                                     g_strdup (ccode_longT),
                                     g_strdup (lang_name));
        }
        if (ccode_id != NULL) {
                g_hash_table_insert (gnome_languages_map,
                                     g_strdup (ccode_id),
                                     g_strdup (lang_name));
        }
}

static void
territories_parse_start_tag (GMarkupParseContext      *ctx,
                             const char               *element_name,
                             const char              **attr_names,
                             const char              **attr_values,
                             gpointer                  user_data,
                             GError                  **error)
{
        const char *acode_2;
        const char *acode_3;
        const char *ncode;
        const char *territory_common_name;
        const char *territory_name;

        if (! g_str_equal (element_name, "iso_3166_entry") || attr_names == NULL || attr_values == NULL) {
                return;
        }

        acode_2 = NULL;
        acode_3 = NULL;
        ncode = NULL;
        territory_common_name = NULL;
        territory_name = NULL;

        while (*attr_names && *attr_values) {
                if (g_str_equal (*attr_names, "alpha_2_code")) {
                        /* skip if empty */
                        if (**attr_values) {
                                if (strlen (*attr_values) != 2) {
                                        return;
                                }
                                acode_2 = *attr_values;
                        }
                } else if (g_str_equal (*attr_names, "alpha_3_code")) {
                        /* skip if empty */
                        if (**attr_values) {
                                if (strlen (*attr_values) != 3) {
                                        return;
                                }
                                acode_3 = *attr_values;
                        }
                } else if (g_str_equal (*attr_names, "numeric_code")) {
                        /* skip if empty */
                        if (**attr_values) {
                                if (strlen (*attr_values) != 3) {
                                        return;
                                }
                                ncode = *attr_values;
                        }
                } else if (g_str_equal (*attr_names, "common_name")) {
                        /* skip if empty */
                        if (**attr_values) {
                                territory_common_name = *attr_values;
                        }
                } else if (g_str_equal (*attr_names, "name")) {
                        territory_name = *attr_values;
                }

                ++attr_names;
                ++attr_values;
        }

        if (territory_common_name != NULL) {
                territory_name = territory_common_name;
        }

        if (territory_name == NULL) {
                return;
        }

        if (acode_2 != NULL) {
                g_hash_table_insert (gnome_territories_map,
                                     g_strdup (acode_2),
                                     g_strdup (territory_name));
        }
        if (acode_3 != NULL) {
                g_hash_table_insert (gnome_territories_map,
                                     g_strdup (acode_3),
                                     g_strdup (territory_name));
        }
        if (ncode != NULL) {
                g_hash_table_insert (gnome_territories_map,
                                     g_strdup (ncode),
                                     g_strdup (territory_name));
        }
}

static void
languages_variant_init (const char *variant)
{
        gboolean res;
        gsize    buf_len;
        g_autofree char *buf = NULL;
        g_autofree char *filename = NULL;
        g_autoptr (GError) error = NULL;

        bindtextdomain (variant, ISO_CODES_LOCALESDIR);
        bind_textdomain_codeset (variant, "UTF-8");

        error = NULL;
        filename = g_strdup_printf (ISO_CODES_DATADIR "/%s.xml", variant);
        res = g_file_get_contents (filename,
                                   &buf,
                                   &buf_len,
                                   &error);
        if (res) {
                g_autoptr (GMarkupParseContext) ctx = NULL;
                GMarkupParser        parser = { languages_parse_start_tag, NULL, NULL, NULL, NULL };

                ctx = g_markup_parse_context_new (&parser, 0, NULL, NULL);

                error = NULL;
                res = g_markup_parse_context_parse (ctx, buf, buf_len, &error);

                if (! res) {
                        g_warning ("Failed to parse '%s': %s",
                                   filename,
                                   error->message);
                }
        } else {
                g_warning ("Failed to load '%s': %s",
                           filename,
                           error->message);
        }
}

static void
languages_init (void)
{
        if (gnome_languages_map)
                return;

        gnome_languages_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

        languages_variant_init ("iso_639");
        languages_variant_init ("iso_639_3");
}

static void
territories_init (void)
{
        gboolean res;
        gsize    buf_len;
        g_autofree char *buf = NULL;
        g_autoptr (GError) error = NULL;

        if (gnome_territories_map)
                return;

        bindtextdomain ("iso_3166", ISO_CODES_LOCALESDIR);
        bind_textdomain_codeset ("iso_3166", "UTF-8");

        gnome_territories_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

        error = NULL;
        res = g_file_get_contents (ISO_CODES_DATADIR "/iso_3166.xml",
                                   &buf,
                                   &buf_len,
                                   &error);
        if (res) {
                g_autoptr (GMarkupParseContext) ctx = NULL;
                GMarkupParser        parser = { territories_parse_start_tag, NULL, NULL, NULL, NULL };

                ctx = g_markup_parse_context_new (&parser, 0, NULL, NULL);

                error = NULL;
                res = g_markup_parse_context_parse (ctx, buf, buf_len, &error);

                if (! res) {
                        g_warning ("Failed to parse '%s': %s",
                                   ISO_CODES_DATADIR "/iso_3166.xml",
                                   error->message);
                }
        } else {
                g_warning ("Failed to load '%s': %s",
                           ISO_CODES_DATADIR "/iso_3166.xml",
                           error->message);
        }
}

/**
 * gnome_get_language_from_locale:
 * @locale: a locale string
 * @translation: (allow-none): a locale string
 *
 * Gets the language description for @locale. If @translation is
 * provided the returned string is translated accordingly.
 *
 * Return value: (transfer full): the language description. Caller
 * takes ownership.
 *
 * Since: 3.8
 */
char *
gnome_get_language_from_locale (const char *locale,
                                const char *translation)
{
        GString *full_language;
        g_autofree char *language_code = NULL;
        g_autofree char *territory_code = NULL;
        g_autofree char *codeset_code = NULL;
        g_autofree char *langinfo_codeset = NULL;
        g_autofree char *translated_language = NULL;
        g_autofree char *translated_territory = NULL;
        gboolean is_utf8 = TRUE;

        g_return_val_if_fail (locale != NULL, NULL);
        g_return_val_if_fail (*locale != '\0', NULL);

        full_language = g_string_new (NULL);

        languages_init ();
        territories_init ();

        gnome_parse_locale (locale,
                            &language_code,
                            &territory_code,
                            &codeset_code,
                            NULL);

        if (language_code == NULL) {
                goto out;
        }

        translated_language = get_translated_language (language_code, translation);
        if (translated_language == NULL) {
                goto out;
        }

        full_language = g_string_append (full_language, translated_language);

	if (is_unique_language (language_code)) {
		goto out;
	}

        if (territory_code != NULL) {
                translated_territory = get_translated_territory (territory_code, translation);
        }
        if (translated_territory != NULL) {
                g_string_append_printf (full_language,
                                        " (%s)",
                                        translated_territory);
        }

        language_name_get_codeset_details (locale, &langinfo_codeset, &is_utf8);

        if (codeset_code == NULL && langinfo_codeset != NULL) {
                codeset_code = g_strdup (langinfo_codeset);
        }

        if (!is_utf8 && codeset_code) {
                g_string_append_printf (full_language,
                                        " [%s]",
                                        codeset_code);
        }

 out:
        if (full_language->len == 0) {
                g_string_free (full_language, TRUE);
                return NULL;
        }

        return g_string_free (full_language, FALSE);
}

/**
 * gnome_get_country_from_locale:
 * @locale: a locale string
 * @translation: (allow-none): a locale string
 *
 * Gets the country description for @locale. If @translation is
 * provided the returned string is translated accordingly.
 *
 * Return value: (transfer full): the country description. Caller
 * takes ownership.
 *
 * Since: 3.8
 */
char *
gnome_get_country_from_locale (const char *locale,
                               const char *translation)
{
        GString *full_name;
        g_autofree char *language_code = NULL;
        g_autofree char *territory_code = NULL;
        g_autofree char *codeset_code = NULL;
        g_autofree char *langinfo_codeset = NULL;
        g_autofree char *translated_language = NULL;
        g_autofree char *translated_territory = NULL;
        gboolean is_utf8 = TRUE;

        g_return_val_if_fail (locale != NULL, NULL);
        g_return_val_if_fail (*locale != '\0', NULL);

        full_name = g_string_new (NULL);

        languages_init ();
        territories_init ();

        gnome_parse_locale (locale,
                            &language_code,
                            &territory_code,
                            &codeset_code,
                            NULL);

        if (territory_code == NULL) {
                goto out;
        }

        translated_territory = get_translated_territory (territory_code, translation);
        g_string_append (full_name, translated_territory);

	if (is_unique_territory (territory_code)) {
		goto out;
	}

        if (language_code != NULL) {
                translated_language = get_translated_language (language_code, translation);
        }
        if (translated_language != NULL) {
                g_string_append_printf (full_name,
                                        " (%s)",
                                        translated_language);
        }

        language_name_get_codeset_details (translation, &langinfo_codeset, &is_utf8);

        if (codeset_code == NULL && langinfo_codeset != NULL) {
                codeset_code = g_strdup (langinfo_codeset);
        }

        if (!is_utf8 && codeset_code) {
                g_string_append_printf (full_name,
                                        " [%s]",
                                        codeset_code);
        }

 out:
        if (full_name->len == 0) {
                g_string_free (full_name, TRUE);
                return NULL;
        }

        return g_string_free (full_name, FALSE);
}

/**
 * gnome_get_all_locales:
 *
 * Gets all locales.
 *
 * Return value: (array zero-terminated=1) (element-type utf8) (transfer full):
 *   a newly allocated %NULL-terminated string array containing the
 *   all locales. Free with g_strfreev().
 *
 * Since: 3.8
 */
char **
gnome_get_all_locales (void)
{
        GHashTableIter iter;
        gpointer key, value;
        GPtrArray *array;

        if (gnome_available_locales_map == NULL) {
                collect_locales ();
        }

        array = g_ptr_array_new ();
        g_hash_table_iter_init (&iter, gnome_available_locales_map);
        while (g_hash_table_iter_next (&iter, &key, &value)) {
                GnomeLocale *locale;

                locale = (GnomeLocale *) value;

                g_ptr_array_add (array, g_strdup (locale->name));
        }
        g_ptr_array_add (array, NULL);

        return (char **) g_ptr_array_free (array, FALSE);
}

/**
 * gnome_get_language_from_code:
 * @code: an ISO 639 code string
 * @translation: (allow-none): a locale string
 *
 * Gets the language name for @code. If @translation is provided the
 * returned string is translated accordingly.
 *
 * Return value: (transfer full): the language name. Caller takes
 * ownership.
 *
 * Since: 3.8
 */
char *
gnome_get_language_from_code (const char *code,
                              const char *translation)
{
        g_return_val_if_fail (code != NULL, NULL);

        languages_init ();

        return get_translated_language (code, translation);
}

/**
 * gnome_get_country_from_code:
 * @code: an ISO 3166 code string
 * @translation: (allow-none): a locale string
 *
 * Gets the country name for @code. If @translation is provided the
 * returned string is translated accordingly.
 *
 * Return value: (transfer full): the country name. Caller takes
 * ownership.
 *
 * Since: 3.8
 */
char *
gnome_get_country_from_code (const char *code,
                             const char *translation)
{
        g_return_val_if_fail (code != NULL, NULL);

        territories_init ();

        return get_translated_territory (code, translation);
}

#if 0
/**
 * gnome_get_input_source_from_locale:
 * @locale: a locale string
 * @type: (out) (transfer none): location to store the input source
 * type
 * @id: (out) (transfer none): location to store the input source
 * identifier
 *
 * Gets the default input source's type and identifier for a given
 * locale.
 *
 * Return value: %TRUE if a input source exists or %FALSE otherwise.
 *
 * Since: 3.8
 */
gboolean
gnome_get_input_source_from_locale (const char  *locale,
                                    const char **type,
                                    const char **id)
{
        static GHashTable *table = NULL;
        DefaultInputSource *dis;
        g_autofree gchar *l_code = NULL;
        g_autofree gchar *c_code = NULL;
        g_autofree gchar *key = NULL;
        gint i;

        g_return_val_if_fail (locale != NULL, FALSE);
        g_return_val_if_fail (type != NULL, FALSE);
        g_return_val_if_fail (id != NULL, FALSE);

        if (!table) {
                table = g_hash_table_new (g_str_hash, g_str_equal);
                for (i = 0; default_input_sources[i].id; ++i) {
                        dis = &default_input_sources[i];
                        g_hash_table_insert (table, (gpointer) dis->locale, dis);
                }
        }

        if (!gnome_parse_locale (locale, &l_code, &c_code, NULL, NULL))
                return FALSE;

        key = g_strconcat (l_code, "_", c_code, NULL);

        dis = g_hash_table_lookup (table, key);
        if (dis) {
                *type = dis->type;
                *id = dis->id;
        }
        return dis != NULL;
}
#endif
