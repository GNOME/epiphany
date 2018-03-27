/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2014 Igalia S.L.
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

#include <glib-object.h>
#include <jsc/jsc.h>

G_BEGIN_DECLS

#define EPHY_TYPE_WEB_OVERVIEW_MODEL (ephy_web_overview_model_get_type())

G_DECLARE_FINAL_TYPE (EphyWebOverviewModel, ephy_web_overview_model, EPHY, WEB_OVERVIEW_MODEL, GObject)

EphyWebOverviewModel *ephy_web_overview_model_new               (void);
void                  ephy_web_overview_model_set_urls          (EphyWebOverviewModel *model,
                                                                 GList                *urls);
void                  ephy_web_overview_model_set_url_thumbnail (EphyWebOverviewModel *model,
                                                                 const char           *url,
                                                                 const char           *path,
                                                                 gboolean              notify);
void                  ephy_web_overview_model_set_url_title     (EphyWebOverviewModel *model,
                                                                 const char           *url,
                                                                 const char           *title);
void                  ephy_web_overview_model_delete_url        (EphyWebOverviewModel *model,
                                                                 const char           *url);
void                  ephy_web_overview_model_delete_host       (EphyWebOverviewModel *model,
                                                                 const char           *host);
void                  ephy_web_overview_model_clear             (EphyWebOverviewModel *model);


typedef struct _EphyWebOverviewModelItem EphyWebOverviewModelItem;
struct _EphyWebOverviewModelItem
{
  char *url;
  char *title;
};

EphyWebOverviewModelItem *ephy_web_overview_model_item_new  (const char               *url,
                                                             const char               *title);
void                      ephy_web_overview_model_item_free (EphyWebOverviewModelItem *item);

JSCValue *ephy_web_overview_model_export_to_js_context  (EphyWebOverviewModel *model,
                                                         JSCContext           *js_context);

G_END_DECLS
