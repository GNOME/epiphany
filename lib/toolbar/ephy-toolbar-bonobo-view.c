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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libgnome/gnome-i18n.h>

#include "ephy-gobject-misc.h"
#include "ephy-marshal.h"
#include "ephy-toolbar-bonobo-view.h"
#include "ephy-bonobo-extensions.h"

#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);
//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

/**
 * Private data
 */
struct _EphyTbBonoboViewPrivate
{
	EphyToolbar *tb;
	BonoboUIComponent *ui;
	gchar *path;
};

/**
 * Private functions, only availble from this file
 */
static void		ephy_tb_bonobo_view_class_init		(EphyTbBonoboViewClass *klass);
static void		ephy_tb_bonobo_view_init		(EphyTbBonoboView *tb);
static void		ephy_tb_bonobo_view_finalize_impl	(GObject *o);
static void		ephy_tb_bonobo_view_rebuild		(EphyTbBonoboView *tbv);
static void		ephy_tb_bonobo_view_tb_changed		(EphyToolbar *tb, EphyTbBonoboView *tbv);

static gpointer g_object_class;

/**
 * TbBonoboView object
 */

MAKE_GET_TYPE (ephy_tb_bonobo_view, "EphyTbBonoboView", EphyTbBonoboView, ephy_tb_bonobo_view_class_init,
	       ephy_tb_bonobo_view_init, G_TYPE_OBJECT);

static void
ephy_tb_bonobo_view_class_init (EphyTbBonoboViewClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_tb_bonobo_view_finalize_impl;

	g_object_class = g_type_class_peek_parent (klass);
}

static void
ephy_tb_bonobo_view_init (EphyTbBonoboView *tb)
{
	EphyTbBonoboViewPrivate *p = g_new0 (EphyTbBonoboViewPrivate, 1);
	tb->priv = p;
}

static void
ephy_tb_bonobo_view_finalize_impl (GObject *o)
{
	EphyTbBonoboView *tbv = EPHY_TB_BONOBO_VIEW (o);
	EphyTbBonoboViewPrivate *p = tbv->priv;

	if (p->tb)
	{
		g_signal_handlers_disconnect_matched (p->tb, G_SIGNAL_MATCH_DATA, 0, 0,
						      NULL, NULL, tbv);
		g_object_unref (p->tb);
	}
	if (p->ui)
	{
		g_object_unref (p->ui);
	}
	if (p->path)
	{
		g_free (p->path);
	}

	g_free (p);

	DEBUG_MSG (("EphyTbBonoboView finalized\n"));

	G_OBJECT_CLASS (g_object_class)->finalize (o);
}

EphyTbBonoboView *
ephy_tb_bonobo_view_new (void)
{
	EphyTbBonoboView *ret = g_object_new (EPHY_TYPE_TB_BONOBO_VIEW, NULL);
	return ret;
}

void
ephy_tb_bonobo_view_set_toolbar (EphyTbBonoboView *tbv, EphyToolbar *tb)
{
	EphyTbBonoboViewPrivate *p = tbv->priv;

	if (p->tb)
	{
		g_signal_handlers_disconnect_matched (p->tb, G_SIGNAL_MATCH_DATA, 0, 0,
						      NULL, NULL, tbv);
		g_object_unref (p->tb);
	}

	p->tb = g_object_ref (tb);
	g_signal_connect (p->tb, "changed", G_CALLBACK (ephy_tb_bonobo_view_tb_changed), tbv);

	if (p->ui)
	{
		ephy_tb_bonobo_view_rebuild (tbv);
	}
}

static void
ephy_tb_bonobo_view_tb_changed (EphyToolbar *tb, EphyTbBonoboView *tbv)
{
	EphyTbBonoboViewPrivate *p = tbv->priv;
	if (p->ui)
	{
		ephy_tb_bonobo_view_rebuild (tbv);
	}
}

void
ephy_tb_bonobo_view_set_path (EphyTbBonoboView *tbv,
			      BonoboUIComponent *ui,
			      const gchar *path)
{
	EphyTbBonoboViewPrivate *p = tbv->priv;

	if (p->ui)
	{
		g_object_unref (p->ui);
	}

	if (p->path)
	{
		g_free (p->path);
	}

	p->ui = g_object_ref (ui);
	p->path = g_strdup (path);

	if (p->tb)
	{
		ephy_tb_bonobo_view_rebuild (tbv);
	}
}

static void
ephy_tb_bonobo_view_rebuild (EphyTbBonoboView *tbv)
{
	EphyTbBonoboViewPrivate *p = tbv->priv;
	GSList *items;
	GSList *li;
	uint index = 0;

	g_return_if_fail (EPHY_IS_TOOLBAR (p->tb));
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (p->ui));
	g_return_if_fail (p->path);

	DEBUG_MSG (("Rebuilding EphyTbBonoboView\n"));

	ephy_bonobo_clear_path (p->ui, p->path);

	items = (GSList *) ephy_toolbar_get_item_list (p->tb);
	for (li = items; li; li = li->next)
	{
		ephy_tb_item_add_to_bonobo_tb (li->data, p->ui, p->path, index++);
	}

	DEBUG_MSG (("Rebuilt EphyTbBonoboView\n"));
}

