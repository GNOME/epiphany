/*
 *  Copyright (C) 2001, 2002 Ricardo Fernández Pascual
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *  Copyright (C) 2003 Christian Persch
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <libgnome/gnome-macros.h>
#include <bonobo/bonobo-zoomable.h>
#include <bonobo/bonobo-ui-util.h>
#include <string.h>

#include "ephy-embed-factory.h"
#include "ephy-embed-popup-control.h"
#include "ephy-nautilus-view.h"
#include "ephy-embed.h"
#include "find-dialog.h"
#include "print-dialog.h"
#include "ephy-encoding-dialog.h"
#include "ephy-zoom.h"
#include "ephy-debug.h"

static void		gnv_embed_location_cb 			(EphyEmbed *embed,
								 const char *new_uri,
								 EphyNautilusView *view);
static void		gnv_embed_title_cb 			(EphyEmbed *embed, 
								 EphyNautilusView *view);
static void		gnv_embed_new_window_cb			(EphyEmbed *embed, 
								 EphyEmbed **new_embed,
								 EphyEmbedChrome chromemask,
								 EphyNautilusView *view);
static void		gnv_embed_link_message_cb 		(EphyEmbed *embed, 
								 EphyNautilusView *view);
static gint		gnv_embed_dom_mouse_click_cb		(EphyEmbed *embed,
								 EphyEmbedEvent *event,
								 EphyNautilusView *view);
static void		gnv_embed_context_menu_cb		(EphyEmbed *embed,
								 EphyEmbedEvent *event,
								 EphyNautilusView *view);
static void		gnv_embed_zoom_change_cb 		(EphyNautilusView *embed,
								 float new_zoom, 
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
static void 		gnv_cmd_file_print			(BonoboUIComponent *uic, 
								 EphyNautilusView *view, 
								 const char* verbname);
static void 		gnv_cmd_edit_find			(BonoboUIComponent *uic, 
								 EphyNautilusView *view, 
								 const char* verbname);
static void 		gnv_cmd_select_encoding			(BonoboUIComponent *uic, 
								 EphyNautilusView *view, 
								 const char* verbname);

/* popups */
static EphyNautilusView *gnv_view_from_popup			(EphyEmbedPopupControl*popup);

static void 		gnv_popup_cmd_new_window		(BonoboUIComponent *uic, 
								 EphyEmbedPopupControl*popup, 
								 const char* verbname);
static void 		gnv_popup_cmd_image_in_new_window	(BonoboUIComponent *uic, 
								 EphyEmbedPopupControl*popup, 
								 const char* verbname);
static void 		gnv_popup_cmd_frame_in_new_window	(BonoboUIComponent *uic, 
								 EphyEmbedPopupControl*popup, 
								 const char* verbname);

#define EPHY_NAUTILUS_VIEW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_NAUTILUS_VIEW, EphyNautilusViewPrivate))

struct EphyNautilusViewPrivate {
	EphyEmbed *embed;
	char *title;
	char *location;
	int load_percent;

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
	BONOBO_UI_VERB ("ViewEncoding", (BonoboUIVerbFn) gnv_cmd_select_encoding),
        BONOBO_UI_VERB_END
};

BONOBO_CLASS_BOILERPLATE (EphyNautilusView, ephy_nautilus_view,
			  NautilusView, NAUTILUS_TYPE_VIEW)

static gboolean
disconnected_idle (EphyShell *shell)
{
	g_object_unref (shell);

	return FALSE;
}

static void
control_disconnected_cb (BonoboControl *control)
{
	g_idle_add ((GSourceFunc)disconnected_idle, ephy_shell);
}

static void
ephy_nautilus_view_instance_init (EphyNautilusView *view)
{
	GtkWidget *w;
	EphyNautilusViewPrivate *p = EPHY_NAUTILUS_VIEW_GET_PRIVATE (view);
	float *levels;
	gchar **names;
	guint i;
	BonoboControl *control;

	view->priv = p;

	view->priv->embed = EPHY_EMBED
		(ephy_embed_factory_new_object ("EphyEmbed"));

	g_signal_connect (view->priv->embed, "link_message",
			  G_CALLBACK (gnv_embed_link_message_cb), 
			  view);
	g_signal_connect (view->priv->embed, "ge_location",
			  G_CALLBACK (gnv_embed_location_cb), 
			  view);
	g_signal_connect (view->priv->embed, "title",
			  G_CALLBACK (gnv_embed_title_cb), 
			  view);
	g_signal_connect (view->priv->embed, "ge_new_window",
			  G_CALLBACK (gnv_embed_new_window_cb), 
			  view);
	g_signal_connect (view->priv->embed, "ge_dom_mouse_click", 
			  G_CALLBACK (gnv_embed_dom_mouse_click_cb), 
			  view);
	g_signal_connect (view->priv->embed, "ge_context_menu", 
			  G_CALLBACK (gnv_embed_context_menu_cb), 
			  view);
	g_signal_connect (view->priv->embed, "ge_zoom_change",
			  G_CALLBACK (gnv_embed_zoom_change_cb), 
			  view);

	w = GTK_WIDGET (view->priv->embed);
	gtk_widget_show (w);

	nautilus_view_construct (NAUTILUS_VIEW (view), w);

	g_signal_connect (G_OBJECT (view), "load_location",
			  G_CALLBACK (gnv_load_location_cb), NULL);

	g_signal_connect (G_OBJECT (view), "stop_loading",
			  G_CALLBACK (gnv_stop_loading_cb), NULL);

	control = nautilus_view_get_bonobo_control (NAUTILUS_VIEW (view));	
	g_object_ref (ephy_shell);

	g_signal_connect (control, "disconnected",
			  G_CALLBACK (control_disconnected_cb), NULL);
        g_signal_connect (control,
                          "activate",
                          G_CALLBACK (gnv_bonobo_control_activate_cb), view);

	view->priv->zoomable = bonobo_zoomable_new ();
	bonobo_object_add_interface (BONOBO_OBJECT (view),
				     BONOBO_OBJECT (view->priv->zoomable));

	/* get zoom levels */
	levels = g_new0 (float, n_zoom_levels);
	names = g_new0 (gchar *, n_zoom_levels);
	for (i = 0; i < n_zoom_levels; i++)
	{
		levels[i] = zoom_levels[i].level;
		names[i] = zoom_levels[i].name;
	}

	bonobo_zoomable_set_parameters_full (view->priv->zoomable,
					     1.0,
					     levels [0],
					     levels [n_zoom_levels-1],
					     FALSE, FALSE, TRUE,
					     levels,
					     (const gchar **) names,
					     n_zoom_levels);

	g_free (levels);
	g_free (names);

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
	bonobo_ui_util_set_ui (p->popup_ui, SHARE_DIR,
			       "nautilus-epiphany-view.xml",
			       "EphyNautilusView", NULL);
	p->popup = ephy_embed_popup_control_new (p->control);
	ephy_embed_popup_control_connect_verbs
		(EPHY_EMBED_POPUP_CONTROL (p->popup), p->popup_ui);
	g_object_set_data (G_OBJECT (p->popup), "NautilisView", view);

	bonobo_ui_component_add_verb_list_with_data (p->popup_ui, ephy_popup_verbs, p->popup);
}

BonoboObject *
ephy_nautilus_view_new_component (EphyShell *shell)
{
	return BONOBO_OBJECT (g_object_new (EPHY_TYPE_NAUTILUS_VIEW, NULL));
}

static void
ephy_nautilus_view_finalize (GObject *object)
{
	EphyNautilusView *view = EPHY_NAUTILUS_VIEW (object);
	EphyNautilusViewPrivate *p = view->priv;

	if (p->find_dialog)
	{
		g_object_remove_weak_pointer (G_OBJECT (p->find_dialog),
					      (gpointer *) &p->find_dialog);
		g_object_unref (p->find_dialog);
	}

	g_object_unref (p->popup);

	g_free (p->title);
	g_free (p->location);

	GNOME_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
ephy_nautilus_view_class_init (EphyNautilusViewClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = ephy_nautilus_view_finalize;

	g_type_class_add_private (object_class, sizeof(EphyNautilusViewPrivate));
}

static gint
gnv_embed_dom_mouse_click_cb (EphyEmbed *embed,
			      EphyEmbedEvent *event,
			      EphyNautilusView *view)
{
	EphyEmbedEventType type;
	EmbedEventContext context;

	type = ephy_embed_event_get_event_type (event);
	context = ephy_embed_event_get_context (event);

	if (type == EPHY_EMBED_EVENT_MOUSE_BUTTON2
	    && (context & EMBED_CONTEXT_LINK))
	{
		const GValue *value;
		const gchar *url;
		ephy_embed_event_get_property (event, "link", &value);
		url = g_value_get_string (value);

		g_return_val_if_fail (url, FALSE);

		nautilus_view_open_location (NAUTILUS_VIEW (view), url,
				             Nautilus_ViewFrame_OPEN_IN_NAVIGATION,
				             0, NULL);
	}

	return FALSE;
}

static void
gnv_embed_context_menu_cb (EphyEmbed *embed,
			   EphyEmbedEvent *event,
			   EphyNautilusView *view)
{
	EphyNautilusViewPrivate *p = view->priv;

	ephy_embed_popup_control_set_event (p->popup, event);
	ephy_embed_popup_control_show (p->popup, embed);
}

static void
gnv_embed_link_message_cb (EphyEmbed *embed, EphyNautilusView *view)
{
	char *message;

	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));

	message = ephy_embed_get_link_message (embed);

	nautilus_view_report_status (NAUTILUS_VIEW (view), message);

	g_free (message);
}

static void
gnv_embed_location_cb (EphyEmbed *embed, const char *new_uri, EphyNautilusView *view)
{
	const char *prefixes_to_ignore[] = 
		{
			"about:",
			"javascript:",
			NULL 
		};
	int i = 0;

 	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));
	g_return_if_fail (view->priv->embed == embed);
 	g_return_if_fail (new_uri != NULL);

	/* don't inform nautilus about uris that it can't understand */
	while (prefixes_to_ignore[i] != NULL)
	{
		if (strncmp (prefixes_to_ignore[i], new_uri, strlen (prefixes_to_ignore[i])) == 0)
		{
			return;
		}
		++i;
	}

	nautilus_view_report_location_change (NAUTILUS_VIEW (view), new_uri, NULL, new_uri);

	g_free (view->priv->location);
	view->priv->location = g_strdup (new_uri);
}

static void
gnv_embed_title_cb (EphyEmbed *embed, EphyNautilusView *view)
{
	EphyNautilusViewPrivate *p;

 	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));
	p = view->priv;
	g_return_if_fail (view->priv->embed == embed);

	g_free (p->title);
	p->title = ephy_embed_get_title (embed);

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
			 EphyEmbedChrome chromemask, EphyNautilusView *view)
{
	EphyTab *new_tab;
	EphyWindow *window;
	
	window = ephy_window_new ();
	new_tab = ephy_tab_new ();
	gtk_widget_show (GTK_WIDGET (new_tab));

	ephy_window_add_tab (window, new_tab, EPHY_NOTEBOOK_INSERT_LAST, FALSE);

	*new_embed = ephy_tab_get_embed (new_tab);
}

static void
gnv_bonobo_control_activate_cb (BonoboControl *control, gboolean state, EphyNautilusView *view)
{
	if (state)
	{
		EphyNautilusViewPrivate *p = view->priv;
		
		p->ui = nautilus_view_set_up_ui (NAUTILUS_VIEW (view), SHARE_DIR,
						 "nautilus-epiphany-view.xml", "EphyNautilusView");
		g_return_if_fail (BONOBO_IS_UI_COMPONENT (p->ui));

		bonobo_ui_component_add_verb_list_with_data (p->ui, ephy_verbs, view);
	}
}

static EphyNautilusView *
gnv_view_from_popup (EphyEmbedPopupControl*popup)
{
	return g_object_get_data (G_OBJECT (popup), "NautilisView");
}


static void 
gnv_popup_cmd_new_window (BonoboUIComponent *uic, 
			  EphyEmbedPopupControl*popup, 
			  const char* verbname)
{
	EphyEmbedEvent *info;
	EphyNautilusView *view;
	const GValue *value;

	view = gnv_view_from_popup (popup);
	
	info = ephy_embed_popup_control_get_event (popup);
	
	ephy_embed_event_get_property (info, "link", &value);

	nautilus_view_open_location (NAUTILUS_VIEW (view),
				     g_value_get_string (value),
				     Nautilus_ViewFrame_OPEN_IN_NAVIGATION,
				     0, NULL);
}

static void 
gnv_popup_cmd_image_in_new_window (BonoboUIComponent *uic, 
				   EphyEmbedPopupControl*popup, 
				   const char* verbname)
{
	EphyEmbedEvent *info;
	EphyNautilusView *view;
	const GValue *value;

	view = gnv_view_from_popup (popup);
	
	info = ephy_embed_popup_control_get_event (popup);
	
	ephy_embed_event_get_property (info, "image", &value);

	nautilus_view_open_location (NAUTILUS_VIEW (view),
				     g_value_get_string (value),
				     Nautilus_ViewFrame_OPEN_IN_NAVIGATION,
				     0, NULL);
}

static void 
gnv_popup_cmd_frame_in_new_window (BonoboUIComponent *uic, 
				   EphyEmbedPopupControl*popup, 
				   const char* verbname)
{
	EphyEmbedEvent *info;
	EphyNautilusView *view;
	gchar *location;

	view = gnv_view_from_popup (popup);
	
	info = ephy_embed_popup_control_get_event (popup);
	
	location = ephy_embed_get_location (view->priv->embed, FALSE);

	nautilus_view_open_location (NAUTILUS_VIEW (view),
				     location,
				     Nautilus_ViewFrame_OPEN_IN_NAVIGATION,
				     0, NULL);
	
	g_free (location);
}

static void
gnv_cmd_select_encoding (BonoboUIComponent *uic, 
			 EphyNautilusView *view, 
			 const char* verbname)
{
	EphyDialog *dialog;
	
	dialog = EPHY_DIALOG (g_object_new (EPHY_TYPE_ENCODING_DIALOG,
					    "embed", view->priv->embed,
					    NULL));

	ephy_dialog_set_modal (dialog, TRUE);
	ephy_dialog_run (dialog);
	g_object_unref (dialog);
}

static void
gnv_cmd_file_print (BonoboUIComponent *uic, 
		    EphyNautilusView *view, 
		    const char* verbname)
{
	EphyDialog *dialog;
	EphyNautilusViewPrivate *p = view->priv;
	
	dialog = ephy_print_dialog_new (NULL, p->embed, FALSE);

	ephy_dialog_set_modal (dialog, TRUE);
	ephy_dialog_run (dialog);
	g_object_unref (dialog);
}

static void
gnv_cmd_edit_find (BonoboUIComponent *uic, 
		   EphyNautilusView *view, 
		   const char* verbname)
{
	EphyNautilusViewPrivate *p = view->priv;

	if (p->find_dialog == NULL)
	{
		p->find_dialog = find_dialog_new (p->embed);
		g_object_add_weak_pointer (G_OBJECT (p->find_dialog),
					   (gpointer *) &p->find_dialog);
	}

	ephy_dialog_show (p->find_dialog);
}

/* zoomable */
static void
gnv_zoomable_set_zoom_level_cb (BonoboZoomable *zoomable,
				float level,
				EphyNautilusView *view)
{
	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));
	ephy_embed_zoom_set (view->priv->embed,
			     ephy_zoom_get_nearest_zoom_level (level), TRUE);
}

static void
gnv_zoomable_zoom_in_cb (BonoboZoomable *zoomable,
			 EphyNautilusView *view)
{
	float zoom, new_zoom;
	
	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));

	zoom = ephy_embed_zoom_get (view->priv->embed);

	new_zoom = ephy_zoom_get_changed_zoom_level (zoom, 1);
	ephy_embed_zoom_set (view->priv->embed, new_zoom, TRUE);
}

static void
gnv_zoomable_zoom_out_cb (BonoboZoomable *zoomable,
			  EphyNautilusView *view)
{
	float zoom, new_zoom;
		
	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));

	zoom = ephy_embed_zoom_get (view->priv->embed);

	new_zoom = ephy_zoom_get_changed_zoom_level (zoom, -1);
	ephy_embed_zoom_set (view->priv->embed, new_zoom, TRUE);
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
	ephy_embed_zoom_set (view->priv->embed, 1.0, TRUE);
}

static void
gnv_embed_zoom_change_cb (EphyNautilusView *embed,
			  float new_zoom, 
			  EphyNautilusView *view)
{
	g_return_if_fail (EPHY_IS_NAUTILUS_VIEW (view));
	
	bonobo_zoomable_report_zoom_level_changed (view->priv->zoomable,
						   new_zoom, NULL);
}
