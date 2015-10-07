/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2015 Igalia S.L.
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

#ifndef EPHY_DOWNLOADS_MANAGER_H
#define EPHY_DOWNLOADS_MANAGER_H

#include "ephy-download.h"

G_BEGIN_DECLS

#define EPHY_TYPE_DOWNLOADS_MANAGER         (ephy_downloads_manager_get_type ())
#define EPHY_DOWNLOADS_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_DOWNLOADS_MANAGER, EphyDownloadsManager))
#define EPHY_DOWNLOADS_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_DOWNLOADS_MANAGER, EphyDownloadsManagerClass))
#define EPHY_IS_DOWNLOADS_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_DOWNLOADS_MANAGER))
#define EPHY_IS_DOWNLOADS_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_DOWNLOADS_MANAGER))
#define EPHY_DOWNLOADS_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_DOWNLOADS_MANAGER, EphyDownloadsManagerClass))

typedef struct _EphyDownloadsManagerClass   EphyDownloadsManagerClass;
typedef struct _EphyDownloadsManager        EphyDownloadsManager;

GType    ephy_downloads_manager_get_type             (void);

void     ephy_downloads_manager_add_download         (EphyDownloadsManager *manager,
                                                      EphyDownload         *download);
void     ephy_downloads_manager_remove_download      (EphyDownloadsManager *manager,
                                                      EphyDownload         *download);
gboolean ephy_downloads_manager_has_active_downloads (EphyDownloadsManager *manager);
GList   *ephy_downloads_manager_get_downloads        (EphyDownloadsManager *manager);

G_END_DECLS

#endif /* EPHY_DOWNLOADS_MANAGER_H */
