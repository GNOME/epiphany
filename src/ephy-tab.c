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
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-tab.h"
#include "ephy-shell.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-embed-prefs.h"
#include "ephy-debug.h"
#include "ephy-string.h"
#include "ephy-notebook.h"
#include "ephy-file-helpers.h"
#include "ephy-zoom.h"
#include "session.h"
#include "ephy-favicon-cache.h"

#include <bonobo/bonobo-i18n.h>
#include <libgnomevfs/gnome-vfs-uri.h>
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
#include <string.h>

#define EPHY_TAB_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TAB, EphyTabPrivate))

struct EphyTabPrivate
{
	EphyEmbed *embed;
	EphyWindow *window;
	char *status_message;
	char *link_message;
	char *icon_address;
	char *address;
	char *title;
	int load_percent;
	gboolean visibility;
	gboolean load_status;
	TabAddressExpire address_expire;
	int cur_requests;
	int total_requests;
	int width;
	int height;
	GtkAction *action;
	float zoom;
	EmbedSecurityLevel security_level;
	TabNavigationFlags nav_flags;
};

static void ephy_tab_class_init		(EphyTabClass *klass);
static void ephy_tab_init		(EphyTab *tab);
static void ephy_tab_finalize		(GObject *object);

enum
{
	PROP_0,
	PROP_ADDRESS,
	PROP_ICON,
	PROP_LOAD_PROGRESS,
	PROP_LOAD_STATUS,
	PROP_MESSAGE,
	PROP_NAVIGATION,
	PROP_SECURITY,
	PROP_TITLE,
	PROP_VISIBLE,
	PROP_WINDOW,
	PROP_ZOOM
};

static GObjectClass *parent_class = NULL;

static gulong tab_id = 0;

/* internal functions, accessible only from this file */
static void	ephy_tab_set_icon_address	(EphyTab *tab,
						 const char *location);
static void	ephy_tab_set_load_status	(EphyTab *tab,
						 gboolean status);
static void	ephy_tab_set_link_message	(EphyTab *tab,
						 const char *message);
static void	ephy_tab_set_load_percent	(EphyTab *tab,
						 int percent);
static void	ephy_tab_update_navigation_flags(EphyTab *tab);
static void	ephy_tab_set_security_level	(EphyTab *tab,
						 EmbedSecurityLevel level);
static void	ephy_tab_set_title		(EphyTab *tab,
						 const char *new_title);
static void	ephy_tab_set_zoom		(EphyTab *tab,
						 float zoom);

/* Class functions */

GType
ephy_tab_get_type (void)
{
        static GType ephy_tab_type = 0;

        if (ephy_tab_type == 0)
        {
                static const GTypeInfo our_info =
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

                ephy_tab_type = g_type_register_static (G_TYPE_OBJECT,
							"EphyTab",
							&our_info, 0);
        }

        return ephy_tab_type;
}

static void
ephy_tab_set_property (GObject *object,
		       guint prop_id,
		       const GValue *value,
		       GParamSpec *pspec)
{
	EphyTab *tab = EPHY_TAB (object);

	switch (prop_id)

	{
		case PROP_ADDRESS:
			ephy_tab_set_location (tab, g_value_get_string (value),
					       TAB_ADDRESS_EXPIRE_NOW);
			break;
		case PROP_WINDOW:
			ephy_tab_set_window (tab, g_value_get_object (value));
			break;
		case PROP_ICON:
		case PROP_LOAD_PROGRESS:
		case PROP_LOAD_STATUS:
		case PROP_MESSAGE:
		case PROP_NAVIGATION:
		case PROP_SECURITY:
		case PROP_TITLE:
		case PROP_VISIBLE:
		case PROP_ZOOM:
			/* read only */
			break;
	}
}

static void
ephy_tab_get_property (GObject *object,
		       guint prop_id,
		       GValue *value,
		       GParamSpec *pspec)
{
	EphyTab *tab = EPHY_TAB (object);

	switch (prop_id)
	{
		case PROP_ADDRESS:
			g_value_set_string (value, tab->priv->address);
			break;
		case PROP_ICON:
			g_value_set_string (value, tab->priv->icon_address);
			break;
		case PROP_LOAD_PROGRESS:
			g_value_set_int (value, tab->priv->load_percent);
			break;
		case PROP_LOAD_STATUS:
			g_value_set_boolean (value, tab->priv->load_status);
			break;
		case PROP_MESSAGE:
			g_value_set_string (value, ephy_tab_get_status_message (tab));
			break;
		case PROP_NAVIGATION:
			g_value_set_int (value, tab->priv->nav_flags);
			break;
		case PROP_SECURITY:
			g_value_set_int (value, tab->priv->security_level);
			break;
		case PROP_TITLE:
			g_value_set_string (value, tab->priv->title);
			break;
		case PROP_VISIBLE:
			g_value_set_boolean (value, tab->priv->visibility);
			break;
		case PROP_WINDOW:
			g_value_set_object (value, tab->priv->window);
			break;
		case PROP_ZOOM:
			g_value_set_float (value, tab->priv->zoom);
			break;
	}
}

static void
ephy_tab_action_activate_cb (GtkAction *action, EphyTab *tab)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	ephy_window_jump_to_tab (tab->priv->window, tab);
}

static void
ephy_tab_class_init (EphyTabClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);

        parent_class = g_type_class_peek_parent (class);

        object_class->finalize = ephy_tab_finalize;
	object_class->get_property = ephy_tab_get_property;
	object_class->set_property = ephy_tab_set_property;

	g_object_class_install_property (object_class,
					 PROP_ADDRESS,
					 g_param_spec_string ("address",
							      "Address",
							      "The tab's address",
							      "",
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_ICON,
					 g_param_spec_string ("icon",
							      "Icon address",
							      "The tab icon's address",
							      NULL,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_LOAD_STATUS,
					 g_param_spec_int ("load-progress",
							   "Load progress",
							   "The tab's load progress in percent",
							   -1,
							   100,
							   0,
							   G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_LOAD_STATUS,
					 g_param_spec_boolean ("load-status",
							       "Load status",
							       "The tab's load status",
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_MESSAGE,
					 g_param_spec_string ("message",
							      "Message",
							      "The tab's statusbar message",
							      NULL,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_NAVIGATION,
					 g_param_spec_int ("navigation",
							   "Navigation flags",
							   "The tab's navigation flags",
							    0,
							    TAB_NAV_UP |
							    TAB_NAV_BACK |
							    TAB_NAV_FORWARD,
							    0,
							    G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_LOAD_STATUS,
					 g_param_spec_int ("security-level",
							   "Security Level",
							   "The tab's security level",
							    STATE_IS_UNKNOWN,
							    STATE_IS_SECURE_HIGH,
							    STATE_IS_UNKNOWN,
							    G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_TITLE,
					 g_param_spec_string ("title",
							      "Title",
							      "The tab's title",
							      _("Blank page"),
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_LOAD_STATUS,
					 g_param_spec_boolean ("visible",
							       "Visibility",
							       "The tab's visibility",
							       TRUE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "Window",
							      "The tab's parent window",
							      EPHY_TYPE_WINDOW,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_ZOOM,
					 g_param_spec_float ("zoom",
							     "Zoom",
							     "The tab's zoom",
							     ZOOM_MINIMAL,
							     ZOOM_MAXIMAL,
							     1.0,
							     G_PARAM_READABLE));

	g_type_class_add_private (object_class, sizeof(EphyTabPrivate));
}

static void
ephy_tab_parent_set_cb (GtkWidget *widget, GtkWidget *previous_parent,
			EphyTab *tab)
{
	GtkWidget *toplevel;

	if (widget->parent == NULL) return;

	toplevel = gtk_widget_get_toplevel (widget);
	ephy_tab_set_window (tab, EPHY_WINDOW (toplevel));
}
static void
ephy_tab_embed_destroy_cb (GtkWidget *widget, EphyTab *tab)
{
	LOG ("GtkMozEmbed destroy signal on EphyTab")
	g_object_unref (tab);
}

static void
ephy_tab_finalize (GObject *object)
{
        EphyTab *tab = EPHY_TAB (object);

	g_idle_remove_by_data (tab->priv->embed);

	if (tab->priv->action)
	{
		g_object_unref (tab->priv->action);
	}

	g_free (tab->priv->title);
	g_free (tab->priv->address);
	g_free (tab->priv->icon_address);
	g_free (tab->priv->link_message);
	g_free (tab->priv->status_message);

	G_OBJECT_CLASS (parent_class)->finalize (object);

	LOG ("EphyTab finalized %p", tab)
}

/* Public functions */

EphyTab *
ephy_tab_new (void)
{
	EphyTab *tab;

	tab = EPHY_TAB (g_object_new (EPHY_TYPE_TAB, NULL));

	return tab;
}

static void
ephy_tab_set_load_status (EphyTab *tab, gboolean status)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	tab->priv->load_status = status;

	g_object_notify (G_OBJECT (tab), "load-status");
}

gboolean
ephy_tab_get_load_status (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), FALSE);

	return tab->priv->load_status;
}

static void
ephy_tab_set_link_message (EphyTab *tab, const char *message)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	g_free (tab->priv->link_message);
	tab->priv->link_message = g_strdup (message);

	g_object_notify (G_OBJECT (tab), "message");
}

const char *
ephy_tab_get_link_message (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), NULL);

	return tab->priv->link_message;
}

EphyEmbed *
ephy_tab_get_embed (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), NULL);

	return tab->priv->embed;
}

void
ephy_tab_set_window (EphyTab *tab, EphyWindow *window)
{
	g_return_if_fail (EPHY_IS_TAB (tab));
	if (window) g_return_if_fail (EPHY_IS_WINDOW (window));

	if (window != tab->priv->window)
	{
		tab->priv->window = window;

		g_object_notify (G_OBJECT (tab), "window");
	}
}

EphyWindow *
ephy_tab_get_window (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), NULL);

	return tab->priv->window;
}

void
ephy_tab_get_size (EphyTab *tab, int *width, int *height)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	*width = tab->priv->width;
	*height = tab->priv->height;
}

gboolean
ephy_tab_get_visibility (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), FALSE);

	return tab->priv->visibility;
}

void
ephy_tab_set_visibility (EphyTab *tab,
                         gboolean visible)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	tab->priv->visibility = visible;

	g_object_notify (G_OBJECT (tab), "visible");
}

static void
ephy_tab_icon_cache_changed_cb (EphyFaviconCache *cache,
				const char *address,
				EphyTab *tab)
{
	g_return_if_fail (address != NULL);

	/* is this for us? */
	if (tab->priv->icon_address != NULL &&
	    strcmp (tab->priv->icon_address, address) == 0)
	{
		/* notify */
		g_object_notify (G_OBJECT (tab), "icon");
	}
}

static void
ephy_tab_set_icon_address (EphyTab *tab, const char *address)
{
	EphyBookmarks *eb;
	EphyHistory *history;

	g_return_if_fail (EPHY_IS_TAB (tab));

	g_free (tab->priv->icon_address);

	tab->priv->icon_address = g_strdup (address);

	if (tab->priv->icon_address)
	{
		eb = ephy_shell_get_bookmarks (ephy_shell);
		history = ephy_embed_shell_get_global_history
			(EPHY_EMBED_SHELL (ephy_shell));
		ephy_bookmarks_set_icon (eb, tab->priv->address,
				         tab->priv->icon_address);
		ephy_history_set_icon (history, tab->priv->address,
				       tab->priv->icon_address);
	}

	g_object_notify (G_OBJECT (tab), "icon");
}

const char *
ephy_tab_get_icon_address (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), NULL);

	return tab->priv->icon_address;
}

/* Private callbacks for embed signals */

static void
ephy_tab_favicon_cb (EphyEmbed *embed,
		     const char *address,
		     EphyTab *tab)
{
	ephy_tab_set_icon_address (tab, address);
}

static void
ephy_tab_link_message_cb (EphyEmbed *embed,
			  const char *message,
			  EphyTab *tab)
{
	ephy_tab_set_link_message (tab, message);
}

static void
ephy_tab_address_cb (EphyEmbed *embed, const char *address, EphyTab *tab)
{
	if (tab->priv->address_expire == TAB_ADDRESS_EXPIRE_NOW)
	{
		ephy_tab_set_location (tab, address, TAB_ADDRESS_EXPIRE_NOW);
	}

	ephy_tab_set_link_message (tab, NULL);
	ephy_tab_set_icon_address (tab, NULL);
	ephy_tab_update_navigation_flags (tab);
}

static void
ephy_tab_zoom_changed_cb (EphyEmbed *embed, float zoom, EphyTab *tab)
{
	ephy_tab_set_zoom (tab, zoom);
}

static void
ephy_tab_title_cb (EphyEmbed *embed, EphyTab *tab)
{
	char *title;

	ephy_embed_get_title (embed, &title);

	ephy_tab_set_title (tab, title);

	g_free (title);
}

static int
build_load_percent (int requests_done, int requests_total)
{
	int percent;

	if (requests_total > 0)
	{
		percent = (requests_done * 100) / requests_total;

		/* Mozilla sometimes report more done requests than
		   total requests. Their progress widget clamp the value */
		percent = CLAMP (percent, 0, 100);
	}
	else
	{
		percent = -1;
	}

	return percent;
}

static char *
get_host_name_from_uri (const char *uri)
{
	GnomeVFSURI *vfs_uri = NULL;
	const char *host = NULL;
	char *result;

	if (uri)
	{
		vfs_uri = gnome_vfs_uri_new (uri);
	}

	if (vfs_uri)
	{
		host = gnome_vfs_uri_get_host_name (vfs_uri);
	}

	if (!host)
	{
		host = _("site");
	}

	result = g_strdup (host);

	if (vfs_uri) gnome_vfs_uri_unref (vfs_uri);

	return result;
}

static char *
build_net_state_message (const char *uri, EmbedState flags)
{
	const char *msg = NULL;
	char *host, *message = NULL;

	host = get_host_name_from_uri (uri);

	/* IS_REQUEST and IS_NETWORK can be both set */

	if (flags & EMBED_STATE_IS_REQUEST)
        {
                if (flags & EMBED_STATE_REDIRECTING)
                {
			msg = _("Redirecting to %s...");
                }
                else if (flags & EMBED_STATE_TRANSFERRING)
                {
			msg = _("Transferring data from %s...");
                }
                else if (flags & EMBED_STATE_NEGOTIATING)
                {
			msg = _("Waiting for authorization from %s...");
                }
        }

	if (flags & EMBED_STATE_IS_NETWORK)
        {
                if (flags & EMBED_STATE_START)
                {
                        msg = _("Loading %s...");
                }
                else if (flags & EMBED_STATE_STOP)
                {
			msg = _("Done.");
                }
        }

	if (msg)
	{
		message = g_strdup_printf (msg, host); 
	}

	g_free (host);

	return message;
}

static void
build_progress_from_requests (EphyTab *tab, EmbedState state)
{
	int load_percent;

	if (state & EMBED_STATE_IS_REQUEST)
        {
                if (state & EMBED_STATE_START)
                {
			tab->priv->total_requests ++;
		}
		else if (state & EMBED_STATE_STOP)
		{
			tab->priv->cur_requests ++;
		}

		load_percent = build_load_percent (tab->priv->cur_requests,
						   tab->priv->total_requests);
		if (load_percent > tab->priv->load_percent || load_percent == -1)
		{
			ephy_tab_set_load_percent (tab, load_percent);
		}
	}
}

static void
ensure_page_info (EphyTab *tab, const char *address)
{
	if (tab->priv->address == NULL &&
	    tab->priv->address_expire == TAB_ADDRESS_EXPIRE_NOW)
        {
		ephy_tab_set_location (tab, address, TAB_ADDRESS_EXPIRE_NOW);
	}

	if (tab->priv->title == NULL)
	{
		ephy_tab_set_title (tab, NULL);
	}
}

static void
ephy_tab_net_state_cb (EphyEmbed *embed, const char *uri,
		       EmbedState state, EphyTab *tab)
{
	char *new_msg;

	new_msg = build_net_state_message (uri, state);
	if (new_msg)
	{
		g_free (tab->priv->status_message);
		tab->priv->status_message = new_msg;
	}

	g_object_notify (G_OBJECT (tab), "message");

	if (state & EMBED_STATE_IS_NETWORK)
	{
		if (state & EMBED_STATE_START)
		{
			tab->priv->total_requests = 0;
			tab->priv->cur_requests = 0;
			ensure_page_info (tab, uri);

			ephy_tab_set_load_percent (tab, 0);
			ephy_tab_set_load_status (tab, TRUE);
			ephy_tab_update_navigation_flags (tab);
		}
		else if (state & EMBED_STATE_STOP)
		{
			/* tab load completed, save in session */
			Session *s;

			s = EPHY_SESSION (ephy_shell_get_session (ephy_shell));
			session_save (s, SESSION_CRASHED);

			ephy_tab_set_load_percent (tab, 0);
			ephy_tab_set_load_status (tab, FALSE);
			ephy_tab_update_navigation_flags (tab);
			tab->priv->address_expire = TAB_ADDRESS_EXPIRE_NOW;
		}
	}

	build_progress_from_requests (tab, state);
}

static void
ephy_tab_new_window_cb (EphyEmbed *embed, EphyEmbed **new_embed,
			EmbedChromeMask chromemask, EphyTab *tab)
{
	EphyTab *new_tab;
	EphyWindow *window;

	window = ephy_window_new ();
	ephy_window_set_chrome (window, chromemask);

	new_tab = ephy_tab_new ();
        ephy_window_add_tab (window, new_tab, EPHY_NOTEBOOK_INSERT_GROUPED, FALSE);

	*new_embed = ephy_tab_get_embed (new_tab);
}

static gboolean
let_me_resize_hack (gpointer data)
{
	gtk_widget_set_size_request (GTK_WIDGET(data),
				     -1, -1);
	return FALSE;
}

static void
ephy_tab_visibility_cb (EphyEmbed *embed, gboolean visibility,
			  EphyTab *tab)
{
	if (visibility)
	{
		gtk_widget_show (GTK_WIDGET(embed));
	}
	else
	{
		gtk_widget_hide (GTK_WIDGET(embed));
	}

	ephy_tab_set_visibility (tab, visibility);
}

static void
ephy_tab_destroy_brsr_cb (EphyEmbed *embed, EphyTab *tab)
{
	g_return_if_fail (EPHY_IS_TAB (tab));
	g_return_if_fail (tab->priv->window != NULL);

	ephy_window_remove_tab (tab->priv->window, tab);
}

static gint
ephy_tab_open_uri_cb (EphyEmbed *embed, const char *uri,
		      EphyTab *tab)
{
	LOG ("ephy_tab_open_uri_cb %s", uri)

	/* FIXME: what is this function supposed to do ? */
	return FALSE;
}

static void
ephy_tab_size_to_cb (EphyEmbed *embed, gint width, gint height,
		     EphyTab *tab)
{
	GList *tabs = NULL;
	EphyWindow *window;
	GtkWidget *widget;
	EmbedChromeMask chromemask;

	tab->priv->width = width;
	tab->priv->height = height;

	window = tab->priv->window;
	tabs = ephy_window_get_tabs (window);
	widget = GTK_WIDGET (embed);
	chromemask = ephy_window_get_chrome (window);

	/* Do not resize window with multiple tabs.
	 * Do not resize window already showed because
	 * it's not possible to calculate a sensible window
	 * size based on the embed size */
	if (g_list_length (tabs) == 1 && !tab->priv->visibility)
	{
		gtk_widget_set_size_request
			(widget, width, height);

		/* HACK reset widget requisition after the container
		 * has been resized. It appears to be the only way
		 * to have the window sized according to embed
		 * size correctly.
		 * We dont do it for XUL dialogs because in that case
		 * a "forced" requisition appear correct.
		 */
		if (!(chromemask & EMBED_CHROME_OPENASCHROME))
		{
			g_idle_add (let_me_resize_hack, embed);
		}
	}

	g_list_free (tabs);
}

static void
open_link_in_new_tab (EphyTab *tab,
		      const char *link_address)
{
	GnomeVFSURI *uri;
	const char *scheme;
	EphyWindow *window;
	gboolean new_tab = FALSE;

	window = ephy_tab_get_window (tab);
	g_return_if_fail (window != NULL);

	uri = gnome_vfs_uri_new (link_address);
	if (uri != NULL)
	{
		scheme = gnome_vfs_uri_get_scheme (uri);

		new_tab = (strcmp (scheme, "http") == 0 ||
			   strcmp (scheme, "https") == 0 ||
	                   strcmp (scheme, "ftp") == 0 ||
	                   strcmp (scheme, "file") == 0);

		gnome_vfs_uri_unref (uri);
	}

	if (new_tab)
	{
		ephy_shell_new_tab (ephy_shell, window, tab,
				    link_address,
				    EPHY_NEW_TAB_OPEN_PAGE);
	}
	else
	{
		ephy_window_load_url (window, link_address);
	}
}

static gint
ephy_tab_dom_mouse_click_cb  (EphyEmbed *embed,
			      EphyEmbedEvent *event,
			      EphyTab *tab)
{
	EphyEmbedEventType type;
	EmbedEventContext context;
	EphyWindow *window;

	window = ephy_tab_get_window (tab);
	g_return_val_if_fail (window != NULL, FALSE);

	g_assert (EPHY_IS_EMBED_EVENT(event));

	ephy_embed_event_get_event_type (event, &type);
	ephy_embed_event_get_context (event, &context);

	if (type == EPHY_EMBED_EVENT_MOUSE_BUTTON2
	    && (context & EMBED_CONTEXT_LINK))
	{
		const GValue *value;
		const char *link_address;

		ephy_embed_event_get_property (event, "link", &value);
		link_address = g_value_get_string (value);
		open_link_in_new_tab (tab, link_address);
	}
	else if (type == EPHY_EMBED_EVENT_MOUSE_BUTTON2 &&
		 eel_gconf_get_boolean (CONF_INTERFACE_MIDDLE_CLICK_OPEN_URL) &&
		 !(context & EMBED_CONTEXT_LINK
		   || context & EMBED_CONTEXT_EMAIL_LINK
		   || context & EMBED_CONTEXT_INPUT))
	{
		/* paste url */
		gtk_selection_convert (GTK_WIDGET (window),
				       GDK_SELECTION_PRIMARY,
				       GDK_SELECTION_TYPE_STRING,
				       GDK_CURRENT_TIME);
	}

	return FALSE;
}

static void
ephy_tab_security_change_cb (EphyEmbed *embed, EmbedSecurityLevel level,
			       EphyTab *tab)
{
	ephy_tab_set_security_level (tab, level);
}

static void
ephy_tab_init (EphyTab *tab)
{
	GObject *embed, *embed_widget;
	EphyEmbedSingle *single;
	EphyFaviconCache *cache;
	char *id;

	LOG ("EphyTab initialising %p", tab)

	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));

	tab->priv = EPHY_TAB_GET_PRIVATE (tab);

	tab->priv->window = NULL;
	tab->priv->status_message = NULL;
	tab->priv->link_message = NULL;
	tab->priv->total_requests = 0;
	tab->priv->cur_requests = 0;
	tab->priv->width = -1;
	tab->priv->height = -1;
	tab->priv->title = NULL;
	tab->priv->address = NULL;
	tab->priv->icon_address = NULL;
	tab->priv->load_percent = 0;
	tab->priv->load_status = FALSE;
	tab->priv->link_message = NULL;
	tab->priv->security_level = STATE_IS_UNKNOWN;
	tab->priv->status_message = NULL;
	tab->priv->zoom = 1.0;
	tab->priv->address_expire = TAB_ADDRESS_EXPIRE_NOW;

	tab->priv->embed = ephy_embed_new (G_OBJECT(single));
	g_assert (tab->priv->embed != NULL);

	embed = G_OBJECT (tab->priv->embed);
	embed_widget = G_OBJECT (tab->priv->embed);

	id = g_strdup_printf ("Tab%lu", tab_id++);

	tab->priv->action = g_object_new (GTK_TYPE_ACTION,
					  "name", id,
					  "label", _("Blank page"),
					  NULL);
	g_free (id);
	g_signal_connect (tab->priv->action, "activate",
			  G_CALLBACK (ephy_tab_action_activate_cb), tab);

	/* set a pointer in the embed's widget back to the tab */
	g_object_set_data (embed_widget, "EphyTab", tab);

	g_signal_connect (embed_widget, "parent_set",
			  G_CALLBACK (ephy_tab_parent_set_cb),
			  tab);
	g_signal_connect (embed_widget, "destroy",
			  G_CALLBACK (ephy_tab_embed_destroy_cb),
			  tab);
	g_signal_connect (embed, "ge_link_message",
			  G_CALLBACK (ephy_tab_link_message_cb),
			  tab);
	g_signal_connect (embed, "ge_location",
			  G_CALLBACK (ephy_tab_address_cb),
			  tab);
	g_signal_connect (embed, "ge_title",
			  G_CALLBACK (ephy_tab_title_cb),
			  tab);
	g_signal_connect (embed, "ge_zoom_change",
			  G_CALLBACK (ephy_tab_zoom_changed_cb),
			  tab);
	g_signal_connect (embed, "ge_net_state",
			  G_CALLBACK (ephy_tab_net_state_cb),
			  tab);
	g_signal_connect (embed, "ge_new_window",
			  G_CALLBACK (ephy_tab_new_window_cb),
			  tab);
	g_signal_connect (embed, "ge_visibility",
			  G_CALLBACK (ephy_tab_visibility_cb),
			  tab);
	g_signal_connect (embed, "ge_destroy_brsr",
			  G_CALLBACK (ephy_tab_destroy_brsr_cb),
			  tab);
	g_signal_connect (embed, "ge_open_uri",
			  G_CALLBACK (ephy_tab_open_uri_cb),
			  tab);
	g_signal_connect (embed, "ge_size_to",
			  G_CALLBACK (ephy_tab_size_to_cb),
			  tab);
	g_signal_connect (embed, "ge_dom_mouse_click",
			  G_CALLBACK (ephy_tab_dom_mouse_click_cb),
			  tab);
	g_signal_connect (embed, "ge_security_change",
			  G_CALLBACK (ephy_tab_security_change_cb),
			  tab);
	g_signal_connect (embed, "ge_favicon",
			  G_CALLBACK (ephy_tab_favicon_cb),
			  tab);

	cache = EPHY_FAVICON_CACHE
		(ephy_embed_shell_get_favicon_cache (EPHY_EMBED_SHELL (ephy_shell)));
	g_signal_connect_object (G_OBJECT (cache), "changed",
				 G_CALLBACK (ephy_tab_icon_cache_changed_cb),
				 tab,  0);
}

void
ephy_tab_set_load_percent (EphyTab *tab, int percent)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	if (percent != tab->priv->load_percent)
	{
		tab->priv->load_percent = percent;

		g_object_notify (G_OBJECT (tab), "load-progress");
	}
}

int
ephy_tab_get_load_percent (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), -1);

	return tab->priv->load_percent;
}

static void
ephy_tab_update_navigation_flags (EphyTab *tab)
{
	EphyEmbed *embed;
	TabNavigationFlags flags = 0;

	embed = tab->priv->embed;

	if (ephy_embed_can_go_up (embed) == G_OK)
	{
		flags |= TAB_NAV_UP;
	}

	if (ephy_embed_can_go_back (embed) == G_OK)
	{
		flags |= TAB_NAV_BACK;
	}

	if (ephy_embed_can_go_forward (embed) == G_OK)
	{
		flags |= TAB_NAV_FORWARD;
	}

	if (flags != tab->priv->nav_flags)
	{
		tab->priv->nav_flags = flags;

		g_object_notify (G_OBJECT (tab), "navigation");
	}
}

TabNavigationFlags
ephy_tab_get_navigation_flags (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), 0);

	return tab->priv->nav_flags;
}

const char *
ephy_tab_get_status_message (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), "");

	if (tab->priv->link_message && tab->priv->link_message[0] != '\0')
	{
		return tab->priv->link_message;
	}
	else if (tab->priv->status_message)
	{
		return tab->priv->status_message;
	}
	else
	{
		return "";
	}
}

#define MAX_LABEL_LENGTH	32

static void
ephy_tab_set_title (EphyTab *tab, const char *new_title)
{
	char *title_short = NULL;
	char *title = NULL;

	g_return_if_fail (EPHY_IS_TAB (tab));

	g_free (tab->priv->title);

	if (new_title == NULL || new_title[0] == '\0')
	{
		GnomeVFSURI *uri = NULL;
		char *address;

		ephy_embed_get_location (tab->priv->embed, TRUE, &address);

		if (address)
		{
			uri = gnome_vfs_uri_new (address);
		}

		if (uri)
		{
			title = gnome_vfs_uri_to_string (uri,
					GNOME_VFS_URI_HIDE_USER_NAME |
					GNOME_VFS_URI_HIDE_PASSWORD |
					GNOME_VFS_URI_HIDE_HOST_PORT |
					GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD |
					GNOME_VFS_URI_HIDE_FRAGMENT_IDENTIFIER);
			gnome_vfs_uri_unref (uri);
		}
		else if (address != NULL && strncmp (address, "about:blank", 11) != 0)
		{
			title = g_strdup (address);
		}

		if (title == NULL || title[0] == '\0')
		{
			g_free (title);
			title = g_strdup (_("Blank page"));
		}

		g_free (address);
	}
	else
	{
		title = g_strdup (new_title);
	}

	tab->priv->title = title;

	title_short = ephy_string_shorten (title, MAX_LABEL_LENGTH);

	/**
	 * FIXME: instead of shortening the title here, use an egg action
	 * which creates menu items with ellipsizing labels
	 */
	g_object_set (G_OBJECT (tab->priv->action),
		      "label", title_short,
		      NULL);

	g_free (title_short);

	g_object_notify (G_OBJECT (tab), "title");
}

const char *
ephy_tab_get_title (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), "");

	return tab->priv->title;
}

const char *
ephy_tab_get_location (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), "");

	return tab->priv->address;
}

void
ephy_tab_set_location (EphyTab *tab,
		       const char *address,
		       TabAddressExpire expire)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	if (tab->priv->address) g_free (tab->priv->address);
	tab->priv->address = g_strdup (address);

	if (expire == TAB_ADDRESS_EXPIRE_CURRENT &&
	    !tab->priv->load_status)
	{
		tab->priv->address_expire = TAB_ADDRESS_EXPIRE_NOW;
	}
	else
	{
		tab->priv->address_expire = expire;
	}

	g_object_notify (G_OBJECT (tab), "address");
}

static void
ephy_tab_set_security_level (EphyTab *tab, EmbedSecurityLevel level)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	tab->priv->security_level = level;

	g_object_notify (G_OBJECT (tab), "security-level");
}

EmbedSecurityLevel
ephy_tab_get_security_level (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), STATE_IS_UNKNOWN);

	return tab->priv->security_level;
}

static void
ephy_tab_set_zoom (EphyTab *tab, float zoom)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	tab->priv->zoom = zoom;

	g_object_notify (G_OBJECT (tab), "zoom");
}

float
ephy_tab_get_zoom (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), 1.0);

	return tab->priv->zoom;
}

GObject *
ephy_tab_get_action (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), NULL);

	return G_OBJECT (tab->priv->action);
}
