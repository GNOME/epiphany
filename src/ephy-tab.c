/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
 *  Copyright (C) 2004 Crispin Flowerday
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
#include "ephy-shell.h"

#include <glib/gi18n.h>
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
#include <gtk/gtkradioaction.h>
#include <gtk/gtkclipboard.h>
#include <string.h>

#define EPHY_TAB_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TAB, EphyTabPrivate))

struct EphyTabPrivate
{
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
	GtkToggleAction *action;
	float zoom;
	gboolean setting_zoom;
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
						 char *message);
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

                ephy_tab_type = g_type_register_static (GTK_TYPE_BIN,
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
ephy_tab_size_allocate (GtkWidget *widget,
			GtkAllocation *allocation)
{
	GtkWidget *child;

	widget->allocation = *allocation;

	child = GTK_BIN (widget)->child;

	if (child && GTK_WIDGET_VISIBLE (child))
	{
		gtk_widget_size_allocate (child, allocation);
	}
}

static void
ephy_tab_parent_set (GtkWidget *widget,
		     GtkWidget *previous_parent)
{
	if (widget->parent)
	{
		GtkWidget *toplevel;
		toplevel = gtk_widget_get_toplevel (widget);

		ephy_tab_set_window (EPHY_TAB (widget), EPHY_WINDOW (toplevel));
	}

	if (GTK_WIDGET_CLASS (parent_class)->parent_set)
	{
		GTK_WIDGET_CLASS (parent_class)->parent_set (widget, previous_parent);
	}
}

static void
ephy_tab_action_activate_cb (GtkAction *action, EphyTab *tab)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) &&
	    ephy_window_get_active_tab (tab->priv->window) != tab)
	{
		ephy_window_jump_to_tab (tab->priv->window, tab);
	}
}

static void
ephy_tab_class_init (EphyTabClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);

        parent_class = g_type_class_peek_parent (class);

        object_class->finalize = ephy_tab_finalize;
	object_class->get_property = ephy_tab_get_property;
	object_class->set_property = ephy_tab_set_property;

	widget_class->size_allocate = ephy_tab_size_allocate;
	widget_class->parent_set = ephy_tab_parent_set;
	
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
					 PROP_LOAD_PROGRESS,
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
ephy_tab_finalize (GObject *object)
{
        EphyTab *tab = EPHY_TAB (object);

	g_idle_remove_by_data (tab);

	if (tab->priv->action != NULL)
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

/* Public functions */

EphyTab *
ephy_tab_new (void)
{
	return EPHY_TAB (g_object_new (EPHY_TYPE_TAB, NULL));
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
ephy_tab_set_link_message (EphyTab *tab, char *message)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	g_free (tab->priv->link_message);
	tab->priv->link_message = ephy_string_blank_chr (message);

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

	return EPHY_EMBED (gtk_bin_get_child (GTK_BIN (tab)));
}

EphyTab *
ephy_tab_for_embed (EphyEmbed *embed)
{
	GtkWidget *parent;

	g_return_val_if_fail (EPHY_IS_EMBED (embed), NULL);

	parent = GTK_WIDGET (embed)->parent;
	g_return_val_if_fail (parent != NULL, NULL);

	return EPHY_TAB (parent);
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
		history = EPHY_HISTORY
			(ephy_embed_shell_get_global_history (embed_shell));
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
			  EphyTab *tab)
{
	ephy_tab_set_link_message (tab, ephy_embed_get_link_message (embed));
}

static void
ephy_tab_address_cb (EphyEmbed *embed, const char *address, EphyTab *tab)
{
	const char *uv_address;

	LOG ("ephy_tab_address_cb tab %p address %s", tab, address)

	/* Do not expose about:blank to the user, an empty address
	   bar will do better */
	if (address && strcmp (address, "about:blank") == 0)
	{
		uv_address = "";
	}
	else
	{
		uv_address = address;
	}

	if (tab->priv->address_expire == TAB_ADDRESS_EXPIRE_NOW)
	{
		ephy_tab_set_location (tab, uv_address, TAB_ADDRESS_EXPIRE_NOW);
	}

	ephy_tab_set_link_message (tab, NULL);
	ephy_tab_set_icon_address (tab, NULL);
	ephy_tab_update_navigation_flags (tab);

	/* restore zoom level */
	if (address_has_web_scheme (address))
	{
		EphyHistory *history;
		EphyNode *host;
		GValue value = { 0, };
		float zoom = 1.0, current_zoom;

		history = EPHY_HISTORY
			(ephy_embed_shell_get_global_history (embed_shell));
		host = ephy_history_get_host (history, address);

		if (host != NULL && ephy_node_get_property
				     (host, EPHY_NODE_HOST_PROP_ZOOM, &value))
		{
			zoom = g_value_get_float (&value);
			g_value_unset (&value);
		}

		current_zoom = ephy_embed_zoom_get (embed);
		if (zoom != current_zoom)
		{
			tab->priv->setting_zoom = TRUE;
			ephy_embed_zoom_set (embed, zoom, FALSE);
			tab->priv->setting_zoom = FALSE;
		}
	}
}

static void
ephy_tab_zoom_changed_cb (EphyEmbed *embed, float zoom, EphyTab *tab)
{
	char *address;

	LOG ("ephy_tab_zoom_changed_cb tab %p zoom %f", tab, zoom)

	ephy_tab_set_zoom (tab, zoom);

	if (tab->priv->setting_zoom)
	{
		return;
	}

	address = ephy_embed_get_location (embed, TRUE);
	if (address_has_web_scheme (address))
	{
		EphyHistory *history;
		EphyNode *host;
		GValue value = { 0, };
		history = EPHY_HISTORY
			(ephy_embed_shell_get_global_history (embed_shell));
		host = ephy_history_get_host (history, address);

		if (host != NULL)
		{
			float zoom;

			zoom = ephy_embed_zoom_get (embed);

			g_value_init (&value, G_TYPE_FLOAT);
			g_value_set_float (&value, zoom);
			ephy_node_set_property
				(host, EPHY_NODE_HOST_PROP_ZOOM, &value);
			g_value_unset (&value);
		}
	}

	g_free (address);
}

static void
ephy_tab_title_cb (EphyEmbed *embed, EphyTab *tab)
{
	char *title;

	title = ephy_embed_get_title (embed);

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

static void
update_net_state_message (EphyTab *tab, const char *uri, EmbedState flags)
{
	const char *msg = NULL;
	char *host;

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
        }

	if ((flags & EMBED_STATE_IS_NETWORK) &&
	    (flags & EMBED_STATE_STOP))
        {
		g_free (tab->priv->status_message);
		tab->priv->status_message = NULL;
		g_object_notify (G_OBJECT (tab), "message");

	}
	else if (msg)
	{
		g_free (tab->priv->status_message);
		tab->priv->status_message = g_strdup_printf (msg, host);
		g_object_notify (G_OBJECT (tab), "message");
	}

	g_free (host);
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

		ephy_tab_set_load_percent (tab, load_percent);
	}
}

static void
ensure_page_info (EphyTab *tab, const char *address)
{
	if ((tab->priv->address == NULL || *tab->priv->address == '\0') &&
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
	update_net_state_message (tab, uri, state);

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
	ephy_window_request_chrome (window, chromemask);

	new_tab = ephy_tab_new ();
	gtk_widget_show (GTK_WIDGET (new_tab));

        ephy_window_add_tab (window, new_tab, EPHY_NOTEBOOK_INSERT_GROUPED, FALSE);

	*new_embed = ephy_tab_get_embed (new_tab);
}

static gboolean
let_me_resize_hack (GtkWidget *tab)
{
	gtk_widget_set_size_request (tab, -1, -1);

	return FALSE;
}

static void
ephy_tab_visibility_cb (EphyEmbed *embed, gboolean visibility,
			EphyTab *tab)
{
	if (visibility)
	{
		gtk_widget_show (GTK_WIDGET (tab));
	}
	else
	{
		gtk_widget_hide (GTK_WIDGET (tab));
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

static void
ephy_tab_size_to_cb (EphyEmbed *embed, gint width, gint height,
		     EphyTab *tab)
{
	GtkWidget *notebook;
	EphyWindow *window;

	tab->priv->width = width;
	tab->priv->height = height;

	window = tab->priv->window;
	notebook = ephy_window_get_notebook (window);

	/* Do not resize window with multiple tabs.
	 * Do not resize window already showed because
	 * it's not possible to calculate a sensible window
	 * size based on the embed size */
	if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) == 1 &&
	    !tab->priv->visibility)
	{
		gtk_widget_set_size_request
			(GTK_WIDGET (tab), width, height);

		/* HACK reset widget requisition after the container
		 * has been resized. It appears to be the only way
		 * to have the window sized according to embed
		 * size correctly.
		 */

		g_idle_add ((GSourceFunc) let_me_resize_hack, tab);
	}
}

static gboolean
open_link_in_new_tab (EphyTab *tab,
		      const char *link_address)
{
	EphyWindow *window;
	gboolean new_tab;

	window = ephy_tab_get_window (tab);
	g_return_val_if_fail (window != NULL, FALSE);

	new_tab = address_has_web_scheme (link_address);

	if (new_tab)
	{
		ephy_shell_new_tab (ephy_shell, window, tab,
				    link_address,
				    EPHY_NEW_TAB_OPEN_PAGE |
				    EPHY_NEW_TAB_IN_EXISTING_WINDOW);
	}

	return new_tab;
}

static void
save_property_url (EphyEmbed *embed,
		   EphyEmbedEvent *event,
		   const char *property,
		   const char *key)
{
	const char *location;
	const GValue *value;
	EphyEmbedPersist *persist;

	ephy_embed_event_get_property (event, property, &value);
	location = g_value_get_string (value);

	persist = EPHY_EMBED_PERSIST
		(ephy_embed_factory_new_object ("EphyEmbedPersist"));

	ephy_embed_persist_set_embed (persist, embed);
	ephy_embed_persist_set_flags (persist, 0);
	ephy_embed_persist_set_persist_key (persist, key);
	ephy_embed_persist_set_source (persist, location);

	ephy_embed_persist_save (persist);

	g_object_unref (G_OBJECT(persist));
}

static void
clipboard_text_received_cb (GtkClipboard *clipboard,
			    const char *text,
			    gpointer *weak_ptr)
{
	if (*weak_ptr != NULL && text != NULL)
	{
		EphyTab *tab = (EphyTab *) *weak_ptr;
		EphyEmbed *embed = ephy_tab_get_embed (tab);

		ephy_embed_load_url (embed, text);
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
	EphyEmbedEventType type;
	EmbedEventContext context;
	EphyWindow *window;
	guint modifier;
	gboolean handled = TRUE;
	gboolean with_control, with_shift, is_left_click, is_middle_click;
	gboolean is_link, is_image, is_middle_clickable;
	gboolean middle_click_opens;
	gboolean is_input;

	g_return_val_if_fail (EPHY_IS_EMBED_EVENT(event), FALSE);

	window = ephy_tab_get_window (tab);
	g_return_val_if_fail (window != NULL, FALSE);

	type = ephy_embed_event_get_event_type (event);
	context = ephy_embed_event_get_context (event);
	modifier = ephy_embed_event_get_modifier (event);

	LOG ("ephy_tab_dom_mouse_click_cb: type %d, context %x, modifier %x",
	     type, context, modifier)

	with_control = (modifier & GDK_CONTROL_MASK) != 0;
	with_shift = (modifier & GDK_SHIFT_MASK) != 0;
	is_left_click = (type == EPHY_EMBED_EVENT_MOUSE_BUTTON1);
	is_middle_click = (type == EPHY_EMBED_EVENT_MOUSE_BUTTON2);

	middle_click_opens =
		eel_gconf_get_boolean (CONF_INTERFACE_MIDDLE_CLICK_OPEN_URL) &&
		!eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_ARBITRARY_URL);

	is_link = (context & EMBED_CONTEXT_LINK) != 0;
	is_image = (context & EMBED_CONTEXT_IMAGE) != 0;
	is_middle_clickable = !((context & EMBED_CONTEXT_LINK)
				|| (context & EMBED_CONTEXT_INPUT)
				|| (context & EMBED_CONTEXT_EMAIL_LINK));
	is_input = (context & EMBED_CONTEXT_INPUT) != 0;

	/* ctrl+click or middle click opens the link in new tab */
	if (is_link && ((is_left_click && with_control) || is_middle_click))
	{
		const GValue *value;
		const char *link_address;

		ephy_embed_event_get_property (event, "link", &value);
		link_address = g_value_get_string (value);
		handled = open_link_in_new_tab (tab, link_address);
	}
	/* shift+click saves the link target */
	else if (is_link && is_left_click && with_shift)
	{
		save_property_url (embed, event, "link", CONF_STATE_SAVE_DIR);
	}
	/* shift+click saves the non-link image
	 * Note: pressing enter to submit a form synthesizes a mouse click event
	 */
	else if (is_image && is_left_click && with_shift && !is_input)
	{
		save_property_url (embed, event, "image", CONF_STATE_SAVE_IMAGE_DIR);
	}
	/* middle click opens the selection url */
	else if (is_middle_clickable && is_middle_click && middle_click_opens)
	{
		/* See bug #133633 for why we do it this way */

		/* We need to make sure we know if the tab is destroyed between
		 * requesting the clipboard contents, and receiving them.
		 */
		gpointer *weak_ptr;

		weak_ptr = g_new (gpointer, 1);
		*weak_ptr = tab;
		g_object_add_weak_pointer (G_OBJECT (tab), weak_ptr);

		gtk_clipboard_request_text
			(gtk_widget_get_clipboard (GTK_WIDGET (tab),
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
ephy_tab_security_change_cb (EphyEmbed *embed, EmbedSecurityLevel level,
			       EphyTab *tab)
{
	ephy_tab_set_security_level (tab, level);
}

static void
ephy_tab_init (EphyTab *tab)
{
	GObject *embed;
	EphyFaviconCache *cache;
	char *id;

	LOG ("EphyTab initialising %p", tab)

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
	tab->priv->zoom = 1.0;
	tab->priv->setting_zoom = FALSE;
	tab->priv->address_expire = TAB_ADDRESS_EXPIRE_NOW;

	embed = ephy_embed_factory_new_object ("EphyEmbed");
	g_assert (embed != NULL);

	gtk_container_add (GTK_CONTAINER (tab), GTK_WIDGET (embed));
	gtk_widget_show (GTK_WIDGET (embed));

	id = g_strdup_printf ("Tab%lu", tab_id++);

	tab->priv->action = g_object_new (GTK_TYPE_TOGGLE_ACTION,
					  "name", id,
					  "label", _("Blank page"),
					  "draw_as_radio", TRUE,
					  NULL);

	g_free (id);
	g_signal_connect (tab->priv->action, "activate",
			  G_CALLBACK (ephy_tab_action_activate_cb), tab);

	g_signal_connect_object (embed, "link_message",
				 G_CALLBACK (ephy_tab_link_message_cb),
				 tab, 0);
	g_signal_connect_object (embed, "ge_location",
				 G_CALLBACK (ephy_tab_address_cb),
				 tab, 0);
	g_signal_connect_object (embed, "title",
				 G_CALLBACK (ephy_tab_title_cb),
				 tab, 0);
	g_signal_connect_object (embed, "ge_zoom_change",
				 G_CALLBACK (ephy_tab_zoom_changed_cb),
				 tab, 0);
	g_signal_connect_object (embed, "ge_net_state",
				 G_CALLBACK (ephy_tab_net_state_cb),
				 tab, 0);
	g_signal_connect_object (embed, "ge_new_window",
				 G_CALLBACK (ephy_tab_new_window_cb),
				 tab, 0);
	g_signal_connect_object (embed, "visibility",
				 G_CALLBACK (ephy_tab_visibility_cb),
				 tab, 0);
	g_signal_connect_object (embed, "destroy_browser",
				 G_CALLBACK (ephy_tab_destroy_brsr_cb),
				 tab, 0);
	g_signal_connect_object (embed, "size_to",
				 G_CALLBACK (ephy_tab_size_to_cb),
				 tab, 0);
	g_signal_connect_object (embed, "ge_dom_mouse_click",
				 G_CALLBACK (ephy_tab_dom_mouse_click_cb),
				 tab, 0);
	g_signal_connect_object (embed, "ge_security_change",
				 G_CALLBACK (ephy_tab_security_change_cb),
				 tab, 0);
	g_signal_connect_object (embed, "ge_favicon",
				 G_CALLBACK (ephy_tab_favicon_cb),
				 tab, 0);

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

	embed = ephy_tab_get_embed (tab);

	if (ephy_embed_can_go_up (embed))
	{
		flags |= TAB_NAV_UP;
	}

	if (ephy_embed_can_go_back (embed))
	{
		flags |= TAB_NAV_BACK;
	}

	if (ephy_embed_can_go_forward (embed))
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
	g_return_val_if_fail (EPHY_IS_TAB (tab), NULL);

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
		return NULL;
	}
}

#define MAX_LABEL_LENGTH	32

static void
ephy_tab_set_title (EphyTab *tab, const char *new_title)
{
	char *title_short, *title_tmp;
	char *title = NULL;

	g_return_if_fail (EPHY_IS_TAB (tab));

	g_free (tab->priv->title);

	if (new_title == NULL || new_title[0] == '\0')
	{
		GnomeVFSURI *uri = NULL;
		char *address;

		address = ephy_embed_get_location (ephy_tab_get_embed (tab), TRUE);

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
	title_tmp = ephy_string_double_underscores (title_short);

	/**
	 * FIXME: instead of shortening the title here, use an egg action
	 * which creates menu items with ellipsizing labels
	 */
	g_object_set (G_OBJECT (tab->priv->action),
		      "label", title_tmp,
		      NULL);

	g_free (title_short);
	g_free (title_tmp);

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
