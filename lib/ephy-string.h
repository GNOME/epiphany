/*
 *  Copyright (C) 2002 Marco Pesenti Gritti
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef EPHY_STRING_H
#define EPHY_STRING_H

#include <glib.h>

G_BEGIN_DECLS

char *           ephy_string_double_underscores		(const char *string);

void		 ephy_string_store_time_in_string	(GDate *t,
							 gchar *str,
							 const char *format);

gchar		*ephy_string_time_to_string		(GDate *t,
							 const char *format);

gboolean	 ephy_str_to_int			(const char *string,
							 gulong *integer);

char		*ephy_str_strip_chr			(const char *source,
							 char remove_this);

int		 ephy_strcasecmp			(const char *string_a,
							 const char *string_b);

int		 ephy_strcasecmp_compare_func		(gconstpointer string_a,
							 gconstpointer string_b);

char	       **ephy_strsplit_with_quotes		(const gchar  *string,
							 const gchar  *delimiter,
							 gint max_tokens,
							 const gchar *quotes);

gchar		*ephy_string_shorten			(const gchar *str,
							 gint target_length);

char	       **ephy_strsplit_multiple_delimiters_with_quotes (const gchar *string,
							        const gchar *delimiters,
							        gint max_tokens,
							        const gchar *quotes);

char		*ephy_str_replace_substring		(const char *string,
							 const char *substring,
							 const char *replacement);

gchar 		*ephy_str_elide_underscores		(const gchar *original);

G_END_DECLS

#endif
