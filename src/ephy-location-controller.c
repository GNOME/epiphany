/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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
#include "ephy-location-controller.h"

#include "ephy-widgets-type-builtins.h"
#include "ephy-completion-model.h"
#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-utils.h"
#include "ephy-link.h"
#include "ephy-dnd.h"
#include "ephy-location-entry.h"
#include "ephy-shell.h"
#include "ephy-title-box.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <string.h>

/**
 * SECTION:ephy-location-controller
 * @short_description: An #EphyLink implementation
 *
 * #EphyLocationController handles navigation together with #EphyLocationEntry
 */

#define EPHY_LOCATION_CONTROLLER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_LOCATION_CONTROLLER, EphyLocationControllerPrivate))

struct _EphyLocationControllerPrivate
{
	EphyWindow *window;
	EphyLocationEntry *location_entry;
	EphyTitleBox *title_box;
	GList *actions;
	char *address;
	EphyNode *smart_bmks;
	EphyBookmarks *bookmarks;
	GdkPixbuf *icon;
	guint editable : 1;
	guint show_icon : 1;
	gboolean sync_address_is_blocked;
};

static void ephy_location_controller_init	    (EphyLocationController *controller);
static void ephy_location_controller_class_init (EphyLocationControllerClass *class);
static void ephy_location_controller_finalize   (GObject *object);
static void user_changed_cb		    (GtkWidget *widget,
					     EphyLocationController *controller);
static void sync_address		    (EphyLocationController *controller,
					     GParamSpec *pspec,
					     GtkWidget *widget);

enum
{
	PROP_0,
	PROP_ADDRESS,
	PROP_EDITABLE,
	PROP_ICON,
	PROP_SHOW_ICON,
	PROP_WINDOW,
	PROP_LOCATION_ENTRY,
	PROP_TITLE_BOX
};

enum
{
	LOCK_CLICKED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (EphyLocationController, ephy_location_controller, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (EPHY_TYPE_LINK,
						NULL))

static gboolean
match_func (GtkEntryCompletion *completion,
	    const char *key,
	    GtkTreeIter *iter,
	    gpointer data)
{
	/* We want every row in the model to show up. */
	return TRUE;
}

static void
action_activated_cb (GtkEntryCompletion *completion,
		     gint index,
		     EphyLocationController *controller)
{
	GtkWidget *entry;
	char *content;

	entry = gtk_entry_completion_get_entry (completion);
	content = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	if (content)
	{
		EphyNode *node;
		const char *smart_url;
		char *url;

		node = (EphyNode *)g_list_nth_data (controller->priv->actions, index);
		smart_url = ephy_node_get_property_string
			(node, EPHY_NODE_BMK_PROP_LOCATION);
		g_return_if_fail (smart_url != NULL);

		url = ephy_bookmarks_resolve_address
			(controller->priv->bookmarks, smart_url, content);
		g_free (content);
		if (url == NULL) return;

		ephy_link_open (EPHY_LINK (controller), url, NULL,
				ephy_link_flags_from_current_event () | EPHY_LINK_TYPED);

		g_free (url);
	}
}

static void
entry_drag_data_received_cb (GtkWidget *widget,
			     GdkDragContext *context,
			     gint x, gint y,
			     GtkSelectionData *selection_data,
			     guint info,
			     guint time,
			     EphyLocationController *controller)
{
	GtkEntry *entry;
	GdkAtom url_type;
	GdkAtom text_type;
	const guchar *sel_data;

	sel_data = gtk_selection_data_get_data (selection_data);

	url_type = gdk_atom_intern (EPHY_DND_URL_TYPE, FALSE);
	text_type = gdk_atom_intern (EPHY_DND_TEXT_TYPE, FALSE);

	if (gtk_selection_data_get_length (selection_data) <= 0 || sel_data == NULL)
		return;

	entry = GTK_ENTRY (widget);

	if (gtk_selection_data_get_target (selection_data) == url_type)
	{
		char **uris;

		uris = g_uri_list_extract_uris ((char *)sel_data);
		if (uris != NULL && uris[0] != NULL && *uris[0] != '\0')
		{
			gtk_entry_set_text (entry, (char *)uris[0]);
			ephy_link_open (EPHY_LINK (controller),
					uris[0],
					NULL,
					ephy_link_flags_from_current_event ());
		}
		g_strfreev (uris);
	} else if (gtk_selection_data_get_target (selection_data) == text_type) {
		char *address;

		gtk_entry_set_text (entry, (const gchar *)sel_data);
		address = ephy_embed_utils_normalize_or_autosearch_address ((const gchar *)sel_data);
		ephy_link_open (EPHY_LINK (controller),
				address,
				NULL,
				ephy_link_flags_from_current_event ());
		g_free (address);
	}
}

static void
entry_activate_cb (GtkEntry *entry,
		   EphyLocationController *controller)
{
	EphyBookmarks *bookmarks;
	const char *content;
	char *address;
	char *effective_address;
	EphyLocationControllerPrivate *priv;

	priv = controller->priv;

	if (priv->sync_address_is_blocked)
	{
		priv->sync_address_is_blocked = FALSE;
		g_signal_handlers_unblock_by_func (controller, G_CALLBACK (sync_address), entry);
	}	

	content = gtk_entry_get_text (entry);
	if (content == NULL || content[0] == '\0') return;

	bookmarks = ephy_shell_get_bookmarks (ephy_shell_get_default ());

	address = ephy_bookmarks_resolve_address (bookmarks, content, NULL);
	g_return_if_fail (address != NULL);

	effective_address = ephy_embed_utils_normalize_or_autosearch_address (g_strstrip (address));
	g_free (address);
#if 0
	if (!ephy_embed_utils_address_has_web_scheme (effective_address))
	{
		/* After normalization there are still some cases that are
		 * impossible to tell apart. One example is <URI>:<PORT> and <NON
		 * WEB SCHEME>:<DATA>. To fix this, let's do a HEAD request to the
		 * effective URI prefxed with http://; if we get OK Status the URI
		 * exists, and we'll go ahead, otherwise we'll try to launch a
		 * proper handler through gtk_show_uri. We only do this in
		 * ephy_web_view_load_url, since this case is only relevant for URIs
		 * typed in the location entry, which uses this method to do the
		 * load. */
		/* TODO: however, this is not really possible, because normalize_or_autosearch_address
		 * prepends http:// for anything that doesn't look like a URL.
		 */
	}
#endif

	ephy_link_open (EPHY_LINK (controller), effective_address, NULL,
			ephy_link_flags_from_current_event () | EPHY_LINK_TYPED);

	g_free (effective_address);
}

static void
update_done_cb (EphyHistoryService *service,
		gboolean success,
		gpointer result_data,
		gpointer user_data)
{
	/* FIXME: this hack is needed for the completion entry popup
	 * to resize smoothly. See:
	 * https://bugzilla.gnome.org/show_bug.cgi?id=671074 */
	gtk_entry_completion_complete (GTK_ENTRY_COMPLETION (user_data));
}

static void
user_changed_cb (GtkWidget *widget, EphyLocationController *controller)
{
	const char *address;
	GtkTreeModel *model;
	GtkEntryCompletion *completion;

	address = ephy_location_entry_get_location (EPHY_LOCATION_ENTRY (widget));

	LOG ("user_changed_cb, address %s", address);

	completion = gtk_entry_get_completion (GTK_ENTRY (widget));
	model = gtk_entry_completion_get_model (completion);

	ephy_completion_model_update_for_string (EPHY_COMPLETION_MODEL (model), address,
						 update_done_cb, completion);
}

static void
lock_clicked_cb (GtkWidget *widget,
		 EphyLocationController *controller)
{
	g_signal_emit (controller, signals[LOCK_CLICKED], 0);
}

static void
sync_address (EphyLocationController *controller,
	      GParamSpec *pspec,
	      GtkWidget *widget)
{
	EphyLocationControllerPrivate *priv = controller->priv;
	EphyLocationEntry *lentry = EPHY_LOCATION_ENTRY (widget);

	LOG ("sync_address %s", controller->priv->address);

	g_signal_handlers_block_by_func (widget, G_CALLBACK (user_changed_cb), controller);
	ephy_location_entry_set_location (lentry, priv->address);
	ephy_title_box_set_address (priv->title_box, priv->address);
	g_signal_handlers_unblock_by_func (widget, G_CALLBACK (user_changed_cb), controller);
}

static void
title_box_mode_changed_cb (EphyTitleBox *title_box,
			   GParamSpec *psec,
			   gpointer user_data)
{
	EphyLocationController *controller = EPHY_LOCATION_CONTROLLER (user_data);
	EphyLocationControllerPrivate *priv = controller->priv;

	sync_address (controller, NULL, GTK_WIDGET (priv->location_entry));
}

static char *
get_location_cb (EphyLocationEntry *entry,
		EphyLocationController *controller)
{
	EphyLocationControllerPrivate *priv = controller->priv;
	EphyEmbed *embed;
	
	embed = ephy_embed_container_get_active_child 
	  (EPHY_EMBED_CONTAINER (priv->window));

	return g_strdup (ephy_web_view_get_address (ephy_embed_get_web_view (embed)));
}

static char *
get_title_cb (EphyLocationEntry *entry,
	      EphyLocationController *controller)
{
	EphyEmbed *embed;

	embed = ephy_embed_container_get_active_child 
	  (EPHY_EMBED_CONTAINER (controller->priv->window));

	return g_strdup (ephy_embed_get_title (embed));
}

static void
remove_completion_actions (EphyLocationController *controller,
			   EphyLocationEntry *lentry)
{
	GtkEntryCompletion *completion;
	GList *l;

	completion = gtk_entry_get_completion (GTK_ENTRY (lentry));

	for (l = controller->priv->actions; l != NULL; l = l->next)
	{
		gtk_entry_completion_delete_action (completion, 0);
	}

	g_signal_handlers_disconnect_by_func
			(completion, G_CALLBACK (action_activated_cb), controller);
}

static void
add_completion_actions (EphyLocationController *controller,
			EphyLocationEntry *lentry)
{
	GtkEntryCompletion *completion;
	GList *l;

	completion = gtk_entry_get_completion (GTK_ENTRY (lentry));

	for (l = controller->priv->actions; l != NULL; l = l->next)
	{
		EphyNode *bmk = l->data;
		const char *title;
		int index;

		index = g_list_position (controller->priv->actions, l);
		title = ephy_node_get_property_string
			(bmk, EPHY_NODE_BMK_PROP_TITLE);
		gtk_entry_completion_insert_action_text (completion, index, (char*)title);
	}

	g_signal_connect (completion, "action_activated",
			  G_CALLBACK (action_activated_cb), controller);
}

static gboolean
focus_in_event_cb (GtkWidget *entry,
		   GdkEventFocus *event,
		   EphyLocationController *controller)
{
	EphyLocationControllerPrivate *priv = controller->priv;

	if (!priv->sync_address_is_blocked)
	{		
		priv->sync_address_is_blocked = TRUE;
		g_signal_handlers_block_by_func (controller, G_CALLBACK (sync_address), entry);
	}
	
	return FALSE;
}

static gboolean
focus_out_event_cb (GtkWidget *entry,
		    GdkEventFocus *event,
		    EphyLocationController *controller)
{
	EphyLocationControllerPrivate *priv = controller->priv;

	if (priv->sync_address_is_blocked)
	{
		priv->sync_address_is_blocked = FALSE;
		g_signal_handlers_unblock_by_func (controller, G_CALLBACK (sync_address), entry);
	}
	
	return FALSE;
}

static void
switch_page_cb (GtkNotebook *notebook,
		GtkWidget *page,
		guint page_num,
		EphyLocationController *controller)
{
	EphyLocationControllerPrivate *priv = controller->priv;

	if (priv->sync_address_is_blocked == TRUE)
	{
		priv->sync_address_is_blocked = FALSE;
		g_signal_handlers_unblock_by_func (controller, G_CALLBACK (sync_address), priv->location_entry);
	}
}

static void
ephy_location_controller_constructed (GObject *object)
{
	EphyLocationController *controller = EPHY_LOCATION_CONTROLLER (object);
	EphyLocationControllerPrivate *priv = controller->priv;
	EphyHistoryService *history_service;
	EphyBookmarks *bookmarks;
	EphyCompletionModel *model;
	GtkWidget *notebook, *widget;

	G_OBJECT_CLASS (ephy_location_controller_parent_class)->constructed (object);

	notebook = ephy_window_get_notebook (priv->window);
	widget = GTK_WIDGET (priv->location_entry);

	g_signal_connect (notebook, "switch-page",
			  G_CALLBACK (switch_page_cb), controller);

	history_service = EPHY_HISTORY_SERVICE (ephy_embed_shell_get_global_history_service (ephy_embed_shell_get_default ()));
	bookmarks = ephy_shell_get_bookmarks (ephy_shell_get_default ());
	model = ephy_completion_model_new (history_service, bookmarks, TRUE);
	ephy_location_entry_set_completion (priv->location_entry,
					    GTK_TREE_MODEL (model),
					    EPHY_COMPLETION_TEXT_COL,
					    EPHY_COMPLETION_ACTION_COL,
					    EPHY_COMPLETION_KEYWORDS_COL,
					    EPHY_COMPLETION_RELEVANCE_COL,
					    EPHY_COMPLETION_URL_COL,
					    EPHY_COMPLETION_EXTRA_COL,
					    EPHY_COMPLETION_FAVICON_COL);
	g_object_unref (model);

	ephy_location_entry_set_match_func (priv->location_entry, 
					    match_func, 
					    priv->location_entry,
					    NULL);

	add_completion_actions (controller, priv->location_entry);

	g_signal_connect_object (priv->title_box, "notify::mode",
				 G_CALLBACK (title_box_mode_changed_cb), controller, 0);

	sync_address (controller, NULL, widget);
	g_signal_connect_object (controller, "notify::address",
				 G_CALLBACK (sync_address), widget, 0);
	g_object_bind_property (controller, "editable",
				priv->location_entry, "editable",
				G_BINDING_SYNC_CREATE);

	g_object_bind_property (controller, "icon",
				priv->location_entry, "favicon",
				G_BINDING_SYNC_CREATE);

	g_object_bind_property (controller, "show-icon",
				priv->location_entry, "show-favicon",
				G_BINDING_SYNC_CREATE);

	g_signal_connect_object (widget, "drag-data-received",
				 G_CALLBACK (entry_drag_data_received_cb),
				 controller, 0);
	g_signal_connect_object (widget, "activate",
				 G_CALLBACK (entry_activate_cb),
				 controller, 0);
	g_signal_connect_object (widget, "user-changed",
				 G_CALLBACK (user_changed_cb), controller, 0);
	g_signal_connect_object (widget, "lock-clicked",
				 G_CALLBACK (lock_clicked_cb), controller, 0);
	g_signal_connect_object (widget, "get-location",
				 G_CALLBACK (get_location_cb), controller, 0);
	g_signal_connect_object (widget, "get-title",
				 G_CALLBACK (get_title_cb), controller, 0);
	g_signal_connect_object (widget, "focus-in-event",
				 G_CALLBACK (focus_in_event_cb), controller, 0);
	g_signal_connect_object (widget, "focus-out-event",
				 G_CALLBACK (focus_out_event_cb), controller, 0);
}

static void
ephy_location_controller_set_property (GObject *object,
				       guint prop_id,
				       const GValue *value,
				       GParamSpec *pspec)
{
	EphyLocationController *controller = EPHY_LOCATION_CONTROLLER (object);
	EphyLocationControllerPrivate *priv = controller->priv;

	switch (prop_id)
	{
		case PROP_ADDRESS:
			ephy_location_controller_set_address (controller, g_value_get_string (value));
			break;
		case PROP_EDITABLE:
			priv->editable = g_value_get_boolean (value);
			break;
		case PROP_ICON:
			if (priv->icon != NULL)
			{
				g_object_unref (priv->icon);
			}
			priv->icon = GDK_PIXBUF (g_value_dup_object (value));
			break;
		case PROP_SHOW_ICON:
			priv->show_icon = g_value_get_boolean (value);
			break;
		case PROP_WINDOW:
			priv->window = EPHY_WINDOW (g_value_get_object (value));
			break;
		case PROP_LOCATION_ENTRY:
			priv->location_entry = EPHY_LOCATION_ENTRY (g_value_get_object (value));
			break;
		case PROP_TITLE_BOX:
			priv->title_box = EPHY_TITLE_BOX (g_value_get_object (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

static void
ephy_location_controller_get_property (GObject *object,
				       guint prop_id,
				       GValue *value,
				       GParamSpec *pspec)
{
	EphyLocationController *controller = EPHY_LOCATION_CONTROLLER (object);
	EphyLocationControllerPrivate *priv = controller->priv;

	switch (prop_id)
	{
		case PROP_ADDRESS:
			g_value_set_string (value, ephy_location_controller_get_address (controller));
			break;
		case PROP_EDITABLE:
			g_value_set_boolean (value, priv->editable);
			break;
		case PROP_ICON:
			g_value_set_object (value, priv->icon);
			break;
		case PROP_SHOW_ICON:
			g_value_set_boolean (value, priv->show_icon);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

static void
ephy_location_controller_dispose (GObject *object)
{
	EphyLocationController *controller = EPHY_LOCATION_CONTROLLER (object);
	EphyLocationControllerPrivate *priv = controller->priv;
	GtkWidget *notebook;

	notebook = ephy_window_get_notebook (priv->window);

	if (notebook == NULL ||
	    priv->location_entry == NULL) {
		return;
	}

	g_signal_handlers_disconnect_matched (controller, G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL, priv->location_entry);
	g_signal_handlers_disconnect_matched (priv->location_entry, G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL, controller);
	g_signal_handlers_disconnect_matched (priv->title_box, G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL, controller);
	g_signal_handlers_disconnect_matched (notebook, G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL, controller);
	priv->location_entry = NULL;

	G_OBJECT_CLASS (ephy_location_controller_parent_class)->dispose (object);
}

static void
ephy_location_controller_class_init (EphyLocationControllerClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = ephy_location_controller_finalize;
	object_class->dispose = ephy_location_controller_dispose;
	object_class->constructed = ephy_location_controller_constructed;
	object_class->get_property = ephy_location_controller_get_property;
	object_class->set_property = ephy_location_controller_set_property;

	/**
	* EphyLocationController::lock-clicked:
	* @controller: the object which received the signal.
	*
	* Emitted when the user clicks on the security icon of the internal
	* #EphyLocationEntry.
	*/
	signals[LOCK_CLICKED] = g_signal_new (
		"lock-clicked",
		EPHY_TYPE_LOCATION_CONTROLLER,
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EphyLocationControllerClass, lock_clicked),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0);

	/**
	* EphyLocationController:address:
	*
	* The address of the current location.
	*/
	g_object_class_install_property (object_class,
					 PROP_ADDRESS,
					 g_param_spec_string ("address",
							      "Address",
							      "The address of the current location",
							      "",
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	/**
	* EphyLocationController:editable:
	*
	* Whether the location bar entry can be edited.
	*/
	g_object_class_install_property (object_class,
					 PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       "Editable",
							       "Whether the location bar entry can be edited",
							       TRUE,
							       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	/**
	* EphyLocationController:icon:
	*
	* The icon corresponding to the current location.
	*/
	g_object_class_install_property (object_class,
					 PROP_ICON,
					 g_param_spec_object ("icon",
							      "Icon",
							      "The icon corresponding to the current location",
							      GDK_TYPE_PIXBUF,
							      G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	/**
	* EphyLocationController:show-icon:
	*
	* If we should show the page icon.
	*/
	g_object_class_install_property (object_class,
					 PROP_SHOW_ICON,
					 g_param_spec_boolean ("show-icon",
							       "Show Icon",
							       "Whether to show the favicon",
							       TRUE,
							       G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	/**
	* EphyLocationController:window:
	*
	* The parent window.
	*/
	g_object_class_install_property (object_class,
					 PROP_WINDOW,
					 g_param_spec_object ("window",
							      "Window",
							      "The parent window",
							      G_TYPE_OBJECT,
							      G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							      G_PARAM_CONSTRUCT_ONLY));

	/**
	* EphyLocationController:location-entry:
	*
	* The controlled location entry.
	*/
	g_object_class_install_property (object_class,
					 PROP_LOCATION_ENTRY,
					 g_param_spec_object ("location-entry",
							      "Location entry",
							      "The controlled location entry",
							      G_TYPE_OBJECT,
							      G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							      G_PARAM_CONSTRUCT_ONLY));

	/**
	* EphyLocationController:title-box:
	*
	* The #EphyLocationController sets the address of this title box.
	*/
	g_object_class_install_property (object_class,
					 PROP_TITLE_BOX,
					 g_param_spec_object ("title-box",
							      "Title box",
							      "The title box whose address will be managed",
							      G_TYPE_OBJECT,
							      G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							      G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (EphyLocationControllerPrivate));
}

static int
compare_actions (gconstpointer a,
		 gconstpointer b)
{
	EphyNode *node_a = (EphyNode *)a;
	EphyNode *node_b = (EphyNode *)b;
	const char *title1, *title2;
	int retval;

	title1 = ephy_node_get_property_string (node_a, EPHY_NODE_BMK_PROP_TITLE);
	title2 = ephy_node_get_property_string (node_b, EPHY_NODE_BMK_PROP_TITLE);

	if (title1 == NULL)
	{
		retval = -1;
	}
	else if (title2 == NULL)
	{
		retval = 1;
	}
	else
	{
		char *str_a, *str_b;

		str_a = g_utf8_casefold (title1, -1);
		str_b = g_utf8_casefold (title2, -1);
		retval = g_utf8_collate (str_a, str_b);
		g_free (str_a);
		g_free (str_b);
	}

	return retval;
}

static void
init_actions_list (EphyLocationController *controller)
{
	GPtrArray *children;
	int i;

	children = ephy_node_get_children (controller->priv->smart_bmks);
	for (i = 0; i < children->len; i++)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		controller->priv->actions = g_list_prepend
			(controller->priv->actions, kid);
	}

	controller->priv->actions =
		g_list_sort (controller->priv->actions, (GCompareFunc) compare_actions);
}

static void
update_actions_list (EphyLocationController *la)
{
	EphyLocationControllerPrivate *priv = la->priv;

	remove_completion_actions (la, priv->location_entry);

	g_list_free (la->priv->actions);
	la->priv->actions = NULL;
	init_actions_list (la);

	add_completion_actions (la, priv->location_entry);
}

static void
actions_child_removed_cb (EphyNode *node,
			  EphyNode *child,
			  guint old_index,
			  EphyLocationController *controller)
{
	update_actions_list (controller);
}

static void
actions_child_added_cb (EphyNode *node,
			EphyNode *child,
			EphyLocationController *controller)
{
	update_actions_list (controller);
}

static void
actions_child_changed_cb (EphyNode *node,
			  EphyNode *child,
			  guint property_id,
			  EphyLocationController *controller)
{
	update_actions_list (controller);
}

static void
ephy_location_controller_init (EphyLocationController *controller)
{
	EphyLocationControllerPrivate *priv;

	priv = controller->priv = EPHY_LOCATION_CONTROLLER_GET_PRIVATE (controller);

	priv->address = g_strdup ("");
	priv->editable = TRUE;
	priv->bookmarks = ephy_shell_get_bookmarks (ephy_shell_get_default ());
	priv->smart_bmks = ephy_bookmarks_get_smart_bookmarks
		(controller->priv->bookmarks);
	priv->sync_address_is_blocked = FALSE;

	init_actions_list (controller);
	
	ephy_node_signal_connect_object (priv->smart_bmks,
					 EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback)actions_child_added_cb,
					 G_OBJECT (controller));
	ephy_node_signal_connect_object (priv->smart_bmks,
					 EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback)actions_child_removed_cb,
					 G_OBJECT (controller));
	ephy_node_signal_connect_object (priv->smart_bmks,
					 EPHY_NODE_CHILD_CHANGED,
					 (EphyNodeCallback)actions_child_changed_cb,
					 G_OBJECT (controller));
}

static void
ephy_location_controller_finalize (GObject *object)
{
	EphyLocationController *controller = EPHY_LOCATION_CONTROLLER (object);
	EphyLocationControllerPrivate *priv = controller->priv;

	if (priv->icon != NULL)
	{
		g_object_unref (priv->icon);
	}

	g_list_free (priv->actions);
	g_free (priv->address);

	G_OBJECT_CLASS (ephy_location_controller_parent_class)->finalize (object);
}

/**
 * ephy_location_controller_get_address:
 * @controller: an #EphyLocationController
 *
 * Retrieves the currently loaded address.
 *
 * Returns: the current address
 **/
const char *
ephy_location_controller_get_address (EphyLocationController *controller)
{
	g_return_val_if_fail (EPHY_IS_LOCATION_CONTROLLER (controller), "");

	return controller->priv->address;
}

/**
 * ephy_location_controller_set_address:
 * @controller: an #EphyLocationController
 * @address: new address
 *
 * Sets @address as the address of @controller.
 **/
void
ephy_location_controller_set_address (EphyLocationController *controller,
				      const char *address)
{
	EphyLocationControllerPrivate *priv;

	g_return_if_fail (EPHY_IS_LOCATION_CONTROLLER (controller));

	priv = controller->priv;

	LOG ("set_address %s", address);

	g_free (priv->address);
	priv->address = g_strdup (address);

	g_object_notify (G_OBJECT (controller), "address");
}
