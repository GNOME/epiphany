/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2013, 2014 Yosef Or Boczko <yoseforb@gnome.org>
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

#ifndef __EPHY_TITLE_BOX_H__
#define __EPHY_TITLE_BOX_H__

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include "ephy-location-entry.h"
#include "ephy-window.h"

G_BEGIN_DECLS

#define EPHY_TYPE_TITLE_BOX             (ephy_title_box_get_type ())
#define EPHY_TITLE_BOX(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_TITLE_BOX, EphyTitleBox))
#define EPHY_TITLE_BOX_CONST(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_TITLE_BOX, EphyTitleBox const))
#define EPHY_TITLE_BOX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_TITLE_BOX, EphyTitleBoxClass))
#define EPHY_IS_TITLE_BOX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_TITLE_BOX))
#define EPHY_IS_TITLE_BOX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_TITLE_BOX))
#define EPHY_TITLE_BOX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_TITLE_BOX, EphyTitleBoxClass))

typedef struct _EphyTitleBox      EphyTitleBox;
typedef struct _EphyTitleBoxClass EphyTitleBoxClass;

typedef enum
{
  EPHY_TITLE_BOX_MODE_LOCATION_ENTRY,
  EPHY_TITLE_BOX_MODE_TITLE
} EphyTitleBoxMode;

struct _EphyTitleBox
{
  GtkStack parent;
};

struct _EphyTitleBoxClass
{
  GtkStackClass parent_class;
};

GType               ephy_title_box_get_type             (void) G_GNUC_CONST;

EphyTitleBox       *ephy_title_box_new                  (EphyWindow           *window);

void                ephy_title_box_set_web_view         (EphyTitleBox         *title_box,
                                                         WebKitWebView        *web_view);

EphyTitleBoxMode    ephy_title_box_get_mode             (EphyTitleBox         *title_box);
void                ephy_title_box_set_mode             (EphyTitleBox         *title_box,
                                                         EphyTitleBoxMode      mode);

void                ephy_title_box_set_security_level   (EphyTitleBox         *title_box,
                                                         EphySecurityLevel     security_level);

GtkWidget          *ephy_title_box_get_location_entry   (EphyTitleBox         *title_box);

void                ephy_title_box_set_address          (EphyTitleBox         *title_box,
                                                         const char           *address);

G_END_DECLS

#endif /* __EPHY_TITLE_BOX_H__ */
