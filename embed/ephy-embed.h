/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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

#ifndef EPHY_EMBED_H
#define EPHY_EMBED_H

#include "ephy-embed-event.h"
#include "ephy-encodings.h"

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

#define EPHY_TYPE_EMBED_CHROME_MASK     (ephy_embed_chrome_get_type ())

typedef struct _EphyEmbed	EphyEmbed; /* dummy typedef */
typedef struct _EphyEmbedIface	EphyEmbedIface;

typedef enum
{
	EMBED_STATE_UNKNOWN		= 0,
	EMBED_STATE_START		= 1 << 0,
	EMBED_STATE_REDIRECTING		= 1 << 1,
	EMBED_STATE_TRANSFERRING	= 1 << 2,
	EMBED_STATE_NEGOTIATING		= 1 << 3,
	EMBED_STATE_STOP		= 1 << 4,

	EMBED_STATE_IS_REQUEST		= 1 << 5,
	EMBED_STATE_IS_DOCUMENT		= 1 << 6,
	EMBED_STATE_IS_NETWORK		= 1 << 7,
	EMBED_STATE_IS_WINDOW		= 1 << 8
} EmbedState;

typedef enum
{
	EPHY_EMBED_CHROME_MENUBAR	= 1 << 0,
	EPHY_EMBED_CHROME_TOOLBAR	= 1 << 1,
	EPHY_EMBED_CHROME_STATUSBAR	= 1 << 2,
	EPHY_EMBED_CHROME_BOOKMARKSBAR	= 1 << 3
} EphyEmbedChrome;

#define EPHY_EMBED_CHROME_ALL (EPHY_EMBED_CHROME_MENUBAR |	\
			       EPHY_EMBED_CHROME_TOOLBAR |	\
			       EPHY_EMBED_CHROME_STATUSBAR |	\
			       EPHY_EMBED_CHROME_BOOKMARKSBAR)

typedef enum
{
	EMBED_RELOAD_NORMAL	= 1 << 0,
	EMBED_RELOAD_FORCE	= 1 << 1
} EmbedReloadFlags;

typedef struct
{
	gboolean print_to_file;
	char *printer;
	char *file;
	char *paper;
	int top_margin;
	int bottom_margin;
	int left_margin;
	int right_margin;
	int pages;
	int from_page;
	int to_page;
	int frame_type;
	int orientation;
	gboolean print_color;
	gboolean preview;

	/*
	 * &T - title
	 * &U - Document URL
	 * &D - Date/Time
	 * &P - Page Number
	 * &PT - Page Number with total Number of Pages (example: 1 of 34)
	 *
	 * So, if headerLeftStr = "&T" the title and the document URL
	 * will be printed out on the top left-hand side of each page.
	 */
	char *header_left_string;
	char *header_center_string;
	char *header_right_string;
	char *footer_left_string;
	char *footer_center_string;
	char *footer_right_string;
}
EmbedPrintInfo;

typedef enum
{
	PRINTPREVIEW_GOTO_PAGENUM	= 0,
	PRINTPREVIEW_PREV_PAGE		= 1,
	PRINTPREVIEW_NEXT_PAGE		= 2,
	PRINTPREVIEW_HOME		= 3,
	PRINTPREVIEW_END		= 4
} EmbedPrintPreviewNavType;

typedef enum
{
	STATE_IS_UNKNOWN,
	STATE_IS_INSECURE,
	STATE_IS_BROKEN,
	STATE_IS_SECURE_MED,
	STATE_IS_SECURE_LOW,
	STATE_IS_SECURE_HIGH
} EmbedSecurityLevel;

struct _EphyEmbedIface
{
	GTypeInterface base_iface;

	/* Signals that we inherit from gtkmozembed
	 *
	 * void (* net_stop)     (GtkMozEmbed *embed);
	 * void (* title)        (EphyEmbed *embed);
	 * void (* visibility)   (EphyEmbed *embed,
	 *			  gboolean visibility);
	 * void (* destroy_brsr) (EphyEmbed *embed);
	 * void (* size_to)      (EphyEmbed *embed,
	 *			  int width,
	 *			  int height);
	 * gint (* open_uri)	 (EphyEmbed *embed,
	 *			  const char *url);
	 */	

	int	 (* context_menu)	(EphyEmbed *embed,
					 EphyEmbedEvent *event);
	void	 (* favicon)		(EphyEmbed *embed,
					 const char *location);
	void	 (* location)		(EphyEmbed *embed,
					 const char *location);
	void	 (* net_state)		(EphyEmbed *embed,
					 const char *uri,
					 EmbedState state);
	void	 (* new_window)		(EphyEmbed *embed,
					 EphyEmbed **new_embed,
					 EphyEmbedChrome chromemask);
	gboolean (* dom_mouse_click)	(EphyEmbed *embed,
					 EphyEmbedEvent *event);
	gboolean (* dom_mouse_down)	(EphyEmbed *embed,
					 EphyEmbedEvent *event);
	void	 (* popup_blocked)	(EphyEmbed *embed);
	void	 (* security_change)	(EphyEmbed *embed,
					 EmbedSecurityLevel level);
	void	 (* zoom_change)	(EphyEmbed *embed,
					 float new_zoom);
	void	 (* content_change)	(EphyEmbed *embed,
					 const char *uri);

	/* Methods  */
	void		   (* load_url)			(EphyEmbed *embed,
							 const char *url);
	void		   (* stop_load)		(EphyEmbed *embed);
	void		   (* reload)			(EphyEmbed *embed,
							 EmbedReloadFlags flags);
	gboolean	   (* can_go_back)		(EphyEmbed *embed);
	gboolean	   (* can_go_forward)		(EphyEmbed *embed);
	gboolean	   (* can_go_up)		(EphyEmbed *embed);
	GSList *	   (* get_go_up_list)		(EphyEmbed *embed);
	void		   (* go_back)			(EphyEmbed *embed);
	void		   (* go_forward)		(EphyEmbed *embed);
	void		   (* go_up)			(EphyEmbed *embed);

	char *		   (* get_title)		(EphyEmbed *embed);
	char *		   (* get_location)		(EphyEmbed *embed,
							 gboolean toplevel);
	char *		   (* get_link_message)		(EphyEmbed *embed);
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
	void		   (* get_security_level)	(EphyEmbed *embed,
						  	 EmbedSecurityLevel *level,
						  	 char **description);
	void		   (* set_zoom)			(EphyEmbed *embed,
							 float zoom);
	float		   (* get_zoom)			(EphyEmbed *embed);
	void		   (* find_set_properties)	(EphyEmbed *embed,
							 const char *search_string,
							 gboolean case_sensitive,
							 gboolean wrap_around);
	gboolean	   (* find_next)		(EphyEmbed *embed,
							 gboolean backwards);
	char *		   (* get_encoding)		(EphyEmbed *embed);
	gboolean	   (* has_automatic_encoding)	(EphyEmbed *embed);
	void		   (* set_encoding)		(EphyEmbed *embed,
							 const char *encoding);
	void		   (* print)			(EphyEmbed *embed,
							 EmbedPrintInfo *info);
	void		   (* print_preview_close)	(EphyEmbed *embed);
	int		   (* print_preview_n_pages)	(EphyEmbed *embed);
	void		   (* print_preview_navigate)	(EphyEmbed *embed,
							 EmbedPrintPreviewNavType type,
							 int page);
	void		   (* activate)			(EphyEmbed *embed);
	gboolean	   (* has_modified_forms)	(EphyEmbed *embed);
};

GType		  ephy_embed_chrome_get_type		(void);

GType		  ephy_embed_get_type			(void);

/* Base */
void		  ephy_embed_load_url			(EphyEmbed *embed,
							 const char *url);

void		  ephy_embed_stop_load			(EphyEmbed *embed);

void		  ephy_embed_reload			(EphyEmbed *embed,
							 EmbedReloadFlags flags);

char 		 *ephy_embed_get_title			(EphyEmbed *embed);

char		 *ephy_embed_get_location		(EphyEmbed *embed,
							 gboolean toplevel);

char		 *ephy_embed_get_link_message		(EphyEmbed *embed);

char		 *ephy_embed_get_js_status		(EphyEmbed *embed);

/* Navigation */
gboolean	  ephy_embed_can_go_back		(EphyEmbed *embed);

gboolean	  ephy_embed_can_go_forward		(EphyEmbed *embed);

gboolean	  ephy_embed_can_go_up			(EphyEmbed *embed);

GSList 		 *ephy_embed_get_go_up_list		(EphyEmbed *embed);

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

void		  ephy_embed_get_security_level		(EphyEmbed *embed,
							 EmbedSecurityLevel *level,
						 	 char **description);

/* Zoom */
void		  ephy_embed_set_zoom			(EphyEmbed *embed,
							 float zoom);

float		  ephy_embed_get_zoom			(EphyEmbed *embed);

/* Find */
void		  ephy_embed_find_set_properties	(EphyEmbed *embed,
							 const char *search_string,
							 gboolean case_sensitive,
							 gboolean wrap_around);

gboolean	  ephy_embed_find_next			(EphyEmbed *embed,
							 gboolean backwards);

/* Encoding */
char		 *ephy_embed_get_encoding		(EphyEmbed *embed);

gboolean	  ephy_embed_has_automatic_encoding	(EphyEmbed *embed);

void		  ephy_embed_set_encoding		(EphyEmbed *embed,
							 const char *encoding);

/* Print */
void		  ephy_embed_print			(EphyEmbed *embed,
							 EmbedPrintInfo *info);

void		  ephy_embed_print_preview_close	(EphyEmbed *embed);

int		  ephy_embed_print_preview_n_pages	(EphyEmbed *embed);

void		  ephy_embed_print_preview_navigate	(EphyEmbed *embed,
							 EmbedPrintPreviewNavType type,
							 int page);

/* Misc. utility */
void		  ephy_embed_activate			(EphyEmbed *embed);

gboolean	  ephy_embed_has_modified_forms		(EphyEmbed *embed);

G_END_DECLS

#endif
