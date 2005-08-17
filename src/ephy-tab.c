/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004, 2005 Christian Persch
 *  Copyright (C) 2004 Crispin Flowerday
 *  Copyright (C) 2004 Adam Hooper
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
#include <libgnomevfs/gnome-vfs-result.h>
#include <libgnomevfs/gnome-vfs-monitor.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <string.h>

#define EPHY_TAB_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_TAB, EphyTabPrivate))

#define MAX_HIDDEN_POPUPS 5

struct _EphyTabPrivate
{
	char *status_message;
	char *link_message;
	char *address;
	char *typed_address;
	char *title;
	char *loading_title;
	char *icon_address;
	GdkPixbuf *icon;
	int cur_requests;
	int total_requests;
	int width;
	int height;
	float zoom;
	GSList *hidden_popups;
	GSList *shown_popups;
	EphyTabNavigationFlags nav_flags;
	EphyEmbedDocumentType document_type;
	guint idle_resize_handler;

	gint8 load_percent;
	/* Flags */
	guint is_blank : 1;
	guint visibility : 1;
	guint is_loading : 1;
	guint is_setting_zoom : 1;
	EphyEmbedSecurityLevel security_level;
	/* guint security_level : 3; ? */
	EphyTabAddressExpire address_expire;
	/* guint address_expire : 2; ? */

	/* File watch */
	GnomeVFSMonitorHandle *monitor;
	guint reload_scheduled_id;	
};

static void ephy_tab_class_init		(EphyTabClass *klass);
static void ephy_tab_init		(EphyTab *tab);
static void ephy_tab_dispose		(GObject *object);
static void ephy_tab_finalize		(GObject *object);

enum
{
	PROP_0,
	PROP_ADDRESS,
	PROP_DOCUMENT_TYPE,
	PROP_ICON,
	PROP_ICON_ADDRESS,
	PROP_LOAD_PROGRESS,
	PROP_LOAD_STATUS,
	PROP_MESSAGE,
	PROP_NAVIGATION,
	PROP_SECURITY,
	PROP_HIDDEN_POPUP_COUNT,
	PROP_POPUPS_ALLOWED,
	PROP_TITLE,
	PROP_TYPED_ADDRESS,
	PROP_VISIBLE,
	PROP_ZOOM
};

typedef struct
{
	char *url;
	char *features;
} PopupInfo;

static GObjectClass *parent_class = NULL;

/* internal functions, accessible only from this file */
static void	ephy_tab_set_load_status	(EphyTab *tab,
						 gboolean status);
static void	ephy_tab_set_link_message	(EphyTab *tab,
						 char *message);
static void	ephy_tab_set_load_percent	(EphyTab *tab,
						 int percent);
static void	ephy_tab_update_navigation_flags(EphyTab *tab,
						 EphyEmbed *embed);
static void	ephy_tab_set_security_level	(EphyTab *tab,
						 EphyEmbedSecurityLevel level);
static void	ephy_tab_set_title		(EphyTab *tab,
						 EphyEmbed *embed,
						 char *new_title);
static void	ephy_tab_set_zoom		(EphyTab *tab,
						 float zoom);
static guint	popup_blocker_n_hidden		(EphyTab *tab);
static gboolean	ephy_tab_get_popups_allowed	(EphyTab *tab);
static void	ephy_tab_set_popups_allowed	(EphyTab *tab,
						 gboolean allowed);
static void	ephy_tab_file_monitor_cancel	(EphyTab *tab);

/* Class functions */

GType
ephy_tab_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0))
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
		static const GInterfaceInfo link_info = 
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
ephy_tab_set_property (GObject *object,
		       guint prop_id,
		       const GValue *value,
		       GParamSpec *pspec)
{
	EphyTab *tab = EPHY_TAB (object);

	switch (prop_id)

	{
		case PROP_TYPED_ADDRESS:
			ephy_tab_set_typed_address (tab, g_value_get_string (value),
						    EPHY_TAB_ADDRESS_EXPIRE_NOW);
			break;
		case PROP_POPUPS_ALLOWED:
			ephy_tab_set_popups_allowed (tab, g_value_get_boolean (value));
			break;
		case PROP_ICON_ADDRESS:
			ephy_tab_set_icon_address (tab, g_value_get_string (value));
			break;
		case PROP_ADDRESS:
		case PROP_DOCUMENT_TYPE:
		case PROP_ICON:
		case PROP_LOAD_PROGRESS:
		case PROP_LOAD_STATUS:
		case PROP_MESSAGE:
		case PROP_NAVIGATION:
		case PROP_SECURITY:
		case PROP_HIDDEN_POPUP_COUNT:
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
	EphyTabPrivate *priv = tab->priv;

	switch (prop_id)
	{
		case PROP_ADDRESS:
			g_value_set_string (value, priv->address);
			break;
		case PROP_DOCUMENT_TYPE:
			g_value_set_enum (value, priv->document_type);
			break;
		case PROP_ICON:
			g_value_set_object (value, priv->icon);
			break;
		case PROP_ICON_ADDRESS:
			g_value_set_string (value, priv->icon_address);
			break;
		case PROP_LOAD_PROGRESS:
			g_value_set_int (value, priv->load_percent);
			break;
		case PROP_LOAD_STATUS:
			g_value_set_boolean (value, priv->is_loading);
			break;
		case PROP_MESSAGE:
			g_value_set_string (value, ephy_tab_get_status_message (tab));
			break;
		case PROP_NAVIGATION:
			g_value_set_flags (value, priv->nav_flags);
			break;
		case PROP_SECURITY:
			g_value_set_enum (value, priv->security_level);
			break;
		case PROP_HIDDEN_POPUP_COUNT:
			g_value_set_int (value, popup_blocker_n_hidden (tab));
			break;
		case PROP_POPUPS_ALLOWED:
			g_value_set_boolean
				(value, ephy_tab_get_popups_allowed (tab));
			break;
		case PROP_TITLE:
			g_value_set_string (value, priv->title);
			break;
		case PROP_TYPED_ADDRESS:
			g_value_set_string (value, ephy_tab_get_typed_address (tab));
			break;
		case PROP_VISIBLE:
			g_value_set_boolean (value, priv->visibility);
			break;
		case PROP_ZOOM:
			g_value_set_float (value, priv->zoom);
			break;
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

static EphyWindow *
ephy_tab_get_window (EphyTab *tab)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (tab));
	g_return_val_if_fail (toplevel != NULL, NULL);

	return EPHY_WINDOW (toplevel);
}

static void
ephy_tab_class_init (EphyTabClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);

        parent_class = g_type_class_peek_parent (class);

	object_class->dispose = ephy_tab_dispose;
	object_class->finalize = ephy_tab_finalize;
	object_class->get_property = ephy_tab_get_property;
	object_class->set_property = ephy_tab_set_property;

	widget_class->size_allocate = ephy_tab_size_allocate;
	widget_class->map = ephy_tab_map;
	widget_class->grab_focus = ephy_tab_grab_focus;

	g_object_class_install_property (object_class,
					 PROP_ADDRESS,
					 g_param_spec_string ("address",
							      "Address",
							      "The tab's address",
							      "",
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_DOCUMENT_TYPE,
					 g_param_spec_enum ("document-type",
							    "Document Type",
							    "The tab's documen type",
							    EPHY_TYPE_EMBED_DOCUMENT_TYPE,
							    EPHY_EMBED_DOCUMENT_HTML,
							    G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_ICON,
					 g_param_spec_object ("icon",
							      "Icon",
							      "The tab icon's",
							      GDK_TYPE_PIXBUF,
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_ICON_ADDRESS,
					 g_param_spec_string ("icon-address",
							      "Icon address",
							      "The tab icon's address",
							      NULL,
							      (G_PARAM_READABLE | G_PARAM_WRITABLE)));

	g_object_class_install_property (object_class,
					 PROP_LOAD_PROGRESS,
					 g_param_spec_int ("load-progress",
							   "Load progress",
							   "The tab's load progress in percent",
							   0,
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
					 g_param_spec_flags ("navigation",
							     "Navigation flags",
							     "The tab's navigation flags",
							     EPHY_TYPE_TAB_NAVIGATION_FLAGS,
							     0,
							     G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_SECURITY,
					 g_param_spec_enum ("security-level",
							    "Security Level",
							    "The tab's security level",
							    EPHY_TYPE_EMBED_SECURITY_LEVEL,
							    EPHY_EMBED_STATE_IS_UNKNOWN,
							     G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_HIDDEN_POPUP_COUNT,
					 g_param_spec_int ("hidden-popup-count",
							   "Number of Blocked Popups",
							   "The tab's number of blocked popup windows",
							    0,
							    G_MAXINT,
							    0,
							    G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_POPUPS_ALLOWED,
					 g_param_spec_boolean ("popups-allowed",
						 	       "Popups Allowed",
							       "Whether popup windows are to be displayed",
							       FALSE,
							       G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class,
					 PROP_TITLE,
					 g_param_spec_string ("title",
							      "Title",
							      "The tab's title",
							      _("Blank page"),
							      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_TYPED_ADDRESS,
					 g_param_spec_string ("typed-address",
							      "Typed Address",
							      "The typed address",
							      "",
							      G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (object_class,
					 PROP_VISIBLE,
					 g_param_spec_boolean ("visibility",
							       "Visibility",
							       "The tab's visibility",
							       TRUE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_ZOOM,
					 g_param_spec_float ("zoom",
							     "Zoom",
							     "The tab's zoom",
							     ZOOM_MINIMAL,
							     ZOOM_MAXIMAL,
							     1.0,
							     G_PARAM_READABLE));

	g_type_class_add_private (object_class, sizeof (EphyTabPrivate));
}

static void
popups_manager_free_info (PopupInfo *popup)
{
	g_free (popup->url);
	g_free (popup->features);
	g_free (popup);
}

static void
popups_manager_add (EphyTab *tab,
		    const char *url,
		    const char *features)
{
	PopupInfo *popup;

	LOG ("popups_manager_add: tab %p, url %s, features %s",
	     tab, url, features);

	g_return_if_fail (EPHY_IS_TAB (tab));

	popup = g_new0 (PopupInfo, 1);

	popup->url = g_strdup (url);
	popup->features = g_strdup (features);

	tab->priv->hidden_popups = g_slist_prepend
		(tab->priv->hidden_popups, popup);

	if (popup_blocker_n_hidden (tab) > MAX_HIDDEN_POPUPS) /* bug #160863 */
	{
		/* Remove the oldest popup */
		GSList *l = tab->priv->hidden_popups;

		while (l->next->next != NULL)
		{
			l = l->next;
		}

		popup = (PopupInfo *) l->next->data;
		popups_manager_free_info (popup);

		l->next = NULL;
	}
	else
	{
		g_object_notify (G_OBJECT (tab), "hidden-popup-count");
	}
}

static gboolean
popups_manager_remove_window (EphyTab *tab,
			      EphyWindow *window)
{
	tab->priv->shown_popups = g_slist_remove (tab->priv->shown_popups,
						  window);

	return FALSE;
}

static void
disconnect_popup (EphyWindow *window,
		  EphyTab *tab)
{
	g_signal_handlers_disconnect_by_func
		(window, G_CALLBACK (popups_manager_remove_window), tab);
}

static void
popups_manager_add_window (EphyTab *tab,
			   EphyWindow *window)
{
	LOG ("popups_manager_add_window: tab %p, window %p", tab, window);

	g_return_if_fail (EPHY_IS_TAB (tab));
	g_return_if_fail (EPHY_IS_WINDOW (window));

	tab->priv->shown_popups = g_slist_prepend
		(tab->priv->shown_popups, window);

	g_signal_connect_swapped (window, "destroy",
				  G_CALLBACK (popups_manager_remove_window),
				  tab);
}

static gboolean
ephy_tab_get_popups_allowed (EphyTab *tab)
{
	EphyPermissionManager *permission_manager;
	EphyPermission response;
	EphyEmbed *embed;
	char *location;
	gboolean allow;

	g_return_val_if_fail (EPHY_IS_TAB (tab), FALSE);

	permission_manager = EPHY_PERMISSION_MANAGER
		(ephy_embed_shell_get_embed_single (embed_shell));
	g_return_val_if_fail (EPHY_IS_PERMISSION_MANAGER (permission_manager),
			      FALSE);

	embed = ephy_tab_get_embed (tab);
	g_return_val_if_fail (EPHY_IS_EMBED (embed), FALSE);

	location = ephy_embed_get_location (embed, TRUE);
	if (location == NULL) return FALSE; /* FALSE, TRUE... same thing */

	response = ephy_permission_manager_test
		(permission_manager, location, EPT_POPUP);

	switch (response)
	{
		case EPHY_PERMISSION_ALLOWED:
			allow = TRUE;
			break;
		case EPHY_PERMISSION_DENIED:
			allow = FALSE;
			break;
		case EPHY_PERMISSION_DEFAULT:
		default:
			allow = eel_gconf_get_boolean
				(CONF_SECURITY_ALLOW_POPUPS);
			break;
	}

	g_free (location);

	LOG ("ephy_tab_get_popups_allowed: tab %p, allowed: %d", tab, allow);

	return allow;
}

static void
popups_manager_show (PopupInfo *popup,
		     EphyTab *tab)
{
	EphyEmbed *embed;
	EphyEmbedSingle *single;

	embed = ephy_tab_get_embed (tab);

	single = EPHY_EMBED_SINGLE
		(ephy_embed_shell_get_embed_single (embed_shell));

	ephy_embed_single_open_window (single, embed, popup->url, "",
				       popup->features);

	popups_manager_free_info (popup);
}

static void
popups_manager_show_all (EphyTab *tab)
{
	LOG ("popup_blocker_show_all: tab %p", tab);

	g_slist_foreach (tab->priv->hidden_popups,
			 (GFunc) popups_manager_show, tab);
	g_slist_free (tab->priv->hidden_popups);
	tab->priv->hidden_popups = NULL;

	g_object_notify (G_OBJECT (tab), "hidden-popup-count");
}

static char *
popups_manager_new_window_info (EphyWindow *window)
{
	EphyTab *tab;
	EphyEmbedChrome chrome;
	gboolean is_popup;
	char *features;

	g_object_get (window, "chrome", &chrome, "is-popup", &is_popup, NULL);
	g_return_val_if_fail (is_popup, g_strdup (""));

	tab = ephy_window_get_active_tab (window);
	g_return_val_if_fail (tab != NULL, g_strdup (""));

	features = g_strdup_printf
		("width=%d,height=%d,menubar=%d,status=%d,toolbar=%d",
		 tab->priv->width, tab->priv->height,
		 (chrome & EPHY_EMBED_CHROME_MENUBAR) > 0,
		 (chrome & EPHY_EMBED_CHROME_STATUSBAR) > 0,
		 (chrome & EPHY_EMBED_CHROME_TOOLBAR) > 0);

	return features;
}

static void
popups_manager_hide (EphyWindow *window,
		     EphyTab *parent_tab)
{
	EphyEmbed *embed;
	char *location;
	char *features;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (EPHY_IS_EMBED (embed));

	location = ephy_embed_get_location (embed, TRUE);
	if (location == NULL) return;

	features = popups_manager_new_window_info (window);

	popups_manager_add (parent_tab, location, features);

	gtk_widget_destroy (GTK_WIDGET (window));

	g_free (location);
	g_free (features);
}

static void
popups_manager_hide_all (EphyTab *tab)
{
	LOG ("popup_blocker_hide_all: tab %p", tab);

	g_slist_foreach (tab->priv->shown_popups,
			 (GFunc) popups_manager_hide, tab);
	g_slist_free (tab->priv->shown_popups);
	tab->priv->shown_popups = NULL;
}

static void
ephy_tab_set_popups_allowed (EphyTab *tab,
			     gboolean allowed)
{
	char *location;
	EphyEmbed *embed;
	EphyPermissionManager *manager;
	EphyPermission permission;

	g_return_if_fail (EPHY_IS_TAB (tab));

	embed = ephy_tab_get_embed (tab);

	location = ephy_embed_get_location (embed, TRUE);
	g_return_if_fail (location != NULL);

	manager = EPHY_PERMISSION_MANAGER
		(ephy_embed_shell_get_embed_single (embed_shell));
	g_return_if_fail (EPHY_IS_PERMISSION_MANAGER (manager));

	permission = allowed ? EPHY_PERMISSION_ALLOWED
			     : EPHY_PERMISSION_DENIED;

	ephy_permission_manager_add (manager, location, EPT_POPUP, permission);

	if (allowed)
	{
		popups_manager_show_all (tab);
	}
	else
	{
		popups_manager_hide_all (tab);
	}

	g_free (location);
}

static guint
popup_blocker_n_hidden (EphyTab *tab)
{
	return g_slist_length (tab->priv->hidden_popups);
}

static void
popups_manager_reset (EphyTab *tab)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	g_slist_foreach (tab->priv->hidden_popups,
			 (GFunc) popups_manager_free_info, NULL);
	g_slist_free (tab->priv->hidden_popups);
	tab->priv->hidden_popups = NULL;

	g_slist_foreach (tab->priv->shown_popups,
			 (GFunc) disconnect_popup, tab);
	g_slist_free (tab->priv->shown_popups);
	tab->priv->shown_popups = NULL;

	g_object_notify (G_OBJECT (tab), "hidden-popup-count");
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

	ephy_tab_file_monitor_cancel (tab);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
ephy_tab_finalize (GObject *object)
{
	EphyTab *tab = EPHY_TAB (object);
	EphyTabPrivate *priv = tab->priv;

	if (priv->icon != NULL)
	{
		g_object_unref (priv->icon);
		priv->icon = NULL;
	}

	g_free (priv->title);
	g_free (priv->address);
	g_free (priv->icon_address);
	g_free (priv->link_message);
	g_free (priv->status_message);
	popups_manager_reset (tab);

	G_OBJECT_CLASS (parent_class)->finalize (object);

	LOG ("EphyTab finalized %p", tab);
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


static char *
get_title_from_address (const char *address)
{
	GnomeVFSURI *uri;
	char *title;

	if (address == NULL) return NULL;
		
	uri = gnome_vfs_uri_new (address);
	if (uri == NULL) return g_strdup (address);
		
	title = gnome_vfs_uri_to_string (uri,
			GNOME_VFS_URI_HIDE_USER_NAME |
			GNOME_VFS_URI_HIDE_PASSWORD |
			GNOME_VFS_URI_HIDE_HOST_PORT |
			GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD |
			GNOME_VFS_URI_HIDE_FRAGMENT_IDENTIFIER);
	gnome_vfs_uri_unref (uri);

	return title;
}

static void
ephy_tab_set_loading_title (EphyTab *tab,
			    const char *title,
			    gboolean is_address)
{
	EphyTabPrivate *priv = tab->priv;
	char *freeme = NULL;

	g_free (priv->loading_title);
	priv->loading_title = NULL;

	if (title == NULL) return;

	if (is_address)
	{
		title = freeme = get_title_from_address (title);	
	}

	if (title != NULL && title[0] != '\0')
	{
		/* translators: %s here is the address of the web page */
		priv->loading_title = g_strdup_printf (_("Loading “%s”..."), title);
	}
	else
	{
		priv->loading_title = g_strdup (_("Loading..."));
	}

	g_free (freeme);
}

/* consumes |address| */
static void
ephy_tab_set_address (EphyTab *tab,
		      char *address)
{
	EphyTabPrivate *priv = tab->priv;
	GObject *object = G_OBJECT (tab);

	g_free (priv->address);
	priv->address = address;

	priv->is_blank = address == NULL ||
			 strcmp (address, "about:blank") == 0;

	if (priv->is_loading &&
	    priv->address_expire == EPHY_TAB_ADDRESS_EXPIRE_NOW &&
	    priv->typed_address != NULL)
	{
		g_free (priv->typed_address);
		priv->typed_address = NULL;

		g_object_notify (object, "typed-address");
	}

	g_object_notify (object, "address");
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

static void
ephy_tab_set_load_status (EphyTab *tab, gboolean status)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	status = status != FALSE;

	tab->priv->is_loading = status;

	g_object_notify (G_OBJECT (tab), "load-status");
}

/**
 * ephy_tab_get_document_type:
 * @tab: an #EphyTab
 *
 * Returns the type of the document loaded in @tab.
 *
 * Return value: the #EphyEmbedDocumentType
 **/
EphyEmbedDocumentType
ephy_tab_get_document_type (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), EPHY_EMBED_DOCUMENT_OTHER);

	return tab->priv->document_type;
}

/**
 * ephy_tab_get_load_status:
 * @tab: an #EphyTab
 *
 * Returns whether the web page in @tab has finished loading. A web page is
 * only finished loading after all images, styles, and other dependencies have
 * been downloaded and rendered.
 *
 * Return value: %TRUE if the page is still loading, %FALSE if complete
 **/
gboolean
ephy_tab_get_load_status (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), FALSE);

	return tab->priv->is_loading;
}

static void
ephy_tab_set_link_message (EphyTab *tab, char *message)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	g_free (tab->priv->link_message);
	tab->priv->link_message = ephy_string_blank_chr (message);

	g_object_notify (G_OBJECT (tab), "message");
}

/**
 * ephy_tab_get_link_message:
 * @tab: an #EphyTab
 *
 * Returns the message displayed in @tab's #EphyWindow's #EphyStatusbar when
 * the user hovers the mouse over a hyperlink.
 *
 * The message returned has a limited lifetime, and so should be copied with
 * g_strdup() if it must be stored.
 *
 * Listen to the "link_message" signal on the @tab's #EphyEmbed to be notified
 * when the link message changes.
 *
 * Return value: The current link statusbar message
 **/
const char *
ephy_tab_get_link_message (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), NULL);

	return tab->priv->link_message;
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

/**
 * ephy_tab_get_visibility:
 * @tab: an #EphyTab
 *
 * FIXME: Nobody really knows what this does. Someone must investigate.
 *
 * Return value; %TRUE if @tab's "visibility" property is set
 **/
gboolean
ephy_tab_get_visibility (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), FALSE);

	return tab->priv->visibility;
}

static void
ephy_tab_load_icon (EphyTab *tab)
{
	EphyTabPrivate *priv = tab->priv;
	EphyEmbedShell *shell;
	EphyFaviconCache *cache;

	if (priv->icon_address == NULL || priv->icon != NULL) return;

	shell = ephy_embed_shell_get_default ();
	cache = EPHY_FAVICON_CACHE (ephy_embed_shell_get_favicon_cache (shell));

	/* ephy_favicon_cache_get returns a reference already */
	priv->icon = ephy_favicon_cache_get (cache, priv->icon_address);

	g_object_notify (G_OBJECT (tab), "icon");
}

static void
ephy_tab_icon_cache_changed_cb (EphyFaviconCache *cache,
				const char *address,
				EphyTab *tab)
{
	EphyTabPrivate *priv = tab->priv;

	g_return_if_fail (address != NULL);

	/* is this for us? */
	if (priv->icon_address != NULL &&
	    strcmp (priv->icon_address, address) == 0)
	{
		ephy_tab_load_icon (tab);
	}
}

void
ephy_tab_set_icon_address (EphyTab *tab,
			   const char *address)
{
	GObject *object = G_OBJECT (tab);
	EphyTabPrivate *priv = tab->priv;
	EphyBookmarks *eb;
	EphyHistory *history;

	g_free (priv->icon_address);
	priv->icon_address = g_strdup (address);

	if (priv->icon != NULL)
	{
		g_object_unref (priv->icon);
		priv->icon = NULL;

		g_object_notify (object, "icon");
	}

	if (priv->icon_address)
	{
		eb = ephy_shell_get_bookmarks (ephy_shell);
		history = EPHY_HISTORY
			(ephy_embed_shell_get_global_history (embed_shell));
		ephy_bookmarks_set_icon (eb, priv->address,
				         priv->icon_address);
		ephy_history_set_icon (history, priv->address,
				       priv->icon_address);

		ephy_tab_load_icon (tab);
	}

	g_object_notify (object, "icon-address");
}

static void
ephy_tab_file_monitor_cancel (EphyTab *tab)
{
	EphyTabPrivate *priv = tab->priv;

	if (priv->monitor != NULL)
	{
		LOG ("Cancelling file monitor");

		gnome_vfs_monitor_cancel (priv->monitor);
		priv->monitor = NULL;
	}

	if (priv->reload_scheduled_id != 0)
	{
		LOG ("Cancelling scheduled reload");

		g_source_remove (priv->reload_scheduled_id);
		priv->reload_scheduled_id = 0;
	}
}

static gboolean
ephy_file_monitor_reload_cb (EphyTab *tab)
{
	EphyTabPrivate *priv = tab->priv;
	EphyEmbed *embed;

	LOG ("Reloading file '%s'\n", priv->address);

	priv->reload_scheduled_id = 0;

	embed = ephy_tab_get_embed (tab);
	if (embed == NULL) return FALSE;

	ephy_embed_reload (embed, TRUE);

	/* don't run again */
	return FALSE;
}

static void
ephy_tab_file_monitor_cb (GnomeVFSMonitorHandle *handle,
			  const gchar *monitor_uri,
			  const gchar *info_uri,
			  GnomeVFSMonitorEventType event_type,
			  EphyTab *tab)
{
	EphyTabPrivate *priv = tab->priv;

	LOG ("File '%s' has changed, scheduling reload", monitor_uri);

	switch (event_type)
	{
		case GNOME_VFS_MONITOR_EVENT_CHANGED:
		case GNOME_VFS_MONITOR_EVENT_CREATED:
			/* We make a lot of assumptions here, but basically we know
			 * that we just have to reload, by construction.
			 * Delay the reload a little bit so we don't endlessly
			 * reload while a file is written.
			 */
			if (priv->reload_scheduled_id != 0)
			{
				g_source_remove (priv->reload_scheduled_id);
			}

			priv->reload_scheduled_id =
				g_timeout_add (100 /* ms */,
					       (GSourceFunc) ephy_file_monitor_reload_cb,
					       tab);
			break;

		case GNOME_VFS_MONITOR_EVENT_DELETED:
		case GNOME_VFS_MONITOR_EVENT_STARTEXECUTING:
		case GNOME_VFS_MONITOR_EVENT_STOPEXECUTING:
		case GNOME_VFS_MONITOR_EVENT_METADATA_CHANGED:
		default:
			break;
	}
}

static void
ephy_tab_update_file_monitor (EphyTab *tab,
			      const gchar *address)
{
	EphyTabPrivate *priv = tab->priv;
	GnomeVFSMonitorHandle *handle = NULL;
	GnomeVFSURI *uri;
	gboolean local;

	if (priv->address != NULL && address != NULL &&
	    strcmp (priv->address, address) == 0)
	{
		/* same address, no change needed */
		return;
	}

	ephy_tab_file_monitor_cancel (tab);

	uri = gnome_vfs_uri_new (address);
	if (uri == NULL) return;

	local = gnome_vfs_uri_is_local (uri);
	gnome_vfs_uri_unref (uri);

	if (local == FALSE) return;
	
	if (gnome_vfs_monitor_add (&handle, address,
				   GNOME_VFS_MONITOR_FILE,
				   (GnomeVFSMonitorCallback) ephy_tab_file_monitor_cb,
				   tab) == GNOME_VFS_OK)
	{
		LOG ("Installed monitor for file '%s'", address);

		priv->monitor = handle;
	}
}

/**
 * ephy_tab_get_icon:
 * @tab: an #EphyTab
 *
 * Returns the tab's site icon as a #GdkPixbuf,
 * or %NULL if it is not available.
 *
 * Return value: a the tab's site icon
 **/
GdkPixbuf *
ephy_tab_get_icon (EphyTab *tab)
{
	EphyTabPrivate *priv;

	g_return_val_if_fail (EPHY_IS_TAB (tab), NULL);

	priv = tab->priv;

	return priv->icon;
}

/**
 * ephy_tab_get_icon_address:
 * @tab: an #EphyTab
 *
 * Returns a URL which points to @tab's site icon.
 *
 * Return value: the URL of @tab's site icon
 **/
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

static gboolean
ephy_tab_open_uri_cb (EphyEmbed *embed,
		      const char *uri,
		      EphyTab *tab)
{
	EphyTabPrivate *priv = tab->priv;

	/* Set the address here if we have a blank page.
	 * See bug #147840.
	 */
	if (priv->is_blank)
	{
		ephy_tab_set_address (tab, g_strdup (uri));
		ephy_tab_set_loading_title (tab, uri, TRUE);
	}

	/* allow load to proceed */
	return FALSE;
}

static void
ephy_tab_address_cb (EphyEmbed *embed,
		     const char *address,
		     EphyTab *tab)
{
	GObject *object = G_OBJECT (tab);

	LOG ("ephy_tab_address_cb tab %p address %s", tab, address);

	g_object_freeze_notify (object);

	/* do this up here so we still have the old address around */
	ephy_tab_update_file_monitor (tab, address);

	/* Do not expose about:blank to the user, an empty address
	   bar will do better */
	if (address == NULL || address[0] == '\0' ||
	    strcmp (address, "about:blank") == 0)
	{
		ephy_tab_set_address (tab, NULL);
		ephy_tab_set_title (tab, embed, NULL);
	}
	else
	{
		char *embed_address;

		/* we do this to get rid of an eventual password in the URL */
		embed_address = ephy_embed_get_location (embed, TRUE);
		ephy_tab_set_address (tab, embed_address);
		ephy_tab_set_loading_title (tab, embed_address, TRUE);
	}

	ephy_tab_set_link_message (tab, NULL);
	ephy_tab_set_icon_address (tab, NULL);
	ephy_tab_update_navigation_flags (tab, embed);

	g_object_notify (object, "title");

	g_object_thaw_notify (object);
}

static void
ephy_tab_content_change_cb (EphyEmbed *embed, const char *address, EphyTab *tab)
{
	EphyTabPrivate *priv = tab->priv;

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

		current_zoom = ephy_embed_get_zoom (embed);
		if (zoom != current_zoom)
		{
			priv->is_setting_zoom = TRUE;
			ephy_embed_set_zoom (embed, zoom);
			priv->is_setting_zoom = FALSE;
		}
	}

	popups_manager_reset (tab);
	g_object_notify (G_OBJECT (tab), "popups-allowed");
}

static void
ephy_tab_document_type_cb (EphyEmbed *embed,
			   EphyEmbedDocumentType type,
			   EphyTab *tab)
{
	if (tab->priv->document_type != type)
	{
		tab->priv->document_type = type;

		g_object_notify (G_OBJECT (tab), "document-type");
	}
}

static void
ephy_tab_zoom_changed_cb (EphyEmbed *embed, float zoom, EphyTab *tab)
{
	char *address;

	LOG ("ephy_tab_zoom_changed_cb tab %p zoom %f", tab, zoom);

	ephy_tab_set_zoom (tab, zoom);

	if (tab->priv->is_setting_zoom)
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

			zoom = ephy_embed_get_zoom (embed);

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
	GObject *object = G_OBJECT (tab);
	char *title;

	title = ephy_embed_get_title (embed);

	g_object_freeze_notify (object);

	ephy_tab_set_loading_title (tab, title, FALSE);

	/* this consumes and/or frees |title| ! */
	ephy_tab_set_title (tab, embed, title);

	g_object_thaw_notify (object);
}

static int
build_load_percent (int requests_done, int requests_total)
{
	int percent= 0;

	if (requests_total > 0)
	{
		percent = (requests_done * 100) / requests_total;

		/* Mozilla sometimes report more done requests than
		   total requests. Their progress widget clamp the value */
		percent = CLAMP (percent, 0, 100);
	}

	return percent;
}

static void
update_net_state_message (EphyTab *tab, const char *uri, EphyEmbedNetState flags)
{
	GnomeVFSURI *vfs_uri = NULL;
	const char *msg = NULL;
	const char *host = NULL;

	if (uri != NULL)
	{
		vfs_uri = gnome_vfs_uri_new (uri);
	}

	if (vfs_uri != NULL)
	{
		host = gnome_vfs_uri_get_host_name (vfs_uri);
	}

	if (host == NULL || host[0] == '\0') goto out;

	/* IS_REQUEST and IS_NETWORK can be both set */
	if (flags & EPHY_EMBED_STATE_IS_REQUEST)
	{
		if (flags & EPHY_EMBED_STATE_REDIRECTING)
		{
			msg = _("Redirecting to %s...");
		}
		else if (flags & EPHY_EMBED_STATE_TRANSFERRING)
		{
			msg = _("Transferring data from %s...");
		}
		else if (flags & EPHY_EMBED_STATE_NEGOTIATING)
		{
			msg = _("Waiting for authorization from %s...");
		}
	}

	if (flags & EPHY_EMBED_STATE_IS_NETWORK)
	{
		if (flags & EPHY_EMBED_STATE_START)
		{
			msg = _("Loading %s...");
		}
	}

	if ((flags & EPHY_EMBED_STATE_IS_NETWORK) &&
	    (flags & EPHY_EMBED_STATE_STOP))
	{
		g_free (tab->priv->status_message);
		tab->priv->status_message = NULL;
		g_object_notify (G_OBJECT (tab), "message");

	}
	else if (msg != NULL)
	{
		g_free (tab->priv->status_message);
		tab->priv->status_message = g_strdup_printf (msg, host);
		g_object_notify (G_OBJECT (tab), "message");
	}

out:
	if (vfs_uri != NULL)
	{
		gnome_vfs_uri_unref (vfs_uri);
	}
}

static void
build_progress_from_requests (EphyTab *tab, EphyEmbedNetState state)
{
	int load_percent;

	if (state & EPHY_EMBED_STATE_IS_REQUEST)
        {
                if (state & EPHY_EMBED_STATE_START)
                {
			tab->priv->total_requests ++;
		}
		else if (state & EPHY_EMBED_STATE_STOP)
		{
			tab->priv->cur_requests ++;
		}

		load_percent = build_load_percent (tab->priv->cur_requests,
						   tab->priv->total_requests);

		ephy_tab_set_load_percent (tab, load_percent);
	}
}

static void
ensure_page_info (EphyTab *tab, EphyEmbed *embed, const char *address)
{
	EphyTabPrivate *priv = tab->priv;

	if ((priv->address == NULL || priv->address[0] == '\0') &&
	    priv->address_expire == EPHY_TAB_ADDRESS_EXPIRE_NOW)
        {
		ephy_tab_set_address (tab, g_strdup (address));
	}

	/* FIXME huh?? */
	if (tab->priv->title == NULL || tab->priv->title[0] == '\0')
	{
		ephy_tab_set_title (tab, embed, NULL);
	}
}

static void
ephy_tab_net_state_cb (EphyEmbed *embed,
		       const char *uri,
		       EphyEmbedNetState state,
		       EphyTab *tab)
{
	EphyTabPrivate *priv = tab->priv;

	update_net_state_message (tab, uri, state);

	if (state & EPHY_EMBED_STATE_IS_NETWORK)
	{
		if (state & EPHY_EMBED_STATE_START)
		{
			GObject *object = G_OBJECT (tab);

			g_object_freeze_notify (object);

			priv->total_requests = 0;
			priv->cur_requests = 0;

			ephy_tab_set_load_percent (tab, 0);
			ephy_tab_set_load_status (tab, TRUE);
			ephy_tab_update_navigation_flags (tab, embed);

			ensure_page_info (tab, embed, uri);

			g_object_notify (object, "title");

			g_object_thaw_notify (object);
		}
		else if (state & EPHY_EMBED_STATE_STOP)
		{
			GObject *object = G_OBJECT (tab);

			g_object_freeze_notify (object);

			ephy_tab_set_load_percent (tab, 100);
			ephy_tab_set_load_status (tab, FALSE);
			ephy_tab_update_navigation_flags (tab, embed);

			g_free (priv->loading_title);
			priv->loading_title = NULL;

			priv->address_expire = EPHY_TAB_ADDRESS_EXPIRE_NOW;

			g_object_notify (object, "title");

			g_object_thaw_notify (object);
		}
	}

	build_progress_from_requests (tab, state);
}

static void
ephy_tab_new_window_cb (EphyEmbed *embed,
			EphyEmbed *new_embed,
			EphyTab *tab)
{
	EphyWindow *window;

	g_return_if_fail (new_embed != NULL);

	window = EPHY_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (new_embed)));
	g_return_if_fail (window != NULL);

	popups_manager_add_window (tab, window);
}

static void
ephy_tab_popup_blocked_cb (EphyEmbed *embed, const char *url,
			   const char *features, EphyTab *tab)
{
	popups_manager_add (tab, url, features);
}

static void
ephy_tab_visibility_cb (EphyEmbed *embed, gboolean visibility,
			EphyTab *tab)
{
	LOG ("ephy_tab_visibility_cb tab %p visibility %d",
	     tab, visibility);

	if (visibility)
	{
		gtk_widget_show (GTK_WIDGET (tab));
	}
	else
	{
		gtk_widget_hide (GTK_WIDGET (tab));
	}

	tab->priv->visibility = visibility;

	g_object_notify (G_OBJECT (tab), "visibility");
}

static void
ephy_tab_destroy_brsr_cb (EphyEmbed *embed, EphyTab *tab)
{
	EphyWindow *window;
	GtkWidget *notebook;

	g_return_if_fail (EPHY_IS_TAB (tab));

	LOG ("ephy_tab_destroy_browser_cb tab %p parent %p",
	     tab, ((GtkWidget *) tab)->parent);

	window = ephy_tab_get_window (tab);
	g_return_if_fail (window != NULL);

	/* Do not use ephy_window_remove_tab because it will
	   check for unsubmitted forms */
	notebook = ephy_window_get_notebook (window);
	ephy_notebook_remove_tab (EPHY_NOTEBOOK (notebook), tab);
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
		return ephy_link_open (EPHY_LINK (tab), link_address,
				       tab, EPHY_LINK_NEW_TAB) != NULL;
	}

	return FALSE;
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
		(ephy_embed_factory_new_object (EPHY_TYPE_EMBED_PERSIST));

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
	gboolean with_control, with_shift, is_left_click, is_middle_click;
	gboolean is_link, is_image, is_middle_clickable;
	gboolean middle_click_opens;
	gboolean is_input;

	g_return_val_if_fail (EPHY_IS_EMBED_EVENT(event), FALSE);

	button = ephy_embed_event_get_button (event);
	context = ephy_embed_event_get_context (event);
	modifier = ephy_embed_event_get_modifier (event);

	LOG ("ephy_tab_dom_mouse_click_cb: button %d, context %x, modifier %x",
	     button, context, modifier);

	with_control = (modifier & GDK_CONTROL_MASK) != 0;
	with_shift = (modifier & GDK_SHIFT_MASK) != 0;
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
ephy_tab_security_change_cb (EphyEmbed *embed, EphyEmbedSecurityLevel level,
			       EphyTab *tab)
{
	ephy_tab_set_security_level (tab, level);
}

static void
ephy_tab_init (EphyTab *tab)
{
	EphyTabPrivate *priv;
	GObject *embed;
	EphyFaviconCache *cache;

	LOG ("EphyTab initialising %p", tab);

	priv = tab->priv = EPHY_TAB_GET_PRIVATE (tab);

	tab->priv->total_requests = 0;
	tab->priv->cur_requests = 0;
	tab->priv->width = -1;
	tab->priv->height = -1;
	tab->priv->load_percent = 0;
	tab->priv->is_loading = FALSE;
	tab->priv->security_level = EPHY_EMBED_STATE_IS_UNKNOWN;
	tab->priv->document_type = EPHY_EMBED_DOCUMENT_HTML;
	tab->priv->zoom = 1.0;
	priv->title = NULL;
	priv->is_blank = TRUE;
	priv->icon_address = NULL;
	priv->icon = NULL;
	priv->address = NULL;
	priv->typed_address = NULL;
	priv->title = NULL;
	priv->loading_title = NULL;
	priv->address_expire = EPHY_TAB_ADDRESS_EXPIRE_NOW;

	embed = ephy_embed_factory_new_object (EPHY_TYPE_EMBED);
	g_assert (embed != NULL);

	gtk_container_add (GTK_CONTAINER (tab), GTK_WIDGET (embed));
	gtk_widget_show (GTK_WIDGET (embed));

	g_signal_connect_object (embed, "link_message",
				 G_CALLBACK (ephy_tab_link_message_cb),
				 tab, 0);
	g_signal_connect_object (embed, "ge_document_type",
				 G_CALLBACK (ephy_tab_document_type_cb),
				 tab, 0);
	g_signal_connect_object (embed, "open_uri",
				 G_CALLBACK (ephy_tab_open_uri_cb),
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
	g_signal_connect_object (embed, "ge_popup_blocked",
				 G_CALLBACK (ephy_tab_popup_blocked_cb),
				 tab, 0);
	g_signal_connect_object (embed, "visibility",
				 G_CALLBACK (ephy_tab_visibility_cb),
				 tab, 0);
	g_signal_connect_object (embed, "destroy_browser",
				 G_CALLBACK (ephy_tab_destroy_brsr_cb),
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
	g_signal_connect_object (embed, "ge_content_change",
				 G_CALLBACK (ephy_tab_content_change_cb),
				 tab, 0);

	cache = EPHY_FAVICON_CACHE
		(ephy_embed_shell_get_favicon_cache (EPHY_EMBED_SHELL (ephy_shell)));
	g_signal_connect_object (G_OBJECT (cache), "changed",
				 G_CALLBACK (ephy_tab_icon_cache_changed_cb),
				 tab,  0);
}

/**
 * ephy_tab_set_load_percent:
 * @tab: an #EphyTab
 * @percent: a percentage, from 0 to 100.
 *
 * Sets the load percentage. This will be displayed in the progressbar.
 **/
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

/**
 * ephy_tab_get_load_percent:
 * @tab: an #EphyTab
 *
 * Returns the page load percentage (displayed in the progressbar).
 *
 * Return value: a percentage from 0 to 100.
 **/
int
ephy_tab_get_load_percent (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), 0);

	return tab->priv->load_percent;
}

static void
ephy_tab_update_navigation_flags (EphyTab *tab, EphyEmbed *embed)
{
	EphyTabNavigationFlags flags = 0;

	if (ephy_embed_can_go_up (embed))
	{
		flags |= EPHY_TAB_NAV_UP;
	}

	if (ephy_embed_can_go_back (embed))
	{
		flags |= EPHY_TAB_NAV_BACK;
	}

	if (ephy_embed_can_go_forward (embed))
	{
		flags |= EPHY_TAB_NAV_FORWARD;
	}

	if (flags != tab->priv->nav_flags)
	{
		tab->priv->nav_flags = flags;

		g_object_notify (G_OBJECT (tab), "navigation");
	}
}

/**
 * ephy_tab_get_navigation_flags:
 * @tab: an #EphyTab
 *
 * Returns @tab's navigation flags.
 *
 * Return value: @tab's navigation flags
 **/
EphyTabNavigationFlags
ephy_tab_get_navigation_flags (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), 0);

	return tab->priv->nav_flags;
}

/**
 * ephy_tab_get_status_message:
 * @tab: an #EphyTab
 *
 * Returns the message displayed in @tab's #EphyWindow's #EphyStatusbar. If the
 * user is hovering the mouse over a hyperlink, this function will return the
 * same value as ephy_tab_get_link_message(). Otherwise, it will return a
 * network status message, or NULL.
 *
 * The message returned has a limited lifetime, and so should be copied with
 * g_strdup() if it must be stored.
 *
 * Listen to "notify::message" to be notified when the message property changes.
 *
 * Return value: The current statusbar message
 **/
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

static void
ephy_tab_set_title (EphyTab *tab,
		    EphyEmbed *embed,
		    char *title)
{
	EphyTabPrivate *priv = tab->priv;

	if (!priv->is_blank && (title == NULL || title[0] == '\0'))
	{

		g_free (title);
		title = get_title_from_address (priv->address);

		/* Fallback */
		if (title == NULL || title[0] == '\0')
		{
			g_free (title);
			title = NULL;
			priv->is_blank = TRUE;
		}
	}

	g_free (priv->title);
	priv->title = title;

	g_object_notify (G_OBJECT (tab), "title");
}

/**
 * ephy_tab_get_title:
 * @tab: an #EphyTab
 *
 * Returns the title of the web page loaded in @tab.
 *
 * Return value: @tab's loaded web page's title. Will never be %NULL.
 **/
const char *
ephy_tab_get_title (EphyTab *tab)
{
	EphyTabPrivate *priv;
	const char *title = "";

	g_return_val_if_fail (EPHY_IS_TAB (tab), NULL);

	priv = tab->priv;

	if (priv->is_blank)
	{
		title = _("Blank page");
	}
	else if (priv->is_loading &&
		 priv->loading_title != NULL)
	{
		title = priv->loading_title;
	}
	else
	{
		title = priv->title;
	}

	return title != NULL ? title : "";
}

/**
 * ephy_tab_get_address:
 * @tab: an #EphyTab
 *
 * Returns the address of the currently loaded page.
 *
 * Return value: @tab's address. Will never be %NULL.
 **/
const char *
ephy_tab_get_address (EphyTab *tab)
{
	EphyTabPrivate *priv;

	g_return_val_if_fail (EPHY_IS_TAB (tab), "");

	priv = tab->priv;

	return priv->address ? priv->address : "about:blank";
}

/**
 * ephy_tab_get_typed_address:
 * @tab: an #EphyTab
 *
 * Returns the text that @tab's #EphyWindow will display in its location toolbar
 * entry when @tab is selected.
 *
 * This is not guaranteed to be the same as @tab's #EphyEmbed's location,
 * available through ephy_embed_get_location(). As the user types a new address
 * into the location entry, ephy_tab_get_location()'s returned string will
 * change.
 *
 * Return value: @tab's #EphyWindow's location entry when @tab is selected
 **/
const char *
ephy_tab_get_typed_address (EphyTab *tab)
{
	EphyTabPrivate *priv;

	g_return_val_if_fail (EPHY_IS_TAB (tab), NULL);

	priv = tab->priv;

	return priv->typed_address;
}

/**
 * ephy_tab_set_typed_address:
 * @tab: an #EphyTab
 * @address: the new typed address, or %NULL to clear it
 * @expire: when to expire this address_expire
 * 
 * Sets the text that @tab's #EphyWindow will display in its location toolbar
 * entry when @tab is selected.
 **/
void
ephy_tab_set_typed_address (EphyTab *tab,
			    const char *address,
			    EphyTabAddressExpire expire)
{
	EphyTabPrivate *priv = tab->priv;

	g_free (priv->typed_address);
	priv->typed_address = g_strdup (address);

	if (expire == EPHY_TAB_ADDRESS_EXPIRE_CURRENT &&
	    !priv->is_loading)
	{
		priv->address_expire = EPHY_TAB_ADDRESS_EXPIRE_NOW;
	}
	else
	{
		priv->address_expire = expire;
	}

	g_object_notify (G_OBJECT (tab), "typed-address");
}

static void
ephy_tab_set_security_level (EphyTab *tab, EphyEmbedSecurityLevel level)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	tab->priv->security_level = level;

	g_object_notify (G_OBJECT (tab), "security-level");
}

/**
 * ephy_tab_get_security_level:
 * @tab: an #EphyTab
 *
 * Returns the security level of the webpage loaded in @tab.
 *
 * Return value: @tab's loaded page's security level
 **/
EphyEmbedSecurityLevel
ephy_tab_get_security_level (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), EPHY_EMBED_STATE_IS_UNKNOWN);

	return tab->priv->security_level;
}

static void
ephy_tab_set_zoom (EphyTab *tab, float zoom)
{
	g_return_if_fail (EPHY_IS_TAB (tab));

	tab->priv->zoom = zoom;

	g_object_notify (G_OBJECT (tab), "zoom");
}

/**
 * ephy_tab_get_zoom:
 * @tab: an #EphyTab
 *
 * Returns the zoom level of the web page loaded in @tab. A return value of
 * 1.0 corresponds to 100% zoom (normal size).
 *
 * Return value: @tab's loaded page's zoom level
 **/
float
ephy_tab_get_zoom (EphyTab *tab)
{
	g_return_val_if_fail (EPHY_IS_TAB (tab), 1.0);

	return tab->priv->zoom;
}
