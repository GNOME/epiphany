/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* eggtraymanager.h
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __EGG_TRAY_MANAGER_H__
#define __EGG_TRAY_MANAGER_H__

#include <gtk/gtkwidget.h>
#include <gdk/gdkx.h>

G_BEGIN_DECLS

#define EGG_TYPE_TRAY_MANAGER			(egg_tray_manager_get_type ())
#define EGG_TRAY_MANAGER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), EGG_TYPE_TRAY_MANAGER, EggTrayManager))
#define EGG_TRAY_MANAGER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), EGG_TYPE_TRAY_MANAGER, EggTrayManagerClass))
#define EGG_IS_TRAY_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), EGG_TYPE_TRAY_MANAGER))
#define EGG_IS_TRAY_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), EGG_TYPE_TRAY_MANAGER))
#define EGG_TRAY_MANAGER_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), EGG_TYPE_TRAY_MANAGER, EggTrayManagerClass))
	
typedef struct _EggTrayManager	     EggTrayManager;
typedef struct _EggTrayManagerClass  EggTrayManagerClass;
typedef struct _EggTrayManagerChild  EggTrayManagerChild;

struct _EggTrayManager
{
  GObject parent_instance;

  Atom opcode_atom;
  Atom selection_atom;
  Atom message_data_atom;
  Atom orientation_atom;
  
  GtkWidget *invisible;
  GdkScreen *screen;
  GtkOrientation orientation;

  GList *messages;
  GHashTable *socket_table;
};

struct _EggTrayManagerClass
{
  GObjectClass parent_class;

  void (* tray_icon_added)   (EggTrayManager      *manager,
			      EggTrayManagerChild *child);
  void (* tray_icon_removed) (EggTrayManager      *manager,
			      EggTrayManagerChild *child);

  void (* message_sent)      (EggTrayManager      *manager,
			      EggTrayManagerChild *child,
			      const gchar         *message,
			      glong                id,
			      glong                timeout);
  
  void (* message_cancelled) (EggTrayManager      *manager,
			      EggTrayManagerChild *child,
			      glong                id);

  void (* lost_selection)    (EggTrayManager      *manager);
};

GType           egg_tray_manager_get_type        (void);

gboolean        egg_tray_manager_check_running   (GdkScreen           *screen);
EggTrayManager *egg_tray_manager_new             (void);
gboolean        egg_tray_manager_manage_screen   (EggTrayManager      *manager,
						  GdkScreen           *screen);
char           *egg_tray_manager_get_child_title (EggTrayManager      *manager,
						  EggTrayManagerChild *child);
void            egg_tray_manager_set_orientation (EggTrayManager      *manager,
						  GtkOrientation       orientation);
GtkOrientation  egg_tray_manager_get_orientation (EggTrayManager      *manager);

G_END_DECLS

#endif /* __EGG_TRAY_MANAGER_H__ */
