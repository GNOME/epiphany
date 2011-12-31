/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2011 Alexandre Mazari
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
 
#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef __EPHY_MIDDLE_CLICKABLE_BUTTON_H__
#define __EPHY_MIDDLE_CLICKABLE_BUTTON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EPHY_TYPE_MIDDLE_CLICKABLE_BUTTON             (ephy_middle_clickable_button_get_type ())
#define EPHY_MIDDLE_CLICKABLE_BUTTON(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj),EPHY_TYPE_MIDDLE_CLICKABLE_BUTTN, EphyMiddleClickableButton))
#define EPHY_MIDDLE_CLICKABLE_BUTTON_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_MIDDLE_CLICKABLE_BUTTN, EphyMiddleClickableButtonClass))
#define EPHY_IS_MIDDLE_CLICKABLE_BUTTON(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_MIDDLE_CLICKABLE_BUTON))
#define EPHY_IS_MIDDLE_CLICKABLE_BUTTON_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_MIDDLE_CLICKABLE_BUTTN))
#define EPHY_MIDDLE_CLICKABLE_BUTTON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_MIDDLE_CLICKABLE_BUTTN, EphyMiddleClickableButtonClass))

typedef struct _EphyMiddleClickableButton       EphyMiddleClickableButton;
typedef struct _EphyMiddleClickableButtonClass  EphyMiddleClickableButtonClass;

struct _EphyMiddleClickableButton {
  GtkButton parent;
};

struct _EphyMiddleClickableButtonClass {
  GtkButtonClass parent_class;
};

GType      ephy_middle_clickable_button_get_type (void) G_GNUC_CONST;
GtkWidget *ephy_middle_clickable_button_new      (void);

G_END_DECLS

#endif
