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
#include <bonobo/bonobo-ui-toolbar-button-item.h>
#include <bonobo/bonobo-property-bag.h>
#include <gtk/gtkstock.h>
#include <string.h>

#include "ephy-tbi-std-toolitem.h"
#include "ephy-gobject-misc.h"
#include "ephy-marshal.h"
#include "ephy-bonobo-extensions.h"

#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);
//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

/**
 * Private data
 */
struct _EphyTbiStdToolitemPrivate 
{
	GtkWidget *widget;
	
	EphyTbiStdToolitemItem item;
};


/**
 * Private functions, only availble from this file
 */
static void		ephy_tbi_std_toolitem_class_init	(EphyTbiStdToolitemClass *klass);
static void		ephy_tbi_std_toolitem_init		(EphyTbiStdToolitem *tb);
static void		ephy_tbi_std_toolitem_finalize_impl (GObject *o);
static GtkWidget *	ephy_tbi_std_toolitem_get_widget_impl (EphyTbItem *i);
static GdkPixbuf *	ephy_tbi_std_toolitem_get_icon_impl (EphyTbItem *i);
static gchar *		ephy_tbi_std_toolitem_get_name_human_impl (EphyTbItem *i);
static gchar *		ephy_tbi_std_toolitem_to_string_impl (EphyTbItem *i);
static gboolean		ephy_tbi_std_toolitem_is_unique_impl (EphyTbItem *i);
static EphyTbItem *	ephy_tbi_std_toolitem_clone_impl	(EphyTbItem *i);
static void		ephy_tbi_std_toolitem_parse_properties_impl (EphyTbItem *i, const gchar *props);
static void		ephy_tbi_std_toolitem_add_to_bonobo_tb_impl (EphyTbItem *i, 
								    BonoboUIComponent *ui, 
								    const char *container_path,
								    guint index);

static gpointer ephy_tb_item_class;

/**
 * TbiStdToolitem object
 */

MAKE_GET_TYPE (ephy_tbi_std_toolitem, "EphyTbiStdToolitem", EphyTbiStdToolitem, 
	       ephy_tbi_std_toolitem_class_init, 
	       ephy_tbi_std_toolitem_init, EPHY_TYPE_TB_ITEM);

static void
ephy_tbi_std_toolitem_class_init (EphyTbiStdToolitemClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_tbi_std_toolitem_finalize_impl;
	
	EPHY_TB_ITEM_CLASS (klass)->get_widget = ephy_tbi_std_toolitem_get_widget_impl;
	EPHY_TB_ITEM_CLASS (klass)->get_icon = ephy_tbi_std_toolitem_get_icon_impl;
	EPHY_TB_ITEM_CLASS (klass)->get_name_human = ephy_tbi_std_toolitem_get_name_human_impl;
	EPHY_TB_ITEM_CLASS (klass)->to_string = ephy_tbi_std_toolitem_to_string_impl;
	EPHY_TB_ITEM_CLASS (klass)->is_unique = ephy_tbi_std_toolitem_is_unique_impl;
	EPHY_TB_ITEM_CLASS (klass)->clone = ephy_tbi_std_toolitem_clone_impl;
	EPHY_TB_ITEM_CLASS (klass)->parse_properties = ephy_tbi_std_toolitem_parse_properties_impl;
	EPHY_TB_ITEM_CLASS (klass)->add_to_bonobo_tb = ephy_tbi_std_toolitem_add_to_bonobo_tb_impl;
	
	ephy_tb_item_class = g_type_class_peek_parent (klass);
}

static void 
ephy_tbi_std_toolitem_init (EphyTbiStdToolitem *tb)
{
	EphyTbiStdToolitemPrivate *p = g_new0 (EphyTbiStdToolitemPrivate, 1);
	tb->priv = p;

	p->item = EPHY_TBI_STD_TOOLITEM_BACK;
}

EphyTbiStdToolitem *
ephy_tbi_std_toolitem_new (void)
{
	EphyTbiStdToolitem *ret = g_object_new (EPHY_TYPE_TBI_STD_TOOLITEM, NULL);
	return ret;
}

static void
ephy_tbi_std_toolitem_finalize_impl (GObject *o)
{
	EphyTbiStdToolitem *it = EPHY_TBI_STD_TOOLITEM (o);
	EphyTbiStdToolitemPrivate *p = it->priv;

	if (p->widget)
	{
		g_object_unref (p->widget);
	}

	g_free (p);
	
	DEBUG_MSG (("EphyTbiStdToolitem finalized\n"));
	
	G_OBJECT_CLASS (ephy_tb_item_class)->finalize (o);
}

static GtkWidget *
ephy_tbi_std_toolitem_get_widget_impl (EphyTbItem *i)
{
	/* no widget avaible ... */
	return NULL;
}

static GdkPixbuf *
ephy_tbi_std_toolitem_get_icon_impl (EphyTbItem *i)
{
	EphyTbiStdToolitemPrivate *p = EPHY_TBI_STD_TOOLITEM (i)->priv;

	static GdkPixbuf *pb_up = NULL;
	static GdkPixbuf *pb_back = NULL;
	static GdkPixbuf *pb_forward = NULL;
	static GdkPixbuf *pb_stop = NULL;
	static GdkPixbuf *pb_reload = NULL;
	static GdkPixbuf *pb_home = NULL;
	static GdkPixbuf *pb_go = NULL;
	static GdkPixbuf *pb_new = NULL;
	
	if (!pb_up)
	{
		/* what's the easier way? */
		GtkWidget *b = gtk_spin_button_new_with_range (0, 1, 0.5);
		pb_up = gtk_widget_render_icon (b,
						GTK_STOCK_GO_UP,
						GTK_ICON_SIZE_SMALL_TOOLBAR,
						NULL);
		pb_back = gtk_widget_render_icon (b,
						  GTK_STOCK_GO_BACK,
						  GTK_ICON_SIZE_SMALL_TOOLBAR,
						  NULL);
		pb_forward = gtk_widget_render_icon (b,
						     GTK_STOCK_GO_FORWARD,
						     GTK_ICON_SIZE_SMALL_TOOLBAR,
						     NULL);
		pb_stop = gtk_widget_render_icon (b,
						  GTK_STOCK_STOP,
						  GTK_ICON_SIZE_SMALL_TOOLBAR,
						  NULL);
		pb_reload = gtk_widget_render_icon (b,
						    GTK_STOCK_REFRESH,
						    GTK_ICON_SIZE_SMALL_TOOLBAR,
						    NULL);
		pb_home = gtk_widget_render_icon (b,
						  GTK_STOCK_HOME,
						  GTK_ICON_SIZE_SMALL_TOOLBAR,
						  NULL);
		pb_go = gtk_widget_render_icon (b, 
						GTK_STOCK_JUMP_TO,
						GTK_ICON_SIZE_SMALL_TOOLBAR,
						NULL);
		pb_new = gtk_widget_render_icon (b, 
						 GTK_STOCK_NEW,
						 GTK_ICON_SIZE_SMALL_TOOLBAR,
						 NULL);
		gtk_widget_destroy (b);
	}

	switch (p->item)
	{
	case EPHY_TBI_STD_TOOLITEM_BACK:
		return g_object_ref (pb_back);
		break;
	case EPHY_TBI_STD_TOOLITEM_FORWARD:
		return g_object_ref (pb_forward);
		break;
	case EPHY_TBI_STD_TOOLITEM_UP:
		return g_object_ref (pb_up);
		break;
	case EPHY_TBI_STD_TOOLITEM_STOP:
		return g_object_ref (pb_stop);
		break;
	case EPHY_TBI_STD_TOOLITEM_RELOAD:
		return g_object_ref (pb_reload);
		break;
	case EPHY_TBI_STD_TOOLITEM_HOME:
		return g_object_ref (pb_home);
		break;
	case EPHY_TBI_STD_TOOLITEM_GO:
		return g_object_ref (pb_go);
		break;
	case EPHY_TBI_STD_TOOLITEM_NEW:
		return g_object_ref (pb_new);
		break;
	default:
		g_assert_not_reached ();
		return NULL;
	}
}

static gchar *
ephy_tbi_std_toolitem_get_name_human_impl (EphyTbItem *i)
{
	EphyTbiStdToolitemPrivate *p = EPHY_TBI_STD_TOOLITEM (i)->priv;
	const gchar *ret;

	switch (p->item)
	{
	case EPHY_TBI_STD_TOOLITEM_BACK:
		ret = _("Back");
		break;
	case EPHY_TBI_STD_TOOLITEM_FORWARD:
		ret = _("Forward");
		break;
	case EPHY_TBI_STD_TOOLITEM_UP:
		ret = _("Up");
		break;
	case EPHY_TBI_STD_TOOLITEM_STOP:
		ret = _("Stop");
		break;
	case EPHY_TBI_STD_TOOLITEM_RELOAD:
		ret = _("Reload");
		break;
	case EPHY_TBI_STD_TOOLITEM_HOME:
		ret = _("Home");
		break;
	case EPHY_TBI_STD_TOOLITEM_GO:
		ret = _("Go");
		break;
	case EPHY_TBI_STD_TOOLITEM_NEW:
		ret = _("New");
		break;
	default:
		g_assert_not_reached ();
		ret = "unknown";
	}

	return g_strdup (ret);
}

static gchar *
ephy_tbi_std_toolitem_to_string_impl (EphyTbItem *i)
{
	EphyTbiStdToolitemPrivate *p = EPHY_TBI_STD_TOOLITEM (i)->priv;

	/* if it had any properties, the string should include them */
	const char *sitem;

	switch (p->item)
	{
	case EPHY_TBI_STD_TOOLITEM_BACK:
		sitem = "back";
		break;
	case EPHY_TBI_STD_TOOLITEM_FORWARD:
		sitem = "forward";
		break;
	case EPHY_TBI_STD_TOOLITEM_UP:
		sitem = "up";
		break;
	case EPHY_TBI_STD_TOOLITEM_STOP:
		sitem = "stop";
		break;
	case EPHY_TBI_STD_TOOLITEM_RELOAD:
		sitem = "reload";
		break;
	case EPHY_TBI_STD_TOOLITEM_HOME:
		sitem = "home";
		break;
	case EPHY_TBI_STD_TOOLITEM_GO:
		sitem = "go";
		break;
	case EPHY_TBI_STD_TOOLITEM_NEW:
		sitem = "new";
		break;
	default:
		g_assert_not_reached ();
		sitem = "unknown";
	}

	return g_strdup_printf ("%s=std_toolitem(item=%s)", i->id, sitem);
}

static gboolean
ephy_tbi_std_toolitem_is_unique_impl (EphyTbItem *i)
{
	return TRUE;
}

static EphyTbItem *
ephy_tbi_std_toolitem_clone_impl (EphyTbItem *i)
{
	EphyTbiStdToolitemPrivate *p = EPHY_TBI_STD_TOOLITEM (i)->priv;

	EphyTbItem *ret = EPHY_TB_ITEM (ephy_tbi_std_toolitem_new ());
	
	ephy_tb_item_set_id (ret, i->id);

	/* should copy properties too, if any */
	ephy_tbi_std_toolitem_set_item (EPHY_TBI_STD_TOOLITEM (ret), p->item);

	return ret;
}


static void
ephy_tbi_std_toolitem_add_to_bonobo_tb_impl (EphyTbItem *i, BonoboUIComponent *ui, 
					    const char *container_path, guint index)
{
	EphyTbiStdToolitemPrivate *p = EPHY_TBI_STD_TOOLITEM (i)->priv;
	gchar *xml_item;

	switch (p->item)
	{
	case EPHY_TBI_STD_TOOLITEM_BACK:
		xml_item = g_strdup_printf  
			("<toolitem name=\"Back\" "
			 "label=\"%s\" "
			 "pixtype=\"stock\" pixname=\"gtk-go-back\" "
			 "priority=\"1\" "
			 "verb=\"GoBack\"/>", _("Back"));;
		break;
	case EPHY_TBI_STD_TOOLITEM_FORWARD:
		xml_item = g_strdup_printf 
			("<toolitem name=\"Forward\" "
			 "label=\"%s\" "
			 "pixtype=\"stock\" pixname=\"gtk-go-forward\" "
			 "verb=\"GoForward\"/>", _("Forward"));
		break;
	case EPHY_TBI_STD_TOOLITEM_UP:
		xml_item = g_strdup_printf
			("<toolitem name=\"Up\" "
			 "label=\"%s\" "
			 "pixtype=\"stock\" pixname=\"gtk-go-up\" "
			 "verb=\"GoUp\"/>", _("Up"));;
		break;
	case EPHY_TBI_STD_TOOLITEM_STOP:
		xml_item = g_strdup_printf 
			("<toolitem name=\"Stop\" "
			 "label=\"%s\" "
			 "pixtype=\"stock\" pixname=\"gtk-stop\" "
			 "verb=\"GoStop\"/>", _("Stop"));
		break;
	case EPHY_TBI_STD_TOOLITEM_RELOAD:
		xml_item = g_strdup_printf
			("<toolitem name=\"Reload\" "
			 "label=\"%s\" "
			 "pixtype=\"stock\" pixname=\"gtk-refresh\" "
			 "verb=\"GoReload\"/>", _("Reload"));
		break;
	case EPHY_TBI_STD_TOOLITEM_HOME:
		xml_item = g_strdup_printf
			("<toolitem name=\"Home\" "
			 "label=\"%s\" "
			 "pixtype=\"stock\" pixname=\"gtk-home\" "
			 "priority=\"1\" "
			 "verb=\"GoHome\"/>", _("Home"));;
		break;
	case EPHY_TBI_STD_TOOLITEM_GO:
		xml_item = g_strdup_printf
			("<toolitem name=\"Go\" "
			 "label=\"%s\" "
			 "pixtype=\"stock\" pixname=\"gtk-jump-to\" "
			 "verb=\"GoGo\"/>", _("Go"));;
		break;
	case EPHY_TBI_STD_TOOLITEM_NEW:
		xml_item = g_strdup_printf
			("<toolitem name=\"New\" "
			 "label=\"%s\" "
			 "pixtype=\"stock\" pixname=\"gtk-new\" "
			 "verb=\"FileNew\"/>", _("New"));;
		break;

	default:
		g_assert_not_reached ();
		xml_item = g_strdup ("");
	}

	bonobo_ui_component_set (ui, container_path, xml_item, NULL);
	g_free (xml_item);
}

static void
ephy_tbi_std_toolitem_parse_properties_impl (EphyTbItem *it, const gchar *props)
{
	EphyTbiStdToolitem *a = EPHY_TBI_STD_TOOLITEM (it);

	/* yes, this is quite hacky, but works */

	/* we have one property */
	const gchar *item_prop;

	item_prop = strstr (props, "item=");
	if (item_prop)
	{
		item_prop += strlen ("item=");
		if (!strncmp (item_prop, "back", 4))
		{
			ephy_tbi_std_toolitem_set_item (a, EPHY_TBI_STD_TOOLITEM_BACK);
		}
		else if (!strncmp (item_prop, "forward", 4))
		{
			ephy_tbi_std_toolitem_set_item (a, EPHY_TBI_STD_TOOLITEM_FORWARD);
		}
		else if (!strncmp (item_prop, "up", 2))
		{
			ephy_tbi_std_toolitem_set_item (a, EPHY_TBI_STD_TOOLITEM_UP);
		}
		else if (!strncmp (item_prop, "stop", 4))
		{
			ephy_tbi_std_toolitem_set_item (a, EPHY_TBI_STD_TOOLITEM_STOP);
		}
		else if (!strncmp (item_prop, "home", 4))
		{
			ephy_tbi_std_toolitem_set_item (a, EPHY_TBI_STD_TOOLITEM_HOME);
		}
		else if (!strncmp (item_prop, "go", 2))
		{
			ephy_tbi_std_toolitem_set_item (a, EPHY_TBI_STD_TOOLITEM_GO);
		}
		else if (!strncmp (item_prop, "reload", 6))
		{
			ephy_tbi_std_toolitem_set_item (a, EPHY_TBI_STD_TOOLITEM_RELOAD);
		}
		else if (!strncmp (item_prop, "new", 3))
		{
			ephy_tbi_std_toolitem_set_item (a, EPHY_TBI_STD_TOOLITEM_NEW);
		}

	}
}

void
ephy_tbi_std_toolitem_set_item (EphyTbiStdToolitem *a, EphyTbiStdToolitemItem i)
{
	EphyTbiStdToolitemPrivate *p = a->priv;

	g_return_if_fail (i == EPHY_TBI_STD_TOOLITEM_UP
			  || i == EPHY_TBI_STD_TOOLITEM_BACK
			  || i == EPHY_TBI_STD_TOOLITEM_FORWARD
			  || i == EPHY_TBI_STD_TOOLITEM_STOP
			  || i == EPHY_TBI_STD_TOOLITEM_RELOAD
			  || i == EPHY_TBI_STD_TOOLITEM_GO
			  || i == EPHY_TBI_STD_TOOLITEM_HOME
			  || i == EPHY_TBI_STD_TOOLITEM_NEW);

	p->item = i;
}

