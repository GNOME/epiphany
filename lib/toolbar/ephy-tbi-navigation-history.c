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
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkstock.h>
#include <string.h>

#include "ephy-tbi-navigation-history.h"
#include "ephy-gobject-misc.h"
#include "ephy-marshal.h"
#include "ephy-bonobo-extensions.h"

#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);
//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

/**
 * Private data
 */
struct _EphyTbiNavigationHistoryPrivate
{
	GtkWidget *widget;

	EphyTbiNavigationHistoryDirection direction;
};

enum
{
        TOOLBAR_ITEM_STYLE_PROP,
        TOOLBAR_ITEM_ORIENTATION_PROP,
	TOOLBAR_ITEM_PRIORITY_PROP
};

/**
 * Private functions, only availble from this file
 */
static void		ephy_tbi_navigation_history_class_init		(EphyTbiNavigationHistoryClass *klass);
static void		ephy_tbi_navigation_history_init		(EphyTbiNavigationHistory *tb);
static void		ephy_tbi_navigation_history_finalize_impl	(GObject *o);
static GtkWidget *	ephy_tbi_navigation_history_get_widget_impl	(EphyTbItem *i);
static GdkPixbuf *	ephy_tbi_navigation_history_get_icon_impl	(EphyTbItem *i);
static gchar *		ephy_tbi_navigation_history_get_name_human_impl	(EphyTbItem *i);
static gchar *		ephy_tbi_navigation_history_to_string_impl	(EphyTbItem *i);
static gboolean		ephy_tbi_navigation_history_is_unique_impl	(EphyTbItem *i);
static EphyTbItem *	ephy_tbi_navigation_history_clone_impl		(EphyTbItem *i);
static void		ephy_tbi_navigation_history_parse_properties_impl (EphyTbItem *i, const gchar *props);
static void		ephy_tbi_navigation_history_add_to_bonobo_tb_impl (EphyTbItem *i,
									  BonoboUIComponent *ui,
									  const char *container_path,
									  guint index);

static gpointer ephy_tb_item_class;

/**
 * TbiNavigationHistory object
 */

MAKE_GET_TYPE (ephy_tbi_navigation_history, "EphyTbiNavigationHistory", EphyTbiNavigationHistory,
	       ephy_tbi_navigation_history_class_init,
	       ephy_tbi_navigation_history_init, EPHY_TYPE_TB_ITEM);

static void
ephy_tbi_navigation_history_class_init (EphyTbiNavigationHistoryClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_tbi_navigation_history_finalize_impl;

	EPHY_TB_ITEM_CLASS (klass)->get_widget = ephy_tbi_navigation_history_get_widget_impl;
	EPHY_TB_ITEM_CLASS (klass)->get_icon = ephy_tbi_navigation_history_get_icon_impl;
	EPHY_TB_ITEM_CLASS (klass)->get_name_human = ephy_tbi_navigation_history_get_name_human_impl;
	EPHY_TB_ITEM_CLASS (klass)->to_string = ephy_tbi_navigation_history_to_string_impl;
	EPHY_TB_ITEM_CLASS (klass)->is_unique = ephy_tbi_navigation_history_is_unique_impl;
	EPHY_TB_ITEM_CLASS (klass)->clone = ephy_tbi_navigation_history_clone_impl;
	EPHY_TB_ITEM_CLASS (klass)->parse_properties = ephy_tbi_navigation_history_parse_properties_impl;
	EPHY_TB_ITEM_CLASS (klass)->add_to_bonobo_tb = ephy_tbi_navigation_history_add_to_bonobo_tb_impl;

	ephy_tb_item_class = g_type_class_peek_parent (klass);
}

static void
ephy_tbi_navigation_history_init (EphyTbiNavigationHistory *tb)
{
	EphyTbiNavigationHistoryPrivate *p = g_new0 (EphyTbiNavigationHistoryPrivate, 1);
	tb->priv = p;

	p->direction = EPHY_TBI_NAVIGATION_HISTORY_BACK;
}

EphyTbiNavigationHistory *
ephy_tbi_navigation_history_new (void)
{
	EphyTbiNavigationHistory *ret = g_object_new (EPHY_TYPE_TBI_NAVIGATION_HISTORY, NULL);
	return ret;
}

static void
ephy_tbi_navigation_history_finalize_impl (GObject *o)
{
	EphyTbiNavigationHistory *it = EPHY_TBI_NAVIGATION_HISTORY (o);
	EphyTbiNavigationHistoryPrivate *p = it->priv;

	if (p->widget)
	{
		g_object_unref (p->widget);
	}

	g_free (p);

	DEBUG_MSG (("EphyTbiNavigationHistory finalized\n"));

	G_OBJECT_CLASS (ephy_tb_item_class)->finalize (o);
}

static GtkWidget *
ephy_tbi_navigation_history_get_widget_impl (EphyTbItem *i)
{
	EphyTbiNavigationHistory *iz = EPHY_TBI_NAVIGATION_HISTORY (i);
	EphyTbiNavigationHistoryPrivate *p = iz->priv;

	DEBUG_MSG (("in ephy_tbi_navigation_history_get_widget_impl\n"));
	if (!p->widget)
	{
		DEBUG_MSG (("in ephy_tbi_navigation_history_get_widget_impl, really\n"));

		p->widget = gtk_toggle_button_new ();
		gtk_button_set_relief (GTK_BUTTON (p->widget), GTK_RELIEF_NONE);

		gtk_container_add (GTK_CONTAINER (p->widget),
				   gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT));

		g_object_ref (p->widget);
		gtk_object_sink (GTK_OBJECT (p->widget));
	}

	return p->widget;
}

static GdkPixbuf *
ephy_tbi_navigation_history_get_icon_impl (EphyTbItem *i)
{
		return NULL;
}

static gchar *
ephy_tbi_navigation_history_get_name_human_impl (EphyTbItem *i)
{
	EphyTbiNavigationHistoryPrivate *p = EPHY_TBI_NAVIGATION_HISTORY (i)->priv;
	const gchar *ret;

	switch (p->direction)
	{
	case EPHY_TBI_NAVIGATION_HISTORY_BACK:
		ret = _("Back History");
		break;
	case EPHY_TBI_NAVIGATION_HISTORY_FORWARD:
		ret = _("Forward History");
		break;
	case EPHY_TBI_NAVIGATION_HISTORY_UP:
		ret = _("Up Several Levels");
		break;
	default:
		g_assert_not_reached ();
		ret = "unknown";
	}

	return g_strdup (ret);
}

static gchar *
ephy_tbi_navigation_history_to_string_impl (EphyTbItem *i)
{
	EphyTbiNavigationHistoryPrivate *p = EPHY_TBI_NAVIGATION_HISTORY (i)->priv;

	/* if it had any properties, the string should include them */
	const char *sdir;

	switch (p->direction)
	{
	case EPHY_TBI_NAVIGATION_HISTORY_BACK:
		sdir = "back";
		break;
	case EPHY_TBI_NAVIGATION_HISTORY_FORWARD:
		sdir = "forward";
		break;
	case EPHY_TBI_NAVIGATION_HISTORY_UP:
		sdir = "up";
		break;
	default:
		g_assert_not_reached ();
		sdir = "unknown";
	}

	return g_strdup_printf ("%s=navigation_history(direction=%s)", i->id, sdir);
}

static gboolean
ephy_tbi_navigation_history_is_unique_impl (EphyTbItem *i)
{
	return TRUE;
}

static EphyTbItem *
ephy_tbi_navigation_history_clone_impl (EphyTbItem *i)
{
	EphyTbiNavigationHistoryPrivate *p = EPHY_TBI_NAVIGATION_HISTORY (i)->priv;
	EphyTbItem *ret = EPHY_TB_ITEM (ephy_tbi_navigation_history_new ());

	ephy_tb_item_set_id (ret, i->id);

	/* should copy properties too, if any */
	ephy_tbi_navigation_history_set_direction (EPHY_TBI_NAVIGATION_HISTORY (ret), p->direction);

	return ret;
}

static void
ephy_tbi_navigation_history_property_set_cb (BonoboPropertyBag *bag,
					     const BonoboArg   *arg,
					     guint              arg_id,
					     CORBA_Environment *ev,
					     gpointer           user_data)
{
	BonoboControl *control;
	BonoboUIToolbarItem *item;
	GtkOrientation orientation;
	BonoboUIToolbarItemStyle style;

	control = BONOBO_CONTROL (user_data);
	item = BONOBO_UI_TOOLBAR_ITEM (bonobo_control_get_widget (control));

	switch (arg_id) {
	case TOOLBAR_ITEM_ORIENTATION_PROP:
		orientation = BONOBO_ARG_GET_INT (arg);
		bonobo_ui_toolbar_item_set_orientation (item, orientation);

		if (GTK_WIDGET (item)->parent) {
			gtk_widget_queue_resize (GTK_WIDGET (item)->parent);
		}
		break;
	case TOOLBAR_ITEM_STYLE_PROP:
		style = BONOBO_ARG_GET_INT (arg);
		bonobo_ui_toolbar_item_set_style (item, style);
		break;
	}
}

static void
ephy_tbi_navigation_history_add_to_bonobo_tb_impl (EphyTbItem *i, BonoboUIComponent *ui,
						   const char *container_path, guint index)
{
	BonoboPropertyBag *pb;
	BonoboControl *wrapper;
	BonoboUIToolbarItem *item;
	GtkWidget *button;

	DEBUG_MSG (("in ephy_tbi_navigation_history_add_to_bonobo_tb_impl\n"));

	item = BONOBO_UI_TOOLBAR_ITEM (bonobo_ui_toolbar_item_new ());

	button = ephy_tb_item_get_widget (i);
	gtk_container_add (GTK_CONTAINER (item), button);
	gtk_widget_show_all (GTK_WIDGET (item));

	wrapper = ephy_bonobo_add_numbered_control (ui, GTK_WIDGET (item), index, container_path);

	pb = bonobo_property_bag_new
		(NULL, ephy_tbi_navigation_history_property_set_cb, wrapper);
	bonobo_property_bag_add (pb, "style",
				 TOOLBAR_ITEM_STYLE_PROP,
				 BONOBO_ARG_INT, NULL, NULL,
				 Bonobo_PROPERTY_WRITEABLE);
	bonobo_property_bag_add (pb, "orientation",
				 TOOLBAR_ITEM_ORIENTATION_PROP,
				 BONOBO_ARG_INT, NULL, NULL,
				 Bonobo_PROPERTY_WRITEABLE);
	bonobo_control_set_properties (wrapper, BONOBO_OBJREF (pb), NULL);
	bonobo_object_unref (pb);
}

static void
ephy_tbi_navigation_history_parse_properties_impl (EphyTbItem *it, const gchar *props)
{
	EphyTbiNavigationHistory *a = EPHY_TBI_NAVIGATION_HISTORY (it);

	/* yes, this is quite hacky, but works */

	/* we have aproperty, the direction */
	const gchar *direc_prop;

	direc_prop = strstr (props, "direction=");
	if (direc_prop)
	{
		direc_prop += strlen ("direction=");
		if (!strncmp (direc_prop, "back", 4))
		{
			ephy_tbi_navigation_history_set_direction (a, EPHY_TBI_NAVIGATION_HISTORY_BACK);
		}
		else if (!strncmp (direc_prop, "forward", 4))
		{
			ephy_tbi_navigation_history_set_direction (a, EPHY_TBI_NAVIGATION_HISTORY_FORWARD);
		}
		else if (!strncmp (direc_prop, "up", 2))
		{
			ephy_tbi_navigation_history_set_direction (a, EPHY_TBI_NAVIGATION_HISTORY_UP);
		}
	}
}

void
ephy_tbi_navigation_history_set_direction (EphyTbiNavigationHistory *a, EphyTbiNavigationHistoryDirection d)
{
	EphyTbiNavigationHistoryPrivate *p = a->priv;

	g_return_if_fail (d == EPHY_TBI_NAVIGATION_HISTORY_UP
			  || d == EPHY_TBI_NAVIGATION_HISTORY_BACK
			  || d == EPHY_TBI_NAVIGATION_HISTORY_FORWARD);

	p->direction = d;

}

