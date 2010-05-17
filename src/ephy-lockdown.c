/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2000, 2001, 2002, 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
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

#include "ephy-embed-container.h"
#include "ephy-embed-utils.h"
#include "ephy-web-view.h"
#include "ephy-lockdown.h"
#include "ephy-extension.h"
#include "ephy-action-helper.h"
#include "ephy-toolbar.h"
#include "ephy-prefs.h"
#include "eel-gconf-extensions.h"
#include "ephy-debug.h"

#include <gtk/gtk.h>

#include <string.h>

static void ephy_lockdown_iface_init (EphyExtensionIface *iface);

/* Make sure these don't overlap with those in ephy-window.c and ephy-toolbar.c */
enum
{
	LOCKDOWN_FLAG = 1 << 31
};

static const char * const keys [] =
{
	CONF_LOCKDOWN_DISABLE_ARBITRARY_URL,
	CONF_LOCKDOWN_DISABLE_BOOKMARK_EDITING,
	CONF_LOCKDOWN_DISABLE_COMMAND_LINE,
	CONF_LOCKDOWN_DISABLE_HISTORY,
	CONF_LOCKDOWN_DISABLE_PRINTING,
	CONF_LOCKDOWN_DISABLE_PRINT_SETUP,
	CONF_LOCKDOWN_DISABLE_SAVE_TO_DISK,
	CONF_LOCKDOWN_DISABLE_TOOLBAR_EDITING,
	CONF_LOCKDOWN_FULLSCREEN
};

#define EPHY_LOCKDOWN_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_LOCKDOWN, EphyLockdownPrivate))

struct _EphyLockdownPrivate
{
	guint notifier_id[G_N_ELEMENTS (keys)];
	GList *windows;
};

static int
find_name (GtkActionGroup *action_group,
	   const char *name)
{
	return strcmp (gtk_action_group_get_name (action_group), name);
}

static GtkActionGroup *
find_action_group (GtkUIManager *manager,
		   const char *name)
{
	GList *list, *element;

	list = gtk_ui_manager_get_action_groups (manager);
	element = g_list_find_custom (list, name, (GCompareFunc) find_name);
	g_return_val_if_fail (element != NULL, NULL);

	return GTK_ACTION_GROUP (element->data);
}

static void
update_location_editable (EphyWindow *window,
			  GtkAction *action,
			  gboolean editable)
{
	EphyEmbed *embed;
	GtkWidget *toolbar;
	char *address;

	g_object_set (action, "editable", editable, NULL);

	/* Restore the real web page address when disabling entry */
	if (editable == FALSE)
	{
		toolbar = ephy_window_get_toolbar (window);
		embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (window));
		/* embed is NULL on startup */
		if (embed != NULL)
		{
			address = ephy_web_view_get_location (ephy_embed_get_web_view (embed), TRUE);
			ephy_toolbar_set_location (EPHY_TOOLBAR (toolbar), address);
			ephy_web_view_set_typed_address (ephy_embed_get_web_view (embed), NULL);
			g_free (address);
		}
	}
}

/* NOTE: If you bring more actions under lockdown control, make sure
 * that all sensitivity updates on them are done using the helpers!
 */
static void
update_window (EphyWindow *window,
	       EphyLockdown *lockdown)
{
	GtkUIManager *manager;
	GtkActionGroup *action_group, *popups_action_group;
	GtkActionGroup *toolbar_action_group, *special_toolbar_action_group;
	GtkAction *action;
	gboolean disabled, fullscreen, print_setup_disabled, writable;

	LOG ("Updating window %p", window);

	manager = GTK_UI_MANAGER (ephy_window_get_ui_manager (window));
	action_group = find_action_group (manager, "WindowActions");
	popups_action_group = find_action_group (manager, "PopupsActions");
	toolbar_action_group = find_action_group (manager, "ToolbarActions");
	special_toolbar_action_group = find_action_group (manager, "SpecialToolbarActions");
	g_return_if_fail (action_group != NULL
			  && popups_action_group != NULL
			  && toolbar_action_group != NULL
			  && special_toolbar_action_group != NULL);

	disabled = eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_PRINTING);
	print_setup_disabled = eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_PRINT_SETUP) ||
			       eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_COMMAND_LINE);
	action = gtk_action_group_get_action (action_group, "FilePrintSetup");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled || print_setup_disabled);
	action = gtk_action_group_get_action (action_group, "FilePrintPreview");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);
	action = gtk_action_group_get_action (action_group, "FilePrint");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);

	writable = eel_gconf_key_is_writable (CONF_WINDOWS_SHOW_TOOLBARS);
	action = gtk_action_group_get_action (action_group, "ViewToolbar");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, !writable);

	disabled = eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_ARBITRARY_URL);
	action = gtk_action_group_get_action (action_group, "GoLocation");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);
	action = gtk_action_group_get_action (special_toolbar_action_group, "Location");
	update_location_editable (window, action, !disabled);
	action = gtk_action_group_get_action (special_toolbar_action_group, "NavigationUp");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);

	disabled = eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_HISTORY);
	action = gtk_action_group_get_action (action_group, "GoHistory");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);
	action = gtk_action_group_get_action (special_toolbar_action_group, "NavigationBack");
	gtk_action_set_visible (action, !disabled);
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);
	action = gtk_action_group_get_action (special_toolbar_action_group, "NavigationForward");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);
	gtk_action_set_visible (action, !disabled);

	disabled = eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_BOOKMARK_EDITING);
	action = gtk_action_group_get_action (action_group, "GoBookmarks");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);
	action = gtk_action_group_get_action (action_group, "FileBookmarkPage");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);
	action = gtk_action_group_get_action (popups_action_group, "BookmarkLink");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);

	disabled = eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_SAVE_TO_DISK);
	action = gtk_action_group_get_action (action_group, "FileSaveAs");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);
	action = gtk_action_group_get_action (popups_action_group, "DownloadLink");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);
	action = gtk_action_group_get_action (popups_action_group, "DownloadLinkAs");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);
	action = gtk_action_group_get_action (popups_action_group, "SaveImageAs");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);
	action = gtk_action_group_get_action (popups_action_group, "OpenImage");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);
	writable = eel_gconf_key_is_writable (CONF_DESKTOP_BG_PICTURE);
	action = gtk_action_group_get_action (popups_action_group, "SetImageAsBackground");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled || !writable);

	disabled = eel_gconf_get_boolean (CONF_LOCKDOWN_DISABLE_TOOLBAR_EDITING);
	action = gtk_action_group_get_action (action_group, "ViewToolbarEditor");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);
	action = gtk_action_group_get_action (toolbar_action_group, "MoveToolItem");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);
	action = gtk_action_group_get_action (toolbar_action_group, "RemoveToolItem");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);
	action = gtk_action_group_get_action (toolbar_action_group, "RemoveToolbar");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, disabled);

	fullscreen = eel_gconf_get_boolean (CONF_LOCKDOWN_FULLSCREEN);
	action = gtk_action_group_get_action (special_toolbar_action_group, "FileNewWindow");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, fullscreen);
	action = gtk_action_group_get_action (action_group, "ViewFullscreen");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, fullscreen);

	action = gtk_action_group_get_action (action_group, "TabsDetach");
	ephy_action_change_sensitivity_flags (action, LOCKDOWN_FLAG, fullscreen);

	if (fullscreen)
	{
		gtk_window_fullscreen (GTK_WINDOW (window));
	}
}

static void
notifier (GConfClient *client,
	  guint cnxn_id,
	  GConfEntry *entry,
	  EphyLockdown *lockdown)
{
	EphyLockdownPrivate *priv = lockdown->priv;

	LOG ("Key %s changed", entry->key);

	g_list_foreach (priv->windows, (GFunc) update_window, lockdown);
}

static void
ephy_lockdown_init (EphyLockdown *lockdown)
{
	EphyLockdownPrivate *priv;
	guint i;

	lockdown->priv = priv = EPHY_LOCKDOWN_GET_PRIVATE (lockdown);

	LOG ("EphyLockdown initialising");

	for (i = 0; i < G_N_ELEMENTS (keys); i++)
	{
		priv->notifier_id[i] =eel_gconf_notification_add
			(keys[i], (GConfClientNotifyFunc) notifier, lockdown);
	}
	/* We know that no windows are open yet,
	 * so we don't need to do notify here.
	 */

	eel_gconf_monitor_add ("/apps/epiphany/lockdown");
	eel_gconf_monitor_add ("/desktop/gnome/lockdown");
}

G_DEFINE_TYPE_WITH_CODE (EphyLockdown, ephy_lockdown, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (EPHY_TYPE_EXTENSION,
						ephy_lockdown_iface_init))

static void
ephy_lockdown_finalize (GObject *object)
{
	EphyLockdown *lockdown = EPHY_LOCKDOWN (object);
	EphyLockdownPrivate *priv = lockdown->priv;
	guint i;

	LOG ("EphyLockdown finalising");

	eel_gconf_monitor_remove ("/apps/epiphany/lockdown");
	eel_gconf_monitor_remove ("/desktop/gnome/lockdown");

	for (i = 0; i < G_N_ELEMENTS (keys); i++)
	{
		eel_gconf_notification_remove (priv->notifier_id[i]);
	}

	G_OBJECT_CLASS (ephy_lockdown_parent_class)->finalize (object);
}

static void
impl_attach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphyLockdown *lockdown = EPHY_LOCKDOWN (extension);
	EphyLockdownPrivate *priv = lockdown->priv;

	priv->windows = g_list_prepend (priv->windows, window);

	update_window (window, lockdown);
}

static void
impl_detach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphyLockdown *lockdown = EPHY_LOCKDOWN (extension);
	EphyLockdownPrivate *priv = lockdown->priv;

	priv->windows = g_list_remove (priv->windows, window);

	/* Since we know that the window closes now, we don't have to
	 * undo anything.
	 */
}

static void
ephy_lockdown_iface_init (EphyExtensionIface *iface)
{
	iface->attach_window = impl_attach_window;
	iface->detach_window = impl_detach_window;
}

static void
ephy_lockdown_class_init (EphyLockdownClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ephy_lockdown_finalize;

	g_type_class_add_private (object_class, sizeof (EphyLockdownPrivate));
}

