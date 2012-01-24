/*
 * Copyright (c) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public 
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License 
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __GD_MAIN_ICON_VIEW_H__
#define __GD_MAIN_ICON_VIEW_H__

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GD_TYPE_MAIN_ICON_VIEW gd_main_icon_view_get_type()

#define GD_MAIN_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   GD_TYPE_MAIN_ICON_VIEW, GdMainIconView))

#define GD_MAIN_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   GD_TYPE_MAIN_ICON_VIEW, GdMainIconViewClass))

#define GD_IS_MAIN_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   GD_TYPE_MAIN_ICON_VIEW))

#define GD_IS_MAIN_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   GD_TYPE_MAIN_ICON_VIEW))

#define GD_MAIN_ICON_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   GD_TYPE_MAIN_ICON_VIEW, GdMainIconViewClass))

typedef struct _GdMainIconView GdMainIconView;
typedef struct _GdMainIconViewClass GdMainIconViewClass;
typedef struct _GdMainIconViewPrivate GdMainIconViewPrivate;

struct _GdMainIconView
{
  GtkIconView parent;

  GdMainIconViewPrivate *priv;
};

struct _GdMainIconViewClass
{
  GtkIconViewClass parent_class;
};

GType gd_main_icon_view_get_type (void) G_GNUC_CONST;

GtkWidget * gd_main_icon_view_new (void);

G_END_DECLS

#endif /* __GD_MAIN_ICON_VIEW_H__ */
