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

#include "popup-commands.h"
#include "ephy-shell.h"

static EphyWindow *
get_window_from_popup (EphyEmbedPopup *popup)
{
	return EPHY_WINDOW (g_object_get_data(G_OBJECT(popup), "EphyWindow"));
}

void popup_cmd_new_window (BonoboUIComponent *uic,
			   EphyEmbedPopup *popup,
			   const char* verbname)
{
	EphyEmbedEvent *info;
	EphyTab *tab;
	GValue *value;

	tab = ephy_window_get_active_tab (get_window_from_popup (popup));

	info = ephy_embed_popup_get_event (popup);

	ephy_embed_event_get_property (info, "link", &value);

	ephy_shell_new_tab (ephy_shell, NULL, tab,
			      g_value_get_string (value),
			      EPHY_NEW_TAB_IN_NEW_WINDOW);
}

void popup_cmd_new_tab (BonoboUIComponent *uic,
			EphyEmbedPopup *popup,
			const char* verbname)
{
	EphyEmbedEvent *info;
	EphyTab *tab;
	EphyWindow *window;
	GValue *value;

	window = get_window_from_popup (popup);
	g_return_if_fail (window != NULL);

	tab = ephy_window_get_active_tab (window);

	info = ephy_embed_popup_get_event (popup);

	ephy_embed_event_get_property (info, "link", &value);

	ephy_shell_new_tab (ephy_shell, window, tab,
			    g_value_get_string (value),
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW);
}

void popup_cmd_image_in_new_tab (BonoboUIComponent *uic,
			         EphyEmbedPopup *popup,
			         const char* verbname)
{
	EphyEmbedEvent *info;
	EphyTab *tab;
	EphyWindow *window;
	GValue *value;

	window = get_window_from_popup (popup);
	g_return_if_fail (window != NULL);

	tab = ephy_window_get_active_tab (window);

	info = ephy_embed_popup_get_event (popup);

	ephy_embed_event_get_property (info, "image", &value);

	ephy_shell_new_tab (ephy_shell, window, tab,
			    g_value_get_string (value),
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW);
}

void popup_cmd_image_in_new_window (BonoboUIComponent *uic,
				    EphyEmbedPopup *popup,
				    const char* verbname)
{
	EphyEmbedEvent *info;
	EphyTab *tab;
	GValue *value;

	tab = ephy_window_get_active_tab (get_window_from_popup (popup));

	info = ephy_embed_popup_get_event (popup);

	ephy_embed_event_get_property (info, "image", &value);

	ephy_shell_new_tab (ephy_shell, NULL, tab,
			    g_value_get_string (value),
			    EPHY_NEW_TAB_IN_NEW_WINDOW);
}

void popup_cmd_add_bookmark (BonoboUIComponent *uic,
			     EphyEmbedPopup *popup,
			     const char* verbname)
{
	GtkWidget *new_bookmark;
	EphyBookmarks *bookmarks;
	EphyEmbedEvent *info = ephy_embed_popup_get_event (popup);
	EphyEmbed *embed;
	GtkWidget *window;
	GValue *link_title;
	GValue *link_rel;
	GValue *link;
	GValue *link_is_smart;
	const char *title;
	const char *location;
	const char *rel;
	gboolean is_smart;

	embed = ephy_embed_popup_get_embed (popup);
	window = gtk_widget_get_toplevel (GTK_WIDGET (embed));

	ephy_embed_event_get_property (info, "link_is_smart", &link_is_smart);
	ephy_embed_event_get_property (info, "link", &link);
	ephy_embed_event_get_property (info, "link_title", &link_title);
	ephy_embed_event_get_property (info, "link_rel", &link_rel);

	title = g_value_get_string (link_title);
	location = g_value_get_string (link);
	rel = g_value_get_string (link_rel);
	is_smart = g_value_get_int (link_is_smart);

	g_return_if_fail (location);

	if (!title || !title[0])
	{
		title = location;
	}

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	new_bookmark = ephy_new_bookmark_new
		(bookmarks, GTK_WINDOW (window), location);
	ephy_new_bookmark_set_title
		(EPHY_NEW_BOOKMARK (new_bookmark), title);
	ephy_new_bookmark_set_smarturl
		(EPHY_NEW_BOOKMARK (new_bookmark), rel);
	gtk_widget_show (new_bookmark);
}

void popup_cmd_frame_in_new_tab (BonoboUIComponent *uic,
			         EphyEmbedPopup *popup,
			         const char* verbname)
{
	EphyTab *tab;
	EphyWindow *window;
	EphyEmbed *embed;
	char *location;

	window = get_window_from_popup (popup);
	g_return_if_fail (window != NULL);

	tab = ephy_window_get_active_tab (window);

	embed = ephy_window_get_active_embed (window);

	ephy_embed_get_location (embed, FALSE, &location);

	ephy_shell_new_tab (ephy_shell, window, tab,
			    location,
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW);

	g_free (location);
}

void popup_cmd_frame_in_new_window (BonoboUIComponent *uic,
				    EphyEmbedPopup *popup,
				    const char* verbname)
{
	EphyTab *tab;
	EphyEmbed *embed;
	EphyWindow *window;
	char *location;

	window = get_window_from_popup (popup);
	g_return_if_fail (window != NULL);

	tab = ephy_window_get_active_tab (window);

	embed = ephy_window_get_active_embed (window);

	ephy_embed_get_location (embed, FALSE, &location);

	ephy_shell_new_tab (ephy_shell, NULL, tab,
			    location,
			    EPHY_NEW_TAB_IN_NEW_WINDOW);

	g_free (location);
}

void popup_cmd_add_frame_bookmark (BonoboUIComponent *uic,
				   EphyEmbedPopup *popup,
			           const char* verbname)
{
	/* FIXME implement */
}

void popup_cmd_view_source (BonoboUIComponent *uic,
			    EphyEmbedPopup *popup,
			    const char* verbname)
{
	/* FIXME implement */
}
