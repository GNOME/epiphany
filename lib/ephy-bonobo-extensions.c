/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* gul-bonobo-extensions.c - implementation of new functions that conceptually
                             belong in bonobo. Perhaps some of these will be
                             actually rolled into bonobo someday.

            This file is based on nautilus-bonobo-extensions.c from
            libnautilus-private.

   Copyright (C) 2000, 2001 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: John Sullivan <sullivan@eazel.com>
            Darin Adler <darin@bentspoon.com>
*/

#include <config.h>

#include "ephy-bonobo-extensions.h"
#include "ephy-string.h"
#include <string.h>

#include <bonobo/bonobo-ui-util.h>
#include <gtk/gtkmain.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <bonobo/bonobo-control.h>

typedef enum {
	NUMBERED_MENU_ITEM_PLAIN,
	NUMBERED_MENU_ITEM_TOGGLE,
	NUMBERED_MENU_ITEM_RADIO
} NumberedMenuItemType;

void
ephy_bonobo_set_accelerator (BonoboUIComponent *ui,
			     const char *path,
			     const char *accelerator)
{
	if (bonobo_ui_component_get_container (ui)) /* should not do this here... */
	{
		bonobo_ui_component_set_prop (ui, path, "accel", accelerator, NULL);
	}
}

void
ephy_bonobo_set_label (BonoboUIComponent *ui,
		       const char *path,
		       const char *label)
{
	if (bonobo_ui_component_get_container (ui)) /* should not do this here... */
	{
		bonobo_ui_component_set_prop (ui, path, "label", label, NULL);
	}
}

void
ephy_bonobo_set_tip (BonoboUIComponent *ui,
		     const char *path,
		     const char *tip)
{
	if (bonobo_ui_component_get_container (ui)) /* should not do this here... */
	{
		bonobo_ui_component_set_prop (ui, path, "tip", tip, NULL);
	}
}

void
ephy_bonobo_set_sensitive (BonoboUIComponent *ui,
			   const char *path,
			   gboolean sensitive)
{
	if (bonobo_ui_component_get_container (ui)) /* should not do this here... */
	{
		bonobo_ui_component_set_prop (ui, path, "sensitive", sensitive ? "1" : "0", NULL);
	}
}

void
ephy_bonobo_set_toggle_state (BonoboUIComponent *ui,
			      const char *path,
			      gboolean state)
{
	if (bonobo_ui_component_get_container (ui)) /* should not do this here... */
	{
		bonobo_ui_component_set_prop (ui, path, "state", state ? "1" : "0", NULL);
	}
}

void
ephy_bonobo_set_hidden (BonoboUIComponent *ui,
		        const char *path,
		        gboolean hidden)
{
	if (bonobo_ui_component_get_container (ui)) /* should not do this here... */
	{
		bonobo_ui_component_set_prop (ui, path, "hidden", hidden ? "1" : "0", NULL);
	}
}

char *
ephy_bonobo_get_label (BonoboUIComponent *ui,
		       const char *path)
{
	if (bonobo_ui_component_get_container (ui)) /* should not do this here... */
	{
		return bonobo_ui_component_get_prop (ui, path, "label", NULL);
	}
	else
	{
		return NULL;
	}
}

gboolean
ephy_bonobo_get_hidden (BonoboUIComponent *ui,
		        const char *path)
{
	char *value;
	gboolean hidden;
	CORBA_Environment ev;

	g_return_val_if_fail (BONOBO_IS_UI_COMPONENT (ui), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	CORBA_exception_init (&ev);
	value = bonobo_ui_component_get_prop (ui, path, "hidden", &ev);
	CORBA_exception_free (&ev);

	if (value == NULL) {
		/* No hidden attribute means not hidden. */
		hidden = FALSE;
	} else {
		/* Anything other than "0" counts as TRUE */
		hidden = strcmp (value, "0") != 0;
		g_free (value);
	}

	return hidden;
}

static char *
get_numbered_menu_item_name (guint index)
{
	return g_strdup_printf ("%u", index);
}

char *
ephy_bonobo_get_numbered_menu_item_path (BonoboUIComponent *ui,
					 const char *container_path,
					 guint index)
{
	char *item_name;
	char *item_path;

	g_return_val_if_fail (BONOBO_IS_UI_COMPONENT (ui), NULL);
	g_return_val_if_fail (container_path != NULL, NULL);

	item_name = get_numbered_menu_item_name (index);
	item_path = g_strconcat (container_path, "/", item_name, NULL);
	g_free (item_name);

	return item_path;
}

char *
ephy_bonobo_get_numbered_menu_item_command (BonoboUIComponent *ui,
					    const char *container_path,
					    guint index)
{
	char *command_name;
	char *path;

	g_return_val_if_fail (BONOBO_IS_UI_COMPONENT (ui), NULL);
	g_return_val_if_fail (container_path != NULL, NULL);

	path = ephy_bonobo_get_numbered_menu_item_path (ui, container_path, index);
	command_name = gnome_vfs_escape_string (path);
	g_free (path);

	return command_name;
}

guint
ephy_bonobo_get_numbered_menu_item_index_from_command (const char *command)
{
	char *path;
	char *index_string;
	int index;
	gboolean got_index;

	path = gnome_vfs_unescape_string (command, NULL);
	index_string = strrchr (path, '/');

	if (index_string == NULL) {
		got_index = FALSE;
	} else {
		got_index = ephy_str_to_int (index_string + 1, &index);
	}
	g_free (path);

	g_return_val_if_fail (got_index, 0);

	return index;
}

char *
ephy_bonobo_get_numbered_menu_item_container_path_from_command (const char *command)
{
	char *path;
	char *index_string;
	char *container_path;

	path = gnome_vfs_unescape_string (command, NULL);
	index_string = strrchr (path, '/');

	container_path = index_string == NULL
		? NULL
		: g_strndup (path, index_string - path);
	g_free (path);

	return container_path;
}

static char *
ephy_bonobo_add_numbered_menu_item_internal (BonoboUIComponent *ui,
					     const char *container_path,
					     guint index,
					     const char *label,
					     NumberedMenuItemType type,
					     GdkPixbuf *pixbuf,
					     const char *radio_group_name)
{
	char *xml_item, *xml_command;
	char *command_name;
	char *item_name, *pixbuf_data;
	char *path;

	g_assert (BONOBO_IS_UI_COMPONENT (ui));
	g_assert (container_path != NULL);
	g_assert (label != NULL);
	g_assert (type == NUMBERED_MENU_ITEM_PLAIN || pixbuf == NULL);
	g_assert (type == NUMBERED_MENU_ITEM_RADIO || radio_group_name == NULL);
	g_assert (type != NUMBERED_MENU_ITEM_RADIO || radio_group_name != NULL);

	item_name = get_numbered_menu_item_name (index);
	command_name = ephy_bonobo_get_numbered_menu_item_command
		(ui, container_path, index);

	switch (type) {
	case NUMBERED_MENU_ITEM_TOGGLE:
		xml_item = g_strdup_printf ("<menuitem name=\"%s\" id=\"%s\" type=\"toggle\"/>\n",
					    item_name, command_name);
		break;
	case NUMBERED_MENU_ITEM_RADIO:
		xml_item = g_strdup_printf ("<menuitem name=\"%s\" id=\"%s\" "
					    "type=\"radio\" group=\"%s\"/>\n",
					    item_name, command_name, radio_group_name);
		break;
	case NUMBERED_MENU_ITEM_PLAIN:
		if (pixbuf != NULL) {
			pixbuf_data = bonobo_ui_util_pixbuf_to_xml (pixbuf);
			xml_item = g_strdup_printf ("<menuitem name=\"%s\" verb=\"%s\" "
						    "pixtype=\"pixbuf\" pixname=\"%s\"/>\n",
						    item_name, command_name, pixbuf_data);
			g_free (pixbuf_data);
		} else {
			xml_item = g_strdup_printf ("<menuitem name=\"%s\" verb=\"%s\"/>\n",
						    item_name, command_name);
		}
		break;
	default:
		g_assert_not_reached ();
		xml_item = NULL;	/* keep compiler happy */
	}

	g_free (item_name);

	bonobo_ui_component_set (ui, container_path, xml_item, NULL);

	g_free (xml_item);

	path = ephy_bonobo_get_numbered_menu_item_path (ui, container_path, index);
	ephy_bonobo_set_label (ui, path, label);
	g_free (path);

	/* Make the command node here too, so callers can immediately set
	 * properties on it (otherwise it doesn't get created until some
	 * time later).
	 */
	xml_command = g_strdup_printf ("<cmd name=\"%s\"/>\n", command_name);
	bonobo_ui_component_set (ui, "/commands", xml_command, NULL);
	g_free (xml_command);

	g_free (command_name);

	return item_name;
}

/* Add a menu item specified by number into a given path. Used for
 * dynamically creating a related series of menu items. Each index
 * must be unique (normal use is to call this in a loop, and
 * increment the index for each item).
 */
void
ephy_bonobo_add_numbered_menu_item (BonoboUIComponent *ui,
				   const char *container_path,
				   guint index,
				   const char *label,
				   GdkPixbuf *pixbuf)
{
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (container_path != NULL);
	g_return_if_fail (label != NULL);

	ephy_bonobo_add_numbered_menu_item_internal (ui, container_path, index, label,
						     NUMBERED_MENU_ITEM_PLAIN, pixbuf, NULL);
}

/* Add a menu item specified by number into a given path. Used for
 * dynamically creating a related series of toggle menu items. Each index
 * must be unique (normal use is to call this in a loop, and
 * increment the index for each item).
 */
void
ephy_bonobo_add_numbered_toggle_menu_item (BonoboUIComponent *ui,
					  const char *container_path,
					  guint index,
					  const char *label)
{
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (container_path != NULL);
	g_return_if_fail (label != NULL);

	ephy_bonobo_add_numbered_menu_item_internal (ui, container_path, index, label,
						     NUMBERED_MENU_ITEM_TOGGLE, NULL, NULL);
}

/* Add a menu item specified by number into a given path. Used for
 * dynamically creating a related series of radio menu items. Each index
 * must be unique (normal use is to call this in a loop, and
 * increment the index for each item).
 */
void
ephy_bonobo_add_numbered_radio_menu_item (BonoboUIComponent *ui,
					 const char *container_path,
					 guint index,
					 const char *label,
					 const char *radio_group_name)
{
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (container_path != NULL);
	g_return_if_fail (label != NULL);

	ephy_bonobo_add_numbered_menu_item_internal (ui, container_path, index, label,
						    NUMBERED_MENU_ITEM_RADIO, NULL, radio_group_name);
}

void
ephy_bonobo_add_numbered_submenu (BonoboUIComponent *ui,
				 const char *container_path,
				 guint index,
				 const char *label,
				 GdkPixbuf *pixbuf)
{
	char *xml_string, *item_name, *pixbuf_data, *submenu_path, *command_name;

	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (container_path != NULL);
	g_return_if_fail (label != NULL);
	g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

	item_name = get_numbered_menu_item_name (index);
	command_name = ephy_bonobo_get_numbered_menu_item_command (ui, container_path, index);

	if (pixbuf != NULL) {
		pixbuf_data = bonobo_ui_util_pixbuf_to_xml (pixbuf);
		xml_string = g_strdup_printf ("<submenu name=\"%s\" pixtype=\"pixbuf\" pixname=\"%s\" "
					      "verb=\"%s\"/>\n",
					      item_name, pixbuf_data, command_name);
		g_free (pixbuf_data);
	} else {
		xml_string = g_strdup_printf ("<submenu name=\"%s\" verb=\"%s\"/>\n", item_name,
					      command_name);
	}

	bonobo_ui_component_set (ui, container_path, xml_string, NULL);

	g_free (xml_string);

	submenu_path = ephy_bonobo_get_numbered_menu_item_path (ui, container_path, index);
	ephy_bonobo_set_label (ui, submenu_path, label);
	g_free (submenu_path);

	g_free (item_name);
	g_free (command_name);
}

void
ephy_bonobo_add_numbered_submenu_no_verb (BonoboUIComponent *ui,
					  const char *container_path,
					  guint index,
				          const char *label,
					  GdkPixbuf *pixbuf)
{
	char *xml_string, *item_name, *pixbuf_data, *submenu_path;

	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (container_path != NULL);
	g_return_if_fail (label != NULL);
	g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

	item_name = get_numbered_menu_item_name (index);

	if (pixbuf != NULL) {
		pixbuf_data = bonobo_ui_util_pixbuf_to_xml (pixbuf);
		xml_string = g_strdup_printf ("<submenu name=\"%s\" pixtype=\"pixbuf\" pixname=\"%s\" "
					      "/>\n",
					      item_name, pixbuf_data);
		g_free (pixbuf_data);
	} else {
		xml_string = g_strdup_printf ("<submenu name=\"%s\"/>\n", item_name);
	}

	bonobo_ui_component_set (ui, container_path, xml_string, NULL);

	g_free (xml_string);

	submenu_path = ephy_bonobo_get_numbered_menu_item_path (ui, container_path, index);
	ephy_bonobo_set_label (ui, submenu_path, label);
	g_free (submenu_path);

	g_free (item_name);
}

void
ephy_bonobo_add_submenu (BonoboUIComponent *ui,
			 const char *path,
			 const char *label,
			 GdkPixbuf *pixbuf)
{
	char *xml_string, *name, *pixbuf_data, *submenu_path;

	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (path != NULL);
	g_return_if_fail (label != NULL);
	g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

	/* Labels may contain characters that are illegal in names. So
	 * we create the name by URI-encoding the label.
	 */
	name = gnome_vfs_escape_string (label);

	if (pixbuf != NULL) {
		pixbuf_data = bonobo_ui_util_pixbuf_to_xml (pixbuf);
		xml_string = g_strdup_printf ("<submenu name=\"%s\" pixtype=\"pixbuf\" pixname=\"%s\"/>\n",
					      name, pixbuf_data);
		g_free (pixbuf_data);
	} else {
		xml_string = g_strdup_printf ("<submenu name=\"%s\"/>\n", name);
	}

	bonobo_ui_component_set (ui, path, xml_string, NULL);

	g_free (xml_string);

	submenu_path = g_strconcat (path, "/", name, NULL);
	ephy_bonobo_set_label (ui, submenu_path, label);
	g_free (submenu_path);

	g_free (name);
}

void
ephy_bonobo_add_menu_separator (BonoboUIComponent *ui, const char *path)
{
	static gint hack = 0;
	gchar *xml;

	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (path != NULL);

	xml = g_strdup_printf ("<separator name=\"sep%d\"/>", ++hack);
	bonobo_ui_component_set (ui, path, xml, NULL);
	g_free (xml);
}

static void
remove_commands (BonoboUIComponent *ui, const char *container_path)
{
	BonoboUINode *path_node;
	BonoboUINode *child_node;
	char *verb_name;
	char *id_name;

	path_node = bonobo_ui_component_get_tree (ui, container_path, TRUE, NULL);
	if (path_node == NULL) {
		return;
	}

	bonobo_ui_component_freeze (ui, NULL);

	for (child_node = bonobo_ui_node_children (path_node);
	     child_node != NULL;
	     child_node = bonobo_ui_node_next (child_node)) {
		verb_name = bonobo_ui_node_get_attr (child_node, "verb");
		if (verb_name != NULL) {
			bonobo_ui_component_remove_verb (ui, verb_name);
			bonobo_ui_node_free_string (verb_name);
		} else {
			/* Only look for an id if there's no verb */
			id_name = bonobo_ui_node_get_attr (child_node, "id");
			if (id_name != NULL) {
				bonobo_ui_component_remove_listener (ui, id_name);
				bonobo_ui_node_free_string (id_name);
			}
		}
	}

	bonobo_ui_component_thaw (ui, NULL);

	bonobo_ui_node_free (path_node);
}

/**
 * ephy_bonobo_remove_menu_items_and_verbs
 *
 * Removes all menu items contained in a menu or placeholder, and
 * their verbs.
 *
 * @uih: The BonoboUIHandler for this menu item.
 * @container_path: The standard bonobo-style path specifier for this placeholder or submenu.
 */
void
ephy_bonobo_remove_menu_items_and_commands (BonoboUIComponent *ui,
					    const char *container_path)
{
	char *remove_wildcard;

	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (container_path != NULL);

	remove_commands (ui, container_path);

	/* For speed, remove menu items themselves all in one fell swoop,
	 * though we removed the verbs one-by-one.
	 */
	remove_wildcard = g_strdup_printf ("%s/*", container_path);
	bonobo_ui_component_rm (ui, remove_wildcard, NULL);
	g_free (remove_wildcard);
}

/* Call to set the user-visible label of a menu item to a string
 * containing an underscore accelerator. The underscore is stripped
 * off before setting the label of the command, because pop-up menu
 * and toolbar button labels shouldn't have the underscore.
 */
void
ephy_bonobo_set_label_for_menu_item_and_command (BonoboUIComponent *ui,
						 const char	*menu_item_path,
						 const char	*command_path,
						 const char	*label_with_underscore)
{
	char *label_no_underscore;

	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui));
	g_return_if_fail (menu_item_path != NULL);
	g_return_if_fail (command_path != NULL);
	g_return_if_fail (label_with_underscore != NULL);

	label_no_underscore = ephy_str_strip_chr (label_with_underscore, '_');
	ephy_bonobo_set_label (ui,
			      menu_item_path,
			      label_with_underscore);
	ephy_bonobo_set_label (ui,
			      command_path,
			      label_no_underscore);

	g_free (label_no_underscore);
}

gchar *
ephy_bonobo_add_dockitem (BonoboUIComponent *uic,
			  const char *name,
			  int band_num)
{
	gchar *xml;
	gchar *sname;
	gchar *path;

	sname = gnome_vfs_escape_string (name);
	xml = g_strdup_printf ("<dockitem name=\"%s\" band_num=\"%d\" "
			       "config=\"0\" behavior=\"exclusive\"/>",
			       sname, band_num);
	path = g_strdup_printf ("/%s", sname);
	bonobo_ui_component_set (uic, "", xml, NULL);

	g_free (sname);
	g_free (xml);
	return path;
}

BonoboControl *
ephy_bonobo_add_numbered_control (BonoboUIComponent *uic, GtkWidget *w,
				  guint index, const char *container_path)
{
	BonoboControl *control;
	char *xml_string, *item_name, *control_path;

	g_return_val_if_fail (BONOBO_IS_UI_COMPONENT (uic), NULL);
	g_return_val_if_fail (container_path != NULL, NULL);

	item_name = get_numbered_menu_item_name (index);
	xml_string = g_strdup_printf ("<control name=\"%s\"/>", item_name);

	bonobo_ui_component_set (uic, container_path, xml_string, NULL);

	g_free (xml_string);

	control_path = ephy_bonobo_get_numbered_menu_item_path (uic, container_path, index);

        control = bonobo_control_new (w);
        bonobo_ui_component_object_set (uic, control_path, BONOBO_OBJREF (control), NULL);
	bonobo_object_unref (control);

	g_free (control_path);
	g_free (item_name);

	return control;
}

void
ephy_bonobo_replace_path (BonoboUIComponent *uic, const gchar *path_src,
			  const char *path_dst)
{
	BonoboUINode *node;
	const char *name;
	char *path_dst_folder;

	name = strrchr (path_dst, '/');
	g_return_if_fail (name != NULL);
	path_dst_folder = g_strndup (path_dst, name - path_dst);
	name++;

	node = bonobo_ui_component_get_tree (uic, path_src, TRUE, NULL);
	bonobo_ui_node_set_attr (node, "name", name);

	ephy_bonobo_clear_path (uic, path_dst);

	bonobo_ui_component_set_tree (uic, path_dst_folder, node, NULL);

	g_free (path_dst_folder);
	bonobo_ui_node_free (node);
}

void
ephy_bonobo_clear_path (BonoboUIComponent *uic,
		        const gchar *path)
{
	if (bonobo_ui_component_path_exists  (uic, path, NULL))
	{
		char *remove_wildcard = g_strdup_printf ("%s/*", path);
		bonobo_ui_component_rm (uic, remove_wildcard, NULL);
		g_free (remove_wildcard);
	}
}
