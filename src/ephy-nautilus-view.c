/*
 *  Copyright (C) 2001, 2002 Ricardo Fernández Pascual
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


#include <config.h>
#include <libgnome/gnome-macros.h>
#include <bonobo/bonobo-zoomable.h>
#include <bonobo/bonobo-ui-util.h>
#include <string.h>
#include "ephy-embed-popup-control.h"
#include "ephy-nautilus-view.h"
#include "ephy-embed.h"
#include "ephy-embed-utils.h"
#include "find-dialog.h"
#include "print-dialog.h"
#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"

#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);
#define DEBUG_MSG(x) g_print x
//#define DEBUG_MSG(x)

static void		gnv_embed_location_cb 			(EphyEmbed *embed, 
								 EphyNautilusView *view);
static void		gnv_embed_title_cb 			(EphyEmbed *embed, 
								 EphyNautilusView *view);
static void		gnv_embed_new_window_cb			(EphyEmbed *embed, 
								 EphyEmbed **new_embed,
								 EmbedChromeMask chromemask,
								 EphyNautilusView *view);
static void		gnv_embed_link_message_cb 		(EphyEmbed *embed, 
								 const char *message,
								 EphyNautilusView *view);
static gint		gnv_embed_dom_mouse_down_cb		(EphyEmbed *embed,
								 EphyEmbedEvent *event,
								 EphyNautilusView *view);
static void		gnv_embed_zoom_change_cb 		(EphyNautilusView *embed,
								 guint new_zoom, 
								 EphyNautilusView *view);


static void 		gnv_load_location_cb 			(EphyNautilusView *view,
								 const char *location,
								 gpointer user_data);
static void		gnv_stop_loading_cb			(EphyNautilusView *view,
								 gpointer user_data);
static void		gnv_bonobo_control_activate_cb 		(BonoboControl *control,
								 gboolean state,
								 EphyNautilusView *view);

/* zoomable */
static void 		gnv_zoomable_set_zoom_level_cb		(BonoboZoomable *zoomable,
								 float level,
								 EphyNautilusView *view);
static void		gnv_zoomable_zoom_in_cb			(BonoboZoomable *zoomable,
								 EphyNautilusView *view);
static void 		gnv_zoomable_zoom_out_cb 		(BonoboZoomable *zoomable,
								 EphyNautilusView *view);
static void		gnv_zoomable_zoom_to_fit_cb		(BonoboZoomable *zoomable,
								 EphyNautilusView *view);
static void		gnv_zoomable_zoom_to_default_cb		(BonoboZoomable *zoomable,
								 EphyNautilusView *view);
/* commands */
static void		gnv_cmd_set_charset			(BonoboUIComponent *uic, 
								 EncodingMenuData *data, 
								 const char* verbname);
static void 		gnv_cmd_file_print			(BonoboUIComponent *uic, 
								 EphyNautilusView *view, 
								 const char* verbname);
static void 		gnv_cmd_edit_find			(BonoboUIComponent *uic, 
								 EphyNautilusView *view, 
								 const char* verbname);


/* popups */
static EphyNautilusView *gnv_view_from_popup			(EphyEmbedPopup *popup);

static void 		gnv_popup_cmd_new_window		(BonoboUIComponent *uic, 
								 EphyEmbedPopup *popup, 
								 const char* verbname);
static void 		gnv_popup_cmd_image_in_new_window	(BonoboUIComponent *uic, 
								 EphyEmbedPopup *popup, 
								 const char* verbname);
static void 		gnv_popup_cmd_frame_in_new_window	(BonoboUIComponent *uic, 
								 EphyEmbedPopup *popup, 
								 const char* verbname);


static float preferred_zoom_levels[] = {
	0.2, 0.4, 0.6, 0.8,
	1.0, 1.2, 1.4, 1.6, 1.8,
	2.0, 2.2, 2.4, 2.6, 2.8,
	3.0, 3.2, 3.4, 3.6, 3.8,
	4.0, 4.2, 4.4, 4.6, 4.8,
	5.0, 5.2, 5.4, 5.6, 5.8,
	6.0, 6.2, 6.4, 6.6, 6.8,
	7.0, 7.2, 7.4, 7.6, 7.8,
	8.0, 8.2, 8.4, 8.6, 8.8,
	9.0, 9.2, 9.4, 9.6, 9.8,
};

static const gchar *preferred_zoom_level_names[] = {
	"20%", "40%", "60%", "80%",
	"100%", "120%", "140%", "160%", "180%",
	"200%", "220%", "240%", "260%", "280%",
	"300%", "320%", "340%", "360%", "380%",
	"400%", "420%", "440%", "460%", "480%",
	"500%", "520%", "540%", "560%", "580%",
	"600%", "620%", "640%", "660%", "680%",
	"700%", "720%", "740%", "760%", "780%",
	"800%", "820%", "840%", "860%", "880%",
	"900%", "920%", "940%", "960%", "980%",
};
#define NUM_ZOOM_LEVELS (sizeof (preferred_zoom_levels) / sizeof (float))

struct EphyNautilusViewPrivate {
	EphyEmbed *embed;
	char *title;
	char *location;
	int load_percent;

	/*
	  BonoboPropertyBag   *property_bag;
	*/

	EphyEmbedPopupControl *popup;
	BonoboUIComponent *popup_ui;
	BonoboControl *control;
	BonoboUIComponent *ui;
	BonoboZoomable *zoomable;

	EphyDialog *find_dialog;
};

static BonoboUIVerb ephy_popup_verbs [] = {
        BONOBO_UI_VERB ("EPOpenInNewWindow", (BonoboUIVerbFn) gnv_popup_cmd_new_window),
	BONOBO_UI_VERB ("EPOpenImageInNewWindow", (BonoboUIVerbFn) gnv_popup_cmd_image_in_new_window),
	BONOBO_UI_VERB ("DPOpenFrameInNewWindow", (BonoboUIVerbFn) gnv_popup_cmd_frame_in_new_window),

        BONOBO_UI_VERB_END
};

BonoboUIVerb ephy_verbs [] = {
        BONOBO_UI_VERB ("FilePrint", (BonoboUIVerbFn) gnv_cmd_file_print),
        BONOBO_UI_VERB ("EditFind", (BonoboUIVerbFn) gnv_cmd_edit_find),
        BONOBO_UI_VERB_END
};

#define CHARSET_MENU_PATH "/menu/View/Encoding"


BONOBO_CLASS_BOILERPLATE (EphyNautilusView, ephy_nautilus_view,
			  NautilusView, NAUTILUS_TYPE_VIEW)

static void
ephy_nautilus_view_instance_init (EphyNautilusView *view)
{
	GtkWidget *w;
	EphyNautilusViewPrivate *p = g_new0 (EphyNautilusViewPrivate, 1);

	view->priv = p;
	view->priv->embed = ephy_embed_new (G_OBJECT (ephy_shell_get_embed_shell (ephy_shell)));
	
	g_object_ref (G_OBJECT (ephy_shell));

	g_signal_connect (view->priv->embed, "ge_link_message",
			  GTK_SIGNAL_FUNC (gnv_embed_link_message_cb), 
			  view);
	g_signal_connect (view->priv->embed, "ge_location",
			  GTK_SIGNAL_FUNC (gnv_embed_location_cb), 
			  view);
	g_signal_connect (view->priv->embed, "ge_title",
			  GTK_SIGNAL_FUNC (gnv_embed_title_cb), 
			  view);
/*
	g_signal_connect (view->priv->embed, "ge_js_status",
			  GTK_SIGNAL_FUNC (gnv_embed_js_status_cb), 
			  view);
	g_signal_connect (view->priv->embed, "ge_progress",
			  GTK_SIGNAL_FUNC (gnv_embed_progress_cb), 
			  view);
	g_signal_connect (view->priv->embed, "ge_net_state",
			  GTK_SIGNAL_FUNC (gnv_embed_net_state_cb), 
			  view);
*/
	g_signal_connect (view->priv->embed, "ge_new_window",
			  GTK_SIGNAL_FUNC (gnv_embed_new_window_cb), 
			  view);
/*
	g_signal_connect (view->priv->embed, "ge_visibility",
			  GTK_SIGNAL_FUNC (gnv_embed_visibility_cb), 
			  view);
	g_signal_connect (view->priv->embed, "ge_destroy_brsr",
			  GTK_SIGNAL_FUNC (gnv_embed_destroy_brsr_cb), 
			  view);
	g_signal_connect (view->priv->embed, "ge_open_uri",
			  GTK_SIGNAL_FUNC (gnv_embed_open_uri_cb), 
			  view);
	g_signal_connect (view->priv->embed, "ge_size_to",
			  GTK_SIGNAL_FUNC (gnv_embed_size_to_cb), 
			  view);
	g_signal_connect (view->priv->embed, "ge_dom_mouse_click",
			  GTK_SIGNAL_FUNC (gnv_embed_dom_mouse_click_cb), 
			  view);
*/
	g_signal_connect (view->priv->embed, "ge_dom_mouse_down", 
			  GTK_SIGNAL_FUNC (gnv_embed_dom_mouse_down_cb), 
			  view);
/*
	g_signal_connect (view->priv->embed, "ge_security_change",
			  GTK_SIGNAL_FUNC (gnv_embed_security_change_cb), 
			  view);
*/
	g_signal_connect (view->priv->embed, "ge_zoom_change",
			  GTK_SIGNAL_FUNC (gnv_embed_zoom_change_cb), 
			  view);

	w = GTK_WIDGET (view->priv->embed);
	gtk_widget_show (w);

	nautilus_view_construct (NAUTILUS_VIEW (view), w);

	g_signal_connect (G_OBJECT (view), "load_location",
			  G_CALLBACK (gnv_load_location_cb), NULL);

	g_signal_connect (G_OBJECT (view), "stop_loading",
			  G_CALLBACK (gnv_stop_loading_cb), NULL);

        g_signal_connect (G_OBJECT (nautilus_view_get_bonobo_control (NAUTILUS_VIEW (view))),
                          "activate",
                          G_CALLBACK (gnv_bonobo_control_activate_cb), view);

	view->priv->zoomable = bonobo_zoomable_new ();
	bonobo_object_add_interface (BONOBO_OBJECT (view),
				     BONOBO_OBJECT (view->priv->zoomable));

	bonobo_zoomable_set_parameters_full (view->priv->zoomable,
					     1.0,
					     preferred_zoom_levels [0],
					     preferred_zoom_levels [NUM_ZOOM_LEVELS - 1],
					     FALSE, FALSE, TRUE,
					     preferred_zoom_levels,
					     preferred_zoom_level_names,
					     NUM_ZOOM_LEVELS);

	bonobo_object_add_interface (BONOBO_OBJECT (view),
				     BONOBO_OBJECT (view->priv->zoomable));

	g_signal_connect (view->priv->zoomable, "set_zoom_level",
			    G_CALLBACK (gnv_zoomable_set_zoom_level_cb), view);
	g_signal_connect (view->priv->zoomable, "zoom_in",
			    G_CALLBACK (gnv_zoomable_zoom_in_cb), view);
	g_signal_connect (view->priv->zoomable, "zoom_out",
			    G_CALLBACK (gnv_zoomable_zoom_out_cb), view);
	g_signal_connect (view->priv->zoomable, "zoom_to_fit",
			    G_CALLBACK (gnv_zoomable_zoom_to_fit_cb), view);
	g_signal_connect (view->priv->zoomable, "zoom_to_default",
			    G_CALLBACK (gnv_zoomable_zoom_to_default_cb), view);

	p->control = nautilus_view_get_bonobo_control (NAUTILUS_VIEW (view));
	
	p->popup_ui = bonobo_control_get_popup_ui_component (p->control);
	g_assert (BONOBO_IS_UI_COMPONENT (p->popup_ui));
	bonobo_ui_util_set_ui (p->popup_ui, DATADIR, 
			       "nautilus-ephy-view-ui.xml", 
			       "EphyNutilusView", NULL);
	p->popup = ephy_embed_popup_control_new (p->control);
	ephy_embed_popup_connect_verbs (EPHY_EMBED_POPUP (p->popup), p->popup_ui);
	g_object_set_data (G_OBJECT (p->popup), "NautilisView", view);

	bonobo_ui_component_add_verb_list_with_data (p->popup_ui, ephy_popup_verbs, p->popup);
}

/**
 * Returns a new EphyNautilusView as a BonoboObject 
 **/
BonoboObject *
ephy_nautilus_view_new_component (EphyShell *gs)
{
	EphyNautilusView *view;
	view = EPHY_NAUTILUS_VIEW (g_object_new (EPHY_TYPE_NAUTILUS_VIEW, NULL));
	return BONOBO_OBJECT (view);
}

static void
ephy_nautilus_view_finalize (GObject *object)
{
	EphyNautilusView *view = EPHY_NAUTILUS_VIEW (object);
	EphyNautilusViewPrivate *p = view->priv;

	if (p->find_dialog)
	{
		g_object_unref (p->find_dialog);
	}

	g_object_unref (p->popup);

	g_free (p->title);
	g_free (p->location);
	g_free (p);

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));

	g_object_unref (G_OBJECT (ephy_shell));
}

static void
ephy_nautilus_view_class_init (EphyNautilusViewClass *class)
{
	G_OBJECT_CLASS (class)->finalize = ephy_nautilus_view_finalize;
}



static gint
gnv_embed_dom_mouse_down_cb (EphyEmbed *embed,
			     EphyEmbedEvent *event,
			     EphyNautilusView *view)
{
	EphyNautilusViewPrivate *p = view->priv;
	int button;
	EmbedEventContext context;

	ephy_embed_event_get_mouse_button (event, &button);
	ephy_embed_event_get_context (event, &context); 

	if (button == 2)
	{
		ephy_embed_popup_set_event (EPHY_EMBED_POPUP (p->popup), event);
		ephy_embed_popup_show (EPHY_EMBED_POPUP (p->popup), embed);
		return TRUE;

	}
	else if (button == 1
		 && (context & EMBED_CONTEXT_LINK))
	{
		GValue *value;
		const gchar *url;
		ephy_embed_event_get_property (event, "link", &value);
		url = g_value_get_string (value);

		g_return_val_if_fail (url, FALSE);

		nautilus_view_open_location_force_new_window (NAUTILUS_VIEW (view),
							      url, NULL);
	}

	return FALSE;
}

static void
gnv_embed_link_message_cb (EphyEmbed *embed, const char *message, EphyNautilusView *view)
{
	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));
	g_return_if_fail (message != NULL);

	nautilus_view_report_status (NAUTILUS_VIEW (view), message);
}

static void
gnv_embed_location_cb (EphyEmbed *embed, EphyNautilusView *view)
{
	EphyNautilusViewPrivate *p;
	const gchar *prefixes_to_ignore[] = 
		{
			"about:",
			"javascript:",
			NULL 
		};
	int i = 0;
	gchar *new_uri;

 	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));
	p = view->priv;
	g_return_if_fail (view->priv->embed == embed);

	ephy_embed_get_location (embed, TRUE, &new_uri);

 	g_return_if_fail (new_uri != NULL);


	/* don't inform nautilus about uris that it can't understand */
	while (prefixes_to_ignore[i] != NULL)
	{
		if (!strncmp (prefixes_to_ignore[i], new_uri, strlen (prefixes_to_ignore[i])))
		{
			g_free (new_uri);
			return;
		}
		++i;
	}

	nautilus_view_report_location_change (NAUTILUS_VIEW (view), new_uri, NULL, new_uri);

 	/* TODO, FIXME 
	   nautilus_view_report_redirect (view, p->location, new_uri, NULL, new_uri);
 	*/


	g_free (p->location);
	p->location = new_uri;
	
	
}

static void
gnv_embed_title_cb (EphyEmbed *embed, EphyNautilusView *view)
{
	EphyNautilusViewPrivate *p;

 	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));
	p = view->priv;
	g_return_if_fail (view->priv->embed == embed);

	g_free (p->title);
	ephy_embed_get_title (embed, &p->title);

 	nautilus_view_set_title (NAUTILUS_VIEW (view), p->title);
}

static void
gnv_load_location_cb (EphyNautilusView *view, const char *location, gpointer user_data)
{
	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));
	g_return_if_fail (location != NULL);

	nautilus_view_report_load_underway (NAUTILUS_VIEW (view));
        ephy_embed_load_url (view->priv->embed, location);

}

static void
gnv_stop_loading_cb (EphyNautilusView *view, gpointer user_data)
{
}

static void
gnv_embed_new_window_cb (EphyEmbed *embed, EphyEmbed **new_embed,
			 EmbedChromeMask chromemask, EphyNautilusView *view)
{
	EphyTab *new_tab;
	EphyWindow *window;
	
	window = ephy_window_new ();
	if (!(chromemask & EMBED_CHROME_OPENASCHROME))
	{
		ephy_window_set_chrome (window, 
					  chromemask | 
					  EMBED_CHROME_OPENASPOPUP);
	}
	new_tab = ephy_tab_new ();
	ephy_window_add_tab (window, new_tab, FALSE);
	
	*new_embed = ephy_tab_get_embed (new_tab);
}


static void
gnv_bonobo_control_activate_cb (BonoboControl *control, gboolean state, EphyNautilusView *view)
{
	if (state)
	{
		EphyNautilusViewPrivate *p = view->priv;
		
		p->ui = nautilus_view_set_up_ui (NAUTILUS_VIEW (view), DATADIR,
						 "nautilus-ephy-view-ui.xml", "EphyNutilusView");
		g_return_if_fail (BONOBO_IS_UI_COMPONENT (p->ui));
		
		ephy_embed_utils_build_charsets_submenu (p->ui,
							   CHARSET_MENU_PATH,
							   (BonoboUIVerbFn) gnv_cmd_set_charset,
							   view);

		bonobo_ui_component_add_verb_list_with_data (p->ui, ephy_verbs, view);
	}
}

static EphyNautilusView *
gnv_view_from_popup (EphyEmbedPopup *popup)
{
	return g_object_get_data (G_OBJECT (popup), "NautilisView");
}


static void 
gnv_popup_cmd_new_window (BonoboUIComponent *uic, 
			  EphyEmbedPopup *popup, 
			  const char* verbname)
{
	EphyEmbedEvent *info;
	EphyNautilusView *view;
	GValue *value;

	view = gnv_view_from_popup (popup);
	
	info = ephy_embed_popup_get_event (popup);
	
	ephy_embed_event_get_property (info, "link", &value);

	nautilus_view_open_location_force_new_window (NAUTILUS_VIEW (view),
						      g_value_get_string (value), NULL);
}

static void 
gnv_popup_cmd_image_in_new_window (BonoboUIComponent *uic, 
				   EphyEmbedPopup *popup, 
				   const char* verbname)
{
	EphyEmbedEvent *info;
	EphyNautilusView *view;
	GValue *value;

	view = gnv_view_from_popup (popup);
	
	info = ephy_embed_popup_get_event (popup);
	
	ephy_embed_event_get_property (info, "image", &value);

	nautilus_view_open_location_force_new_window (NAUTILUS_VIEW (view),
						      g_value_get_string (value), NULL);
}

static void 
gnv_popup_cmd_frame_in_new_window (BonoboUIComponent *uic, 
				   EphyEmbedPopup *popup, 
				   const char* verbname)
{
	EphyEmbedEvent *info;
	EphyNautilusView *view;
	gchar *location;

	view = gnv_view_from_popup (popup);
	
	info = ephy_embed_popup_get_event (popup);
	
	ephy_embed_get_location (view->priv->embed, FALSE, &location);

	nautilus_view_open_location_force_new_window (NAUTILUS_VIEW (view),
						      location, NULL);
}

void 
gnv_cmd_set_charset (BonoboUIComponent *uic, 
		     EncodingMenuData *data, 
		     const char* verbname)
{
	EphyNautilusView *view = data->data;
	EphyNautilusViewPrivate *p;

	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));

	p = view->priv;
	
	DEBUG_MSG ((data->encoding));
	ephy_embed_set_charset (p->embed, data->encoding);
}

static void
gnv_cmd_file_print (BonoboUIComponent *uic, 
		    EphyNautilusView *view, 
		    const char* verbname)
{
	EphyDialog *dialog;
	EphyNautilusViewPrivate *p = view->priv;
	
	dialog = print_dialog_new (p->embed, NULL);

	//g_signal_connect (G_OBJECT(dialog),
	//		  "preview",
	//		  G_CALLBACK (print_dialog_preview_cb),
	//		  window);

	ephy_dialog_set_modal (dialog, TRUE);
	ephy_dialog_show (dialog);

}

static void
gnv_cmd_edit_find (BonoboUIComponent *uic, 
		   EphyNautilusView *view, 
		   const char* verbname)
{
	EphyNautilusViewPrivate *p = view->priv;

	if (!p->find_dialog)
	{
		p->find_dialog = find_dialog_new (p->embed);
	}

	ephy_dialog_show (p->find_dialog);
}


/* zoomable */
static void
gnv_zoomable_set_zoom_level_cb (BonoboZoomable *zoomable,
				float level,
				EphyNautilusView *view)
{
	gint zoom = level * 100;
	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));
	if (zoom < 10) return;
	if (zoom > 1000) return;
	ephy_embed_zoom_set (view->priv->embed, zoom, TRUE);
}

static void
gnv_zoomable_zoom_in_cb (BonoboZoomable *zoomable,
			 EphyNautilusView *view)
{
	gint zoom;
	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));
	ephy_embed_zoom_get (view->priv->embed, &zoom);
	if (zoom > 990) return;
	ephy_embed_zoom_set (view->priv->embed, zoom + 10, TRUE);
}

static void
gnv_zoomable_zoom_out_cb (BonoboZoomable *zoomable,
			  EphyNautilusView *view)
{
	gint zoom;
	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));
	ephy_embed_zoom_get (view->priv->embed, &zoom);
	if (zoom < 20) return;
	ephy_embed_zoom_set (view->priv->embed, zoom - 10, TRUE);
}

static void
gnv_zoomable_zoom_to_fit_cb (BonoboZoomable *zoomable,
			     EphyNautilusView *view)
{
	gnv_zoomable_zoom_to_default_cb (zoomable, view);
}

static void
gnv_zoomable_zoom_to_default_cb	(BonoboZoomable *zoomable,
				 EphyNautilusView *view)
{
	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));
	ephy_embed_zoom_set (view->priv->embed, 100, TRUE);
}

static void
gnv_embed_zoom_change_cb (EphyNautilusView *embed,
			  guint new_zoom, 
			  EphyNautilusView *view)
{
	float flevel;
	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));
	
	flevel = ((float) new_zoom) / 100.0;
	
	bonobo_zoomable_report_zoom_level_changed (view->priv->zoomable,
						   flevel, NULL);
	
}


#ifdef IM_TOO_LAZY_TO_MOVE_THIS_TO_ANOTHER_FILE


/* property bag properties */
enum {
	ICON_NAME,
	COMPONENT_INFO
};


static void
get_bonobo_properties (BonoboPropertyBag *bag,
			BonoboArg *arg,
			guint arg_id,
			CORBA_Environment *ev,
			gpointer callback_data)
{
	EphyNautilusView *content_view;
	
	content_view = (EphyNautilusView*) callback_data;

	switch (arg_id) {
        	case ICON_NAME:	
			if (!strncmp (content_view->priv->uri, "man:", 4)) {
                   		BONOBO_ARG_SET_STRING (arg, "manual");					
			} else if (!strncmp (content_view->priv->uri, "http:", 5)) {
                		BONOBO_ARG_SET_STRING (arg, "i-web");					
			} else if (!strncmp (content_view->priv->uri, "https:", 6)) {
				/* FIXME: put a nice icon for secure sites */
                		BONOBO_ARG_SET_STRING (arg, "i-web");					
			} else {
                		BONOBO_ARG_SET_STRING (arg, "");					
                	}
                	break;

        	case COMPONENT_INFO:
               		BONOBO_ARG_SET_STRING (arg, "");					
                 	break;
        		
        	default:
                	g_warning ("Unhandled arg %d", arg_id);
                	break;
	}
}

/* there are no settable properties, so complain if someone tries to set one */
static void
set_bonobo_properties (BonoboPropertyBag *bag,
			const BonoboArg *arg,
			guint arg_id,
			CORBA_Environment *ev,
			gpointer callback_data)
{
                g_warning ("Bad Property set on view: property ID %d",
			   arg_id);
}

static void
ephy_nautilus_view_initialize (EphyNautilusView *view)
{


#ifdef NOT_PORTED
	bonobo_control_set_properties (nautilus_view_get_bonobo_control (view->priv->nautilus_view),
				       view->priv->property_bag);
#endif
	bonobo_property_bag_add (view->priv->property_bag, "icon_name", ICON_NAME, 
				 BONOBO_ARG_STRING, NULL,
				 _("name of icon for the mozilla view"), 0);
	bonobo_property_bag_add (view->priv->property_bag, "summary_info", COMPONENT_INFO,
				 BONOBO_ARG_STRING, NULL,
				 _("mozilla summary info"), 0);
}


	/* free the property bag */
	if (view->priv->property_bag != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (view->priv->property_bag));
		view->priv->property_bag = NULL;
	}

}



void
ephy_nautilus_view_report_load_progress (EphyNautilusView *view,
					   double value)
{
	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));

	if (value < 0.0) value = 0.0;
	if (value > 1.0) value = 1.0;
	
	nautilus_view_report_load_progress (view->priv->nautilus_view, value);
}

/***********************************************************************************/

/**
 * vfs_open_cb
 *
 * Callback for gnome_vfs_async_open. Attempt to read data from handle
 * and pass to mozilla streaming callback.
 * 
 **/
static void
vfs_open_cb (GnomeVFSAsyncHandle *handle, GnomeVFSResult result, gpointer data)
{
	EphyNautilusView *view = data;

	DEBUG_MSG (("+%s GnomeVFSResult: %u\n", G_GNUC_FUNCTION, (unsigned)result));

	if (result != GNOME_VFS_OK)
	{
		gtk_moz_embed_close_stream (GTK_MOZ_EMBED (view->priv->embed->mozembed));
		/* NOTE: the view may go away after a call to report_load_failed */
		DEBUG_MSG ((">nautilus_view_report_load_failed\n"));
		nautilus_view_report_load_failed (view->priv->nautilus_view);
	} else {
		if (view->priv->vfs_read_buffer == NULL) {
			view->priv->vfs_read_buffer = g_malloc (VFS_READ_BUFFER_SIZE);
		}
		gtk_moz_embed_open_stream (GTK_MOZ_EMBED (view->priv->embed->mozembed), "file:///", "text/html");
		gnome_vfs_async_read (handle, view->priv->vfs_read_buffer, VFS_READ_BUFFER_SIZE, vfs_read_cb, view);
	}
	DEBUG_MSG (("-%s\n", G_GNUC_FUNCTION));
}

/**
 * vfs_read_cb:
 *
 * Read data from buffer and copy into mozilla stream.
 **/

static void
vfs_read_cb (GnomeVFSAsyncHandle *handle, GnomeVFSResult result, gpointer buffer,
		   GnomeVFSFileSize bytes_requested,
		   GnomeVFSFileSize bytes_read,
		   gpointer data)
{
	EphyNautilusView *view = data;

	DEBUG_MSG (("+%s %ld/%ld bytes\n", G_GNUC_FUNCTION, (long)bytes_requested, (long) bytes_read));

	if (bytes_read != 0) {
		gtk_moz_embed_append_data (GTK_MOZ_EMBED (view->priv->embed->mozembed), buffer, bytes_read);
	}

	if (bytes_read == 0 || result != GNOME_VFS_OK) {
		gtk_moz_embed_close_stream (GTK_MOZ_EMBED (view->priv->embed->mozembed));
		view->priv->vfs_handle = NULL;
		g_free (view->priv->vfs_read_buffer);
		view->priv->vfs_read_buffer = NULL;
		
		gnome_vfs_async_close (handle, (GnomeVFSAsyncCloseCallback) gtk_true, NULL);

		DEBUG_MSG ((">nautilus_view_report_load_complete\n"));
		nautilus_view_report_load_complete (view->priv->nautilus_view);

		DEBUG_MSG (("=%s load complete\n", G_GNUC_FUNCTION));
    	} else {
		gnome_vfs_async_read (handle, view->priv->vfs_read_buffer, VFS_READ_BUFFER_SIZE, vfs_read_cb, view);
	}

	DEBUG_MSG (("-%s\n", G_GNUC_FUNCTION));
}

/***********************************************************************************/

static void
cancel_pending_vfs_operation (EphyNautilusView *view)
{
	if (view->priv->vfs_handle != NULL) {
		gnome_vfs_async_cancel (view->priv->vfs_handle);
		gtk_moz_embed_close_stream (GTK_MOZ_EMBED (view->priv->embed->mozembed));
	}

	view->priv->vfs_handle = NULL;
	g_free (view->priv->vfs_read_buffer);
	view->priv->vfs_read_buffer = NULL;
}


/* this takes a "nautilus" uri, not a "mozilla" uri and uses (sometimes) GnomeVFS */
static void
navigate_mozilla_to_nautilus_uri (EphyNautilusView *view,
			 	  const char *uri)
{
	char *old_uri;

	cancel_pending_vfs_operation (view);
	
	if (!GTK_WIDGET_REALIZED (view->priv->embed->mozembed)) {
		
		/* Doing certain things to gtkmozembed before
		 * the widget has realized (specifically, opening
		 * content streams) can cause crashes.  To avoid
		 * this, we postpone all navigations
		 * until the widget has realized (we believe
		 * premature realization may cause other issues)
		 */
		
		DEBUG_MSG (("=%s: Postponing navigation request to widget realization\n", G_GNUC_FUNCTION));
		/* Note that view->priv->uri is still set below */
	} else {
		if (should_mozilla_load_uri_directly (uri)) {

			/* See if the current URI is the same as what mozilla already
			 * has.  If so, issue a reload rather than a load.
			 * We ask mozilla for it's uri rather than using view->priv->uri because,
			 * from time to time, our understanding of mozilla's URI can become inaccurate
			 * (in particular, certain errors may cause embedded mozilla to not change
			 * locations)
			 */

			old_uri = view->priv->embed->location;

			if (old_uri != NULL && uris_identical (uri, old_uri)) {
				DEBUG_MSG (("=%s uri's identical, telling ephy to reload\n", G_GNUC_FUNCTION));
				embed_reload (view->priv->embed,
					      GTK_MOZ_EMBED_FLAG_RELOADBYPASSCACHE);
			} else {
				embed_load_url (view->priv->embed, uri);
			}

		} else {
			DEBUG_MSG (("=%s loading URI via gnome-vfs\n", G_GNUC_FUNCTION));
			gnome_vfs_async_open (&(view->priv->vfs_handle), uri,
					      GNOME_VFS_OPEN_READ, GNOME_VFS_PRIORITY_DEFAULT, 
					      vfs_open_cb, view);
		}
	}

	g_free (view->priv->uri);
	view->priv->uri = g_strdup (uri);

	DEBUG_MSG (("=%s current URI is now '%s'\n", G_GNUC_FUNCTION, view->priv->uri));
}

/*
 * This a list of URI schemes that mozilla should load directly, rather than load through gnome-vfs
 */
static gboolean
should_mozilla_load_uri_directly (const char *uri)
{
	static const char *handled_by_mozilla[] =
	{
		"http",
		"file",
		"toc",
		"man",
		"info",
		"ghelp",
		"gnome-help",
		"https",
		NULL
	};
	gint i;
	gint uri_length;

	if (uri == NULL) return FALSE;

	uri_length = strlen (uri);

	for (i = 0; handled_by_mozilla[i] != NULL; i++)
	{
		const gchar *current = handled_by_mozilla[i];
		gint current_length = strlen (current);
		if ((uri_length >= current_length) 
		    && (!strncasecmp (uri, current, current_length))) 
			return TRUE;
	}
	return FALSE;
}



#endif
