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
#include "ephy-new-bookmark.h"
#include "ephy-embed-persist.h"
#include "ephy-prefs.h"
#include "ephy-embed-utils.h"
#include "eel-gconf-extensions.h"
#include "ephy-file-helpers.h"

#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkclipboard.h>

static EphyEmbedEvent *
get_event_info (EphyWindow *window)
{
	EphyEmbedEvent *info;

	info = EPHY_EMBED_EVENT (g_object_get_data
		(G_OBJECT (window), "context_event"));
	g_return_val_if_fail (info != NULL, NULL);

	return info;
}

void
popup_cmd_link_in_new_window (GtkAction *action,
		              EphyWindow *window)
{
	EphyEmbedEvent *info;
	EphyTab *tab;
	const GValue *value;

	tab = ephy_window_get_active_tab (window);

	info = get_event_info (window);

	ephy_embed_event_get_property (info, "link", &value);

	ephy_shell_new_tab (ephy_shell, NULL, tab,
			    g_value_get_string (value),
			    EPHY_NEW_TAB_OPEN_PAGE |
			    EPHY_NEW_TAB_IN_NEW_WINDOW);
}

void
popup_cmd_link_in_new_tab (GtkAction *action,
		           EphyWindow *window)
{
	EphyEmbedEvent *info;
	EphyTab *tab;
	const GValue *value;

	tab = ephy_window_get_active_tab (window);

	info = get_event_info (window);

	ephy_embed_event_get_property (info, "link", &value);

	ephy_shell_new_tab (ephy_shell, window, tab,
			    g_value_get_string (value),
			    EPHY_NEW_TAB_OPEN_PAGE |
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW);
}

void
popup_cmd_image_in_new_tab (GtkAction *action,
			    EphyWindow *window)
{
	EphyEmbedEvent *info;
	EphyTab *tab;
	const GValue *value;

	tab = ephy_window_get_active_tab (window);

	info = get_event_info (window);

	ephy_embed_event_get_property (info, "image", &value);

	ephy_shell_new_tab (ephy_shell, window, tab,
			    g_value_get_string (value),
			    EPHY_NEW_TAB_OPEN_PAGE |
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW);
}

void
popup_cmd_image_in_new_window (GtkAction *action,
			       EphyWindow *window)
{
	EphyEmbedEvent *info;
	EphyTab *tab;
	const GValue *value;

	tab = ephy_window_get_active_tab (window);

	info = get_event_info (window);

	ephy_embed_event_get_property (info, "image", &value);

	ephy_shell_new_tab (ephy_shell, NULL, tab,
			    g_value_get_string (value),
			    EPHY_NEW_TAB_OPEN_PAGE |
			    EPHY_NEW_TAB_IN_NEW_WINDOW);
}

void
popup_cmd_bookmark_link (GtkAction *action,
			 EphyWindow *window)
{
	GtkWidget *new_bookmark;
	EphyBookmarks *bookmarks;
	EphyEmbedEvent *info;
	EphyEmbed *embed;
	const GValue *link_title;
	const GValue *link_rel;
	const GValue *link;
	const GValue *link_is_smart;
	const GValue *linktext;
	const char *title;
	const char *location;
	const char *rel;
	gboolean is_smart;

	info = get_event_info (window);
	embed = ephy_window_get_active_embed (window);

	ephy_embed_event_get_property (info, "link_is_smart", &link_is_smart);
	ephy_embed_event_get_property (info, "link", &link);
	ephy_embed_event_get_property (info, "link_title", &link_title);
	ephy_embed_event_get_property (info, "link_rel", &link_rel);
	ephy_embed_event_get_property (info, "linktext", &linktext);

	location = g_value_get_string (link);
	g_return_if_fail (location);

	rel = g_value_get_string (link_rel);
	is_smart = g_value_get_int (link_is_smart);

	title = g_value_get_string (link_title);

	if (title == NULL || title[0] == '\0')
	{
		title = g_value_get_string (linktext);
	}

	if (title == NULL || title[0] == '\0')
	{
		title = location;
	}

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	if (ephy_new_bookmark_is_unique (bookmarks, GTK_WINDOW (window),
					 location))
	{
		new_bookmark = ephy_new_bookmark_new
			(bookmarks, GTK_WINDOW (window),
			 is_smart ? rel : location);
		ephy_new_bookmark_set_title
			(EPHY_NEW_BOOKMARK (new_bookmark), title);
		gtk_widget_show (new_bookmark);
	}
}

void
popup_cmd_frame_in_new_tab (GtkAction *action,
			    EphyWindow *window)
{
	EphyTab *tab;
	EphyEmbed *embed;
	char *location;

	tab = ephy_window_get_active_tab (window);

	embed = ephy_window_get_active_embed (window);

	ephy_embed_get_location (embed, FALSE, &location);

	ephy_shell_new_tab (ephy_shell, window, tab,
			    location,
			    EPHY_NEW_TAB_OPEN_PAGE |
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW);

	g_free (location);
}

void
popup_cmd_frame_in_new_window (GtkAction *action,
			       EphyWindow *window)
{
	EphyTab *tab;
	EphyEmbed *embed;
	char *location;

	tab = ephy_window_get_active_tab (window);

	embed = ephy_window_get_active_embed (window);

	ephy_embed_get_location (embed, FALSE, &location);

	ephy_shell_new_tab (ephy_shell, NULL, tab,
			    location,
			    EPHY_NEW_TAB_OPEN_PAGE |
			    EPHY_NEW_TAB_IN_NEW_WINDOW);

	g_free (location);
}

static void
popup_cmd_copy_to_clipboard (EphyWindow *window, const char *text)
{
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_NONE),
				text, -1);
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY),
				text, -1);
}

void
popup_cmd_copy_link_address (GtkAction *action,
			     EphyWindow *window)
{
	EphyEmbedEvent *event;
	const char *address;
	const GValue *value;

	event = get_event_info (window);
	g_return_if_fail (EPHY_IS_EMBED_EVENT (event));

	if (event->context & EMBED_CONTEXT_EMAIL_LINK)
	{
		ephy_embed_event_get_property (event, "email", &value);
		address = g_value_get_string (value);
		popup_cmd_copy_to_clipboard (window, address);
	}
	else if (event->context & EMBED_CONTEXT_LINK)
	{
		ephy_embed_event_get_property (event, "link", &value);
		address = g_value_get_string (value);
		popup_cmd_copy_to_clipboard (window, address);
	}
}

static void
save_property_url (GtkAction *action,
		   const char *title,
		   EphyWindow *window,
		   gboolean ask_dest,
		   const char *property)
{
	EphyEmbedEvent *info;
	const char *location;
	const GValue *value;
	GtkWidget *widget;
	EphyEmbedPersist *persist;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	info = get_event_info (window);
	ephy_embed_event_get_property (info, property, &value);
	location = g_value_get_string (value);

	widget = GTK_WIDGET (embed);

	persist = ephy_embed_persist_new (embed);

	ephy_embed_persist_set_source (persist, location);

	ephy_embed_utils_save (GTK_WIDGET (window), title,
			       CONF_STATE_DOWNLOADING_DIR,
			       ask_dest, persist);

	g_object_unref (G_OBJECT(persist));
}

void
popup_cmd_open_link (GtkAction *action,
		     EphyWindow *window)
{
	EphyEmbedEvent *info;
	const char *location;
	const GValue *value;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	info = get_event_info (window);
	ephy_embed_event_get_property (info, "link", &value);
	location = g_value_get_string (value);

	ephy_embed_load_url (embed, location);
}

void
popup_cmd_download_link (GtkAction *action,
			 EphyWindow *window)
{
	save_property_url (action, _("Download link"), window,
		           eel_gconf_get_boolean
		           (CONF_ASK_DOWNLOAD_DEST),
		           "link");
}

void
popup_cmd_save_image_as (GtkAction *action,
			 EphyWindow *window)
{
	save_property_url (action, _("Save Image As"),
			   window, TRUE, "image");
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

void
popup_cmd_set_image_as_background (GtkAction *action,
				   EphyWindow *window)
{
	EphyEmbedEvent *info;
	const char *location;
	char *dest, *base;
	const GValue *value;
	EphyEmbedPersist *persist;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	info = get_event_info (window);
	ephy_embed_event_get_property (info, "image", &value);
	location = g_value_get_string (value);

	persist = ephy_embed_persist_new (embed);

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

void
popup_cmd_copy_image_location (GtkAction *action,
			       EphyWindow *window)
{
	EphyEmbedEvent *info;
	const char *location;
	const GValue *value;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	info = get_event_info (window);
	ephy_embed_event_get_property (info, "image", &value);
	location = g_value_get_string (value);
	popup_cmd_copy_to_clipboard (window, location);
}

void
popup_cmd_save_background_as (GtkAction *action,
			      EphyWindow *window)
{
	save_property_url (action, _("Save Background As"),
			   window, TRUE, "background_image");
}

void
popup_cmd_open_frame (GtkAction *action,
		      EphyWindow *window)
{
	char *location;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_get_location (embed, FALSE, &location);

	ephy_embed_load_url (embed, location);

	g_free (location);
}

void
popup_cmd_open_image (GtkAction *action,
		      EphyWindow *window)
{
	EphyEmbedEvent *info;
	const char *location;
	const GValue *value;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	info = get_event_info (window);
	ephy_embed_event_get_property (info, "image", &value);
	location = g_value_get_string (value);

	ephy_embed_load_url (embed, location);
}
