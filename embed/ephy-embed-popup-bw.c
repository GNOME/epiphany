/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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

#include "ephy-embed-popup-bw.h"
#include "ephy-gobject-misc.h"

#include <gtk/gtkmain.h>

enum
{
        PROP_0,
        PROP_BONOBO_WINDOW
};

struct EphyEmbedPopupBWPrivate
{
	BonoboWindow *window;
	GtkWidget *menu;
};

static void
ephy_embed_popup_bw_class_init (EphyEmbedPopupBWClass *klass);
static void
ephy_embed_popup_bw_init (EphyEmbedPopupBW *gep);
static void
ephy_embed_popup_bw_finalize (GObject *object);
static void
ephy_embed_popup_bw_set_property (GObject *object,
				  guint prop_id,
				  const GValue *value,
				  GParamSpec *pspec);
static void
ephy_embed_popup_bw_get_property (GObject *object,
				  guint prop_id,
				  GValue *value,
				  GParamSpec *pspec);
static void
ephy_embed_popup_bw_set_window (EphyEmbedPopupBW *p,
				BonoboWindow *window);
static void
ephy_embed_popup_bw_show_impl (EphyEmbedPopup *p,
			       EphyEmbed *embed);

static EphyEmbedPopupClass *parent_class = NULL;

MAKE_GET_TYPE (ephy_embed_popup_bw, "EphyEmbedPopupBW", EphyEmbedPopupBW,
	       ephy_embed_popup_bw_class_init, ephy_embed_popup_bw_init, EPHY_EMBED_POPUP_TYPE);

static void
ephy_embed_popup_bw_class_init (EphyEmbedPopupBWClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = EPHY_EMBED_POPUP_CLASS (g_type_class_peek_parent (klass));

        object_class->finalize = ephy_embed_popup_bw_finalize;
	object_class->set_property = ephy_embed_popup_bw_set_property;
        object_class->get_property = ephy_embed_popup_bw_get_property;

	g_object_class_install_property (object_class,
                                         PROP_BONOBO_WINDOW,
                                         g_param_spec_object ("BonoboWindow",
                                                              "BonoboWindow",
                                                              "Bonobo window",
                                                              BONOBO_TYPE_WINDOW,
                                                              G_PARAM_READWRITE));

	EPHY_EMBED_POPUP_CLASS (klass)->show = ephy_embed_popup_bw_show_impl;
}

static void
ephy_embed_popup_bw_init (EphyEmbedPopupBW *gep)
{
        gep->priv = g_new0 (EphyEmbedPopupBWPrivate, 1);
	gep->priv->window = NULL;
	gep->priv->menu = NULL;
}

static void
ephy_embed_popup_bw_finalize (GObject *object)
{
	EphyEmbedPopupBW *gep;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_EPHY_EMBED_POPUP_BW (object));

        gep = EPHY_EMBED_POPUP_BW (object);

        g_return_if_fail (gep->priv != NULL);

        g_free (gep->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_embed_popup_bw_set_property (GObject *object,
				    guint prop_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
        EphyEmbedPopupBW *p = EPHY_EMBED_POPUP_BW (object);

        switch (prop_id)
        {
	case PROP_BONOBO_WINDOW:
		ephy_embed_popup_bw_set_window (p, g_value_get_object (value));
		break;
        }
}

static void
ephy_embed_popup_bw_get_property (GObject *object,
				    guint prop_id,
				    GValue *value,
				    GParamSpec *pspec)
{
        EphyEmbedPopupBW *p = EPHY_EMBED_POPUP_BW (object);

        switch (prop_id)
        {
	case PROP_BONOBO_WINDOW:
		g_value_set_object (value, p->priv->window);
		break;
        }
}

static void
ephy_embed_popup_bw_set_window (EphyEmbedPopupBW *p,
				  BonoboWindow *window)
{
	p->priv->window = window;
}

EphyEmbedPopupBW *
ephy_embed_popup_bw_new (BonoboWindow *window)
{
	EphyEmbedPopupBW *p;

        p = EPHY_EMBED_POPUP_BW (g_object_new (EPHY_EMBED_POPUP_BW_TYPE,
						 "BonoboWindow", window,
						 NULL));

        g_return_val_if_fail (p->priv != NULL, NULL);

        return p;
}

static void
popup_menu_at_coords (GtkMenu *menu, gint *x, gint *y, gboolean *push_in,
		      gpointer user_data)
{
	EphyEmbedEvent *event = user_data;

	*x = event->x;
	*y = event->y;
	*push_in = FALSE;
}

static void
ephy_embed_popup_bw_show_impl (EphyEmbedPopup *pp,
				 EphyEmbed *embed)
{
	EphyEmbedPopupBW *p = EPHY_EMBED_POPUP_BW (pp);
	EphyEmbedEvent *event = ephy_embed_popup_get_event (pp);
	guint button;

	ephy_embed_popup_set_embed (pp, embed);

	ephy_embed_event_get_mouse_button (event, &button);

	p->priv->menu = gtk_menu_new ();
        gtk_widget_show (p->priv->menu);

	bonobo_window_add_popup (p->priv->window,
				 GTK_MENU (p->priv->menu),
				 ephy_embed_popup_get_popup_path (EPHY_EMBED_POPUP (p)));

	gtk_menu_popup (GTK_MENU (p->priv->menu),
			NULL, NULL, button == 2 ? popup_menu_at_coords : NULL, event,
			button, gtk_get_current_event_time ());
}

