/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2002 Marco Pesenti Gritti
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
#include <glib-object.h>

G_BEGIN_DECLS

gboolean  ephy_string_to_int                   (const char *string,
                                                gulong *integer);

char     *ephy_string_blank_chr                (char *source);

char     *ephy_string_shorten                  (char *str,
                                                gsize target_length);

char     *ephy_string_collate_key_for_domain   (const char *host,
                                                gssize len);

char     *ephy_string_get_host_name            (const char *url);

char    **ephy_string_commandline_args_to_uris (char **arguments, GError **error);

char     *ephy_string_find_and_replace         (const char *string,
                                                const char *to_find,
                                                const char *to_repl);

char     *ephy_string_remove_leading           (char *string,
                                                char  ch);
char     *ephy_string_remove_trailing          (char *string,
                                                char  ch);

char    **ephy_strv_remove                     (const char * const *strv,
                                                const char *str);

char    **ephy_strv_append                     (const char * const *strv,
                                                const char         *str);

G_END_DECLS
