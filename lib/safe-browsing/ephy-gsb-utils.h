/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2017 Gabriel Ivascu <gabrielivascu@gnome.org>
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

G_BEGIN_DECLS

typedef struct {
  char *threat_type;
  char *platform_type;
  char *threat_entry_type;
  char *client_state;
  gint64 timestamp;
} EphyGSBThreatList;

EphyGSBThreatList *ephy_gsb_threat_list_new   (const char *threat_type,
                                               const char *platform_type,
                                               const char *threat_entry_type,
                                               const char *client_state,
                                               gint64      timestamp);
void               ephy_gsb_threat_list_free  (EphyGSBThreatList *list);

char              *ephy_gsb_utils_make_list_updates_request (GList *threat_lists);

char              *ephy_gsb_utils_canonicalize              (const char  *url,
                                                             char       **host_out,
                                                             char       **path_out,
                                                             char       **query_out);
GList             *ephy_gsb_utils_compute_hashes            (const char *url);

G_END_DECLS
