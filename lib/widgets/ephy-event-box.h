/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef EPHY_EVENT_BOX_H
#define EPHY_EVENT_BOX_H

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtkbin.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EVENT_BOX		(ephy_event_box_get_type ())
#define EPHY_EVENT_BOX(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_EVENT_BOX, EphyEventBox))
#define EPHY_EVENT_BOX_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_EVENT_BOX, EphyEventBoxClass))
#define EPHY_IS_EVENT_BOX(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_EVENT_BOX))
#define EPHY_IS_EVENT_BOX_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_EVENT_BOX))
#define EPHY_EVENT_BOX_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_EVENT_BOX, EphyEventBoxClass))


typedef struct _EphyEventBox		EphyEventBox;
typedef struct _EphyEventBoxClass	EphyEventBoxClass;

struct _EphyEventBox
{
  GtkBin bin;
  GdkWindow *input_window;
};

struct _EphyEventBoxClass
{
  GtkBinClass parent_class;
};

GType		ephy_event_box_get_type	(void) G_GNUC_CONST;

GtkWidget      *ephy_event_box_new	(void);

G_END_DECLS

#endif /* EPHY_EVENT_BOX_H */
