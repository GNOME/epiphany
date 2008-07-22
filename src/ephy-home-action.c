/*
*  Copyright © 2004 Christian Persch
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

#include "config.h"

#include "ephy-home-action.h"
#include "ephy-link.h"
#include "ephy-prefs.h"
#include "ephy-gui.h"
#include "ephy-dnd.h"
#include "eel-gconf-extensions.h"

#include <string.h>

#include <gtk/gtk.h>

#define INSANE_NUMBER_OF_URLS 20

typedef struct
{
	GObject *weak_ptr;
	EphyLinkFlags flags;
} ClipboardCtx;

G_DEFINE_TYPE (EphyHomeAction, ephy_home_action, EPHY_TYPE_LINK_ACTION)

static const GtkTargetEntry url_drag_types [] = 
{
	{ EPHY_DND_URI_LIST_TYPE,   0, 0 },
	{ EPHY_DND_URL_TYPE,        0, 1 }
};

static void
clipboard_text_received_cb (GtkClipboard *clipboard,
			    const char *text,
			    ClipboardCtx *ctx)
{
	if (ctx->weak_ptr != NULL && text != NULL)
	{
		ephy_link_open (EPHY_LINK (ctx->weak_ptr), text, NULL, ctx->flags);
	}

	if (ctx->weak_ptr != NULL)
	{
		GObject **object = &(ctx->weak_ptr);
		g_object_remove_weak_pointer (G_OBJECT (ctx->weak_ptr), 
					      (gpointer *)object);
	}

	g_free (ctx);
}

static void
ephy_home_action_with_clipboard (GtkAction *action,
				 EphyLinkFlags flags)
{
	ClipboardCtx *ctx;
	GObject **object;

	ctx = g_new (ClipboardCtx, 1);
	ctx->flags = flags;

	/* We need to make sure we know if the action is destroyed between
	 * requesting the clipboard contents, and receiving them.
	 */
	ctx->weak_ptr = G_OBJECT (action);
	object = &(ctx->weak_ptr);
	g_object_add_weak_pointer (ctx->weak_ptr, (gpointer *)object);

	gtk_clipboard_request_text
		(gtk_clipboard_get_for_display (gdk_display_get_default(), 
					        GDK_SELECTION_PRIMARY),
		 (GtkClipboardTextReceivedFunc) clipboard_text_received_cb,
		 ctx);

}

static void
ephy_home_action_open (GtkAction *action, 
		       const char *address, 
		       EphyLinkFlags flags)
{
	if (ephy_gui_is_middle_click ())
	{
		ephy_home_action_with_clipboard (action, flags);
	}
	else /* Left button */
	{
		ephy_link_open (EPHY_LINK (action),
				address != NULL && address[0] != '\0' ? address : "about:blank",
				NULL,
				flags);
	}
}

static void
action_name_association (GtkAction *action,
			 char *action_name,
			 char *address,
			 gboolean is_drag_action)
{
	if (strcmp (action_name, "FileNewTab") == 0)
	{
		if (is_drag_action)
		{
			ephy_link_open (EPHY_LINK (action),
					address, NULL,
					EPHY_LINK_NEW_TAB | EPHY_LINK_JUMP_TO);
		}
		else
		{
			ephy_home_action_open (action, 
					       address, 
					       EPHY_LINK_NEW_TAB | EPHY_LINK_JUMP_TO);
		}
	}
	else if (strcmp (action_name, "FileNewWindow") == 0)
	{
		if (is_drag_action)
		{
			ephy_link_open (EPHY_LINK (action),
					address, NULL,
					EPHY_LINK_NEW_WINDOW);
		}
		else
		{
			ephy_home_action_open (action,
					       address,
					       EPHY_LINK_NEW_WINDOW);
		}
	}
	else if (strcmp (action_name, "GoHome") == 0)
	{
		ephy_link_open (EPHY_LINK (action),
				address != NULL && address[0] != '\0' ? address : "about:blank",
				NULL,
				ephy_link_flags_from_current_event ());
	}
}	

static void
ephy_home_action_activate (GtkAction *action)
{
	char *action_name;
	char *address;

	g_object_get (G_OBJECT (action), "name", &action_name, NULL);
		
	address = eel_gconf_get_string (CONF_GENERAL_HOMEPAGE);

	action_name_association (action, action_name, address, FALSE);

	g_free (address);
}

static void
home_action_drag_data_received_cb (GtkWidget* widget,
				   GdkDragContext *context,
				   gint x,
				   gint y,
				   GtkSelectionData *selection_data,
				   guint info,
				   guint time,
				   EphyHomeAction *action)
{
	gchar *action_name;
			
	g_object_get (action, "name", &action_name, NULL);
	
	g_signal_stop_emission_by_name (widget, "drag_data_received");

	if (selection_data->length <= 0 || selection_data->data == NULL) return;

	if (selection_data->target == gdk_atom_intern (EPHY_DND_URL_TYPE, FALSE))
	{
		char **split;

		split = g_strsplit ((const gchar *)selection_data->data, "\n", 2);
		if (split != NULL && split[0] != NULL && split[0][0] != '\0')
		{			
			action_name_association (GTK_ACTION (action), 
						 action_name, split[0], TRUE);
		}
		g_strfreev (split);
	}
	else if (selection_data->target == gdk_atom_intern (EPHY_DND_URI_LIST_TYPE, FALSE))
	{
		char **uris;
		int i;
	
		uris = gtk_selection_data_get_uris (selection_data);
		if (uris == NULL) return;

		for (i = 0; uris[i] != NULL && i < INSANE_NUMBER_OF_URLS; i++)
		{
			action_name_association (GTK_ACTION (action),
						 action_name, uris[i], TRUE);
		}

		g_strfreev (uris);
	}
	else
	{
		char *text;
	       
		text = (char *) gtk_selection_data_get_text (selection_data);
		if (text != NULL) 
		{			
			action_name_association (GTK_ACTION (action),
						 action_name, text, TRUE);
		}
	}
}

static void
connect_proxy (GtkAction *action,
	       GtkWidget *proxy)
{      
	gchar *action_name;
	
	GTK_ACTION_CLASS (ephy_home_action_parent_class)->connect_proxy (action, proxy);
	
	 g_object_get (action, "name", &action_name, NULL);

	if (GTK_IS_TOOL_ITEM (proxy) && (strcmp (action_name, "GoHome") != 0))
	{
		g_signal_connect (GTK_BIN (proxy)->child, "drag-data-received",
				  G_CALLBACK (home_action_drag_data_received_cb), action);
		gtk_drag_dest_set (GTK_BIN (proxy)->child,
				   GTK_DEST_DEFAULT_ALL,
				   url_drag_types, G_N_ELEMENTS (url_drag_types),
				   GDK_ACTION_MOVE | GDK_ACTION_COPY);
		gtk_drag_dest_add_text_targets (GTK_BIN (proxy)->child);
	}
}

static void
disconnect_proxy (GtkAction *action,
		  GtkWidget *proxy)
{
	g_signal_handlers_disconnect_by_func
		(proxy, G_CALLBACK (gtk_action_activate), action);

	GTK_ACTION_CLASS (ephy_home_action_parent_class)->disconnect_proxy (action, proxy);
}

static void
ephy_home_action_class_init (EphyHomeActionClass *class)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);
	
	action_class->activate = ephy_home_action_activate;
	action_class->connect_proxy = connect_proxy;
	action_class->disconnect_proxy = disconnect_proxy;
}

static void
ephy_home_action_init (EphyHomeAction *action)
{
        /* Empty, needed for G_DEFINE_TYPE macro */
}
