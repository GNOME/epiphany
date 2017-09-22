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
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define GSB_HASH_CUE_LEN    4
#define GSB_RICE_PREFIX_LEN 4

#define GSB_HASH_TYPE G_CHECKSUM_SHA256
#define GSB_HASH_SIZE (g_checksum_type_get_length (GSB_HASH_TYPE))

#define GSB_COMPRESSION_TYPE_RAW         "RAW"
#define GSB_COMPRESSION_TYPE_RICE        "RICE"
#define GSB_COMPRESSION_TYPE_UNSPECIFIED "COMPRESSION_TYPE_UNSPECIFIED"

#define GSB_THREAT_TYPE_MALWARE            "MALWARE"
#define GSB_THREAT_TYPE_SOCIAL_ENGINEERING "SOCIAL_ENGINEERING"
#define GSB_THREAT_TYPE_UNWANTED_SOFTWARE  "UNWANTED_SOFTWARE"

typedef struct {
  char *threat_type;
  char *platform_type;
  char *threat_entry_type;
  char *client_state;
} EphyGSBThreatList;

typedef struct {
  GBytes   *prefix; /* The first 4-32 bytes of the hash */
  gboolean  negative_expired;
} EphyGSBHashPrefixLookup;

typedef struct {
  GBytes   *hash; /* The 32 bytes full hash */
  char     *threat_type;
  char     *platform_type;
  char     *threat_entry_type;
  gboolean  expired;
} EphyGSBHashFullLookup;

EphyGSBThreatList       *ephy_gsb_threat_list_new                 (const char *threat_type,
                                                                   const char *platform_type,
                                                                   const char *threat_entry_type,
                                                                   const char *client_state);
void                     ephy_gsb_threat_list_free                (EphyGSBThreatList *list);
gboolean                 ephy_gsb_threat_list_equal               (EphyGSBThreatList *l1,
                                                                   EphyGSBThreatList *l2);

EphyGSBHashPrefixLookup *ephy_gsb_hash_prefix_lookup_new          (const guint8 *prefix,
                                                                   gsize         length,
                                                                   gboolean      negative_expired);
void                     ephy_gsb_hash_prefix_lookup_free         (EphyGSBHashPrefixLookup *lookup);

EphyGSBHashFullLookup   *ephy_gsb_hash_full_lookup_new            (const guint8 *hash,
                                                                   const char   *threat_type,
                                                                   const char   *platform_type,
                                                                   const char   *threat_entry_type,
                                                                   gboolean      expired);
void                     ephy_gsb_hash_full_lookup_free           (EphyGSBHashFullLookup *lookup);

char                    *ephy_gsb_utils_make_list_updates_request (GList *threat_lists);
char                    *ephy_gsb_utils_make_full_hashes_request  (GList *threat_lists,
                                                                   GList *hash_prefixes);

guint32                 *ephy_gsb_utils_rice_delta_decode         (JsonObject *rde,
                                                                   gsize      *num_items);

char                    *ephy_gsb_utils_canonicalize              (const char  *url,
                                                                   char       **host_out,
                                                                   char       **path_out,
                                                                   char       **query_out);
GList                   *ephy_gsb_utils_compute_hashes            (const char *url);
GList                   *ephy_gsb_utils_get_hash_cues             (GList *hashes);
gboolean                 ephy_gsb_utils_hash_has_prefix           (GBytes *hash,
                                                                   GBytes *prefix);

G_END_DECLS
