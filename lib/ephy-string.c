/*
 *  Copyright © 2002 Marco Pesenti Gritti
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

#include "ephy-string.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <sys/types.h>
#include <pwd.h>

#define ELLIPSIS "\xe2\x80\xa6"

gboolean
ephy_string_to_int (const char *string, gulong *integer)
{
	gulong result;
	char *parse_end;

	/* Check for the case of an empty string. */
	if (string == NULL || *string == '\0')
	{
		return FALSE;
	}

	/* Call the standard library routine to do the conversion. */
	errno = 0;
	result = strtol (string, &parse_end, 0);

	/* Check that the result is in range. */
	if ((result == G_MINLONG || result == G_MAXLONG) && errno == ERANGE)
	{
		return FALSE;
	}

	/* Check that all the trailing characters are spaces. */
	while (*parse_end != '\0')
	{
		if (!g_ascii_isspace (*parse_end++))
		{
			return FALSE;
		}
	}

	/* Return the result. */
	*integer = result;
	return TRUE;
}

char *
ephy_string_blank_chr (char *source)
{
	char *p;

        if (source == NULL)
	{
		return NULL;
	}

	p = source;
	while (*p != '\0')
	{
		if ((guchar) *p < 0x20)
		{
			*p = ' ';
		}
		p++;
	}

        return source;
}

/**
 * ephy_string_shorten: shortens a string
 * @str: the string to shorten, in UTF-8
 * @target_length: the length of the shortened string (in characters)
 * 
 * If @str is already short enough, it is returned. Otherwise a new string
 * is allocated and @str is consumed.
 * 
 * Return value: a newly allocated string, not longer than target_length
 * characters.
 */
char *
ephy_string_shorten (char *str,
		     gsize target_length)
{
        char *new_str;
        glong actual_length;
        gulong bytes;

	g_return_val_if_fail (target_length > 0, NULL);

        if (!str) return NULL;

 	/* FIXME: this function is a big mess. While it is utf-8 safe now,
	 * it can still split a sequence of combining characters.
	 */

        actual_length = g_utf8_strlen (str, -1);

        /* if the string is already short enough, or if it's too short for
         * us to shorten it, return a new copy */
        if (actual_length <= target_length) return str;

        /* create string */
        bytes = GPOINTER_TO_UINT (g_utf8_offset_to_pointer (str, target_length - 1) - str);

        new_str = g_new (gchar, bytes + strlen(ELLIPSIS) + 1);

        strncpy (new_str, str, bytes);
        strncpy (new_str + bytes, ELLIPSIS, strlen (ELLIPSIS));
	new_str[bytes + strlen (ELLIPSIS)] = '\0';

	g_free (str);

        return new_str;
}

/* This is a collation key that is very very likely to sort before any
   collation key that libc strxfrm generates. We use this before any
   special case (dot or number) to make sure that its sorted before
   anything else.
 */
#define COLLATION_SENTINEL "\1\1\1"

/**
 * ephy_string_collate_key_for_domain:
 * @host:
 * @len: the length of @host, or -1 to use the entire null-terminated @host string
 * 
 * Return value: a collation key for @host.
 */
char*
ephy_string_collate_key_for_domain (const char *str,
				    gssize len)
{
	GString *result;
	const char *dot;
	gssize newlen;

	if (len < 0) len = strlen (str);

	result = g_string_sized_new (len + 6 * strlen (COLLATION_SENTINEL));

	/* Note that we could do even better by using
	 * g_utf8_collate_key_for_filename on the dot-separated
	 * components, but this seems good enough for now.
	 */
	while ((dot = g_strrstr_len (str, len, ".")) != NULL)
	{
		newlen = dot - str;

		g_string_append_len (result, dot + 1, len - newlen - 1);
		g_string_append (result, COLLATION_SENTINEL);

		len = newlen;
	}

	if (len > 0)
	{
		g_string_append_len (result, str, len);
	}

	return g_string_free (result, FALSE);
}

guint
ephy_string_flags_from_string (GType type,
			       const char *flags_string)
{
	GFlagsClass *flags_class;
	const GFlagsValue *value;
	gchar **flags;
	guint retval = 0, i;

	g_return_val_if_fail (flags_string != NULL, 0);

	flags = g_strsplit (flags_string, "|", -1);
	if (!flags) return 0;

	flags_class = g_type_class_ref (type);

	for (i = 0; flags[i] != NULL; ++i)
	{
		value = g_flags_get_value_by_nick (flags_class, flags[i]);
		if (value != NULL)
		{
			retval |= value->value;
		}
	}

	g_type_class_unref (flags_class);

	return retval;
}

char *
ephy_string_flags_to_string (GType type,
			     guint flags_value)
{
	GFlagsClass *flags_class;
	GString *string;
	gboolean first = TRUE;
	guint i;

	string = g_string_sized_new (128);

	flags_class = g_type_class_ref (type);

	for (i = 0; i < flags_class->n_values; ++i)
	{
		if (flags_value & flags_class->values[i].value)
		{
			if (!first)
			{
				g_string_append_c (string, '|');
			}
			first = FALSE;
			g_string_append (string, flags_class->values[i].value_nick);
		}
	}

	g_type_class_unref (flags_class);

	return g_string_free (string, FALSE);
}

guint
ephy_string_enum_from_string (GType type,
			      const char *enum_string)
{
	GEnumClass *enum_class;
	const GEnumValue *value;
	guint retval = 0;

	g_return_val_if_fail (enum_string != NULL, 0);

	enum_class = g_type_class_ref (type);
	value = g_enum_get_value_by_nick (enum_class, enum_string);
	if (value != NULL)
	{
		retval = value->value;
	}

	g_type_class_unref (enum_class);

	return retval;
}

char *
ephy_string_enum_to_string (GType type,
			    guint enum_value)
{
	GEnumClass *enum_class;
	GEnumValue *value;
	char *retval = NULL;

	enum_class = g_type_class_ref (type);

	value = g_enum_get_value (enum_class, enum_value);
	if (value)
	{
		retval = g_strdup (value->value_nick);
	}

	g_type_class_unref (enum_class);

	return retval;
}

/* Following code copied from gnome-vfs-private-utils.c */

static int
find_next_slash (const char *path, int current_offset)
{
	const char *match;
	
	g_assert (current_offset <= strlen (path));
	
	match = strchr (path + current_offset, G_DIR_SEPARATOR);
	return match == NULL ? -1 : match - path;
}

static int
find_slash_before_offset (const char *path, int to)
{
	int result;
	int next_offset;

	result = -1;
	next_offset = 0;
	for (;;) {
		next_offset = find_next_slash (path, next_offset);
		if (next_offset < 0 || next_offset >= to) {
			break;
		}
		result = next_offset;
		next_offset++;
	}
	return result;
}

static void
collapse_slash_runs (char *path, int from_offset)
{
	int i;
	/* Collapse multiple `/'s in a row. */
	for (i = from_offset;; i++) {
		if (path[i] != G_DIR_SEPARATOR) {
			break;
		}
	}

	if (from_offset < i) {
		memmove (path + from_offset, path + i, strlen (path + i) + 1);
		i = from_offset + 1;
	}
}

/* Canonicalize path, and return a new path.  Do everything in situ.  The new
   path differs from path in:

     Multiple `/'s are collapsed to a single `/'.
     Leading `./'s and trailing `/.'s are removed.
     Non-leading `../'s and trailing `..'s are handled by removing
     portions of the path.  */
char *
ephy_string_canonicalize_pathname (const char *cpath)
{
	char *path;
	int i, marker;
	
	path = g_strdup (cpath);

	if (path == NULL || strlen (path) == 0) {
		return "";
	}

	/* Walk along path looking for things to compact. */
	for (i = 0, marker = 0;;) {
		if (!path[i])
			break;

		/* Check for `../', `./' or trailing `.' by itself. */
		if (path[i] == '.') {
			/* Handle trailing `.' by itself. */
			if (path[i + 1] == '\0') {
				if (i > 1 && path[i - 1] == G_DIR_SEPARATOR) {
					/* strip the trailing /. */
					path[i - 1] = '\0';
				} else {
					/* convert path "/." to "/" */
					path[i] = '\0';
				}
				break;
			}

			/* Handle `./'. */
			if (path[i + 1] == G_DIR_SEPARATOR) {
				memmove (path + i, path + i + 2, 
					 strlen (path + i + 2) + 1);
				if (i == 0) {
					/* don't leave leading '/' for paths that started
					 * as relative (.//foo)
					 */
					collapse_slash_runs (path, i);
					marker = 0;
				}
				continue;
			}

			/* Handle `../' or trailing `..' by itself. 
			 * Remove the previous xxx/ part 
			 */
			if (path[i + 1] == '.'
			    && (path[i + 2] == G_DIR_SEPARATOR
				|| path[i + 2] == '\0')) {

				/* ignore ../ at the beginning of a path */
				if (i != 0) {
					marker = find_slash_before_offset (path, i - 1);

					/* Either advance past '/' or point to the first character */
					marker ++;
					if (path [i + 2] == '\0' && marker > 1) {
						/* If we are looking at a /.. at the end of the uri and we
						 * need to eat the last '/' too.
						 */
						 marker--;
					}
					g_assert(marker < i);
					
					if (path[i + 2] == G_DIR_SEPARATOR) {
						/* strip the entire ../ string */
						i++;
					}

					memmove (path + marker, path + i + 2,
						 strlen (path + i + 2) + 1);
					i = marker;
				} else {
					i = 2;
					if (path[i] == G_DIR_SEPARATOR) {
						i++;
					}
				}
				collapse_slash_runs (path, i);
				continue;
			}
		}
		
		/* advance to the next '/' */
		i = find_next_slash (path, i);

		/* If we didn't find any slashes, then there is nothing left to do. */
		if (i < 0) {
			break;
		}

		marker = i++;
		collapse_slash_runs (path, i);
	}
	return path;
}

/* End of copied code */

char *
ephy_string_get_host_name (const char *url)
{
	const char *start;
	const char *p;
	
	if (url == NULL || g_str_has_prefix (url, "file://")) return NULL;
	
	start = strstr (url, "//");
	if (start == NULL || start == '\0')
	{
		/* Not an URL */
		return NULL;
	}
	if (strlen (start) > 2)
	{
		/* Go past the protocol part */
		start = start + 2;
	}
	else
	{
		/* Not an URL again */
		return NULL;
	}
	p = strchr (start, '@');
	if (p != NULL)
	{
		/* We have a username:password@hostname scheme, skip it. */
		if (strlen (p) > 1) start = ++p;
	}
	p = strchr (start, ':');
	if (p != NULL)
	{
		/* hostname:port, skip port */
		return g_strndup (start, (p - start));
	}
	p = strchr (start, '/');
	if (p == NULL)
	{
		/* No more slashes in the url, we assume it's a host name */
		return g_strdup (start);
	}

	return g_strndup (start, (p - start));
}

char *
ephy_string_expand_initial_tilde (const char *path)
{
	char *slash_after_user_name, *user_name;
	struct passwd *passwd_file_entry;

	g_return_val_if_fail (path != NULL, NULL);

	if (path[0] != '~') {
		return g_strdup (path);
	}
	
	if (path[1] == '/' || path[1] == '\0') {
		return g_strconcat (g_get_home_dir (), &path[1], NULL);
	}

	slash_after_user_name = strchr (&path[1], '/');
	if (slash_after_user_name == NULL) {
		user_name = g_strdup (&path[1]);
	} else {
		user_name = g_strndup (&path[1],
				       slash_after_user_name - &path[1]);
	}
	passwd_file_entry = getpwnam (user_name);
	g_free (user_name);

	if (passwd_file_entry == NULL || passwd_file_entry->pw_dir == NULL) {
		return g_strdup (path);
	}

	return g_strconcat (passwd_file_entry->pw_dir,
			    slash_after_user_name,
			    NULL);
}
