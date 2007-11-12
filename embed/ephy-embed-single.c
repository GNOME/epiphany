 /*
 *  Copyright © 2000-2003 Marco Pesenti Gritti
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

#include "ephy-embed-single.h"
#include "ephy-embed-type-builtins.h"
#include "ephy-marshal.h"
#include "ephy-signal-accumulator.h"

static void ephy_embed_single_iface_init (gpointer g_class);

GType
ephy_embed_single_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyEmbedSingleIface),
			ephy_embed_single_iface_init,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EphyEmbedSingle",
					       &our_info,
					       (GTypeFlags) 0);
	}

	return type;
}

static void
ephy_embed_single_iface_init (gpointer g_iface)
{
	static gboolean initialised = FALSE;

	if (initialised == FALSE)
	{
/**
 * EphyEmbedSingle::new-window:
 * @single:
 * @parent_embed: the #EphyEmbed requesting the new window, or %NULL
 * @mask: a #EphyEmbedChrome
 *
 * The ::new_window signal is emitted when a new window needs to be opened.
 * For example, when a JavaScript popup window was opened.
 *
 * Return a new #EphyEmbed.
 **/
	g_signal_new ("new-window",
		      EPHY_TYPE_EMBED_SINGLE,
		      G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (EphyEmbedSingleIface, new_window),
		      ephy_signal_accumulator_object, ephy_embed_get_type,
		      ephy_marshal_OBJECT__OBJECT_FLAGS,
		      GTK_TYPE_WIDGET,
		      2,
		      GTK_TYPE_WIDGET,
		      EPHY_TYPE_EMBED_CHROME);

/**
 * EphyEmbedSingle::handle_content:
 * @single:
 * @mime_type: the MIME type of the content
 * @address: the URL to the content
 *
 * The ::handle_content signal is emitted when encountering content of a mime
 * type Epiphany is unable to handle itself.
 *
 * If a connected callback returns %TRUE, the signal will stop propagating. For
 * example, this could be used by a download manager to prevent other
 * ::handle_content listeners from being called.
 **/
	g_signal_new ("handle_content",
		      EPHY_TYPE_EMBED_SINGLE,
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (EphyEmbedSingleIface, handle_content),
		      g_signal_accumulator_true_handled, NULL,
		      ephy_marshal_BOOLEAN__STRING_STRING,
		      G_TYPE_BOOLEAN,
		      2,
		      G_TYPE_STRING,
		      G_TYPE_STRING);

/**
 * EphyEmbedSingle::add-sidebar:
 * @single:
 * @url: The url of the sidebar to be added
 * @title: The title of the sidebar to be added
 *
 * The ::add-sidebar signal is emitted when the user clicks a javascript link that
 * requests adding a url to the sidebar.
 **/
	g_signal_new ("add-sidebar",
		      EPHY_TYPE_EMBED_SINGLE,
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (EphyEmbedSingleIface, add_sidebar),
		      g_signal_accumulator_true_handled, NULL,
		      ephy_marshal_BOOLEAN__STRING_STRING,
		      G_TYPE_BOOLEAN,
		      2,
		      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
		      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

/**
 * EphyEmbedSingle::add-search-engine
 * @single:
 * @url: The url of the search engine definition file
 * @icon_url: The url of the icon to use for this engine
 * @title: The title of the search engine to be added
 *
 * The ::add-search-engine signal is emitted when the user clicks a javascript link that
 * requests adding a search engine to the sidebar.
 **/
	g_signal_new ("add-search-engine",
		      EPHY_TYPE_EMBED_SINGLE,
		      G_SIGNAL_RUN_LAST,
		      G_STRUCT_OFFSET (EphyEmbedSingleIface, add_search_engine),
		      g_signal_accumulator_true_handled, NULL,
		      ephy_marshal_BOOLEAN__STRING_STRING_STRING,
		      G_TYPE_BOOLEAN,
		      3,
		      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
		      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
		      G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

/**
 * EphyEmbedSingle::network-status:
 * 
 * Whether the network is on-line.
 */
	g_object_interface_install_property
		(g_iface,
		 g_param_spec_boolean ("network-status",
				       "network-status",
				       "network-status",
				       FALSE,
				       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	initialised = TRUE;
	}
}

/**
 * ephy_embed_single_init:
 * @single: the #EphyEmbedSingle
 * 
 * Performs startup initialisations. Must be called before calling
 * any other methods.
 **/
gboolean
ephy_embed_single_init (EphyEmbedSingle *single)
{
	EphyEmbedSingleIface *iface = EPHY_EMBED_SINGLE_GET_IFACE (single);
	return iface->init (single);
}

/**
 * ephy_embed_single_clear_cache:
 * @single: the #EphyEmbedSingle
 * 
 * Clears the Mozilla cache (temporarily saved web pages).
 **/
void
ephy_embed_single_clear_cache (EphyEmbedSingle *single)
{
	EphyEmbedSingleIface *iface = EPHY_EMBED_SINGLE_GET_IFACE (single);
	iface->clear_cache (single);
}

/**
 * ephy_embed_single_clear_auth_cache:
 * @single: the #EphyEmbedSingle
 * 
 * Clears the Mozilla HTTP authentication cache.
 *
 * This does not clear regular website passwords; it only clears the HTTP
 * authentication cache. Websites which use HTTP authentication require the
 * browser to send a password along with every HTTP request; the browser will
 * ask the user for the password once and then cache the password for subsequent
 * HTTP requests. This function will clear the HTTP authentication cache,
 * meaning the user will have to re-enter a username and password the next time
 * Epiphany requests a web page secured with HTTP authentication.
 **/
void
ephy_embed_single_clear_auth_cache (EphyEmbedSingle *single)
{
	EphyEmbedSingleIface *iface = EPHY_EMBED_SINGLE_GET_IFACE (single);
	iface->clear_auth_cache (single);
}

/**
 * ephy_embed_single_get_nework_status:
 * @single: the #EphyEmbedSingle
 * @offline: %TRUE if the network is on-line
 * 
 * Sets the state of the network connection.
 **/
void
ephy_embed_single_set_network_status (EphyEmbedSingle *single,
				      gboolean status)
{
	EphyEmbedSingleIface *iface = EPHY_EMBED_SINGLE_GET_IFACE (single);
	iface->set_network_status (single, status);
}

/**
 * ephy_embed_single_get_network_status:
 * @single: the #EphyEmbedSingle
 * 
 * Gets the state of the network connection.
 * 
 * Returns: %TRUE iff the network is on-line.
 **/
gboolean
ephy_embed_single_get_network_status (EphyEmbedSingle *single)
{
	EphyEmbedSingleIface *iface = EPHY_EMBED_SINGLE_GET_IFACE (single);
	return iface->get_network_status (single);
}

/**
 * ephy_embed_single_get_font_list:
 * @single: the #EphyEmbedSingle
 * @lang_group: a mozilla font language group name, or %NULL
 * 
 * Returns the list of fonts matching @lang_group, or all fonts if @lang_group
 * is %NULL.
 *
 * The available @lang_group arguments are listed in Epiphany's Fonts and Colors
 * preferences.
 * 
 * Return value: a list of font names
 **/
GList *
ephy_embed_single_get_font_list (EphyEmbedSingle *single,
				 const char *lang_group)
{
	EphyEmbedSingleIface *iface = EPHY_EMBED_SINGLE_GET_IFACE (single);
	return iface->get_font_list (single, lang_group);
}

/**
 * ephy_embed_single_open_window:
 * @single: the #EphyEmbedSingle
 * @parent: the requested window's parent #EphyEmbed
 * @address: the URL to load
 * @name: a name for the window
 * @features: a Javascript features string
 *
 * Opens a new window, as if it were opened in @parent using the Javascript
 * method and arguments: <code>window.open(&quot;@address&quot;,
 * &quot;_blank&quot;, &quot;@features&quot;);</code>.
 * 
 * Returns: the new embed. This is either a #EphyEmbed, or, when @features specified
 * "chrome", a #GtkMozEmbed.
 *
 * NOTE: Use ephy_shell_new_tab() unless this handling of the @features string is
 * required.
 */
GtkWidget *
ephy_embed_single_open_window (EphyEmbedSingle *single,
			       EphyEmbed *parent,
			       const char *address,
			       const char *name,
			       const char *features)
{
	EphyEmbedSingleIface *iface = EPHY_EMBED_SINGLE_GET_IFACE (single);
	return iface->open_window (single, parent, address, name, features);
}

/**
 * ephy_embed_single_get_backend_name
 * @single: the #EphyEmbedSingle
 *
 * Can be used to find the particular backend that is being used
 * for rendering pages.
 * 
 * Returns: the name of the backend. Valid returns are "gecko-1.7", 
 * "gecko-1.8", "gecko-1.9" and "WebKit"
 */
const char *
ephy_embed_single_get_backend_name (EphyEmbedSingle *single)
{
	EphyEmbedSingleIface *iface = EPHY_EMBED_SINGLE_GET_IFACE (single);
	return iface->get_backend_name (single);
}
