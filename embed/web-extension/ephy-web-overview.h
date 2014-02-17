/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2014 Igalia S.L.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef _EPHY_WEB_OVERVIEW_H
#define _EPHY_WEB_OVERVIEW_H

#include "ephy-web-overview-model.h"
#include <webkit2/webkit-web-extension.h>
#include <JavaScriptCore/JavaScript.h>

G_BEGIN_DECLS

#define EPHY_TYPE_WEB_OVERVIEW            (ephy_web_overview_get_type())
#define EPHY_WEB_OVERVIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_WEB_OVERVIEW, EphyWebOverview))
#define EPHY_IS_WEB_OVERVIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_WEB_OVERVIEW))
#define EPHY_WEB_OVERVIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_WEB_OVERVIEW, EphyWebOverviewClass))
#define EPHY_IS_WEB_OVERVIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_WEB_OVERVIEW))
#define EPHY_WEB_OVERVIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_WEB_OVERVIEW, EphyWebOverviewClass))

typedef struct _EphyWebOverview        EphyWebOverview;
typedef struct _EphyWebOverviewClass   EphyWebOverviewClass;
typedef struct _EphyWebOverviewPrivate EphyWebOverviewPrivate;

struct _EphyWebOverview
{
  GObject parent;

  EphyWebOverviewPrivate *priv;
};

struct _EphyWebOverviewClass
{
  GObjectClass parent_class;
};

GType            ephy_web_overview_get_type (void) G_GNUC_CONST;

EphyWebOverview *ephy_web_overview_new      (WebKitWebPage        *web_page,
                                             EphyWebOverviewModel *model);
void             ephy_web_overview_init_js  (EphyWebOverview      *overview,
                                             JSGlobalContextRef    context);

G_END_DECLS

#endif /* _EPHY_WEB_OVERVIEW_H */
