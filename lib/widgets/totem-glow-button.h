/*
 * (C) Copyright 2007 Bastien Nocera <hadess@hadess.net>
 *
 * Glow code from libwnck/libwnck/tasklist.c:
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2003 Kim Woelders
 * Copyright © 2003 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301  USA.
 */

#ifndef __TOTEM_GLOW_BUTTON_H__
#define __TOTEM_GLOW_BUTTON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TOTEM_TYPE_GLOW_BUTTON     (totem_glow_button_get_type ())
#define TOTEM_GLOW_BUTTON(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), TOTEM_TYPE_GLOW_BUTTON, TotemGlowButton))
#define TOTEM_IS_GLOW_BUTTON(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TOTEM_TYPE_GLOW_BUTTON))

typedef struct _TotemGlowButton TotemGlowButton;

typedef struct _TotemGlowButtonClass {
  GtkButtonClass parent_class;

  gpointer __bla[4];
} TotemGlowButtonClass;

GType		totem_glow_button_get_type	(void) G_GNUC_CONST;

GtkWidget *	totem_glow_button_new		(void);
void		totem_glow_button_set_glow	(TotemGlowButton *button, gboolean glow);
gboolean	totem_glow_button_get_glow	(TotemGlowButton *button);

G_END_DECLS

#endif /* __TOTEM_GLOW_BUTTON_H__ */
