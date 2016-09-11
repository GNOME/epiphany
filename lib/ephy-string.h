/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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


G_END_DECLS
