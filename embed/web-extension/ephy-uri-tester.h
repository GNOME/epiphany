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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef URI_TESTER_H
#define URI_TESTER_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define EPHY_TYPE_URI_TESTER         (ephy_uri_tester_get_type ())
#define EPHY_URI_TESTER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_URI_TESTER, EphyUriTester))
#define EPHY_URI_TESTER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_URI_TESTER, EphyUriTesterClass))
#define EPHY_IS_URI_TESTER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_URI_TESTER))
#define EPHY_IS_URI_TESTER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_URI_TESTER))
#define EPHY_URI_TESTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_URI_TESTER, EphyUriTesterClass))

typedef struct _EphyUriTester        EphyUriTester;
typedef struct _EphyUriTesterClass   EphyUriTesterClass;
typedef struct _EphyUriTesterPrivate EphyUriTesterPrivate;

struct _EphyUriTester
{
  GObject parent_instance;

  /*< private >*/
  EphyUriTesterPrivate *priv;
};

struct _EphyUriTesterClass
{
  GObjectClass parent_class;
};

GType          ephy_uri_tester_get_type    (void);

EphyUriTester *ephy_uri_tester_new         (const char *base_data_dir);

gboolean       ephy_uri_tester_test_uri    (EphyUriTester *tester,
                                            const char *req_uri,
                                            const char *page_uri);

void           ephy_uri_tester_set_filters (EphyUriTester *tester,
                                            GSList *filters);

G_END_DECLS

#endif /* URI_TESTER_H */
