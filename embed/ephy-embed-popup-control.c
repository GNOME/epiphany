/*
 *  Copyright (C) 2000, 2001, 2002 Ricardo Fernández Pascual
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

#include "ephy-embed-popup-control.h"
#include "ephy-gobject-misc.h"
#include "ephy-bonobo-extensions.h"

#include <gtk/gtkmain.h>

enum
{
        PROP_0,
        PROP_BONOBO_CONTROL
};

struct EphyEmbedPopupControlPrivate
{
	BonoboControl *control;
};

static void
ephy_embed_popup_control_class_init (EphyEmbedPopupControlClass *klass);
static void
ephy_embed_popup_control_init (EphyEmbedPopupControl *gep);
static void
ephy_embed_popup_control_finalize (GObject *object);
static void
ephy_embed_popup_control_set_property (GObject *object,
				       guint prop_id,
				       const GValue *value,
				       GParamSpec *pspec);
static void
ephy_embed_popup_control_get_property (GObject *object,
				       guint prop_id,
				       GValue *value,
				       GParamSpec *pspec);
static void
ephy_embed_popup_control_set_control (EphyEmbedPopupControl *p,
				      BonoboControl *control);
static void
ephy_embed_popup_control_show_impl (EphyEmbedPopup *p,
				    EphyEmbed *embed);

static EphyEmbedPopupClass *parent_class = NULL;

MAKE_GET_TYPE (ephy_embed_popup_control, "EphyEmbedPopupControl", EphyEmbedPopupControl,
	       ephy_embed_popup_control_class_init, ephy_embed_popup_control_init,
	       EPHY_EMBED_POPUP_TYPE);

static void
ephy_embed_popup_control_class_init (EphyEmbedPopupControlClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = EPHY_EMBED_POPUP_CLASS (g_type_class_peek_parent (klass));

        object_class->finalize = ephy_embed_popup_control_finalize;
	object_class->set_property = ephy_embed_popup_control_set_property;
        object_class->get_property = ephy_embed_popup_control_get_property;

	g_object_class_install_property (object_class,
                                         PROP_BONOBO_CONTROL,
                                         g_param_spec_object ("BonoboControl",
                                                              "BonoboControl",
                                                              "Bonobo control",
                                                              BONOBO_TYPE_CONTROL,
                                                              G_PARAM_READWRITE));

	EPHY_EMBED_POPUP_CLASS (klass)->show = ephy_embed_popup_control_show_impl;
}

static void
ephy_embed_popup_control_init (EphyEmbedPopupControl *gep)
{
        gep->priv = g_new0 (EphyEmbedPopupControlPrivate, 1);
	gep->priv->control = NULL;
}

static void
ephy_embed_popup_control_finalize (GObject *object)
{
	EphyEmbedPopupControl *gep;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_EPHY_EMBED_POPUP_CONTROL (object));

        gep = EPHY_EMBED_POPUP_CONTROL (object);

        g_return_if_fail (gep->priv != NULL);

        g_free (gep->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
ephy_embed_popup_control_set_property (GObject *object,
				       guint prop_id,
				       const GValue *value,
				       GParamSpec *pspec)
{
        EphyEmbedPopupControl *p = EPHY_EMBED_POPUP_CONTROL (object);

        switch (prop_id)
        {
	case PROP_BONOBO_CONTROL:
		ephy_embed_popup_control_set_control (p, g_value_get_object (value));
		break;
        }
}

static void
ephy_embed_popup_control_get_property (GObject *object,
				       guint prop_id,
				       GValue *value,
				       GParamSpec *pspec)
{
        EphyEmbedPopupControl *p = EPHY_EMBED_POPUP_CONTROL (object);

        switch (prop_id)
        {
	case PROP_BONOBO_CONTROL:
		g_value_set_object (value, p->priv->control);
		break;
        }
}

static void
ephy_embed_popup_control_set_control (EphyEmbedPopupControl *p,
				      BonoboControl *control)
{
	p->priv->control = control;
}

EphyEmbedPopupControl *
ephy_embed_popup_control_new (BonoboControl *control)
{
	EphyEmbedPopupControl *p;

        p = EPHY_EMBED_POPUP_CONTROL (g_object_new (EPHY_EMBED_POPUP_CONTROL_TYPE,
						      "BonoboControl", control,
						      NULL));

        g_return_val_if_fail (p->priv != NULL, NULL);

        return p;
}

static void
ephy_embed_popup_control_show_impl (EphyEmbedPopup *pp,
				    EphyEmbed *embed)
{
	EphyEmbedPopupControl *p = EPHY_EMBED_POPUP_CONTROL (pp);
	EphyEmbedEvent *event = ephy_embed_popup_get_event (pp);
	BonoboUIComponent *uic = bonobo_control_get_popup_ui_component (p->priv->control);
	const char *path;
	char *path_dst;
	guint button;

	ephy_embed_event_get_mouse_button (event, &button);
	ephy_embed_popup_set_embed (pp, embed);
	path = ephy_embed_popup_get_popup_path (pp);
	path_dst = g_strdup_printf ("/popups/button%d", button);

	/* this is a hack because bonobo apis for showing popups are broken */
	ephy_bonobo_replace_path (uic, path, path_dst);

	bonobo_control_do_popup (p->priv->control, button,
				 gtk_get_current_event_time ());

	g_free (path_dst);
}

