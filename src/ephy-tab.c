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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-tab.h"
#include "ephy-shell.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-embed-prefs.h"
#include "ephy-debug.h"
#include "egg-menu-merge.h"
#include "ephy-string.h"
#include "ephy-notebook.h"
#include "ephy-file-helpers.h"
#include "ephy-zoom.h"

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
#include <string.h>

struct EphyTabPrivate
{
	EphyEmbed *embed;
	EphyWindow *window;
	EphyEmbedEvent *event;
	gboolean is_active;
	TabLoadStatus load_status;
	char *status_message;
	char *link_message;
	char *icon_address;
	char *address;
	char *title;
	int load_percent;
	gboolean visibility;
	int cur_requests;
	int total_requests;
	int width;
	int height;
	EggAction *action;
	float zoom;
	EmbedSecurityLevel security_level;
};

static void ephy_tab_class_init		(EphyTabClass *klass);
static void ephy_tab_init		(EphyTab *tab);
static void ephy_tab_finalize		(GObject *object);
static void ephy_tab_update_color	(EphyTab *tab);

enum
{
	PROP_0,
	PROP_ADDRESS,
	PROP_ICON,
	PROP_LOAD_PROGRESS,
	PROP_LOAD_STATUS,
	PROP_MESSAGE,
	PROP_SECURITY,
	PROP_TITLE,
	PROP_WINDOW,
	PROP_ZOOM
};

static GObjectClass *parent_class = NULL;

static gulong tab_id = 0;

/* internal functions, accessible only from this file */
void		ephy_tab_set_icon_address	(EphyTab *tab,
						 const char *location);
void		ephy_tab_set_load_status	(EphyTab *tab,
						 TabLoadStatus status);
void		ephy_tab_set_link_message	(EphyTab *tab,
						 const char *message);
void		ephy_tab_set_load_percent	(EphyTab *tab,
						 int percent);
void		ephy_tab_set_security_level	(EphyTab *tab,
						 EmbedSecurityLevel level);
void		ephy_tab_set_title		(EphyTab *tab,
						 const char *new_title);
void		ephy_tab_set_zoom		(EphyTab *tab,
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
			ephy_tab_set_location (tab, g_value_get_string (value));
			break;
		case PROP_WINDOW:
			ephy_tab_set_window (tab, g_value_get_object (value));
			break;
		case PROP_ICON:
		case PROP_LOAD_PROGRESS:
		case PROP_LOAD_STATUS:
		case PROP_MESSAGE:
		case PROP_SECURITY:
		case PROP_TITLE:
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
			g_value_set_int (value, tab->priv->load_status);
			break;
		case PROP_MESSAGE:
			g_value_set_string (value, ephy_tab_get_status_message (tab));
			break;
		case PROP_SECURITY:
			g_value_set_int (value, tab->priv->security_level);
			break;
		case PROP_TITLE:
			g_value_set_string (value, tab->priv->title);
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
ephy_tab_action_activate_cb (EggAction *action, EphyTab *tab)
{
	g_return_if_fail (IS_EPHY_TAB (tab));

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
					 g_param_spec_int ("load-status",
							   "Load status",
							   "The tab's load status",
							    TAB_LOAD_NONE,
							    TAB_LOAD_COMPLETED,
							    TAB_LOAD_NONE,
							    G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_MESSAGE,
					 g_param_spec_string ("message",
							      "Message",
							      "The tab's statusbar message",
							      NULL,
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
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "Window",
							      "The tab's parent window",
							      EPHY_WINDOW_TYPE,
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
}

static void
ephy_tab_parent_set_cb (GtkWidget *widget, GtkWidget *previous_parent,
			EphyTab *tab)
{
	GtkWidget *toplevel;

	if (widget->parent == NULL) return;

	toplevel = gtk_widget_get_toplevel (widget);

	tab->priv->window = EPHY_WINDOW (toplevel);
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
        EphyTab *tab;

	g_return_if_fail (IS_EPHY_TAB (object));

	tab = EPHY_TAB (object);

        g_return_if_fail (tab->priv != NULL);

	g_idle_remove_by_data (tab->priv->embed);

	if (tab->priv->event)
	{
		g_object_unref (tab->priv->event);
	}

	if (tab->priv->action)
	{
		g_object_unref (tab->priv->action);
	}

	g_free (tab->priv->title);
	g_free (tab->priv->address);
	g_free (tab->priv->icon_address);
	g_free (tab->priv->link_message);
	g_free (tab->priv->status_message);

        g_free (tab->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);

	LOG ("EphyTab finalized %p", tab)
}

/* Public functions */

EphyTab *
ephy_tab_new (void)
{
	EphyTab *tab;

	tab = EPHY_TAB (g_object_new (EPHY_TAB_TYPE, NULL));
	g_return_val_if_fail (tab->priv != NULL, NULL);
	return tab;
}

void
ephy_tab_set_load_status (EphyTab *tab,
			  TabLoadStatus status)
{
	if (status == TAB_LOAD_COMPLETED)
	{
		Session *s;
		s = ephy_shell_get_session (ephy_shell);
		session_save (s, SESSION_CRASHED);
	}

	if (ephy_tab_get_is_active (tab) &&
	    status == TAB_LOAD_COMPLETED)
	{
		tab->priv->load_status = TAB_LOAD_NONE;
	}
	else
	{
		tab->priv->load_status = status;
	}

	ephy_tab_update_color (tab);

	g_object_notify (G_OBJECT (tab), "load-status");
}

TabLoadStatus
ephy_tab_get_load_status (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (tab), TAB_LOAD_NONE);

	return tab->priv->load_status;
}

void
ephy_tab_set_link_message (EphyTab *tab, const char *message)
{
	g_return_if_fail (IS_EPHY_TAB (tab));

	g_free (tab->priv->link_message);
	tab->priv->link_message = g_strdup (message);
	
	g_object_notify (G_OBJECT (tab), "message");
}

const char *
ephy_tab_get_link_message (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (tab), NULL);

	return tab->priv->link_message;
}

EphyEmbed *
ephy_tab_get_embed (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (tab), NULL);

	return tab->priv->embed;
}

void
ephy_tab_set_window (EphyTab *tab, EphyWindow *window)
{
	g_return_if_fail (IS_EPHY_TAB (tab));
	if (window) g_return_if_fail (IS_EPHY_WINDOW (window));

	tab->priv->window = window;	
}

EphyWindow *
ephy_tab_get_window (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (tab), NULL);

	return tab->priv->window;
}

EphyEmbedEvent *
ephy_tab_get_event (EphyTab *tab)
{
	return tab->priv->event;
}

static void
ephy_tab_update_color (EphyTab *tab)
{
	TabLoadStatus status = ephy_tab_get_load_status (tab);
	EphyNotebookPageLoadStatus page_status = 0;
	GtkWidget *nb;

	nb = ephy_window_get_notebook (tab->priv->window);

	switch (status)
	{
		case TAB_LOAD_NONE:
			page_status = EPHY_NOTEBOOK_TAB_LOAD_NORMAL;
		break;
		case TAB_LOAD_STARTED:
			page_status = EPHY_NOTEBOOK_TAB_LOAD_LOADING;
		break;
		case TAB_LOAD_COMPLETED:
			page_status = EPHY_NOTEBOOK_TAB_LOAD_COMPLETED;
			break;
	}

	ephy_notebook_set_page_status (EPHY_NOTEBOOK (nb),
				      GTK_WIDGET (tab->priv->embed),
				      page_status);
}

void
ephy_tab_set_is_active (EphyTab *tab, gboolean is_active)
{
	g_return_if_fail (IS_EPHY_TAB (tab));

	tab->priv->is_active = is_active;

	if (tab->priv->load_status == TAB_LOAD_COMPLETED)
	{
		ephy_tab_set_load_status (tab, TAB_LOAD_NONE);
	}
}

gboolean
ephy_tab_get_is_active (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (tab), FALSE);

	return tab->priv->is_active;
}

void
ephy_tab_get_size (EphyTab *tab, int *width, int *height)
{
	g_return_if_fail (IS_EPHY_TAB (tab));

	*width = tab->priv->width;
	*height = tab->priv->height;
}

gboolean
ephy_tab_get_visibility (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (tab), FALSE);

	return tab->priv->visibility;
}

void
ephy_tab_set_visibility (EphyTab *tab,
                         gboolean visible)
{
	g_return_if_fail (IS_EPHY_TAB (tab));
	g_return_if_fail (tab->priv->window != NULL);

	tab->priv->visibility = visible;
}

static void
ephy_tab_set_favicon (EphyTab *tab,
		      GdkPixbuf *favicon)
{
	GtkWidget *nb;

	nb = ephy_window_get_notebook (tab->priv->window);
	ephy_notebook_set_page_icon (EPHY_NOTEBOOK (nb),
				     GTK_WIDGET (tab->priv->embed),
				     favicon);
}

static void
ephy_tab_icon_cache_changed_cb (EphyFaviconCache *cache,
				const char *address,
				EphyTab *tab)
{
	GdkPixbuf *pixbuf = NULL;

	/* is this for us? */
	if (tab->priv->icon_address == NULL ||
	    strcmp (tab->priv->icon_address, address) != 0) return;

	/* notify */
	g_object_notify (G_OBJECT (tab), "icon");
	
	/* set favicon */
	if (tab->priv->icon_address)
	{
		pixbuf = ephy_favicon_cache_get (cache, tab->priv->icon_address);
		ephy_tab_set_favicon (tab, pixbuf);

		if (pixbuf)
		{
			g_object_unref (pixbuf);
		}
	}
}

void
ephy_tab_set_icon_address (EphyTab *tab, const char *address)
{
	EphyBookmarks *eb;
	EphyHistory *history;
	EphyFaviconCache *cache;
	GdkPixbuf *pixbuf = NULL;

	g_return_if_fail (IS_EPHY_TAB (tab));

	g_free (tab->priv->icon_address);
	tab->priv->icon_address = g_strdup (address);

	cache = ephy_embed_shell_get_favicon_cache
		(EPHY_EMBED_SHELL (ephy_shell));

	if (tab->priv->icon_address)
	{
		eb = ephy_shell_get_bookmarks (ephy_shell);
		history = ephy_embed_shell_get_global_history
			(EPHY_EMBED_SHELL (ephy_shell));
		ephy_bookmarks_set_icon (eb, tab->priv->address,
				         tab->priv->icon_address);
		ephy_history_set_icon (history, tab->priv->address,
				       tab->priv->icon_address);

		pixbuf = ephy_favicon_cache_get (cache, tab->priv->icon_address);

		ephy_tab_set_favicon (tab, pixbuf);

		if (pixbuf)
		{
			g_object_unref (pixbuf);
		}
	}
	else
	{
		ephy_tab_set_favicon (tab, NULL);
	}

	ephy_window_update_control (tab->priv->window,
				    FaviconControl);

	g_object_notify (G_OBJECT (tab), "icon");
}

const char *
ephy_tab_get_icon_address (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (tab), "");

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
			  const gchar *message,
			  EphyTab *tab)
{
	if (!tab->priv->is_active) return;

	ephy_tab_set_link_message (tab, message);

	ephy_window_update_control (tab->priv->window,
				    StatusbarMessageControl);
}

static void
ephy_tab_address_cb (EphyEmbed *embed, EphyTab *tab)
{
	char *address;

	ephy_embed_get_location (embed, TRUE, &address);
	ephy_tab_set_location (tab, address);
	g_free (address);

	ephy_tab_set_link_message (tab, NULL);
	ephy_tab_set_icon_address (tab, NULL);

	if (tab->priv->is_active)
	{
		ephy_window_update_control (tab->priv->window, LocationControl);
		ephy_window_update_control (tab->priv->window, NavControl);
		ephy_window_update_control (tab->priv->window, FaviconControl);
	}
}

static void
ephy_tab_zoom_changed_cb (EphyEmbed *embed, float zoom, EphyTab *tab)
{
	ephy_tab_set_zoom (tab, zoom);

	if (tab->priv->is_active)
	{
		ephy_window_update_control (tab->priv->window, ZoomControl);
	}
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
build_load_percent (int bytes_loaded, int max_bytes_loaded)
{
	if (max_bytes_loaded > 0)
	{
		return (bytes_loaded * 100) / max_bytes_loaded;
	}
	else
	{
		return -1;
	}
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
		if (load_percent > tab->priv->load_percent)
		{
			ephy_tab_set_load_percent (tab, load_percent);
		}
	}
}

static void
ensure_address (EphyTab *tab, const char *address)
{
	if (tab->priv->address == NULL)
	{
		ephy_tab_set_location (tab, address);
		ephy_window_update_control (tab->priv->window, LocationControl);
	}
}

static void
ephy_tab_net_state_cb (EphyEmbed *embed, const char *uri,
		       EmbedState state, EphyTab *tab)
{
	g_free (tab->priv->status_message);
	tab->priv->status_message = build_net_state_message (uri, state);

	g_object_notify (G_OBJECT (tab), "message");

	ephy_window_update_control (tab->priv->window, StatusbarMessageControl);

	if (state & EMBED_STATE_IS_NETWORK)
	{
		if (state & EMBED_STATE_START)
		{
			tab->priv->total_requests = 0;
			tab->priv->cur_requests = 0;
			ephy_tab_set_load_percent (tab, 0);

			ensure_address (tab, uri);

			ephy_tab_set_load_status (tab, TAB_LOAD_STARTED);

			ephy_window_update_control (tab->priv->window,
						      NavControl);
			ephy_window_update_control (tab->priv->window,
						      SpinnerControl);
		}
		else if (state & EMBED_STATE_STOP)
		{
			ephy_tab_set_load_percent (tab, 0);

			ephy_tab_set_load_status (tab, TAB_LOAD_COMPLETED);

			ephy_window_update_control (tab->priv->window,
						      NavControl);
			ephy_window_update_control (tab->priv->window,
						      SpinnerControl);
			ephy_tab_update_color (tab);
		}
	}

	build_progress_from_requests (tab, state);

	ephy_window_update_control (tab->priv->window,
				      StatusbarProgressControl);
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

	g_return_if_fail (tab->priv->window != NULL);

	ephy_window_update_control (tab->priv->window, WindowVisibilityControl);
}

static void
ephy_tab_destroy_brsr_cb (EphyEmbed *embed, EphyTab *tab)
{
	EphyWindow *window;

	g_return_if_fail (IS_EPHY_TAB (tab));
	g_return_if_fail (tab->priv->window != NULL);

	window = ephy_tab_get_window (tab);

	ephy_window_remove_tab (window, tab);

	ephy_embed_shell_remove_embed (EPHY_EMBED_SHELL (ephy_shell),
				       tab->priv->embed);
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
ephy_tab_set_event (EphyTab *tab,
		    EphyEmbedEvent *event)
{
	if (tab->priv->event) g_object_unref (tab->priv->event);
	g_object_ref (event);
	tab->priv->event = event;
}

static void
ephy_tab_show_embed_popup (EphyTab *tab, EphyEmbedEvent *event)
{
	EmbedEventContext context;
	const char *popup;
	const GValue *value;
	gboolean framed;
	EphyWindow *window;
	char *path;
	GtkWidget *widget;

	g_return_if_fail (IS_EPHY_TAB (tab));
	window = tab->priv->window;

	ephy_embed_event_get_property (event, "framed_page", &value);
	framed = g_value_get_int (value);

	ephy_embed_event_get_context (event, &context);

	if ((context & EMBED_CONTEXT_LINK) &&
	    (context & EMBED_CONTEXT_IMAGE))
	{
		popup = "EphyImageLinkPopup";
	}
	else if (context & EMBED_CONTEXT_LINK)
	{
		popup = "EphyLinkPopup";
	}
	else if (context & EMBED_CONTEXT_IMAGE)
	{
		popup = "EphyImagePopup";
	}
	else if (context & EMBED_CONTEXT_INPUT)
	{
		popup = "EphyInputPopup";
	}
	else
	{
		popup = framed ? "EphyFramedDocumentPopup" :
				 "EphyDocumentPopup";
	}

	path = g_strconcat ("/popups/", popup, NULL);
	widget = egg_menu_merge_get_widget (EGG_MENU_MERGE (window->ui_merge),
				            path);
	g_free (path);

	g_return_if_fail (widget != NULL);

	ephy_tab_set_event (tab, event);
	gtk_menu_popup (GTK_MENU (widget), NULL, NULL, NULL, NULL, 2,
			gtk_get_current_event_time ());
}

static gint
ephy_tab_dom_mouse_click_cb  (EphyEmbed *embed,
			      EphyEmbedEvent *event,
			      EphyTab *tab)
{
	EphyWindow *window;
	int button;
	EmbedEventContext context;

	g_assert (IS_EPHY_EMBED_EVENT(event));

	window = ephy_tab_get_window (tab);
	g_return_val_if_fail (window != NULL, FALSE);

	ephy_embed_event_get_mouse_button (event, &button);
	ephy_embed_event_get_context (event, &context);

	if (button == 1
	    && (context & EMBED_CONTEXT_LINK))
	{
		const GValue *value;

		ephy_embed_event_get_property (event, "link", &value);
		ephy_shell_new_tab (ephy_shell, window, tab,
				      g_value_get_string (value),
				      EPHY_NEW_TAB_OPEN_PAGE);
	}
	else if (button == 1 &&
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

static gint
ephy_tab_dom_mouse_down_cb  (EphyEmbed *embed,
			     EphyEmbedEvent *event,
			     EphyTab *tab)
{
	EphyWindow *window;
	int button;

	g_assert (IS_EPHY_EMBED_EVENT(event));

	window = ephy_tab_get_window (tab);
	g_return_val_if_fail (window != NULL, FALSE);

	ephy_embed_event_get_mouse_button (event, &button);

	if (button == 2)
	{
		ephy_tab_show_embed_popup (tab, event);
	}

	return FALSE;
}

static void
ephy_tab_security_change_cb (EphyEmbed *embed, EmbedSecurityLevel level,
			       EphyTab *tab)
{
	ephy_tab_set_security_level (tab, level);

	if (!tab->priv->is_active) return;

	ephy_window_update_control (tab->priv->window,
				      StatusbarSecurityControl);
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

        tab->priv = g_new0 (EphyTabPrivate, 1);

	tab->priv->window = NULL;
	tab->priv->event = NULL;
	tab->priv->is_active = FALSE;
	tab->priv->status_message = NULL;
	tab->priv->link_message = NULL;
	tab->priv->total_requests = 0;
	tab->priv->cur_requests = 0;
	tab->priv->width = -1;
	tab->priv->height = -1;

	tab->priv->embed = ephy_embed_new (G_OBJECT(single));
	ephy_embed_shell_add_embed (EPHY_EMBED_SHELL (ephy_shell),
				    tab->priv->embed);

	embed = G_OBJECT (tab->priv->embed);
	embed_widget = G_OBJECT (tab->priv->embed);

	id = g_strdup_printf ("Tab%lu", tab_id++);

	tab->priv->action = g_object_new (EGG_TYPE_ACTION,
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
	g_signal_connect (embed, "ge_dom_mouse_down",
			  G_CALLBACK (ephy_tab_dom_mouse_down_cb),
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

	cache = ephy_embed_shell_get_favicon_cache (EPHY_EMBED_SHELL (ephy_shell));
	g_signal_connect_object (G_OBJECT (cache), "changed",
				 G_CALLBACK (ephy_tab_icon_cache_changed_cb),
				 tab,  0);
}

void
ephy_tab_set_load_percent (EphyTab *tab, int percent)
{
	g_return_if_fail (IS_EPHY_TAB (tab));

	tab->priv->load_percent = percent;

	g_object_notify (G_OBJECT (tab), "load-progress");
}

int
ephy_tab_get_load_percent (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (tab), -1);

	return tab->priv->load_percent;
}

const char *
ephy_tab_get_status_message (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (tab), "");

	if (tab->priv->link_message)
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
	
void
ephy_tab_set_title (EphyTab *tab, const char *new_title)
{
	GtkWidget *nb;
	GnomeVFSURI *uri;
	char *title_short = NULL;
	char *title = NULL;

	g_return_if_fail (IS_EPHY_TAB (tab));

	g_free (tab->priv->title);

	if (new_title == NULL || new_title[0] == '\0')
	{
		uri = gnome_vfs_uri_new (tab->priv->address);
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

		if (title == NULL || title[0] == '\0')
		{
			g_free (title);
			title = g_strdup (_("Blank page"));
		}
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

	if (tab->priv->window != NULL)
	{
		nb = ephy_window_get_notebook (tab->priv->window);
		ephy_notebook_set_page_title (EPHY_NOTEBOOK (nb),
					      GTK_WIDGET (tab->priv->embed),
					      tab->priv->title);

		if (tab->priv->is_active)
		{
			ephy_window_update_control (tab->priv->window, TitleControl);
		}
	}

	g_object_notify (G_OBJECT (tab), "title");
}

const char *
ephy_tab_get_title (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (tab), "");

	return tab->priv->title;
}

const char *
ephy_tab_get_location (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (tab), "");

	return tab->priv->address;
}

void
ephy_tab_set_location (EphyTab *tab,
		       const char *address)
{
	g_return_if_fail (IS_EPHY_TAB (tab));

	if (tab->priv->address) g_free (tab->priv->address);
	tab->priv->address = g_strdup (address);
	
	g_object_notify (G_OBJECT (tab), "address");
}

void
ephy_tab_set_security_level (EphyTab *tab, EmbedSecurityLevel level)
{
	g_return_if_fail (IS_EPHY_TAB (tab));

	tab->priv->security_level = level;

	g_object_notify (G_OBJECT (tab), "security-level");
}

EmbedSecurityLevel
ephy_tab_get_security_level (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (tab), STATE_IS_UNKNOWN);

	return tab->priv->security_level;
}

void
ephy_tab_set_zoom (EphyTab *tab, float zoom)
{
	g_return_if_fail (IS_EPHY_TAB (tab));

	tab->priv->zoom = zoom;

	g_object_notify (G_OBJECT (tab), "zoom");
}

float
ephy_tab_get_zoom (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (tab), 1.0);

	return tab->priv->zoom;
}

EggAction *
ephy_tab_get_action (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (tab), NULL);

	return tab->priv->action;
}
