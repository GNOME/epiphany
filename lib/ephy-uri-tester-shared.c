/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2016 Igalia S.L.
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

#include "config.h"
#include "ephy-uri-tester-shared.h"

GFile *
ephy_uri_tester_get_adblock_filter_file (const char *adblock_data_dir)
{
  char *filter_filename, *filter_path;
  GFile *filter_file;

  filter_filename = g_compute_checksum_for_string (G_CHECKSUM_MD5, ADBLOCK_FILTER_URL, -1);
  filter_path = g_build_filename (adblock_data_dir, filter_filename, NULL);
  g_free (filter_filename);
  filter_file = g_file_new_for_path (filter_path);
  g_free (filter_path);

  return filter_file;
}
