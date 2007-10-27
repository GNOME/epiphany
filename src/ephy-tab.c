/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *  Copyright © 2004 Crispin Flowerday
 *  Copyright © 2004 Adam Hooper
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

#include "ephy-tab.h"
#include "ephy-type-builtins.h"
#include "ephy-embed-type-builtins.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-embed-factory.h"
#include "ephy-embed-prefs.h"
#include "ephy-debug.h"
#include "ephy-string.h"
#include "ephy-notebook.h"
#include "ephy-file-helpers.h"
#include "ephy-zoom.h"
#include "ephy-favicon-cache.h"
#include "ephy-embed-persist.h"
#include "ephy-history.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-shell.h"
#include "ephy-permission-manager.h"
#include "ephy-link.h"

#include <glib/gi18n.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkmisc.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtkstyle.h>
#include <gtk/gtkselection.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkclipboard.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-result.h>
#include <libgnomevfs/gnome-vfs-monitor.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <string.h>

#ifdef ENABLE_PYTHON
#include "ephy-python.h"
#endif

#define EPHY_TAB_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TAB, EphyTabPrivate))

struct _EphyTabPrivate
{
	guint id;

	int width;
	int height;
	guint idle_resize_handler;
};

static void ephy_tab_class_init		(EphyTabClass *klass);
static void ephy_tab_init		(EphyTab *tab);
static void ephy_tab_dispose		(GObject *object);
static void ephy_tab_finalize		(GObject *object);

enum
{
	PROP_0,
};

static GObjectClass *parent_class;

/* We need to assign unique IDs to tabs, otherwise accels get confused in the
 * tabs menu (bug #339548). We could use a serial #, but the ID is used in the
 * action name which is stored in a GQuark and so we should use them sparingly.
 */

static GArray *tabs_id_array = NULL;
static guint n_tabs = 0;

/* Class functions */

GType
ephy_tab_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0))
        {
                const GTypeInfo our_info =
                {
                        sizeof (EphyTabClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_tab_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (EphyTab),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) ephy_tab_init
                };
		const GInterfaceInfo link_info = 
		{
			NULL,
			NULL,
			NULL
		};


                type = g_type_register_static (GTK_TYPE_BIN,
					       "EphyTab",
					       &our_info, 0);

		g_type_add_interface_static (type,
					     EPHY_TYPE_LINK,
					     &link_info);
        }

        return type;
}

static void
ephy_tab_size_request (GtkWidget *widget,
		       GtkRequisition *requisition)
{
	GtkWidget *child;

	GTK_WIDGET_CLASS (parent_class)->size_request (widget, requisition);

	child = GTK_BIN (widget)->child;
	
	if (child && GTK_WIDGET_VISIBLE (child))
	{
		GtkRequisition child_requisition;
		gtk_widget_size_request (GTK_WIDGET (child), &child_requisition);
	}
}

static void
ephy_tab_size_allocate (GtkWidget *widget,
			GtkAllocation *allocation)
{
	GtkWidget *child;
	GtkAllocation invalid = { -1, -1, 1, 1 };

	widget->allocation = *allocation;

	child = GTK_BIN (widget)->child;
	g_return_if_fail (child != NULL);

	/* only resize if we're mapped (bug #128191),
	 * or if this is the initial size-allocate (bug #156854).
	 */
	if (GTK_WIDGET_MAPPED (child) ||
	    memcmp (&child->allocation, &invalid, sizeof (GtkAllocation)) == 0)
	{
		gtk_widget_size_allocate (child, allocation);
	}
}

static void
ephy_tab_map (GtkWidget *widget)
{
	GtkWidget *child;

	g_return_if_fail (GTK_WIDGET_REALIZED (widget));

	child = GTK_BIN (widget)->child;
	g_return_if_fail (child != NULL);

	/* we do this since the window might have been resized while this
	 * tab wasn't mapped (i.e. was a non-active tab during the resize).
	 * See bug #156854.
	 */
	if (memcmp (&widget->allocation, &child->allocation,
	    sizeof (GtkAllocation)) != 0)
	{
		gtk_widget_size_allocate (child, &widget->allocation);
	}

	GTK_WIDGET_CLASS (parent_class)->map (widget);	
}

static void
ephy_tab_grab_focus (GtkWidget *widget)
{
	EphyTab *tab = EPHY_TAB (widget);

	gtk_widget_grab_focus (GTK_WIDGET (ephy_tab_get_embed (tab)));
}

static void
ephy_tab_class_init (EphyTabClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);

        parent_class = g_type_class_peek_parent (class);

	object_class->dispose = ephy_tab_dispose;
	object_class->finalize = ephy_tab_finalize;

	widget_class->size_allocate = ephy_tab_size_allocate;
	widget_class->size_request = ephy_tab_size_request;
	widget_class->map = ephy_tab_map;
	widget_class->grab_focus = ephy_tab_grab_focus;

	g_type_class_add_private (object_class, sizeof (EphyTabPrivate));
}

static void
ephy_tab_dispose (GObject *object)
{
	EphyTab *tab = EPHY_TAB (object);
	EphyTabPrivate *priv = tab->priv;

	if (priv->idle_resize_handler != 0)
	{
		g_source_remove (priv->idle_resize_handler);
		priv->idle_resize_handler = 0;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
ephy_tab_finalize (GObject *object)
{
	EphyTab *tab = EPHY_TAB (object);
	EphyTabPrivate *priv = tab->priv;
	guint id = priv->id;

	G_OBJECT_CLASS (parent_class)->finalize (object);

#ifdef ENABLE_PYTHON
	ephy_python_schedule_gc ();
#endif

	LOG ("EphyTab finalized %p", tab);

	/* Remove the ID */
	g_array_index (tabs_id_array, gpointer, id) = NULL;
	if (--n_tabs == 0)
	{
		g_array_free (tabs_id_array, TRUE);
		tabs_id_array = NULL;
	}
}

static gboolean
address_has_web_scheme (const char *address)
{
	gboolean has_web_scheme;

	if (address == NULL) return FALSE;

	has_web_scheme = (g_str_has_prefix (address, "http:") ||
			  g_str_has_prefix (address, "https:") ||
			  g_str_has_prefix (address, "ftp:") ||
			  g_str_has_prefix (address, "file:") ||
			  g_str_has_prefix (address, "data:") ||
			  g_str_has_prefix (address, "about:") ||
			  g_str_has_prefix (address, "gopher:"));

	return has_web_scheme;
}

static gboolean
let_me_resize_hack (EphyTab *tab)
{
	EphyTabPrivate *priv = tab->priv;

	gtk_widget_set_size_request (GTK_WIDGET (tab), -1, -1);

	priv->idle_resize_handler = 0;	
	return FALSE;
}

/* Public functions */

/**
 * ephy_tab_new:
 *
 * Equivalent to g_object_new(), but returns an #EphyTab so you don't have to
 * cast it.
 *
 * Returns: a new #EphyTab
 **/
EphyTab *
ephy_tab_new (void)
{
	return EPHY_TAB (g_object_new (EPHY_TYPE_TAB, NULL));
}

/**
 * ephy_tab_get_embed:
 * @tab: an #EphyTab
 *
 * Returns @tab's #EphyEmbed.
 *
 * Return value: @tab's #EphyEmbed
 **/
EphyEmbed *
ephy_tab_get_embed (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), NULL);

	return EPHY_EMBED (gtk_bin_get_child (GTK_BIN (tab)));
}

/**
 * ephy_tab_for_embed
 * @embed: an #EphyEmbed
 *
 * Returns the #EphyTab which holds @embed.
 *
 * Return value: the #EphyTab which holds @embed
 **/
EphyTab *
ephy_tab_for_embed (EphyEmbed *embed)
{
	GtkWidget *parent;

	g_return_val_if_fail (EPHY_IS_EMBED (embed), NULL);

	parent = GTK_WIDGET (embed)->parent;
	g_return_val_if_fail (parent != NULL, NULL);

	return EPHY_TAB (parent);
}

/**
 * ephy_tab_get_size:
 * @tab: an #EphyTab
 * @width: return location for width, or %NULL
 * @height: return location for height, or %NULL
 *
 * Obtains the size of @tab. This is not guaranteed to be the actual number of
 * pixels occupied by the #EphyTab.
 **/
void
ephy_tab_get_size (EphyTab *tab, int *width, int *height)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	if (width != NULL)
	{
		*width = tab->priv->width;
	}
	if (height != NULL)
	{
		*height = tab->priv->height;
	}
}

/**
 * ephy_tab_set_size:
 * @tab: an #EphyTab
 * @width:
 * @height:
 *
 * Sets the size of @tab. This is not guaranteed to actually resize the tab.
 **/
void
ephy_tab_set_size (EphyTab *tab,
		   int width,
		   int height)
{
	EphyTabPrivate *priv = tab->priv;
	GtkWidget *widget = GTK_WIDGET (tab);
	GtkAllocation allocation;

	priv->width = width;
	priv->height = height;

	gtk_widget_set_size_request (widget, width, height);

	/* HACK: When the web site changes both width and height,
	 * we will first get a width change, then a height change,
	 * without actually resizing the window in between (since
	 * that happens only on idle).
	 * If we don't set the allocation, GtkMozEmbed doesn't tell
	 * mozilla the new width, so the height change sets the width
	 * back to the old value!
	 */
	allocation.x = widget->allocation.x;
	allocation.y = widget->allocation.y;
	allocation.width = width;
	allocation.height = height;
	gtk_widget_size_allocate (widget, &allocation);

	/* HACK: reset widget requisition after the container
	 * has been resized. It appears to be the only way
	 * to have the window sized according to embed
	 * size correctly.
	 */
	if (priv->idle_resize_handler == 0)
	{
		priv->idle_resize_handler =
			g_idle_add ((GSourceFunc) let_me_resize_hack, tab);
	}
}

/* Private callbacks for embed signals */

static gboolean
open_link_in_new (EphyTab *tab,
		  const char *link_address,
		  guint state)
{
	EphyTab *dest;

	if (!address_has_web_scheme (link_address)) return FALSE;

	dest = ephy_link_open (EPHY_LINK (tab), link_address, tab,
			       state & GDK_SHIFT_MASK ? EPHY_LINK_NEW_WINDOW
						      : EPHY_LINK_NEW_TAB);

	if (dest)
	{
		ephy_embed_shistory_copy (ephy_tab_get_embed (tab),
					  ephy_tab_get_embed (dest),
					  TRUE,   /* back history */
					  FALSE,  /* forward history */
					  FALSE); /* current index */
		return TRUE;
	}

	return FALSE;
}

static gboolean
save_property_url (EphyEmbed *embed,
		   EphyEmbedEvent *event,
		   const char *property,
		   const char *key)
{
	const char *location;
	const GValue *value;
	EphyEmbedPersist *persist;

	value = ephy_embed_event_get_property (event, property);
	location = g_value_get_string (value);

	if (!address_has_web_scheme (location)) return FALSE;

	persist = EPHY_EMBED_PERSIST
		(ephy_embed_factory_new_object (EPHY_TYPE_EMBED_PERSIST));

	ephy_embed_persist_set_embed (persist, embed);
	ephy_embed_persist_set_flags (persist, 0);
	ephy_embed_persist_set_persist_key (persist, key);
	ephy_embed_persist_set_source (persist, location);

	ephy_embed_persist_save (persist);

	g_object_unref (G_OBJECT(persist));

	return TRUE;
}

static void
clipboard_text_received_cb (GtkClipboard *clipboard,
			    const char *text,
			    gpointer *weak_ptr)
{
	if (*weak_ptr != NULL && text != NULL)
	{
		EphyEmbed *embed = (EphyEmbed *) *weak_ptr;
		EphyTab *tab = ephy_tab_for_embed (embed);

		ephy_link_open (EPHY_LINK (tab), text, tab, 0);
	}

	if (*weak_ptr != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (*weak_ptr), weak_ptr);
	}

	g_free (weak_ptr);
}

static gboolean
ephy_tab_dom_mouse_click_cb (EphyEmbed *embed,
			     EphyEmbedEvent *event,
			     EphyTab *tab)
{
	EphyEmbedEventContext context;
	guint button, modifier;
	gboolean handled = TRUE;
	gboolean with_control, with_shift, with_shift_control;
	gboolean is_left_click, is_middle_click;
	gboolean is_link, is_image, is_middle_clickable;
	gboolean middle_click_opens;
	gboolean is_input;

	g_return_val_if_fail (EPHY_IS_EMBED_EVENT(event), FALSE);

	button = ephy_embed_event_get_button (event);
	context = ephy_embed_event_get_context (event);
	modifier = ephy_embed_event_get_modifier (event);

	LOG ("ephy_tab_dom_mouse_click_cb: button %d, context %x, modifier %x",
	     button, context, modifier);

	with_control = (modifier & GDK_CONTROL_MASK) == GDK_CONTROL_MASK;
	with_shift = (modifier & GDK_SHIFT_MASK) == GDK_SHIFT_MASK;
	with_shift_control = (modifier & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) == (GDK_SHIFT_MASK | GDK_CONTROL_MASK);
	is_left_click = (button == 1);
	is_middle_click = (button == 2);

	middle_click_opens =
		eel_gconf_get_boolean (CONF_INTERFACE_MIDDLE_CLICK_OPEN_URL) &&
		!eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_ARBITRARY_URL);

	is_link = (context & EPHY_EMBED_CONTEXT_LINK) != 0;
	is_image = (context & EPHY_EMBED_CONTEXT_IMAGE) != 0;
	is_middle_clickable = !((context & EPHY_EMBED_CONTEXT_LINK)
				|| (context & EPHY_EMBED_CONTEXT_INPUT)
				|| (context & EPHY_EMBED_CONTEXT_EMAIL_LINK));
	is_input = (context & EPHY_EMBED_CONTEXT_INPUT) != 0;

	/* ctrl+click or middle click opens the link in new tab */
	if (is_link &&
	    ((is_left_click && (with_control || with_shift_control)) ||
	     is_middle_click))
	{
		const GValue *value;
		const char *link_address;

		value = ephy_embed_event_get_property (event, "link");
		link_address = g_value_get_string (value);
		handled = open_link_in_new (tab, link_address, modifier);
	}
	/* shift+click saves the link target */
	else if (is_link && is_left_click && with_shift)
	{
		handled = save_property_url (embed, event, "link", CONF_STATE_SAVE_DIR);
	}
	/* shift+click saves the non-link image
	 * Note: pressing enter to submit a form synthesizes a mouse click event
	 */
	else if (is_image && is_left_click && with_shift && !is_input)
	{
		handled = save_property_url (embed, event, "image", CONF_STATE_SAVE_IMAGE_DIR);
	}
	/* middle click opens the selection url */
	else if (is_middle_clickable && is_middle_click && middle_click_opens)
	{
		/* See bug #133633 for why we do it this way */

		/* We need to make sure we know if the embed is destroyed between
		 * requesting the clipboard contents, and receiving them.
		 */
		gpointer *weak_ptr;

		weak_ptr = g_new (gpointer, 1);
		*weak_ptr = embed;
		g_object_add_weak_pointer (G_OBJECT (embed), weak_ptr);

		gtk_clipboard_request_text
			(gtk_widget_get_clipboard (GTK_WIDGET (embed),
						   GDK_SELECTION_PRIMARY),
			 (GtkClipboardTextReceivedFunc) clipboard_text_received_cb,
			 weak_ptr);
	}
	/* we didn't handle the event */
	else
	{
		handled = FALSE;
	}

	return handled;
}

static void
ephy_tab_init (EphyTab *tab)
{
	EphyTabPrivate *priv;
	GObject *embed;
	guint id;

	LOG ("EphyTab initialising %p", tab);

	priv = tab->priv = EPHY_TAB_GET_PRIVATE (tab);

	/* Make tab ID */
	++n_tabs;

	if (tabs_id_array == NULL)
	{
		tabs_id_array = g_array_sized_new (FALSE /* zero-terminate */,
					       TRUE /* clear */,
					       sizeof (gpointer),
					       64 /* initial size */);
	}

	for (id = 0; id < tabs_id_array->len; ++id)
	{
		if (g_array_index (tabs_id_array, gpointer, id) == NULL) break;
	}

	priv->id = id;

	/* Grow array if necessary */
	if (id >= tabs_id_array->len)
	{
		g_array_append_val (tabs_id_array, tab);
		g_assert (g_array_index (tabs_id_array, gpointer, id) == tab);
	}
	else
	{
		g_array_index (tabs_id_array, gpointer, id) = tab;
	}

	tab->priv->width = -1;
	tab->priv->height = -1;

	embed = ephy_embed_factory_new_object (EPHY_TYPE_EMBED);
	g_assert (embed != NULL);

	gtk_container_add (GTK_CONTAINER (tab), GTK_WIDGET (embed));
	gtk_widget_show (GTK_WIDGET (embed));

	g_signal_connect_object (embed, "ge_dom_mouse_click",
				 G_CALLBACK (ephy_tab_dom_mouse_click_cb),
				 tab, 0);
}

/* private */
guint
_ephy_tab_get_id (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), (guint) -1);

	return tab->priv->id;
}
