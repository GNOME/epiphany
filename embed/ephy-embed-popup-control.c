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
 *
 *  $Id$
 */

#include "ephy-embed-popup-control.h"
#include "ephy-bonobo-extensions.h"
#include "ephy-embed-utils.h"
#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-file-helpers.h"

#include <gtk/gtkmain.h>
#include <string.h>
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkclipboard.h>
#include <libgnome/gnome-exec.h>

typedef enum
{
	EMBED_POPUP_INPUT,
	EMBED_POPUP_DOCUMENT,
	EMBED_POPUP_ELEMENT
} EmbedPopupType;

#define EPHY_EMBED_POPUP_CONTROL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EMBED_POPUP_CONTROL, EphyEmbedPopupControlPrivate))

struct EphyEmbedPopupControlPrivate
{
	EphyEmbedEvent *event;
	EphyEmbed *embed;
	EmbedEventContext context;
	BonoboUIComponent *ui_component;
	char *selection;
	EmbedPopupType popup_type;
	BonoboControl *control;
};

static void
embed_popup_copy_location_cmd (BonoboUIComponent *uic,
                               EphyEmbedPopupControl *popup,
                               const char* verbname);
static void
embed_popup_copy_email_cmd (BonoboUIComponent *uic,
                            EphyEmbedPopupControl *popup,
                            const char* verbname);
static void
embed_popup_copy_link_location_cmd (BonoboUIComponent *uic,
				    EphyEmbedPopupControl *popup,
                                    const char* verbname);
static void
embed_popup_open_link_cmd (BonoboUIComponent *uic,
			   EphyEmbedPopupControl *popup,
                           const char* verbname);
static void
embed_popup_download_link_cmd (BonoboUIComponent *uic,
			       EphyEmbedPopupControl *popup,
                               const char* verbname);
static void
embed_popup_save_image_as_cmd (BonoboUIComponent *uic,
			       EphyEmbedPopupControl *popup,
                               const char* verbname);
static void
embed_popup_set_image_as_background_cmd (BonoboUIComponent *uic,
					 EphyEmbedPopupControl *popup,
					 const char* verbname);
static void
embed_popup_copy_image_location_cmd (BonoboUIComponent *uic,
				     EphyEmbedPopupControl *popup,
                                     const char* verbname);
static void
embed_popup_save_page_as_cmd (BonoboUIComponent *uic,
			      EphyEmbedPopupControl *popup,
                              const char* verbname);
static void
embed_popup_save_background_as_cmd (BonoboUIComponent *uic,
				    EphyEmbedPopupControl *popup,
                                    const char* verbname);
static void
embed_popup_open_frame_cmd (BonoboUIComponent *uic,
			    EphyEmbedPopupControl *popup,
                            const char* verbname);
static void
embed_popup_reload_frame_cmd (BonoboUIComponent *uic,
			      EphyEmbedPopupControl *popup,
                              const char* verbname);

static void
embed_popup_open_image_cmd (BonoboUIComponent *uic,
			    EphyEmbedPopupControl *popup,
                            const char* verbname);
static void
embed_popup_copy_to_clipboard (EphyEmbedPopupControl *popup, const char *text);

#define DOCUMENT_POPUP_PATH "/popups/EphyEmbedDocumentPopup"
#define ELEMENT_POPUP_PATH "/popups/EphyEmbedElementPopup"
#define INPUT_POPUP_PATH "/popups/EphyEmbedInputPopup"

#define EPHY_POPUP_NAVIGATION_ITEMS_PLACEHOLDER "/popups/EphyEmbedDocumentPopup/NavigationItems"
#define EPHY_POPUP_LINK_ITEMS_PLACEHOLDER "/popups/EphyEmbedElementPopup/LinkItems"
#define EPHY_POPUP_EMAIL_LINK_ITEMS_PLACEHOLDER "/popups/EphyEmbedElementPopup/EmailLinkItems"
#define EPHY_POPUP_IMAGE_ITEMS_PLACEHOLDER "/popups/EphyEmbedElementPopup/ImageItems"
#define EPHY_POPUP_FRAME_ITEMS_PLACEHOLDER "/popups/EphyEmbedDocumentPopup/FrameItems"
#define EPHY_POPUP_BETWEEN_ELEMENTS1_PLACEHOLDER "/popups/EphyEmbedElementPopup/BetweenElements1"
#define EPHY_POPUP_SAVE_BG_PATH "/commands/DPSaveBackgroundAs"
#define EPHY_POPUP_OPEN_IMAGE_PATH "/commands/EPOpenImage"

BonoboUIVerb embed_popup_verbs [] = {
	BONOBO_UI_VERB ("EPCopyLinkLocation", (BonoboUIVerbFn)embed_popup_copy_link_location_cmd),
	BONOBO_UI_VERB ("EPDownloadLink", (BonoboUIVerbFn)embed_popup_download_link_cmd),
	BONOBO_UI_VERB ("EPOpenLink", (BonoboUIVerbFn)embed_popup_open_link_cmd),
	BONOBO_UI_VERB ("EPOpenImage", (BonoboUIVerbFn)embed_popup_open_image_cmd),
	BONOBO_UI_VERB ("EPSaveImageAs", (BonoboUIVerbFn)embed_popup_save_image_as_cmd),
	BONOBO_UI_VERB ("EPSetImageAsBackground", (BonoboUIVerbFn)embed_popup_set_image_as_background_cmd),
	BONOBO_UI_VERB ("EPCopyImageLocation", (BonoboUIVerbFn)embed_popup_copy_image_location_cmd),

	BONOBO_UI_VERB ("DPCopyLocation", (BonoboUIVerbFn)embed_popup_copy_location_cmd),
	BONOBO_UI_VERB ("EPCopyEmail", (BonoboUIVerbFn)embed_popup_copy_email_cmd),
        BONOBO_UI_VERB ("DPSavePageAs", (BonoboUIVerbFn)embed_popup_save_page_as_cmd),
	BONOBO_UI_VERB ("DPSaveBackgroundAs", (BonoboUIVerbFn)embed_popup_save_background_as_cmd),
	BONOBO_UI_VERB ("DPOpenFrame", (BonoboUIVerbFn)embed_popup_open_frame_cmd),
	BONOBO_UI_VERB ("DPReloadFrame", (BonoboUIVerbFn)embed_popup_reload_frame_cmd),

	BONOBO_UI_VERB_END
};

enum
{
        PROP_0,
        PROP_BONOBO_CONTROL
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

static GObjectClass *parent_class = NULL;

GType
ephy_embed_popup_control_get_type (void)
{
       static GType ephy_embed_popup_control_type = 0;

        if (ephy_embed_popup_control_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyEmbedPopupControlClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_embed_popup_control_class_init,
                        NULL, /* class_finalize */
                        NULL, /* class_data */
                        sizeof (EphyEmbedPopupControl),
                        0,    /* n_preallocs */
                        (GInstanceInitFunc) ephy_embed_popup_control_init
                };


                ephy_embed_popup_control_type = g_type_register_static (G_TYPE_OBJECT,
								        "EphyEmbedPopupControl",
								        &our_info, 0);
        }

        return ephy_embed_popup_control_type;
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
ephy_embed_popup_control_class_init (EphyEmbedPopupControlClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

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

	g_type_class_add_private (object_class, sizeof(EphyEmbedPopupControlPrivate));
}

static void
ephy_embed_popup_control_init (EphyEmbedPopupControl *gep)
{
        gep->priv = EPHY_EMBED_POPUP_CONTROL_GET_PRIVATE (gep);

	gep->priv->control = NULL;
	gep->priv->embed = NULL;
	gep->priv->event = NULL;
	gep->priv->ui_component = NULL;
}

static void
ephy_embed_popup_control_finalize (GObject *object)
{
	EphyEmbedPopupControl *gep = EPHY_EMBED_POPUP_CONTROL (object);

	if (gep->priv->event)
	{
		g_object_unref (G_OBJECT (gep->priv->event));
	}

	g_free (gep->priv->selection);

        G_OBJECT_CLASS (parent_class)->finalize (object);
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

	p = EPHY_EMBED_POPUP_CONTROL (g_object_new (EPHY_TYPE_EMBED_POPUP_CONTROL,
						    "BonoboControl", control,
						    NULL));

        return p;
}

static const char *
get_popup_path (EphyEmbedPopupControl *p)
{
	const char *result = NULL;

	switch (p->priv->popup_type)
	{
		case EMBED_POPUP_INPUT:
			result = INPUT_POPUP_PATH;
			break;
		case EMBED_POPUP_ELEMENT:
			result = ELEMENT_POPUP_PATH;
			break;
		case EMBED_POPUP_DOCUMENT:
			result = DOCUMENT_POPUP_PATH;
			break;
	}

	return result;
}

void
ephy_embed_popup_control_show (EphyEmbedPopupControl *pp,
			       EphyEmbed *embed)
{
	EphyEmbedPopupControl *p = EPHY_EMBED_POPUP_CONTROL (pp);
	BonoboUIComponent *uic = bonobo_control_get_popup_ui_component (p->priv->control);
	const char *path;
	char *path_dst;

	p->priv->embed = embed;
	path = get_popup_path (pp);
	path_dst = g_strdup_printf ("/popups/button%d", 2);

	/* this is a hack because bonobo apis for showing popups are broken */
	ephy_bonobo_replace_path (uic, path, path_dst);

	bonobo_control_do_popup (p->priv->control, 2,
				 gtk_get_current_event_time ());
}

static void
setup_element_menu (EphyEmbedPopupControl *p)
{
	gboolean is_link, is_image, is_email_link;

	is_image = p->priv->context & EMBED_CONTEXT_IMAGE;
	is_email_link =  p->priv->context & EMBED_CONTEXT_EMAIL_LINK;
	is_link = (p->priv->context & EMBED_CONTEXT_LINK) && !is_email_link;

	ephy_bonobo_set_hidden (p->priv->ui_component,
			       EPHY_POPUP_LINK_ITEMS_PLACEHOLDER,
			       !is_link);
	ephy_bonobo_set_hidden (p->priv->ui_component,
			       EPHY_POPUP_IMAGE_ITEMS_PLACEHOLDER,
			       !is_image);
	ephy_bonobo_set_hidden (p->priv->ui_component,
			       EPHY_POPUP_EMAIL_LINK_ITEMS_PLACEHOLDER,
			       !is_email_link);
	ephy_bonobo_set_hidden (p->priv->ui_component,
			       EPHY_POPUP_BETWEEN_ELEMENTS1_PLACEHOLDER,
			       !is_image || (!is_link && !is_email_link));
}

static void
setup_document_menu (EphyEmbedPopupControl *p)
{
	gboolean is_framed;
	const GValue *value;
	gboolean has_background;

	ephy_embed_event_get_property (p->priv->event,
					 "framed_page", &value);
	is_framed = g_value_get_int (value);
	ephy_bonobo_set_hidden (BONOBO_UI_COMPONENT(p->priv->ui_component),
			       EPHY_POPUP_FRAME_ITEMS_PLACEHOLDER, !is_framed);

	has_background = ephy_embed_event_has_property (p->priv->event,
							  "background_image");
	ephy_bonobo_set_hidden (BONOBO_UI_COMPONENT(p->priv->ui_component),
			       EPHY_POPUP_SAVE_BG_PATH, !has_background);
}

void
ephy_embed_popup_control_set_event (EphyEmbedPopupControl *p,
			            EphyEmbedEvent *event)
{
	EmbedEventContext context;

	if (p->priv->event)
	{
		g_object_unref (G_OBJECT (p->priv->event));
	}

	ephy_embed_event_get_context (event, &context);

	p->priv->context = context;

	p->priv->event = event;
	g_object_ref (G_OBJECT(event));

	if ((p->priv->context & EMBED_CONTEXT_LINK) ||
	    (p->priv->context & EMBED_CONTEXT_EMAIL_LINK) ||
	    (p->priv->context & EMBED_CONTEXT_IMAGE))
	{
		setup_element_menu (p);
		p->priv->popup_type = EMBED_POPUP_ELEMENT;
	}
	else if (p->priv->context & EMBED_CONTEXT_INPUT)
	{
		p->priv->popup_type = EMBED_POPUP_INPUT;
	}
	else
	{
		setup_document_menu (p);
		p->priv->popup_type = EMBED_POPUP_DOCUMENT;
	}
}

void
ephy_embed_popup_control_connect_verbs (EphyEmbedPopupControl *p,
				        BonoboUIComponent *ui_component)
{

	p->priv->ui_component = BONOBO_UI_COMPONENT (ui_component);

	bonobo_ui_component_add_verb_list_with_data (BONOBO_UI_COMPONENT(ui_component),
                                                     embed_popup_verbs,
                                                     p);
}

EphyEmbedEvent *
ephy_embed_popup_control_get_event (EphyEmbedPopupControl *p)
{
	g_return_val_if_fail (EPHY_IS_EMBED_POPUP_CONTROL (p), NULL);

	return p->priv->event;
}

static void
embed_popup_copy_location_cmd (BonoboUIComponent *uic,
                               EphyEmbedPopupControl *popup,
                               const char* verbname)
{
	char *location;
	ephy_embed_get_location (popup->priv->embed, FALSE, &location);
	embed_popup_copy_to_clipboard (popup, location);
	g_free (location);
}

static void
embed_popup_copy_email_cmd (BonoboUIComponent *uic,
                            EphyEmbedPopupControl *popup,
                            const char* verbname)
{
	EphyEmbedEvent *info;
	const char *location;
	const GValue *value;

	info = ephy_embed_popup_control_get_event (popup);
	ephy_embed_event_get_property (info, "email", &value);
	location = g_value_get_string (value);
	embed_popup_copy_to_clipboard (popup, location);
}

static void
embed_popup_copy_link_location_cmd (BonoboUIComponent *uic,
				    EphyEmbedPopupControl *popup,
                                    const char* verbname)
{
	EphyEmbedEvent *info;
	const char *location;
	const GValue *value;

	info = ephy_embed_popup_control_get_event (popup);
	ephy_embed_event_get_property (info, "link", &value);
	location = g_value_get_string (value);
	embed_popup_copy_to_clipboard (popup, location);
}

static void
save_property_url (EphyEmbedPopupControl *popup,
		   const char *title,
		   gboolean ask_dest,
		   const char *property)
{
	EphyEmbedEvent *info;
	const char *location;
	const GValue *value;
	GtkWidget *widget;
	GtkWidget *window;
	EphyEmbedPersist *persist;

	info = ephy_embed_popup_control_get_event (popup);
	ephy_embed_event_get_property (info, property, &value);
	location = g_value_get_string (value);

	widget = GTK_WIDGET (popup->priv->embed);
	window = gtk_widget_get_toplevel (widget);

	persist = ephy_embed_persist_new (popup->priv->embed);

	ephy_embed_persist_set_source (persist, location);

	ephy_embed_utils_save (window, title,
			       CONF_STATE_DOWNLOADING_DIR,
			       ask_dest, persist);
}

/* commands */

static void
embed_popup_open_link_cmd (BonoboUIComponent *uic,
			   EphyEmbedPopupControl *popup,
                           const char* verbname)
{
	EphyEmbedEvent *info;
	const char *location;
	const GValue *value;

	info = ephy_embed_popup_control_get_event (popup);
	ephy_embed_event_get_property (info, "link", &value);
	location = g_value_get_string (value);

	ephy_embed_load_url (popup->priv->embed, location);
}

static void
embed_popup_download_link_cmd (BonoboUIComponent *uic,
			       EphyEmbedPopupControl *popup,
                               const char* verbname)
{
	save_property_url (popup,
			   _("Download Link"),
		           eel_gconf_get_boolean
		           (CONF_ASK_DOWNLOAD_DEST),
		           "link");
}

static void
embed_popup_save_image_as_cmd (BonoboUIComponent *uic,
			       EphyEmbedPopupControl *popup,
                               const char* verbname)
{
	save_property_url (popup, _("Save Image As"), TRUE, "image");
}

#define CONF_DESKTOP_BG_PICTURE "/desktop/gnome/background/picture_filename"
#define CONF_DESKTOP_BG_TYPE "/desktop/gnome/background/picture_options"

static void
background_download_completed (EphyEmbedPersist *persist,
			       gpointer data)
{
	const char *bg;
	char *type;

	ephy_embed_persist_get_dest (persist, &bg);
	eel_gconf_set_string (CONF_DESKTOP_BG_PICTURE, bg);

	type = eel_gconf_get_string (CONF_DESKTOP_BG_TYPE);
	if (type || strcmp (type, "none") == 0)
	{
		eel_gconf_set_string (CONF_DESKTOP_BG_TYPE,
				      "wallpaper");
	}

	g_free (type);

	g_object_unref (persist);
}

static void
embed_popup_set_image_as_background_cmd (BonoboUIComponent *uic,
					 EphyEmbedPopupControl *popup,
					 const char* verbname)
{
	EphyEmbedEvent *info;
	const char *location;
	char *dest, *base;
	const GValue *value;
	EphyEmbedPersist *persist;

	info = ephy_embed_popup_control_get_event (popup);
	ephy_embed_event_get_property (info, "image", &value);
	location = g_value_get_string (value);

	persist = ephy_embed_persist_new (popup->priv->embed);

	base = g_path_get_basename (location);
	dest = g_build_filename (ephy_dot_dir (),
				 base, NULL);

	ephy_embed_persist_set_source (persist, location);
	ephy_embed_persist_set_dest (persist, dest);

	ephy_embed_persist_save (persist);

	g_signal_connect (persist, "completed",
			  G_CALLBACK (background_download_completed),
			  NULL);

	g_free (dest);
	g_free (base);
}

static void
embed_popup_copy_image_location_cmd (BonoboUIComponent *uic,
				     EphyEmbedPopupControl *popup,
                                     const char* verbname)
{
	EphyEmbedEvent *info;
	const char *location;
	const GValue *value;

	info = ephy_embed_popup_control_get_event (popup);
	ephy_embed_event_get_property (info, "image", &value);
	location = g_value_get_string (value);
	embed_popup_copy_to_clipboard (popup, location);
}

static void
save_url (EphyEmbedPopupControl *popup,
	  const char *title,
	  gboolean ask_dest,
	  const char *url)
{
	GtkWidget *widget;
	GtkWidget *window;
	EphyEmbedPersist *persist;

	widget = GTK_WIDGET (popup->priv->embed);
	window = gtk_widget_get_toplevel (widget);

	persist = ephy_embed_persist_new (popup->priv->embed);
	ephy_embed_persist_set_source (persist, url);

	ephy_embed_utils_save (window, title,
			       CONF_STATE_DOWNLOADING_DIR,
			       ask_dest, persist);
}

static void
embed_popup_save_page_as_cmd (BonoboUIComponent *uic,
			      EphyEmbedPopupControl *popup,
                              const char* verbname)
{
	char *location;

	ephy_embed_get_location (popup->priv->embed,
				   FALSE, &location);
	save_url (popup, _("Save Page As"), TRUE, location);
	g_free (location);
}

static void
embed_popup_save_background_as_cmd (BonoboUIComponent *uic,
				    EphyEmbedPopupControl *popup,
                                    const char* verbname)
{
	save_property_url (popup, _("Save Background As"),
			   TRUE, "background_image");
}

static void
embed_popup_open_frame_cmd (BonoboUIComponent *uic,
			    EphyEmbedPopupControl *popup,
                            const char* verbname)
{
	char *location;

	ephy_embed_get_location (popup->priv->embed,
				   FALSE, &location);

	ephy_embed_load_url (popup->priv->embed, location);

	g_free (location);
}

static void
embed_popup_reload_frame_cmd (BonoboUIComponent *uic,
			      EphyEmbedPopupControl *popup,
                              const char* verbname)
{
	/* FIXME implement */
}

static void
embed_popup_open_image_cmd (BonoboUIComponent *uic,
			    EphyEmbedPopupControl *popup,
                            const char* verbname)
{
	EphyEmbedEvent *info;
	const char *location;
	const GValue *value;

	info = ephy_embed_popup_control_get_event (popup);
	ephy_embed_event_get_property (info, "image", &value);
	location = g_value_get_string (value);

	ephy_embed_load_url (popup->priv->embed, location);
}

static void
embed_popup_copy_to_clipboard (EphyEmbedPopupControl *popup, const char *text)
{
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_NONE),
				text, -1);
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY),
				text, -1);
}
