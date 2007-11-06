/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  arch-tag: File containing code cut and pasted from elsewhere
 *
 *  Copyright © 2000 Eazel, Inc.
 *  Copyright © 2002 Jorn Baayen
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
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 *  Authors: John Sullivan <sullivan@eazel.com>
 *           Jorn Baayen
 */

/* Following code is copied from Rhythmbox rb-cut-and-paste-code.c */

#include <config.h>

#include <string.h>
#include <glib/gi18n.h>
#include <glib.h>

#include "ephy-time-helpers.h"

/* Legal conversion specifiers, as specified in the C standard. */
#define C_STANDARD_STRFTIME_CHARACTERS "aAbBcdHIjmMpSUwWxXyYZ"
#define C_STANDARD_NUMERIC_STRFTIME_CHARACTERS "dHIjmMSUwWyY"
#define SUS_EXTENDED_STRFTIME_MODIFIERS "EO"

/**
 * eel_strdup_strftime:
 *
 * Cover for standard date-and-time-formatting routine strftime that returns
 * a newly-allocated string of the correct size. The caller is responsible
 * for g_free-ing the returned string.
 *
 * Besides the buffer management, there are two differences between this
 * and the library strftime:
 *
 *   1) The modifiers "-" and "_" between a "%" and a numeric directive
 *      are defined as for the GNU version of strftime. "-" means "do not
 *      pad the field" and "_" means "pad with spaces instead of zeroes".
 *   2) Non-ANSI extensions to strftime are flagged at runtime with a
 *      warning, so it's easy to notice use of the extensions without
 *      testing with multiple versions of the library.
 *
 * @format: format string to pass to strftime. See strftime documentation
 * for details.
 * @time_pieces: date/time, in struct format.
 *
 * Return value: Newly allocated string containing the formatted time.
 **/

char *
eel_strdup_strftime (const char *format, struct tm *time_pieces)
{
	GString *string;
	const char *remainder, *percent;
	char code[4], buffer[512];
	char *piece, *result, *converted;
	size_t string_length;
	gboolean strip_leading_zeros, turn_leading_zeros_to_spaces;
	char modifier;
	int i;

	/* Format could be translated, and contain UTF-8 chars,
	 * so convert to locale encoding which strftime uses */
	converted = g_locale_from_utf8 (format, -1, NULL, NULL, NULL);
	g_return_val_if_fail (converted != NULL, NULL);

	string = g_string_new ("");
	remainder = converted;

	/* Walk from % character to % character. */
	for (;;) {
		percent = strchr (remainder, '%');
		if (percent == NULL) {
			g_string_append (string, remainder);
			break;
		}
		g_string_append_len (string, remainder,
				     percent - remainder);

		/* Handle the "%" character. */
		remainder = percent + 1;
		switch (*remainder) {
		case '-':
			strip_leading_zeros = TRUE;
			turn_leading_zeros_to_spaces = FALSE;
			remainder++;
			break;
		case '_':
			strip_leading_zeros = FALSE;
			turn_leading_zeros_to_spaces = TRUE;
			remainder++;
			break;
		case '%':
			g_string_append_c (string, '%');
			remainder++;
			continue;
		case '\0':
			g_warning ("Trailing %% passed to eel_strdup_strftime");
			g_string_append_c (string, '%');
			continue;
		default:
			strip_leading_zeros = FALSE;
			turn_leading_zeros_to_spaces = FALSE;
			break;
		}

		modifier = 0;
		if (strchr (SUS_EXTENDED_STRFTIME_MODIFIERS, *remainder) != NULL) {
			modifier = *remainder;
			remainder++;

			if (*remainder == 0) {
				g_warning ("Unfinished %%%c modifier passed to eel_strdup_strftime", modifier);
				break;
			}
		}

		if (strchr (C_STANDARD_STRFTIME_CHARACTERS, *remainder) == NULL) {
			g_warning ("eel_strdup_strftime does not support "
				   "non-standard escape code %%%c",
				   *remainder);
		}

		/* Convert code to strftime format. We have a fixed
		 * limit here that each code can expand to a maximum
		 * of 512 bytes, which is probably OK. There's no
		 * limit on the total size of the result string.
		 */
		i = 0;
		code[i++] = '%';
		if (modifier != 0) {
#ifdef HAVE_STRFTIME_EXTENSION
			code[i++] = modifier;
#endif
		}
		code[i++] = *remainder;
		code[i++] = '\0';
		string_length = strftime (buffer, sizeof (buffer),
					  code, time_pieces);
		if (string_length == 0) {
			/* We could put a warning here, but there's no
			 * way to tell a successful conversion to
			 * empty string from a failure.
			 */
			buffer[0] = '\0';
		}

		/* Strip leading zeros if requested. */
		piece = buffer;
		if (strip_leading_zeros || turn_leading_zeros_to_spaces) {
			if (strchr (C_STANDARD_NUMERIC_STRFTIME_CHARACTERS, *remainder) == NULL) {
				g_warning ("eel_strdup_strftime does not support "
					   "modifier for non-numeric escape code %%%c%c",
					   remainder[-1],
					   *remainder);
			}
			if (*piece == '0') {
				do {
					piece++;
				} while (*piece == '0');
				if (!g_ascii_isdigit (*piece)) {
				    piece--;
				}
			}
			if (turn_leading_zeros_to_spaces) {
				memset (buffer, ' ', piece - buffer);
				piece = buffer;
			}
		}
		remainder++;

		/* Add this piece. */
		g_string_append (string, piece);
	}

	/* Convert the string back into utf-8. */
	result = g_locale_to_utf8 (string->str, -1, NULL, NULL, NULL);

	g_string_free (string, TRUE);
	g_free (converted);

	return result;
}

/* Based on evolution/mail/message-list.c:filter_date() */
char *
ephy_time_helpers_utf_friendly_time (time_t date)
{
	time_t nowdate;
	time_t yesdate;
	struct tm then, now, yesterday;
	const char *format = NULL;
	char *str = NULL;
	gboolean done = FALSE;

	nowdate = time (NULL);

	if (date == 0)
		return NULL;

	localtime_r (&date, &then);
	localtime_r (&nowdate, &now);

	if (then.tm_mday == now.tm_mday &&
	    then.tm_mon == now.tm_mon &&
	    then.tm_year == now.tm_year) {
		/* Translators: "friendly time" string for the current day, strftime format. like "Today 12:34 am" */
		format = _("Today %I:%M %p");
		done = TRUE;
	}

	if (! done) {
		yesdate = nowdate - 60 * 60 * 24;
		localtime_r (&yesdate, &yesterday);
		if (then.tm_mday == yesterday.tm_mday &&
		    then.tm_mon == yesterday.tm_mon &&
		    then.tm_year == yesterday.tm_year) {
			/* Translators: "friendly time" string for the previous day,
			 * strftime format. e.g. "Yesterday 12:34 am"
			 */
			format = _("Yesterday %I:%M %p");
			done = TRUE;
		}
	}

	if (! done) {
		int i;
		for (i = 2; i < 7; i++) {
			yesdate = nowdate - 60 * 60 * 24 * i;
			localtime_r (&yesdate, &yesterday);
			if (then.tm_mday == yesterday.tm_mday &&
			    then.tm_mon == yesterday.tm_mon &&
			    then.tm_year == yesterday.tm_year) {
				/* Translators: "friendly time" string for a day in the current week,
				 * strftime format. e.g. "Wed 12:34 am"
				 */
				format = _("%a %I:%M %p");
				done = TRUE;
				break;
			}
		}
	}

	if (! done) {
		if (then.tm_year == now.tm_year) {
			/* Translators: "friendly time" string for a day in the current year,
			 * strftime format. e.g. "Feb 12 12:34 am"
			 */
			format = _("%b %d %I:%M %p");
		} else {
			/* Translators: "friendly time" string for a day in a different year,
			 * strftime format. e.g. "Feb 12 1997"
			 */
			format = _("%b %d %Y");
		}
	}

	if (format != NULL) {
		str = eel_strdup_strftime (format, &then);
	}

	if (str == NULL) {
		/* impossible time or broken locale settings */
		str = g_strdup (_("Unknown"));
	}

	return str;
}
