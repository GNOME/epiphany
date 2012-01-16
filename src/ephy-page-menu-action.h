/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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

#ifndef __EPHY_PAGE_MENU_ACTION_H__
#define __EPHY_PAGE_MENU_ACTION_H__

#include "ephy-window-action.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_PAGE_MENU_ACTION (ephy_page_menu_action_get_type())
#define EPHY_PAGE_MENU_ACTION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_PAGE_MENU_ACTION, EphyPageMenuAction))
#define EPHY_PAGE_MENU_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_PAGE_MENU_ACTION, EphyPageMenuActionClass))
#define EPHY_IS_PAGE_MENU_ACTION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_PAGE_MENU_ACTION))
#define EPHY_IS_PAGE_MENU_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_PAGE_MENU_ACTION))
#define EPHY_PAGE_MENU_ACTION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_PAGE_MENU_ACTION, EphyPageMenuActionClass))

typedef struct _EphyPageMenuAction      EphyPageMenuAction;
typedef struct _EphyPageMenuActionClass EphyPageMenuActionClass;

struct _EphyPageMenuActionClass {
    EphyWindowActionClass parent_class;
};

struct _EphyPageMenuAction {
    EphyWindowAction parent_instance;
};

GType ephy_page_menu_action_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __EPHY_PAGE_MENU_ACTION_H__ */
