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
	char status_message[255];
	char *link_message;
	char favicon_url[255];
	char *title;
	char *location;
	int load_percent;
	gboolean visibility;
	int cur_requests;
	int total_requests;
	int width;
	int height;
};

static void
ephy_tab_class_init (EphyTabClass *klass);
static void
ephy_tab_init (EphyTab *tab);
static void
ephy_tab_finalize (GObject *object);

enum
{
	PROP_0,
	PROP_EPHY_SHELL
};

static void
ephy_tab_set_favicon (EphyTab *tab,
		      GdkPixbuf *favicon);
static void
ephy_tab_favicon_cb (EphyEmbed *embed,
		     const char *url,
		     EphyTab *tab);
static void
ephy_tab_favicon_cache_changed_cb (EphyFaviconCache *cache, 
				   char *url, 
				   EphyTab *tab);
static void
ephy_tab_link_message_cb (EphyEmbed *embed,
			  const char *message,
			  EphyTab *tab);
static void
ephy_tab_location_cb (EphyEmbed *embed, EphyTab *tab);
static void
ephy_tab_title_cb (EphyEmbed *embed, EphyTab *tab);
static void
ephy_tab_net_state_cb (EphyEmbed *embed, const char *uri,
		       EmbedState state, EphyTab *tab);
static void
ephy_tab_new_window_cb (EphyEmbed *embed, EphyEmbed **new_embed,
			EmbedChromeMask chromemask, EphyTab *tab);
static void
ephy_tab_visibility_cb (EphyEmbed *embed, gboolean visibility,
			EphyTab *tab);
static void
ephy_tab_destroy_brsr_cb (EphyEmbed *embed, EphyTab *tab);
static gint
ephy_tab_open_uri_cb (EphyEmbed *embed, const char *uri,
		      EphyTab *tab);
static void
ephy_tab_size_to_cb (EphyEmbed *embed, gint width, gint height,
		     EphyTab *tab);
static gint
ephy_tab_dom_mouse_down_cb (EphyEmbed *embed,
			    EphyEmbedEvent *event,
			    EphyTab *tab);
static void
ephy_tab_security_change_cb (EphyEmbed *embed, EmbedSecurityLevel level,
			     EphyTab *tab);
static void
ephy_tab_zoom_changed_cb (EphyEmbed *embed, gint zoom,
			  EphyTab *tab);

static GObjectClass *parent_class = NULL;

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
ephy_tab_class_init (EphyTabClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_tab_finalize;
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
ephy_tab_init (EphyTab *tab)
{
	GObject *embed, *embed_widget;
	EphyEmbedSingle *single;
	EphyFaviconCache *cache;

	single = ephy_embed_shell_get_embed_single
		(EPHY_EMBED_SHELL (ephy_shell));

        tab->priv = g_new0 (EphyTabPrivate, 1);

	tab->priv->window = NULL;
	tab->priv->event = NULL;
	tab->priv->is_active = FALSE;
	*tab->priv->status_message = '\0';
	tab->priv->link_message = NULL;
	*tab->priv->favicon_url = '\0';
	tab->priv->load_status = TAB_LOAD_NONE;
	tab->priv->load_percent = 0;
	tab->priv->title = NULL;
	tab->priv->location = NULL;
	tab->priv->total_requests = 0;
	tab->priv->cur_requests = 0;
	tab->priv->width = -1;
	tab->priv->height = -1;

	tab->priv->embed = ephy_embed_new (G_OBJECT(single));
	ephy_embed_shell_add_embed (EPHY_EMBED_SHELL (ephy_shell),
				    tab->priv->embed);

	embed = G_OBJECT (tab->priv->embed);
	embed_widget = G_OBJECT (tab->priv->embed);

	/* set a pointer in the embed's widget back to the tab */
	g_object_set_data (embed_widget, "EphyTab", tab);

	g_signal_connect (embed_widget, "parent_set",
			  G_CALLBACK (ephy_tab_parent_set_cb),
			  tab);
	g_signal_connect (embed_widget, "destroy",
			  GTK_SIGNAL_FUNC (ephy_tab_embed_destroy_cb),
			  tab);
	g_signal_connect (embed, "ge_link_message",
			  GTK_SIGNAL_FUNC (ephy_tab_link_message_cb),
			  tab);
	g_signal_connect (embed, "ge_location",
			  GTK_SIGNAL_FUNC (ephy_tab_location_cb),
			  tab);
	g_signal_connect (embed, "ge_title",
			  GTK_SIGNAL_FUNC (ephy_tab_title_cb),
			  tab);
	g_signal_connect (embed, "ge_zoom_change",
			  GTK_SIGNAL_FUNC (ephy_tab_zoom_changed_cb),
			  tab);
	g_signal_connect (embed, "ge_net_state",
			  GTK_SIGNAL_FUNC (ephy_tab_net_state_cb),
			  tab);
	g_signal_connect (embed, "ge_new_window",
			  GTK_SIGNAL_FUNC (ephy_tab_new_window_cb),
			  tab);
	g_signal_connect (embed, "ge_visibility",
			  GTK_SIGNAL_FUNC (ephy_tab_visibility_cb),
			  tab);
	g_signal_connect (embed, "ge_destroy_brsr",
			  GTK_SIGNAL_FUNC (ephy_tab_destroy_brsr_cb),
			  tab);
	g_signal_connect (embed, "ge_open_uri",
			  GTK_SIGNAL_FUNC (ephy_tab_open_uri_cb),
			  tab);
	g_signal_connect (embed, "ge_size_to",
			  GTK_SIGNAL_FUNC (ephy_tab_size_to_cb),
			  tab);
	g_signal_connect (embed, "ge_dom_mouse_down",
			  GTK_SIGNAL_FUNC (ephy_tab_dom_mouse_down_cb),
			  tab);
	g_signal_connect (embed, "ge_security_change",
			  GTK_SIGNAL_FUNC (ephy_tab_security_change_cb),
			  tab);
	g_signal_connect (embed, "ge_favicon",
			  GTK_SIGNAL_FUNC (ephy_tab_favicon_cb),
			  tab);

	cache = ephy_embed_shell_get_favicon_cache (EPHY_EMBED_SHELL (ephy_shell));
	g_signal_connect_object (G_OBJECT (cache), "changed",
				 G_CALLBACK (ephy_tab_favicon_cache_changed_cb),
				 tab,  0);
}

/* Destructor */

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

	g_free (tab->priv->location);
	g_free (tab->priv->link_message);

        g_free (tab->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);

	LOG ("EphyTab finalized %p", tab)
}

/* Public functions */

EphyTab *
ephy_tab_new ()
{
	EphyTab *tab;

	tab = EPHY_TAB (g_object_new (EPHY_TAB_TYPE, NULL));
	g_return_val_if_fail (tab->priv != NULL, NULL);
	return tab;
}

static void
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
}

EphyEmbed *
ephy_tab_get_embed (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (G_OBJECT (tab)), NULL);

	return tab->priv->embed;
}

void
ephy_tab_set_window (EphyTab *tab, EphyWindow *window)
{
	g_return_if_fail (IS_EPHY_TAB (G_OBJECT (tab)));
	if (window) g_return_if_fail (IS_EPHY_WINDOW (G_OBJECT (window)));

	tab->priv->window = window;
}

EphyWindow *
ephy_tab_get_window (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (G_OBJECT (tab)), NULL);

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
	TabLoadStatus status;

	g_return_if_fail (IS_EPHY_TAB (G_OBJECT (tab)));

	tab->priv->is_active = is_active;

	status = ephy_tab_get_load_status (tab);
	if (status == TAB_LOAD_COMPLETED)
	{
		ephy_tab_set_load_status (tab, TAB_LOAD_NONE);
	}
	ephy_tab_update_color (tab);
}

gboolean
ephy_tab_get_is_active (EphyTab *tab)
{
	g_return_val_if_fail (IS_EPHY_TAB (G_OBJECT (tab)), FALSE);

	return tab->priv->is_active;
}

gboolean
ephy_tab_get_visibility (EphyTab *tab)
{
	return tab->priv->visibility;
}

void
ephy_tab_get_size (EphyTab *tab, int *width, int *height)
{
	*width = tab->priv->width;
	*height = tab->priv->height;
}

static void
ephy_tab_set_visibility (EphyTab *tab,
                         gboolean visible)
{
	g_return_if_fail (tab->priv->window != NULL);

	tab->priv->visibility = visible;
}

static void
ephy_tab_set_favicon (EphyTab *tab,
		      GdkPixbuf *favicon)
{
	GtkWidget *nb;
	EphyBookmarks *eb;

	nb = ephy_window_get_notebook (tab->priv->window);
	ephy_notebook_set_page_icon (EPHY_NOTEBOOK (nb),
				     GTK_WIDGET (tab->priv->embed),
				     favicon);

	if (!tab->priv->is_active) return;

	eb = ephy_shell_get_bookmarks (ephy_shell);
	ephy_bookmarks_set_icon (eb, tab->priv->location,
			         tab->priv->favicon_url);
	ephy_window_update_control (tab->priv->window,
				    FaviconControl);
}

/* Private callbacks for embed signals */

static void
ephy_tab_favicon_cache_changed_cb (EphyFaviconCache *cache,
				   char *url,
				   EphyTab *tab)
{
	GdkPixbuf *pixbuf = NULL;

	/* is this for us? */
	if (strncmp (tab->priv->favicon_url, url, 255) != 0) return;

	/* set favicon */
	pixbuf = ephy_favicon_cache_get (cache, tab->priv->favicon_url);
	ephy_tab_set_favicon (tab, pixbuf);

	if (pixbuf) g_object_unref (pixbuf);
}

static void
ephy_tab_favicon_cb (EphyEmbed *embed,
		     const char *url,
		     EphyTab *tab)
{
	EphyFaviconCache *cache;
	GdkPixbuf *pixbuf = NULL;

	g_strlcpy (tab->priv->favicon_url,
		   url, 255);

	cache = ephy_embed_shell_get_favicon_cache
		(EPHY_EMBED_SHELL (ephy_shell));

	if (url && url[0] != '\0')
	{
		pixbuf = ephy_favicon_cache_get (cache, tab->priv->favicon_url);
		ephy_tab_set_favicon (tab, pixbuf);
		if (pixbuf) g_object_unref (pixbuf);
	}
	else
	{
		ephy_tab_set_favicon (tab, NULL);
	}
}

static void
ephy_tab_link_message_cb (EphyEmbed *embed,
			  const char *message,
			  EphyTab *tab)
{
	if (!tab->priv->is_active) return;

	g_free (tab->priv->link_message);
	tab->priv->link_message = g_strdup (message);

	ephy_window_update_control (tab->priv->window,
				    StatusbarMessageControl);
}

static void
ephy_tab_location_cb (EphyEmbed *embed, EphyTab *tab)
{
	if (tab->priv->location) g_free (tab->priv->location);
	ephy_embed_get_location (embed, TRUE,
				 &tab->priv->location);
	tab->priv->link_message = NULL;
	tab->priv->favicon_url[0] = '\0';
	ephy_tab_set_favicon (tab, NULL);

	if (tab->priv->is_active)
	{
		ephy_window_update_control (tab->priv->window, LocationControl);
		ephy_window_update_control (tab->priv->window, NavControl);
		ephy_window_update_control (tab->priv->window, FaviconControl);
	}
}

static void
ephy_tab_zoom_changed_cb (EphyEmbed *embed, gint zoom, EphyTab *tab)
{
	if (tab->priv->is_active)
	{
		ephy_window_update_control (tab->priv->window, ZoomControl);
	}
}

static void
ephy_tab_set_title (EphyTab *tab, const char *title)
{
	GtkWidget *nb;

	nb = ephy_window_get_notebook (tab->priv->window);
	ephy_notebook_set_page_title (EPHY_NOTEBOOK (nb),
				      GTK_WIDGET (tab->priv->embed),
				      title);
}

static void
ephy_tab_title_cb (EphyEmbed *embed, EphyTab *tab)
{
	if (tab->priv->title) g_free (tab->priv->title);
	ephy_embed_get_title (embed, &tab->priv->title);

	if (*(tab->priv->title) == '\0')
	{
		g_free (tab->priv->title);
		tab->priv->title = g_strdup (_("Untitled"));
	}

	ephy_tab_set_title (tab, tab->priv->title);

	if (tab->priv->is_active)
	{
		ephy_window_update_control (tab->priv->window,
					    TitleControl);
	}
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

static void
build_net_state_message (char *message,
			 const char *uri,
			 EmbedState flags)
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
                else if (flags & EMBED_STATE_STOP)
                {
			msg = _("Done.");
                }
        }

	if (msg)
	{
		g_snprintf (message, 255, msg, host);
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
		if (load_percent > tab->priv->load_percent)
		{
			tab->priv->load_percent = load_percent;
		}
	}
}

static void
ensure_location (EphyTab *tab, const char *uri)
{
	if (tab->priv->location == NULL)
	{
		tab->priv->location = g_strdup (uri);
		ephy_window_update_control (tab->priv->window,
					      LocationControl);
	}
}

static void
ephy_tab_net_state_cb (EphyEmbed *embed, const char *uri,
			 EmbedState state, EphyTab *tab)
{
	build_net_state_message (tab->priv->status_message, uri, state);

	ephy_window_update_control (tab->priv->window,
				      StatusbarMessageControl);

	if (state & EMBED_STATE_IS_NETWORK)
	{
		if (state & EMBED_STATE_START)
		{
			tab->priv->total_requests = 0;
			tab->priv->cur_requests = 0;
			tab->priv->load_percent = 0;

			ensure_location (tab, uri);

			ephy_tab_set_load_status (tab, TAB_LOAD_STARTED);

			ephy_window_update_control (tab->priv->window,
						      NavControl);
			ephy_window_update_control (tab->priv->window,
						      SpinnerControl);
			ephy_tab_update_color (tab);
		}
		else if (state & EMBED_STATE_STOP)
		{
			tab->priv->load_percent = 0;

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
	gboolean open_in_tab;

	open_in_tab = (chromemask & EMBED_CHROME_OPENASCHROME) ?
		      FALSE :
		      eel_gconf_get_boolean (CONF_TABS_TABBED_POPUPS);

	if (open_in_tab)
	{
		window = ephy_tab_get_window (tab);
	}
	else
	{
		window = ephy_window_new ();
		ephy_window_set_chrome (window, chromemask);
	}

	new_tab = ephy_tab_new ();
        ephy_window_add_tab (window, new_tab, TRUE, FALSE);

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
	EphyWindow *window;

	if (visibility)
	{
		gtk_widget_show (GTK_WIDGET(embed));
	}
	else
	{
		gtk_widget_hide (GTK_WIDGET(embed));
	}

	ephy_tab_set_visibility (tab, visibility);

	window = ephy_tab_get_window (tab);
	g_return_if_fail (window != NULL);

	ephy_window_update_control (window, WindowVisibilityControl);
}

static void
ephy_tab_destroy_brsr_cb (EphyEmbed *embed, EphyTab *tab)
{
	EphyWindow *window;

	window = ephy_tab_get_window (tab);

	ephy_window_remove_tab (window, tab);

	ephy_embed_shell_remove_embed (EPHY_EMBED_SHELL (ephy_shell),
				       tab->priv->embed);
}

static gint
ephy_tab_open_uri_cb (EphyEmbed *embed, const char *uri,
			EphyTab *tab)
{
	return FALSE;
}

static void
ephy_tab_size_to_cb (EphyEmbed *embed, gint width, gint height,
		       EphyTab *tab)
{
	GList *tabs;
	EphyWindow *window;
	GtkWidget *widget;
	EmbedChromeMask chromemask;

	tab->priv->width = width;
	tab->priv->height = height;

	window = ephy_tab_get_window (tab);
	tabs = (GList *) ephy_window_get_tabs (window);
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

	window = ephy_tab_get_window (tab);

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
ephy_tab_dom_mouse_down_cb  (EphyEmbed *embed,
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

	if (button == 2)
	{
		ephy_tab_show_embed_popup (tab, event);
	}
	else if (button == 1
		 && (context & EMBED_CONTEXT_LINK))
	{
		const GValue *value;

		ephy_embed_event_get_property (event, "link", &value);
		ephy_shell_new_tab (ephy_shell, window, tab,
				      g_value_get_string (value), 0);
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

static void
ephy_tab_security_change_cb (EphyEmbed *embed, EmbedSecurityLevel level,
			       EphyTab *tab)
{
	if (!tab->priv->is_active) return;

	ephy_window_update_control (tab->priv->window,
				      StatusbarSecurityControl);
}

TabLoadStatus
ephy_tab_get_load_status (EphyTab *tab)
{
	return tab->priv->load_status;
}

int
ephy_tab_get_load_percent (EphyTab *tab)
{
	return tab->priv->load_percent;
}

const char *
ephy_tab_get_status_message (EphyTab *tab)
{
	if (tab->priv->link_message)
	{
		return tab->priv->link_message;
	}
	else
	{
		return tab->priv->status_message;
	}
}

const char *
ephy_tab_get_title (EphyTab *tab)
{
	if (tab->priv->title &&
	    g_utf8_strlen(tab->priv->title, -1))
	{
		return tab->priv->title;
	}
	else
	{
		return _("Untitled");
	}
}

const char *
ephy_tab_get_location (EphyTab *tab)
{
	return tab->priv->location;
}

const char *
ephy_tab_get_favicon_url (EphyTab *tab)
{
	if (tab->priv->favicon_url[0] == '\0')
	{
		return NULL;
	}
	else
	{
		return tab->priv->favicon_url;
	}
}

void
ephy_tab_set_location (EphyTab *tab,
		       char *location)
{
	if (tab->priv->location) g_free (tab->priv->location);
	tab->priv->location = g_strdup (location);
}

void
ephy_tab_update_control (EphyTab *tab,
			   TabControlID id)
{
	switch (id)
	{
		case TAB_CONTROL_TITLE:
			ephy_tab_set_title (tab, tab->priv->title);
			break;
	}
}
