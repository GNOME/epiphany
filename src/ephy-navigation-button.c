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

#include "ephy-gobject-misc.h"
#include "ephy-marshal.h"
#include "ephy-tb-button.h"
#include "ephy-gui.h"
#include "ephy-string.h"
#include "ephy-navigation-button.h"
#include "ephy-debug.h"

#include <gtk/gtkstock.h>
#include <string.h>
#include <libgnome/gnome-i18n.h>

/**
 * Private data
 */
struct _EphyNavigationButtonPrivate
{
	EphyTbButton *widget;
	EphyNavigationDirection direction;
	gboolean show_arrow;
	gboolean sensitive;
};

enum
{
        TOOLBAR_ITEM_STYLE_PROP,
        TOOLBAR_ITEM_ORIENTATION_PROP,
	TOOLBAR_ITEM_WANT_LABEL_PROP
};

/**
 * Private functions, only availble from this file
 */
static void		ephy_navigation_button_class_init		(EphyNavigationButtonClass *klass);
static void		ephy_navigation_button_init			(EphyNavigationButton *tb);
static void		ephy_navigation_button_finalize_impl		(GObject *o);
static GtkWidget *	ephy_navigation_button_get_widget_impl	(EphyTbItem *i);
static GdkPixbuf *	ephy_navigation_button_get_icon_impl		(EphyTbItem *i);
static gchar *		ephy_navigation_button_get_name_human_impl	(EphyTbItem *i);
static gchar *		ephy_navigation_button_to_string_impl		(EphyTbItem *i);
static EphyTbItem *	ephy_navigation_button_clone_impl		(EphyTbItem *i);
static void		ephy_navigation_button_parse_properties_impl	(EphyTbItem *i, const gchar *props);
static void		ephy_navigation_button_menu_activated_cb	(EphyTbButton *w, EphyNavigationButton *b);
static void		ephy_navigation_button_clicked_cb		(GtkWidget *w, EphyNavigationButton *b);


static gpointer ephy_tb_item_class;

/**
 * TbiZoom object
 */

MAKE_GET_TYPE (ephy_navigation_button, "EphyNavigationButton", EphyNavigationButton,
	       ephy_navigation_button_class_init,
	       ephy_navigation_button_init, EPHY_TYPE_TBI);

static void
ephy_navigation_button_class_init (EphyNavigationButtonClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_navigation_button_finalize_impl;

	EPHY_TB_ITEM_CLASS (klass)->get_widget = ephy_navigation_button_get_widget_impl;
	EPHY_TB_ITEM_CLASS (klass)->get_icon = ephy_navigation_button_get_icon_impl;
	EPHY_TB_ITEM_CLASS (klass)->get_name_human = ephy_navigation_button_get_name_human_impl;
	EPHY_TB_ITEM_CLASS (klass)->to_string = ephy_navigation_button_to_string_impl;
	EPHY_TB_ITEM_CLASS (klass)->clone = ephy_navigation_button_clone_impl;
	EPHY_TB_ITEM_CLASS (klass)->parse_properties = ephy_navigation_button_parse_properties_impl;

	ephy_tb_item_class = g_type_class_peek_parent (klass);
}

static void 
ephy_navigation_button_init (EphyNavigationButton *tbi)
{
	EphyNavigationButtonPrivate *p = g_new0 (EphyNavigationButtonPrivate, 1);
	tbi->priv = p;

	p->direction = EPHY_NAVIGATION_DIRECTION_UP;
	p->show_arrow = TRUE;
	p->sensitive = TRUE;
}

EphyNavigationButton *
ephy_navigation_button_new (void)
{
	EphyNavigationButton *ret = g_object_new (EPHY_TYPE_NAVIGATION_BUTTON, NULL);
	return ret;
}

static void
ephy_navigation_button_finalize_impl (GObject *o)
{
	EphyNavigationButton *it = EPHY_NAVIGATION_BUTTON (o);
	EphyNavigationButtonPrivate *p = it->priv;

	if (p->widget)
	{
		g_object_unref (p->widget);
	}

	g_free (p);

	LOG ("EphyNavigationButton finalized")

	G_OBJECT_CLASS (ephy_tb_item_class)->finalize (o);
}

static void
ephy_navigation_button_setup_widget (EphyNavigationButton *b)
{
	EphyNavigationButtonPrivate *p = b->priv;
	const gchar *label;
	const gchar *tip;
	gboolean prio;

	if (!p->widget)
	{
		ephy_navigation_button_get_widget_impl (EPHY_TB_ITEM (b));
	}
	g_assert (EPHY_IS_TB_BUTTON (p->widget));

	switch (p->direction)
	{
	case EPHY_NAVIGATION_DIRECTION_UP:
		label = "gtk-go-up";
		tip = _("Go up");
		prio = FALSE;
		break;
	case EPHY_NAVIGATION_DIRECTION_BACK:
		label = "gtk-go-back";
		tip = _("Go back");
		prio = TRUE;
		break;
	case EPHY_NAVIGATION_DIRECTION_FORWARD:
		label = "gtk-go-forward";
		tip = _("Go forward");
		prio = FALSE;
		break;
	default:
		g_assert_not_reached ();
		label = NULL;
		tip = NULL;
		prio = FALSE;
		break;
	}

	ephy_tb_button_set_label (p->widget, label);
	ephy_tb_button_set_tooltip_text (p->widget, tip);
	ephy_tb_button_set_priority (p->widget, prio);
	ephy_tb_button_set_show_arrow (p->widget, p->show_arrow);
	ephy_tb_button_set_sensitivity (p->widget, p->sensitive);
}

static GtkWidget *
ephy_navigation_button_get_widget_impl (EphyTbItem *i)
{
	EphyNavigationButton *iz = EPHY_NAVIGATION_BUTTON (i);
	EphyNavigationButtonPrivate *p = iz->priv;

	if (!p->widget)
	{
		p->widget = ephy_tb_button_new ();
		g_object_ref (p->widget);
		ephy_tb_button_set_use_stock (p->widget, TRUE);
		ephy_tb_button_set_enable_menu (p->widget, TRUE);

		ephy_navigation_button_setup_widget (iz);

		gtk_widget_show (GTK_WIDGET (p->widget));

		g_signal_connect (p->widget, "menu-activated",
				  G_CALLBACK (ephy_navigation_button_menu_activated_cb), i);
		g_signal_connect (ephy_tb_button_get_button (p->widget), "clicked",
				  G_CALLBACK (ephy_navigation_button_clicked_cb), i);
	}

	return GTK_WIDGET (p->widget);
}

static GdkPixbuf *
ephy_navigation_button_get_icon_impl (EphyTbItem *i)
{
	EphyNavigationButtonPrivate *p = EPHY_NAVIGATION_BUTTON (i)->priv;

	static GdkPixbuf *pb_up = NULL;
	static GdkPixbuf *pb_back = NULL;
	static GdkPixbuf *pb_forward = NULL;

	if (!pb_up)
	{
		/* what's the easier way? */
		GtkWidget *b = gtk_button_new ();
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
		gtk_widget_destroy (b);
	}

	switch (p->direction)
	{
	case EPHY_NAVIGATION_DIRECTION_BACK:
		return g_object_ref (pb_back);
		break;
	case EPHY_NAVIGATION_DIRECTION_FORWARD:
		return g_object_ref (pb_forward);
		break;
	case EPHY_NAVIGATION_DIRECTION_UP:
		return g_object_ref (pb_up);
		break;
	default:
		g_assert_not_reached ();
		return NULL;
	}
}

static gchar *
ephy_navigation_button_get_name_human_impl (EphyTbItem *i)
{
	EphyNavigationButtonPrivate *p = EPHY_NAVIGATION_BUTTON (i)->priv;
	const gchar *ret;

	switch (p->direction)
	{
	case EPHY_NAVIGATION_DIRECTION_BACK:
		ret = p->show_arrow
			? _("Back (with menu)")
			: _("Back");
		break;
	case EPHY_NAVIGATION_DIRECTION_FORWARD:
		ret = p->show_arrow
			? _("Forward (with menu)")
			: _("Forward");
		break;
	case EPHY_NAVIGATION_DIRECTION_UP:
		ret = p->show_arrow
			? _("Up (with menu)")
			: _("Up");
		break;
	default:
		g_assert_not_reached ();
		ret = "Error: unexpected direction";
		break;
	}

	return g_strdup (ret);
}

static gchar *
ephy_navigation_button_to_string_impl (EphyTbItem *i)
{
	EphyNavigationButtonPrivate *p = EPHY_NAVIGATION_BUTTON (i)->priv;

	/* if it had any properties, the string should include them */
	const char *sdir;

	switch (p->direction)
	{
	case EPHY_NAVIGATION_DIRECTION_BACK:
		sdir = "back";
		break;
	case EPHY_NAVIGATION_DIRECTION_FORWARD:
		sdir = "forward";
		break;
	case EPHY_NAVIGATION_DIRECTION_UP:
		sdir = "up";
		break;
	default:
		g_assert_not_reached ();
		sdir = "unknown";
	}

	return g_strdup_printf ("%s=navigation_button(direction=%s,arrow=%s)",
				i->id, sdir, p->show_arrow ? "TRUE" : "FALSE");
}

static EphyTbItem *
ephy_navigation_button_clone_impl (EphyTbItem *i)
{
	EphyTbItem *ret = EPHY_TB_ITEM (ephy_navigation_button_new ());
	EphyNavigationButtonPrivate *p = EPHY_NAVIGATION_BUTTON (i)->priv;

	ephy_tb_item_set_id (ret, i->id);

	ephy_navigation_button_set_direction (EPHY_NAVIGATION_BUTTON (ret), p->direction);
	ephy_navigation_button_set_show_arrow (EPHY_NAVIGATION_BUTTON (ret), p->show_arrow);

	return ret;
}

static void
ephy_navigation_button_parse_properties_impl (EphyTbItem *it, const gchar *props)
{
	EphyNavigationButton *b = EPHY_NAVIGATION_BUTTON (it);

	/* we have two properties, the direction and the arrow */
	const gchar *direc_prop;
	const gchar *show_arrow_prop;

	direc_prop = strstr (props, "direction=");
	if (direc_prop)
	{
		direc_prop += strlen ("direction=");
		if (!strncmp (direc_prop, "back", 4))
		{
			ephy_navigation_button_set_direction (b, EPHY_NAVIGATION_DIRECTION_BACK);
		}
		else if (!strncmp (direc_prop, "forward", 4))
		{
			ephy_navigation_button_set_direction (b, EPHY_NAVIGATION_DIRECTION_FORWARD);
		}
		else if (!strncmp (direc_prop, "up", 2))
		{
			ephy_navigation_button_set_direction (b, EPHY_NAVIGATION_DIRECTION_UP);
		}
	}

	show_arrow_prop = strstr (props, "arrow=");
	if (show_arrow_prop)
	{
		show_arrow_prop += strlen ("arrow=");
		if (show_arrow_prop[0] == 'T')
		{
			ephy_navigation_button_set_show_arrow (b, TRUE);
		}
		else
		{
			ephy_navigation_button_set_show_arrow (b, FALSE);
		}
	}
}


void
ephy_navigation_button_set_direction (EphyNavigationButton *b,
					EphyNavigationDirection d)
{
	EphyNavigationButtonPrivate *p = b->priv;
	p->direction = d;
	ephy_navigation_button_setup_widget (b);
}

void
ephy_navigation_button_set_show_arrow	(EphyNavigationButton *b,
					 gboolean value)
{
	EphyNavigationButtonPrivate *p = b->priv;
	p->show_arrow = value;
	if (p->widget)
	{
		ephy_tb_button_set_show_arrow (p->widget, p->show_arrow);
	}
	else
	{
		ephy_navigation_button_get_widget_impl (EPHY_TB_ITEM (b));
	}
}

EphyNavigationDirection
ephy_navigation_button_get_direction (EphyNavigationButton *b)
{
	return b->priv->direction;
}

void
ephy_navigation_button_set_sensitive (EphyNavigationButton *b, gboolean s)
{
	EphyNavigationButtonPrivate *p = b->priv;
	p->sensitive = s;
	if (p->widget)
	{
		ephy_tb_button_set_sensitivity (p->widget, s);
	}
	else
	{
		ephy_navigation_button_get_widget_impl (EPHY_TB_ITEM (b));
	}
}

static void
ephy_navigation_button_clicked_cb (GtkWidget *w, EphyNavigationButton *b)
{
	EphyNavigationButtonPrivate *p = b->priv;
	EphyWindow *window;
	EphyEmbed *embed;

	window = ephy_tbi_get_window (EPHY_TBI (b));
	g_return_if_fail (window != NULL);

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	switch (p->direction)
	{
	case EPHY_NAVIGATION_DIRECTION_UP:
		ephy_embed_go_up (embed);
		break;
	case EPHY_NAVIGATION_DIRECTION_BACK:
		ephy_embed_go_back (embed);
		break;
	case EPHY_NAVIGATION_DIRECTION_FORWARD:
		ephy_embed_go_forward (embed);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}

/* TODO: clean all this, it came from toolbar.c and is messy */

static GtkWidget *
new_history_menu_item (gchar *origtext,
                       const GdkPixbuf *ico)
{
        GtkWidget *item = gtk_image_menu_item_new ();
        GtkWidget *hb = gtk_hbox_new (FALSE, 0);
        GtkWidget *label = gtk_label_new (origtext);

        gtk_box_pack_start (GTK_BOX (hb), label, FALSE, FALSE, 0);
        gtk_container_add (GTK_CONTAINER (item), hb);

        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
                                       gtk_image_new_from_pixbuf ((GdkPixbuf *) ico));

        gtk_widget_show_all (item);

        return item;
}

static void
activate_back_or_forward_menu_item_cb (GtkWidget *menu, EphyWindow *window)
{
	EphyEmbed *embed;
	int go_nth;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	go_nth = (int)g_object_get_data (G_OBJECT(menu), "go_nth");

	ephy_embed_shistory_go_nth (embed, go_nth);
}

static void
activate_up_menu_item_cb (GtkWidget *menu, EphyWindow *window)
{
	EphyEmbed *embed;
	int go_nth;
	GSList *l;
	gchar *url;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	go_nth = (int)g_object_get_data (G_OBJECT(menu), "go_nth");

	ephy_embed_get_go_up_list (embed, &l);

	url = g_slist_nth_data (l, go_nth);
	if (url)
	{
		ephy_embed_load_url (embed, url);
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);
}

static void
setup_back_or_forward_menu (EphyWindow *window, GtkMenuShell *ms, EphyNavigationDirection dir)
{
	int pos, count;
	EphyEmbed *embed;
	int start, end;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_shistory_get_pos (embed, &pos);
	ephy_embed_shistory_count (embed, &count);

	if (count == 0) return;

	if (dir == EPHY_NAVIGATION_DIRECTION_BACK)
	{
		start = pos - 1;
		end = -1;
	}
	else
	{
		start = pos + 1;
		end = count;
	}

	while (start != end)
	{
		char *title, *url;
		GtkWidget *item;
		ephy_embed_shistory_get_nth (embed, start, FALSE, &url, &title);
		item = new_history_menu_item (url, NULL);
		gtk_menu_shell_append (ms, item);
		g_object_set_data (G_OBJECT (item), "go_nth", GINT_TO_POINTER (start));
		g_signal_connect (item, "activate",
                                  G_CALLBACK (activate_back_or_forward_menu_item_cb), window);
		gtk_widget_show_all (item);

		g_free (url);
		g_free (title);

		if (start < end)
		{
			start++;
		}
		else
		{
			start--;
		}
	}
}

static void
setup_up_menu (EphyWindow *window, GtkMenuShell *ms)
{
	EphyEmbed *embed;
	GSList *l;
	GSList *li;
	int count = 0;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_get_go_up_list (embed, &l);

	for (li = l; li; li = li->next)
	{
		char *url = li->data;
		GtkWidget *item;

		item = new_history_menu_item (url, NULL);
		gtk_menu_shell_append (ms, item);
		g_object_set_data (G_OBJECT(item), "go_nth", GINT_TO_POINTER (count));
		g_signal_connect (item, "activate",
                                  G_CALLBACK (activate_up_menu_item_cb), window);
		gtk_widget_show_all (item);
		count ++;
	}

	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);
}

static void
ephy_navigation_button_menu_activated_cb (EphyTbButton *w, EphyNavigationButton *b)
{
	EphyNavigationButtonPrivate *p = b->priv;
	GtkMenuShell *ms = ephy_tb_button_get_menu (p->widget);
	EphyWindow *win = ephy_tbi_get_window (EPHY_TBI (b));
	GList *children;
	GList *li;

	children = gtk_container_get_children (GTK_CONTAINER (ms));
	for (li = children; li; li = li->next)
	{
		gtk_container_remove (GTK_CONTAINER (ms), li->data);
	}
	g_list_free (children);

	switch (p->direction)
	{
	case EPHY_NAVIGATION_DIRECTION_UP:
		setup_up_menu (win, ms);
		break;
	case EPHY_NAVIGATION_DIRECTION_FORWARD:
	case EPHY_NAVIGATION_DIRECTION_BACK:
		setup_back_or_forward_menu (win, ms, p->direction);
		break;
	default:
		g_assert_not_reached ();
		break;
	}
}
