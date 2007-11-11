/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#ifndef EPHY_EMBED_H
#define EPHY_EMBED_H

#include "ephy-embed-event.h"

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define EPHY_TYPE_EMBED			(ephy_embed_get_type ())
#define EPHY_EMBED(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_EMBED, EphyEmbed))
#define EPHY_EMBED_IFACE(k)		(G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_EMBED, EphyEmbedIface))
#define EPHY_IS_EMBED(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_EMBED))
#define EPHY_IS_EMBED_IFACE(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_EMBED))
#define EPHY_EMBED_GET_IFACE(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_EMBED, EphyEmbedIface))

typedef struct _EphyEmbed	EphyEmbed;
typedef struct _EphyEmbedIface	EphyEmbedIface;

typedef enum
{
	EPHY_EMBED_STATE_UNKNOWN	= 0,
	EPHY_EMBED_STATE_START		= 1 << 0,
	EPHY_EMBED_STATE_REDIRECTING	= 1 << 1,
	EPHY_EMBED_STATE_TRANSFERRING	= 1 << 2,
	EPHY_EMBED_STATE_NEGOTIATING	= 1 << 3,
	EPHY_EMBED_STATE_STOP		= 1 << 4,

	EPHY_EMBED_STATE_IS_REQUEST	= 1 << 5,
	EPHY_EMBED_STATE_IS_DOCUMENT	= 1 << 6,
	EPHY_EMBED_STATE_IS_NETWORK	= 1 << 7,
	EPHY_EMBED_STATE_IS_WINDOW	= 1 << 8,
	EPHY_EMBED_STATE_RESTORING	= 1 << 9
} EphyEmbedNetState;

typedef enum
{
	EPHY_EMBED_CHROME_MENUBAR	= 1 << 0,
	EPHY_EMBED_CHROME_TOOLBAR	= 1 << 1,
	EPHY_EMBED_CHROME_STATUSBAR	= 1 << 2,
	EPHY_EMBED_CHROME_BOOKMARKSBAR	= 1 << 3
} EphyEmbedChrome;

typedef enum
{
	EPHY_EMBED_LOAD_FLAGS_NONE			= 1 << 0,
	EPHY_EMBED_LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP	= 1 << 1,
} EphyEmbedLoadFlags;

#define EPHY_EMBED_CHROME_ALL (EPHY_EMBED_CHROME_MENUBAR |	\
			       EPHY_EMBED_CHROME_TOOLBAR |	\
			       EPHY_EMBED_CHROME_STATUSBAR |	\
			       EPHY_EMBED_CHROME_BOOKMARKSBAR)

typedef enum
{
	EPHY_EMBED_PRINTPREVIEW_GOTO_PAGENUM	= 0,
	EPHY_EMBED_PRINTPREVIEW_PREV_PAGE	= 1,
	EPHY_EMBED_PRINTPREVIEW_NEXT_PAGE	= 2,
	EPHY_EMBED_PRINTPREVIEW_HOME		= 3,
	EPHY_EMBED_PRINTPREVIEW_END		= 4
} EphyEmbedPrintPreviewNavType;

typedef enum
{
	EPHY_EMBED_STATE_IS_UNKNOWN,
	EPHY_EMBED_STATE_IS_INSECURE,
	EPHY_EMBED_STATE_IS_BROKEN,
	EPHY_EMBED_STATE_IS_SECURE_LOW,
	EPHY_EMBED_STATE_IS_SECURE_MED,
	EPHY_EMBED_STATE_IS_SECURE_HIGH
} EphyEmbedSecurityLevel;

typedef enum
{
	EPHY_EMBED_DOCUMENT_HTML,
	EPHY_EMBED_DOCUMENT_XML,
	EPHY_EMBED_DOCUMENT_IMAGE,
	EPHY_EMBED_DOCUMENT_OTHER
} EphyEmbedDocumentType;

typedef enum
{
	EPHY_EMBED_NAV_UP	= 1 << 0,
	EPHY_EMBED_NAV_BACK	= 1 << 1,
	EPHY_EMBED_NAV_FORWARD	= 1 << 2
} EphyEmbedNavigationFlags;

typedef enum
{
	EPHY_EMBED_ADDRESS_EXPIRE_NOW,
	EPHY_EMBED_ADDRESS_EXPIRE_NEXT,
	EPHY_EMBED_ADDRESS_EXPIRE_CURRENT
} EphyEmbedAddressExpire;

struct _EphyEmbedIface
{
	GTypeInterface base_iface;

	/* Signals that we inherit from gtkmozembed
	 *
	 * void (* net_stop)	 (GtkMozEmbed *embed);
	 * void (* title)	 (EphyEmbed *embed);
	 * void (* visibility)	 (EphyEmbed *embed,
	 *			  gboolean visibility);
	 * void (* destroy_brsr) (EphyEmbed *embed);
	 * void (* size_to)	 (EphyEmbed *embed,
	 *			  int width,
	 *			  int height);
	 * gint (* open_uri)	 (EphyEmbed *embed,
	 *			  const char *url);
	 */	

	int	 (* context_menu)	(EphyEmbed *embed,
					 EphyEmbedEvent *event);
	void	 (* favicon)		(EphyEmbed *embed,
					 const char *location);
	void	 (* feed_link)		(EphyEmbed *embed,
					 const char *type,
					 const char *title,
					 const char *address);
	void	 (* search_link)	(EphyEmbed *embed,
					 const char *type,
					 const char *title,
					 const char *address);
	gboolean (* dom_mouse_click)	(EphyEmbed *embed,
					 EphyEmbedEvent *event);
	gboolean (* dom_mouse_down)	(EphyEmbed *embed,
					 EphyEmbedEvent *event);
	void	 (* dom_content_loaded)	(EphyEmbed *embed,
					 gpointer event);
	void	 (* popup_blocked)	(EphyEmbed *embed,
					 const char *address,
					 const char *target,
					 const char *features);
	void	 (* zoom_change)	(EphyEmbed *embed,
					 float new_zoom);
	void	 (* content_blocked)	(EphyEmbed *embed,
					 const char *uri);
	gboolean (* modal_alert)	(EphyEmbed *embed);
	void	 (* modal_alert_closed)	(EphyEmbed *embed);
	void	 (* document_type)	(EphyEmbed *embed,
					 EphyEmbedDocumentType type);
	void	 (* new_window)		(EphyEmbed *embed,
					 EphyEmbed *new_embed);
	gboolean (* search_key_press)	(EphyEmbed *embed,
					 GdkEventKey *event);
	gboolean (* close_request)	(EphyEmbed *embed);

	/* Methods  */
	void		   (* load_url)			(EphyEmbed *embed,
							 const char *url);
	void		   (* load)			(EphyEmbed *embed,
							 const char *url,
							 EphyEmbedLoadFlags flags,
							 EphyEmbed *referring_embed);
	void		   (* stop_load)		(EphyEmbed *embed);
	void		   (* reload)			(EphyEmbed *embed,
							 gboolean force);
	gboolean	   (* can_go_back)		(EphyEmbed *embed);
	gboolean	   (* can_go_forward)		(EphyEmbed *embed);
	gboolean	   (* can_go_up)		(EphyEmbed *embed);
	GSList *	   (* get_go_up_list)		(EphyEmbed *embed);
	void		   (* go_back)			(EphyEmbed *embed);
	void		   (* go_forward)		(EphyEmbed *embed);
	void		   (* go_up)			(EphyEmbed *embed);

	const char *	   (* get_title)		(EphyEmbed *embed);
	char *		   (* get_location)		(EphyEmbed *embed,
							 gboolean toplevel);
	const char *	   (* get_link_message)		(EphyEmbed *embed);
	char *		   (* get_js_status)		(EphyEmbed *embed);
	int		   (* shistory_n_items)		(EphyEmbed *embed);
	void		   (* shistory_get_nth)		(EphyEmbed *embed,
							 int nth,
							 gboolean is_relative,
							 char **url,
							 char **title);
	int		   (* shistory_get_pos)		(EphyEmbed *embed);
	void		   (* shistory_go_nth)		(EphyEmbed *embed,
							 int nth);
	void		   (* shistory_copy)		(EphyEmbed *source,
							 EphyEmbed *dest,
							 gboolean copy_back,
							 gboolean copy_forward,
							 gboolean copy_current);
	void		   (* get_security_level)	(EphyEmbed *embed,
							 EphyEmbedSecurityLevel *level,
							 char **description);
	void		   (* show_page_certificate)	(EphyEmbed *embed);
	void		   (* set_zoom)			(EphyEmbed *embed,
							 float zoom);
	float		   (* get_zoom)			(EphyEmbed *embed);
	void		   (* scroll_lines)		(EphyEmbed *embed,
							 int num_lines);
	void		   (* scroll_pages)		(EphyEmbed *embed,
							 int num_pages);
	void		   (* scroll_pixels)		(EphyEmbed *embed,
							 int dx,
							 int dy);
	char *		   (* get_encoding)		(EphyEmbed *embed);
	gboolean	   (* has_automatic_encoding)	(EphyEmbed *embed);
	void		   (* set_encoding)		(EphyEmbed *embed,
							 const char *encoding);
	void		   (* print)			(EphyEmbed *embed);
	void		   (* set_print_preview_mode)	(EphyEmbed *embed,
							 gboolean mode);
	int		   (* print_preview_n_pages)	(EphyEmbed *embed);
	void		   (* print_preview_navigate)	(EphyEmbed *embed,
							 EphyEmbedPrintPreviewNavType type,
							 int page);
	gboolean	   (* has_modified_forms)	(EphyEmbed *embed);
	void		   (* close)			(EphyEmbed *embed);
	EphyEmbedDocumentType	(* get_document_type)	(EphyEmbed *embed);
	int		   (* get_load_percent)		(EphyEmbed *embed);
	gboolean	   (* get_load_status)		(EphyEmbed *embed);
	EphyEmbedNavigationFlags (* get_navigation_flags) (EphyEmbed *embed);
	const char *	   (* get_typed_address)	(EphyEmbed *embed);
	void		   (* set_typed_address)	(EphyEmbed *embed,
							 const char *address,
							 EphyEmbedAddressExpire expire);
	const char *	   (* get_address)		(EphyEmbed *embed);
	const char *	   (* get_status_message)	(EphyEmbed *embed);
	GdkPixbuf *	   (* get_icon)			(EphyEmbed *embed);
	const char *	   (* get_icon_address)		(EphyEmbed *embed);
	gboolean	   (* get_is_blank)		(EphyEmbed *embed);
	const char *	   (* get_loading_title)	(EphyEmbed *embed);
};

GType		  ephy_embed_net_state_get_type		(void);

GType		  ephy_embed_chrome_get_type		(void);

GType		  ephy_embed_ppv_navigation_get_type	(void);

GType		  ephy_embed_security_level_get_type	(void);

GType		  ephy_embed_document_type_get_type	(void);

GType		  ephy_embed_get_type			(void);

/* Base */
void		  ephy_embed_load_url			(EphyEmbed *embed,
							 const char *url);
void		  ephy_embed_load			(EphyEmbed *embed,
							 const char *url,
							 EphyEmbedLoadFlags flags,
							 EphyEmbed *referring_embed);

void		  ephy_embed_stop_load			(EphyEmbed *embed);

void		  ephy_embed_reload			(EphyEmbed *embed,
							 gboolean force);

const char	 *ephy_embed_get_title			(EphyEmbed *embed);

char		 *ephy_embed_get_location		(EphyEmbed *embed,
							 gboolean toplevel);
const char	 *ephy_embed_get_link_message		(EphyEmbed *embed);

char		 *ephy_embed_get_js_status		(EphyEmbed *embed);

/* Navigation */
gboolean	  ephy_embed_can_go_back		(EphyEmbed *embed);

gboolean	  ephy_embed_can_go_forward		(EphyEmbed *embed);

gboolean	  ephy_embed_can_go_up			(EphyEmbed *embed);

GSList		 *ephy_embed_get_go_up_list		(EphyEmbed *embed);

void		  ephy_embed_go_back			(EphyEmbed *embed);

void		  ephy_embed_go_forward			(EphyEmbed *embed);

void		  ephy_embed_go_up			(EphyEmbed *embed);

int		  ephy_embed_shistory_n_items		(EphyEmbed *embed);

void		  ephy_embed_shistory_get_nth		(EphyEmbed *embed,
							 int nth,
							 gboolean is_relative,
							 char **url,
							 char **title);

int		  ephy_embed_shistory_get_pos		(EphyEmbed *embed);

void		  ephy_embed_shistory_go_nth		(EphyEmbed *embed,
							 int nth);

void		  ephy_embed_shistory_copy		(EphyEmbed *source,
							 EphyEmbed *dest,
							 gboolean copy_back,
							 gboolean copy_forward,
							 gboolean copy_current);

void		  ephy_embed_get_security_level		(EphyEmbed *embed,
							 EphyEmbedSecurityLevel *level,
							 char **description);

void		  ephy_embed_show_page_certificate	(EphyEmbed *embed);

/* Zoom */
void		  ephy_embed_set_zoom			(EphyEmbed *embed,
							 float zoom);

float		  ephy_embed_get_zoom			(EphyEmbed *embed);

/* Scroll */
void		  ephy_embed_scroll			(EphyEmbed *embed,
							 int num_lines);

void		  ephy_embed_page_scroll		(EphyEmbed *embed,
							 int num_pages);
							 
void		  ephy_embed_scroll_pixels		(EphyEmbed *embed,
							 int dx,
							 int dy);
/* Document type */
EphyEmbedDocumentType	ephy_embed_get_document_type	(EphyEmbed *embed);

/* Progress */
int		 ephy_embed_get_load_percent		(EphyEmbed *embed);

/* Load status */
gboolean	 ephy_embed_get_load_status		(EphyEmbed *embed);

/* Navigation flags */

EphyEmbedNavigationFlags ephy_embed_get_navigation_flags (EphyEmbed *embed);

/* Typed address */
const char	 *ephy_embed_get_typed_address		(EphyEmbed *embed);
void		 ephy_embed_set_typed_address		(EphyEmbed *embed,
							 const char *address,
							 EphyEmbedAddressExpire expire);
/* Address */
const char *	 ephy_embed_get_address			(EphyEmbed *embed);

/* Status messages */
const char *	   ephy_embed_get_status_message	(EphyEmbed *embed);

/* Icon and Icon Address */

GdkPixbuf *	   ephy_embed_get_icon			(EphyEmbed *embed);
const char *	   ephy_embed_get_icon_address		(EphyEmbed *embed);

/* Is blank */
gboolean	  ephy_embed_get_is_blank		(EphyEmbed *embed);

const char *	 ephy_embed_get_loading_title		(EphyEmbed *embed);

/* Encoding */
char		 *ephy_embed_get_encoding		(EphyEmbed *embed);

gboolean	  ephy_embed_has_automatic_encoding	(EphyEmbed *embed);

void		  ephy_embed_set_encoding		(EphyEmbed *embed,
							 const char *encoding);

/* Print */
void		  ephy_embed_print			(EphyEmbed *embed);

void		  ephy_embed_set_print_preview_mode	(EphyEmbed *embed,
							 gboolean preview_mode);

int		  ephy_embed_print_preview_n_pages	(EphyEmbed *embed);

void		  ephy_embed_print_preview_navigate	(EphyEmbed *embed,
							 EphyEmbedPrintPreviewNavType type,
							 int page);

/* Misc. utility */
void		  ephy_embed_close			(EphyEmbed *embed);

gboolean	  ephy_embed_has_modified_forms		(EphyEmbed *embed);

G_END_DECLS

#endif
