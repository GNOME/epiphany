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

#ifndef EPHY_EMBED_H
#define EPHY_EMBED_H

#include "ephy-embed-types.h"
#include "ephy-embed-event.h"

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

typedef struct EphyEmbedClass EphyEmbedClass;

#define EPHY_EMBED_TYPE             (ephy_embed_get_type ())
#define EPHY_EMBED(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_EMBED_TYPE, EphyEmbed))
#define EPHY_EMBED_CLASS(vtable)    (G_TYPE_CHECK_CLASS_CAST ((vtable), EPHY_EMBED_TYPE, EphyEmbedClass))
#define IS_EPHY_EMBED(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_EMBED_TYPE))
#define IS_EPHY_EMBED_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), EPHY_EMBED_TYPE))
#define EPHY_EMBED_GET_CLASS(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_EMBED_TYPE, EphyEmbedClass))

typedef struct _EphyEmbed EphyEmbed;

typedef enum
{
	EMBED_STATE_UNKNOWN = 0,
	EMBED_STATE_START = 1 << 0,
	EMBED_STATE_REDIRECTING = 1 << 1,
	EMBED_STATE_TRANSFERRING = 1 << 2,
	EMBED_STATE_NEGOTIATING = 1 << 3,
	EMBED_STATE_STOP = 1 << 4,

	EMBED_STATE_IS_REQUEST = 1 << 5,
	EMBED_STATE_IS_DOCUMENT = 1 << 6,
	EMBED_STATE_IS_NETWORK = 1 << 7,
	EMBED_STATE_IS_WINDOW = 1 << 8
} EmbedState;

typedef enum
{
	EMBED_CLIPBOARD_CAP = 1 << 0,
	EMBED_COOKIES_CAP = 1 << 1,
	EMBED_LINKS_CAP = 1 << 2,
	EMBED_ZOOM_CAP = 1 << 3,
	EMBED_PRINT_CAP = 1 << 6,
	EMBED_FIND_CAP = 1 << 7,
	EMBED_SCROLL_CAP = 1 << 8,
	EMBED_SECURITY_CAP = 1 << 9,
	EMBED_ENCODING_CAP = 1 << 10,
	EMBED_SHISTORY_CAP = 1 << 11
} EmbedCapabilities;

typedef struct
{
	char *modification_date;

	/* lists of hashtables with gvalues */
	GList *images;      /* url, alt, title, width, height */
	GList *forms;       /* action, type */
	GList *links;       /* url, title, type */
	GList *stylesheets; /* url, title */
} EmbedPageInfo;

typedef enum
{
	EMBED_RELOAD_NORMAL = 1 << 1,
	EMBED_RELOAD_BYPASSCACHE = 1 << 2,
	EMBED_RELOAD_BYPASSPROXY = 1 << 3
} EmbedReloadFlags;

typedef enum
{
        DISPLAY_AS_SOURCE = 1U,
        DISPLAY_NORMAL = 2U
} EmbedDisplayType;

typedef struct
{
        gchar *search_string;
        gboolean backwards;
        gboolean wrap;
        gboolean entire_word;
        gboolean match_case;
        gboolean search_frames;
	gboolean interactive;
} EmbedFindInfo;

typedef struct
{
        gboolean print_to_file;
        gchar *printer;
        gchar *file;
        gchar *paper;
        gint top_margin;
        gint bottom_margin;
        gint left_margin;
        gint right_margin;
        gint pages;
        gint from_page;
        gint to_page;
        gint frame_type;
        gint orientation;
        gboolean print_color;

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
        gchar *header_left_string;
        gchar *header_center_string;
        gchar *header_right_string;
        gchar *footer_left_string;
        gchar *footer_center_string;
        gchar *footer_right_string;

	gboolean preview;
}
EmbedPrintInfo;

typedef enum
{
	PRINTPREVIEW_GOTO_PAGENUM = 0,
	PRINTPREVIEW_PREV_PAGE = 1,
	PRINTPREVIEW_NEXT_PAGE = 2,
	PRINTPREVIEW_HOME = 3,
	PRINTPREVIEW_END = 4
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

struct EphyEmbedClass
{
        GTypeInterface base_iface;

	gint (* context_menu)	 (EphyEmbed *embed,
				  EphyEmbedEvent *event);
	void (* favicon)	 (EphyEmbed *embed,
				  const char *location);
	void (* link_message)    (EphyEmbed *embed,
				  const char *link);
	void (* js_status)       (EphyEmbed *embed,
				  const char *status);
	void (* location)        (EphyEmbed *embed);
	void (* title)           (EphyEmbed *embed);
	void (* progress)        (EphyEmbed *embed,
				  const char *uri,
			          gint curprogress,
				  gint maxprogress);
	void (* net_state)       (EphyEmbed *embed,
				  const char *uri,
			          EmbedState state);
	void (* new_window)      (EphyEmbed *embed,
			          EphyEmbed **new_embed,
                                  EmbedChromeMask chromemask);
	void (* visibility)      (EphyEmbed *embed,
			          gboolean visibility);
	void (* destroy_brsr)    (EphyEmbed *embed);
	gint (* open_uri)        (EphyEmbed *embed,
			          const char *uri);
	void (* size_to)         (EphyEmbed *embed,
			          gint width,
			          gint height);
	gint (* dom_mouse_click) (EphyEmbed *embed,
			          EphyEmbedEvent *event);
	void (* security_change) (EphyEmbed *embed,
                                  EmbedSecurityLevel level);
	void (* zoom_change)	 (EphyEmbed *embed,
                                  guint new_zoom);

	/* Methods  */
        void (* get_capabilities)          (EphyEmbed *embed,
				            EmbedCapabilities *caps);
	gresult   (* load_url)             (EphyEmbed *embed,
					    const char *url);
	gresult   (* stop_load)            (EphyEmbed *embed);
	gresult   (* can_go_back)          (EphyEmbed *embed);
	gresult   (* can_go_forward)       (EphyEmbed *embed);
	gresult   (* can_go_up)            (EphyEmbed *embed);
	gresult   (* get_go_up_list)       (EphyEmbed *embed, GSList **l);
	gresult   (* go_back)              (EphyEmbed *embed);
	gresult   (* go_forward)           (EphyEmbed *embed);
	gresult   (* go_up)                (EphyEmbed *embed);
	gresult   (* render_data)          (EphyEmbed *embed,
					    const char *data,
					    guint32 len,
					    const char *base_uri,
					    const char *mime_type);
	gresult   (* open_stream)          (EphyEmbed *embed,
					    const char *base_uri,
					    const char *mime_type);
	gresult   (* append_data)          (EphyEmbed *embed,
					    const char *data,
					    guint32 len);
	gresult   (* close_stream)         (EphyEmbed *embed);
	gresult   (* get_title)            (EphyEmbed *embed,
					    char **title);
	gresult   (* get_location)         (EphyEmbed *embed,
				            gboolean toplevel,
				            char **location);
	gresult   (* reload)               (EphyEmbed *embed,
					    EmbedReloadFlags flags);
	gresult   (* copy_page)		   (EphyEmbed *dest,
					    EphyEmbed *source,
					    EmbedDisplayType display_type);
	gresult   (* zoom_set)             (EphyEmbed *embed,
					    float zoom,
					    gboolean reflow);
	gresult   (* zoom_get)             (EphyEmbed *embed,
				            float *zoom);
	gresult   (* selection_can_cut)    (EphyEmbed *embed);
	gresult   (* selection_can_copy)   (EphyEmbed *embed);
	gresult   (* can_paste)            (EphyEmbed *embed);
	gresult   (* selection_cut)        (EphyEmbed *embed);
	gresult   (* selection_copy)       (EphyEmbed *embed);
	gresult   (* paste)                (EphyEmbed *embed);
	gresult   (* select_all)           (EphyEmbed *embed);
	gresult   (* shistory_count)	   (EphyEmbed *embed,
					    int *count);
	gresult   (* shistory_get_nth)     (EphyEmbed *embed,
				            int nth,
				            gboolean is_relative,
				            char **url,
				            char **title);
	gresult   (* shistory_get_pos)     (EphyEmbed *embed,
				            int *pos);
	gresult   (* shistory_go_nth)      (EphyEmbed *embed,
					    int nth);
	gboolean  (* shistory_copy)        (EphyEmbed *source,
				            EphyEmbed *dest);
	gresult   (* get_security_level)   (EphyEmbed *embed,
					    EmbedSecurityLevel *level,
					    char **description);
	gresult   (* find)                 (EphyEmbed *embed,
				            EmbedFindInfo *find);
	gresult   (* print)                (EphyEmbed *embed,
				            EmbedPrintInfo *info);
	gresult	  (* print_preview_close)  (EphyEmbed *embed);
	gresult   (* print_preview_num_pages)	(EphyEmbed *embed,
						 gint *retNum);
	gresult   (* print_preview_navigate)	(EphyEmbed *embed,
						 EmbedPrintPreviewNavType navType,
						 gint pageNum);
	gresult   (* set_encoding)         (EphyEmbed *embed,
					    const char *encoding);
};

GType         ephy_embed_get_type             (void);

/* Base */

EphyEmbed    *ephy_embed_new                  (GObject *single);

void          ephy_embed_get_capabilities     (EphyEmbed *embed,
					       EmbedCapabilities *caps);

gresult       ephy_embed_load_url             (EphyEmbed *embed,
					       const char *url);

gresult       ephy_embed_stop_load            (EphyEmbed *embed);

gresult       ephy_embed_can_go_back          (EphyEmbed *embed);

gresult       ephy_embed_can_go_forward       (EphyEmbed *embed);

gresult       ephy_embed_can_go_up            (EphyEmbed *embed);

gresult       ephy_embed_get_go_up_list       (EphyEmbed *embed,
					       GSList **l);

gresult       ephy_embed_go_back              (EphyEmbed *embed);

gresult       ephy_embed_go_forward           (EphyEmbed *embed);

gresult       ephy_embed_go_up                (EphyEmbed *embed);

gresult       ephy_embed_render_data          (EphyEmbed *embed,
					       const char *data,
					       guint32 len,
					       const char *base_uri,
					       const char *mime_type);

gresult       ephy_embed_open_stream          (EphyEmbed *embed,
					       const char *base_uri,
					       const char *mime_type);

gresult       ephy_embed_append_data          (EphyEmbed *embed,
					       const char *data,
					       guint32 len);

gresult       ephy_embed_close_stream         (EphyEmbed *embed);

gresult       ephy_embed_get_title            (EphyEmbed *embed,
					       char **title);

gresult       ephy_embed_get_location         (EphyEmbed *embed,
					       gboolean toplevel,
					       char **location);

gresult       ephy_embed_reload               (EphyEmbed *embed,
					       EmbedReloadFlags flags);

gresult	      ephy_embed_copy_page	      (EphyEmbed *dest,
					       EphyEmbed *source,
					       EmbedDisplayType display_type);

/* Zoom */
gresult       ephy_embed_zoom_set             (EphyEmbed *embed,
					       float zoom,
					       gboolean reflow);

gresult       ephy_embed_zoom_get             (EphyEmbed *embed,
					       float *zoom);

/* Clipboard */
gresult       ephy_embed_selection_can_cut    (EphyEmbed *embed);

gresult       ephy_embed_selection_can_copy   (EphyEmbed *embed);

gresult       ephy_embed_can_paste            (EphyEmbed *embed);

gresult       ephy_embed_selection_cut        (EphyEmbed *embed);

gresult       ephy_embed_selection_copy       (EphyEmbed *embed);

gresult       ephy_embed_paste                (EphyEmbed *embed);

gresult       ephy_embed_select_all           (EphyEmbed *embed);

/* Session history */
gresult       ephy_embed_shistory_count       (EphyEmbed *embed,
					       int *count);

gresult       ephy_embed_shistory_get_nth     (EphyEmbed *embed,
					       int nth,
					       gboolean is_relative,
					       char **url,
					       char **title);

gresult       ephy_embed_shistory_get_pos     (EphyEmbed *embed,
					       int *pos);

gresult       ephy_embed_shistory_go_nth      (EphyEmbed *embed,
					       int nth);

gboolean      ephy_embed_shistory_copy        (EphyEmbed *source,
				               EphyEmbed *dest);

/* Utils */

gresult       ephy_embed_get_security_level   (EphyEmbed *embed,
					       EmbedSecurityLevel *level,
					       char **description);

gresult       ephy_embed_find                 (EphyEmbed *embed,
					       EmbedFindInfo *find);

gresult       ephy_embed_set_encoding         (EphyEmbed *embed,
					       const char *encoding);

/* Printing */

gresult       ephy_embed_print                (EphyEmbed *embed,
					       EmbedPrintInfo *info);

gresult	      ephy_embed_print_preview_close  (EphyEmbed *embed);

gresult       ephy_embed_print_preview_num_pages (EphyEmbed *embed,
						  gint *retNum);

gresult       ephy_embed_print_preview_navigate	 (EphyEmbed *embed,
						  EmbedPrintPreviewNavType navType,
						  gint pageNum);

G_END_DECLS

#endif
