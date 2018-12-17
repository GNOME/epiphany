/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2012 Igalia S.L.
 *  Copyright © 2018 Jan-Michael Brummer
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

#ifndef _EPHY_EVINCE_DOCUMENT_VIEW_H
#define _EPHY_EVINCE_DOCUMENT_VIEW_H

#include <gtk/gtk.h>

#include "ephy-embed.h"

G_BEGIN_DECLS

#define EPHY_TYPE_EVINCE_DOCUMENT_VIEW (ephy_evince_document_view_get_type ())

G_DECLARE_FINAL_TYPE (EphyEvinceDocumentView, ephy_evince_document_view, EPHY, EVINCE_DOCUMENT_VIEW, GtkBox)


GType      ephy_evince_document_view_get_type (void);

GtkWidget *ephy_evince_document_view_new (void);
void       ephy_evince_document_view_load_uri (EphyEvinceDocumentView *self,
                                               const char             *uri);
void       ephy_evince_document_set_embed (EphyEvinceDocumentView *self,
                                           EphyEmbed              *embed);

G_END_DECLS

#endif /* _EPHY_EVINCE_DOCUMENT_VIEW_H */
