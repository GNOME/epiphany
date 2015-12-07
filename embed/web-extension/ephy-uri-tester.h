/*
 *  Copyright © 2011 Igalia S.L.
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

#ifndef URI_TESTER_H
#define URI_TESTER_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_URI_TESTER (ephy_uri_tester_get_type ())

G_DECLARE_FINAL_TYPE (EphyUriTester, ephy_uri_tester, EPHY, URI_TESTER, GObject)

EphyUriTester *ephy_uri_tester_new         (const char *base_data_dir);

gboolean       ephy_uri_tester_test_uri    (EphyUriTester *tester,
                                            const char *req_uri,
                                            const char *page_uri);

void           ephy_uri_tester_set_filters (EphyUriTester *tester,
                                            GSList *filters);

G_END_DECLS

#endif /* URI_TESTER_H */
