/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef EPHY_SMAPS_H
#define EPHY_SMAPS_H

#include <glib-object.h>

#define EPHY_TYPE_SMAPS            (ephy_smaps_get_type ())
#define EPHY_SMAPS(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), EPHY_TYPE_SMAPS, EphySMaps))
#define EPHY_SMAPS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_SMAPS, EphySMapsClass))
#define EPHY_IS_SMAPS(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), EPHY_TYPE_SMAPS))
#define EPHY_IS_SMAPS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_SMAPS))
#define EPHY_SMAPS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_SMAPS, EphySMapsClass))

typedef struct _EphySMapsPrivate EphySMapsPrivate;

typedef struct {
  GObject parent;

  EphySMapsPrivate *priv;
} EphySMaps;

typedef struct {
  GObjectClass parent;

} EphySMapsClass;

GType       ephy_smaps_get_type (void);
EphySMaps * ephy_smaps_new      (void);
char      * ephy_smaps_to_html  (EphySMaps *smaps);

#endif /* EPHY_SMAPS_H */
