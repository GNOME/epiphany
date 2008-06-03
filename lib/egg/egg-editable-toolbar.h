/*
 *  Copyright (C) 2003, 2004  Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004, 2005  Christian Persch
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef EGG_EDITABLE_TOOLBAR_H
#define EGG_EDITABLE_TOOLBAR_H

#include "egg-toolbars-model.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_EDITABLE_TOOLBAR             (egg_editable_toolbar_get_type ())
#define EGG_EDITABLE_TOOLBAR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EGG_TYPE_EDITABLE_TOOLBAR, EggEditableToolbar))
#define EGG_EDITABLE_TOOLBAR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EGG_TYPE_EDITABLE_TOOLBAR, EggEditableToolbarClass))
#define EGG_IS_EDITABLE_TOOLBAR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EGG_TYPE_EDITABLE_TOOLBAR))
#define EGG_IS_EDITABLE_TOOLBAR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EGG_TYPE_EDITABLE_TOOLBAR))
#define EGG_EDITABLE_TOOLBAR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EGG_TYPE_EDITABLE_TOOLBAR, EggEditableToolbarClass))

typedef struct _EggEditableToolbar        EggEditableToolbar;
typedef struct _EggEditableToolbarPrivate EggEditableToolbarPrivate;
typedef struct _EggEditableToolbarClass   EggEditableToolbarClass;

struct _EggEditableToolbar
{
  GtkVBox parent_object;

  /*< private >*/
  EggEditableToolbarPrivate *priv;
};

struct _EggEditableToolbarClass
{
  GtkVBoxClass parent_class;

  void (* action_request) (EggEditableToolbar *etoolbar,
			   const char *action_name);
};

GType               egg_editable_toolbar_get_type        (void);
GtkWidget	   *egg_editable_toolbar_new		 (GtkUIManager         *manager,
							  const char           *visibility_path);
GtkWidget	   *egg_editable_toolbar_new_with_model	 (GtkUIManager         *manager,
							  EggToolbarsModel     *model,
							  const char           *visibility_path);
void		    egg_editable_toolbar_set_model       (EggEditableToolbar   *etoolbar,
							  EggToolbarsModel     *model);
EggToolbarsModel   *egg_editable_toolbar_get_model       (EggEditableToolbar   *etoolbar);
GtkUIManager       *egg_editable_toolbar_get_manager     (EggEditableToolbar   *etoolbar);
void		    egg_editable_toolbar_set_edit_mode	 (EggEditableToolbar   *etoolbar,
							  gboolean              mode);
gboolean	    egg_editable_toolbar_get_edit_mode	 (EggEditableToolbar   *etoolbar);
void		    egg_editable_toolbar_show		 (EggEditableToolbar   *etoolbar,
							  const char           *name);
void		    egg_editable_toolbar_hide		 (EggEditableToolbar   *etoolbar,
							  const char           *name);
void		    egg_editable_toolbar_set_fixed       (EggEditableToolbar   *etoolbar,
							  GtkToolbar           *fixed_toolbar);

GtkWidget *         egg_editable_toolbar_get_selected    (EggEditableToolbar   *etoolbar);
void                egg_editable_toolbar_set_selected    (EggEditableToolbar   *etoolbar,
							  GtkWidget            *widget);

void              egg_editable_toolbar_add_visibility    (EggEditableToolbar   *etoolbar,
							  const char           *path);

/* Private Functions */

GtkWidget 	   *_egg_editable_toolbar_new_separator_image (void);

G_END_DECLS

#endif
