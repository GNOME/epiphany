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

#ifndef EPHY_TB_BUTTON_H
#define EPHY_TB_BUTTON_H

#include <gtk/gtkhbox.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtkbutton.h>

G_BEGIN_DECLS

/* object forward declarations */

typedef struct _EphyTbButton EphyTbButton;
typedef struct _EphyTbButtonClass EphyTbButtonClass;
typedef struct _EphyTbButtonPrivate EphyTbButtonPrivate;

/**
 * TbButton object
 */

#define EPHY_TYPE_TB_BUTTON		(ephy_tb_button_get_type())
#define EPHY_TB_BUTTON(object)		(G_TYPE_CHECK_INSTANCE_CAST((object), EPHY_TYPE_TB_BUTTON,\
					 EphyTbButton))
#define EPHY_TB_BUTTON_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EPHY_TYPE_TB_BUTTON,\
					 EphyTbButtonClass))
#define EPHY_IS_TB_BUTTON(object)	(G_TYPE_CHECK_INSTANCE_TYPE((object), EPHY_TYPE_TB_BUTTON))
#define EPHY_IS_TB_BUTTON_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EPHY_TYPE_TB_BUTTON))
#define EPHY_TB_BUTTON_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_TB_BUTTON,\
					 EphyTbButtonClass))

struct _EphyTbButtonClass
{
	GtkHBoxClass parent_class;

	void		(*menu_activated)		(EphyTbButton *b);
};

/* Remember: fields are public read-only */
struct _EphyTbButton
{
	GtkHBox parent_object;
	EphyTbButtonPrivate *priv;
};

/* this class is abstract */

GType			ephy_tb_button_get_type		(void);
EphyTbButton *		ephy_tb_button_new		(void);
void			ephy_tb_button_set_label	(EphyTbButton *b, const gchar *text);
void			ephy_tb_button_set_use_stock	(EphyTbButton *b, gboolean value);
void			ephy_tb_button_set_priority	(EphyTbButton *b, gboolean priority);
void			ephy_tb_button_set_image	(EphyTbButton *b, GtkWidget *image);
void			ephy_tb_button_set_tooltip_text	(EphyTbButton *b, const gchar *text);
void			ephy_tb_button_set_show_arrow	(EphyTbButton *b, gboolean value);
void			ephy_tb_button_set_enable_menu	(EphyTbButton *b, gboolean value);
GtkMenuShell *		ephy_tb_button_get_menu		(EphyTbButton *b);
GtkButton *		ephy_tb_button_get_button	(EphyTbButton *b);
void			ephy_tb_button_set_sensitivity	(EphyTbButton *b, gboolean value);

G_END_DECLS

#endif
