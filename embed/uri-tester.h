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

#include "ephy-adblock.h"

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define TYPE_URI_TESTER         (uri_tester_get_type ())
#define URI_TESTER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_URI_TESTER, UriTester))
#define URI_TESTER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TYPE_URI_TESTER, UriTesterClass))
#define IS_URI_TESTER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TYPE_URI_TESTER))
#define IS_URI_TESTER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TYPE_URI_TESTER))
#define URI_TESTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_URI_TESTER, UriTesterClass))

typedef struct _UriTester        UriTester;
typedef struct _UriTesterClass   UriTesterClass;
typedef struct _UriTesterPrivate UriTesterPrivate;

struct _UriTester
{
  GObject parent_instance;

  /*< private >*/
  UriTesterPrivate *priv;
};

struct _UriTesterClass
{
  GObjectClass parent_class;
};

GType      uri_tester_get_type    (void);

void       uri_tester_register    (GTypeModule *module);

UriTester *uri_tester_new         (void);

gboolean   uri_tester_test_uri    (UriTester *tester,
                                   const char *req_uri,
                                   const char *page_uri,
                                   AdUriCheckType type);

void       uri_tester_set_filters (UriTester *tester, GSList *filters);

GSList    *uri_tester_get_filters (UriTester *tester);

void       uri_tester_reload      (UriTester *tester);

G_END_DECLS

#endif /* URI_TESTER_H */
