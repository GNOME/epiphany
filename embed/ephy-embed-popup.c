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

#include "ephy-embed-popup.h"
#include "ephy-embed-event.h"
#include "ephy-embed-utils.h"
#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-bonobo-extensions.h"
#include "ephy-file-helpers.h"

#include <string.h>
#include <bonobo/bonobo-ui-component.h>
#include <gtk/gtkclipboard.h>
#include <libgnome/gnome-exec.h>

typedef enum
{
	EMBED_POPUP_INPUT,
	EMBED_POPUP_DOCUMENT,
	EMBED_POPUP_ELEMENT
} EmbedPopupType;

struct EphyEmbedPopupPrivate
{
	EphyEmbedEvent *event;
	EphyEmbed *embed;
	EmbedEventContext context;
	BonoboUIComponent *ui_component;
	char *selection;
	EmbedPopupType popup_type;
};

static void
ephy_embed_popup_class_init (EphyEmbedPopupClass *klass);
static void
ephy_embed_popup_init (EphyEmbedPopup *gep);
static void
ephy_embed_popup_finalize (GObject *object);
static void
embed_popup_copy_location_cmd (BonoboUIComponent *uic,
                               EphyEmbedPopup *popup,
                               const char* verbname);
static void
embed_popup_copy_email_cmd (BonoboUIComponent *uic,
                            EphyEmbedPopup *popup,
                            const char* verbname);
static void
embed_popup_copy_link_location_cmd (BonoboUIComponent *uic,
				    EphyEmbedPopup *popup,
                                    const char* verbname);
static void
embed_popup_download_link_cmd (BonoboUIComponent *uic,
			       EphyEmbedPopup *popup,
                               const char* verbname);
static void
embed_popup_save_image_as_cmd (BonoboUIComponent *uic,
			       EphyEmbedPopup *popup,
                               const char* verbname);
static void
embed_popup_set_image_as_background_cmd (BonoboUIComponent *uic,
					 EphyEmbedPopup *popup,
					 const char* verbname);
static void
embed_popup_copy_image_location_cmd (BonoboUIComponent *uic,
				     EphyEmbedPopup *popup,
                                     const char* verbname);
static void
embed_popup_save_page_as_cmd (BonoboUIComponent *uic,
			      EphyEmbedPopup *popup,
                              const char* verbname);
static void
embed_popup_save_background_as_cmd (BonoboUIComponent *uic,
				    EphyEmbedPopup *popup,
                                    const char* verbname);
static void
embed_popup_open_frame_cmd (BonoboUIComponent *uic,
			    EphyEmbedPopup *popup,
                            const char* verbname);
static void
embed_popup_reload_frame_cmd (BonoboUIComponent *uic,
			      EphyEmbedPopup *popup,
                              const char* verbname);

static void
embed_popup_open_image_cmd (BonoboUIComponent *uic,
			    EphyEmbedPopup *popup,
                            const char* verbname);
static void
embed_popup_copy_to_clipboard (EphyEmbedPopup *popup, const char *text);

static GObjectClass *parent_class = NULL;

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

GType
ephy_embed_popup_get_type (void)
{
       static GType ephy_embed_popup_type = 0;

        if (ephy_embed_popup_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyEmbedPopupClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_embed_popup_class_init,
                        NULL, /* class_finalize */
                        NULL, /* class_data */
                        sizeof (EphyEmbedPopup),
                        0,    /* n_preallocs */
                        (GInstanceInitFunc) ephy_embed_popup_init
                };


                ephy_embed_popup_type = g_type_register_static (G_TYPE_OBJECT,
								  "EphyEmbedPopup",
								  &our_info, 0);
        }

        return ephy_embed_popup_type;
}

static void
ephy_embed_popup_class_init (EphyEmbedPopupClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

        object_class->finalize = ephy_embed_popup_finalize;

	klass->show = NULL; /* abstract */
}

static void
ephy_embed_popup_init (EphyEmbedPopup *gep)
{
        gep->priv = g_new0 (EphyEmbedPopupPrivate, 1);
	gep->priv->embed = NULL;
	gep->priv->event = NULL;
	gep->priv->ui_component = NULL;
}

static void
ephy_embed_popup_finalize (GObject *object)
{
	EphyEmbedPopup *gep;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_EPHY_EMBED_POPUP (object));

        gep = EPHY_EMBED_POPUP (object);

        g_return_if_fail (gep->priv != NULL);

	if (gep->priv->event)
	{
		g_object_unref (G_OBJECT (gep->priv->event));
	}

	g_free (gep->priv->selection);

        g_free (gep->priv);

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
setup_element_menu (EphyEmbedPopup *p)
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
setup_document_menu (EphyEmbedPopup *p)
{
	gboolean is_framed;
	GValue *value;
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
ephy_embed_popup_set_event (EphyEmbedPopup *p,
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
ephy_embed_popup_set_embed (EphyEmbedPopup *p,
			    EphyEmbed *e)
{
	p->priv->embed = e;
}

EphyEmbed *
ephy_embed_popup_get_embed (EphyEmbedPopup *p)
{
	return p->priv->embed;
}

void
ephy_embed_popup_show (EphyEmbedPopup *p,
			 EphyEmbed *embed)
{
	EphyEmbedPopupClass *klass = EPHY_EMBED_POPUP_GET_CLASS (p);
	return klass->show (p, embed);

}

void
ephy_embed_popup_connect_verbs (EphyEmbedPopup *p,
				  BonoboUIComponent *ui_component)
{

	p->priv->ui_component = BONOBO_UI_COMPONENT (ui_component);

	bonobo_ui_component_add_verb_list_with_data (BONOBO_UI_COMPONENT(ui_component),
                                                     embed_popup_verbs,
                                                     p);
}

EphyEmbedEvent *
ephy_embed_popup_get_event (EphyEmbedPopup *p)
{
	g_return_val_if_fail (IS_EPHY_EMBED_POPUP (p), NULL);

	return p->priv->event;
}

static void
embed_popup_copy_location_cmd (BonoboUIComponent *uic,
                               EphyEmbedPopup *popup,
                               const char* verbname)
{
	char *location;
	ephy_embed_get_location (popup->priv->embed, FALSE,
				   FALSE, &location);
	embed_popup_copy_to_clipboard (popup, location);
	g_free (location);
}

static void
embed_popup_copy_email_cmd (BonoboUIComponent *uic,
                            EphyEmbedPopup *popup,
                            const char* verbname)
{
	EphyEmbedEvent *info;
	const char *location;
	GValue *value;

	info = ephy_embed_popup_get_event (popup);
	ephy_embed_event_get_property (info, "email", &value);
	location = g_value_get_string (value);
	embed_popup_copy_to_clipboard (popup, location);
}

static void
embed_popup_copy_link_location_cmd (BonoboUIComponent *uic,
				    EphyEmbedPopup *popup,
                                    const char* verbname)
{
	EphyEmbedEvent *info;
	const char *location;
	GValue *value;

	info = ephy_embed_popup_get_event (popup);
	ephy_embed_event_get_property (info, "link", &value);
	location = g_value_get_string (value);
	embed_popup_copy_to_clipboard (popup, location);
}

static void
save_property_url (EphyEmbedPopup *popup,
		   gboolean ask_dest,
		   gboolean show_progress,
		   const char *property)
{
	EphyEmbedEvent *info;
	const char *location;
	GValue *value;
	GtkWidget *widget;
	GtkWidget *window;
	EphyEmbedPersist *persist;

	info = ephy_embed_popup_get_event (popup);
	ephy_embed_event_get_property (info, property, &value);
	location = g_value_get_string (value);

	widget = GTK_WIDGET (popup->priv->embed);
	window = gtk_widget_get_toplevel (widget);

	persist = ephy_embed_persist_new (popup->priv->embed);

	ephy_embed_persist_set_source (persist, location);

	if (show_progress)
	{
		ephy_embed_persist_set_flags (persist,
					      EMBED_PERSIST_SHOW_PROGRESS);
	}

	ephy_embed_utils_save (window,
			       CONF_STATE_DOWNLOADING_DIR,
			       ask_dest,
                               FALSE,
                               persist);
}

const char *
ephy_embed_popup_get_popup_path (EphyEmbedPopup *p)
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

/* commands */

static void
embed_popup_download_link_cmd (BonoboUIComponent *uic,
			       EphyEmbedPopup *popup,
                               const char* verbname)
{
	save_property_url (popup,
		           eel_gconf_get_boolean
		           (CONF_STATE_DOWNLOADING_DIR),
		           TRUE, "link");
}

static void
embed_popup_save_image_as_cmd (BonoboUIComponent *uic,
			       EphyEmbedPopup *popup,
                               const char* verbname)
{
	save_property_url (popup, TRUE, FALSE, "image");
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
					 EphyEmbedPopup *popup,
					 const char* verbname)
{
	EphyEmbedEvent *info;
	const char *location;
	char *dest, *base;
	GValue *value;
	EphyEmbedPersist *persist;

	info = ephy_embed_popup_get_event (popup);
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
				     EphyEmbedPopup *popup,
                                     const char* verbname)
{
	EphyEmbedEvent *info;
	const char *location;
	GValue *value;

	info = ephy_embed_popup_get_event (popup);
	ephy_embed_event_get_property (info, "image", &value);
	location = g_value_get_string (value);
	embed_popup_copy_to_clipboard (popup, location);
}

static void
save_url (EphyEmbedPopup *popup,
	  gboolean ask_dest,
	  gboolean show_progress,
	  const char *url)
{
	GtkWidget *widget;
	GtkWidget *window;
	EphyEmbedPersist *persist;

	widget = GTK_WIDGET (popup->priv->embed);
	window = gtk_widget_get_toplevel (widget);

	persist = ephy_embed_persist_new (popup->priv->embed);
	ephy_embed_persist_set_source (persist, url);

	if (show_progress)
	{
		ephy_embed_persist_set_flags (persist,
						EMBED_PERSIST_SHOW_PROGRESS);
	}

	ephy_embed_utils_save (window,
			       CONF_STATE_DOWNLOADING_DIR,
			       ask_dest,
                               FALSE,
                               persist);
}

static void
embed_popup_save_page_as_cmd (BonoboUIComponent *uic,
			      EphyEmbedPopup *popup,
                              const char* verbname)
{
	char *location;

	ephy_embed_get_location (popup->priv->embed,
				   FALSE, FALSE, &location);
	save_url (popup, TRUE, FALSE, location);
	g_free (location);
}

static void
embed_popup_save_background_as_cmd (BonoboUIComponent *uic,
				    EphyEmbedPopup *popup,
                                    const char* verbname)
{
	save_property_url (popup, TRUE, FALSE, "background_image");
}

static void
embed_popup_open_frame_cmd (BonoboUIComponent *uic,
			    EphyEmbedPopup *popup,
                            const char* verbname)
{
	char *location;

	ephy_embed_get_location (popup->priv->embed,
				   FALSE, FALSE, &location);

	ephy_embed_load_url (popup->priv->embed, location);
}

static void
embed_popup_reload_frame_cmd (BonoboUIComponent *uic,
			      EphyEmbedPopup *popup,
                              const char* verbname)
{
	/* FIXME implement */
}

static void
embed_popup_open_image_cmd (BonoboUIComponent *uic,
			    EphyEmbedPopup *popup,
                            const char* verbname)
{
	EphyEmbedEvent *info;
	const char *location;
	GValue *value;

	info = ephy_embed_popup_get_event (popup);
	ephy_embed_event_get_property (info, "image", &value);
	location = g_value_get_string (value);

	ephy_embed_load_url (popup->priv->embed, location);
}

static void
embed_popup_copy_to_clipboard (EphyEmbedPopup *popup, const char *text)
{
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_NONE),
				text, -1);
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY),
				text, -1);
}
