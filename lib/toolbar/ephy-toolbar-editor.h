/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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
 */

#ifndef EPHY_TOOLBAR_EDITOR_H
#define EPHY_TOOLBAR_EDITOR_H

#include <glib-object.h>
#include <gtk/gtkbutton.h>

#include "ephy-toolbar.h"

G_BEGIN_DECLS

/* object forward declarations */

typedef struct _EphyTbEditor EphyTbEditor;
typedef struct _EphyTbEditorClass EphyTbEditorClass;
typedef struct _EphyTbEditorPrivate EphyTbEditorPrivate;

/**
 * TbEditor object
 */

#define EPHY_TYPE_TB_EDITOR		(ephy_tb_editor_get_type())
#define EPHY_TB_EDITOR(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), \
					 EPHY_TYPE_TB_EDITOR,\
					 EphyTbEditor))
#define EPHY_TB_EDITOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_TB_EDITOR,\
					 EphyTbEditorClass))
#define EPHY_IS_TB_EDITOR(object)	(G_TYPE_CHECK_INSTANCE_TYPE((object), \
					 EPHY_TYPE_TB_EDITOR))
#define EPHY_IS_TB_EDITOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_TB_EDITOR))
#define EPHY_TB_EDITOR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_TB_EDITOR,\
					 EphyTbEditorClass))

struct _EphyTbEditorClass
{
	GObjectClass parent_class;
};

/* Remember: fields are public read-only */
struct _EphyTbEditor
{
	GObject parent_object;

	EphyTbEditorPrivate *priv;
};

/* this class is abstract */

GType			ephy_tb_editor_get_type		(void);
EphyTbEditor *		ephy_tb_editor_new		(void);
void			ephy_tb_editor_set_toolbar	(EphyTbEditor *tbe, EphyToolbar *tb);
EphyToolbar *		ephy_tb_editor_get_toolbar	(EphyTbEditor *tbe);
void			ephy_tb_editor_set_available	(EphyTbEditor *tbe, EphyToolbar *tb);
EphyToolbar *		ephy_tb_editor_get_available	(EphyTbEditor *tbe);
void			ephy_tb_editor_set_parent	(EphyTbEditor *tbe, GtkWidget *parent);
void			ephy_tb_editor_show		(EphyTbEditor *tbe);
/* the revert button is hidden initially */
GtkButton *		ephy_tb_editor_get_revert_button	(EphyTbEditor *tbe);

G_END_DECLS

#endif

