/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
*/

#include "config.h"
#include "ephy-home-action.h"

#include "ephy-gui.h"
#include "ephy-link.h"

#include <gtk/gtk.h>

typedef struct
{
	GObject *weak_ptr;
	EphyLinkFlags flags;
} ClipboardCtx;

G_DEFINE_TYPE (EphyHomeAction, ephy_home_action, EPHY_TYPE_LINK_ACTION)

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
			 char *address)
{
	if (g_str_equal (action_name, "FileNewTab"))
	{
		ephy_home_action_open (action, 
				       address, 
				       EPHY_LINK_NEW_TAB | EPHY_LINK_JUMP_TO);
	}
}	

static void
ephy_home_action_activate (GtkAction *action)
{
	char *action_name;

	g_object_get (G_OBJECT (action), "name", &action_name, NULL);
		
	action_name_association (action, action_name, "about:blank");

	g_free (action_name);
}

static void
ephy_home_action_class_init (EphyHomeActionClass *class)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);
	
	action_class->activate = ephy_home_action_activate;
}

static void
ephy_home_action_init (EphyHomeAction *action)
{
        /* Empty, needed for G_DEFINE_TYPE macro */
}
