/*
 *  Copyright (C) 2000 Marco Pesenti Gritti
 *            (C) 2001, 2002 Jorn Baayen
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

#include "ephy-editable-toolbar.h"
#include "ephy-file-helpers.h"
#include "ephy-debug.h"
#include "ephy-dnd.h"
#include "eggtoolitem.h"
#include "eggtoolbar.h"

#include <libxml/parser.h>
#include <libgnome/gnome-i18n.h>
#include <string.h>

/* This is copied from gtkscrollbarwindow.c */
#define DEFAULT_SCROLLBAR_SPACING  3

#define SCROLLBAR_SPACING(w)                                                            \
  (GTK_SCROLLED_WINDOW_GET_CLASS (w)->scrollbar_spacing >= 0 ?                          \
   GTK_SCROLLED_WINDOW_GET_CLASS (w)->scrollbar_spacing : DEFAULT_SCROLLBAR_SPACING)

static GtkTargetEntry dest_drag_types [] =
{
        { "EPHY_TOOLBAR_BUTTON", 0, 0 },
	/* FIXME generic way to add types */
        { EPHY_DND_URL_TYPE, 0, EPHY_DND_URL }
};

static GtkTargetEntry source_drag_types [] =
{
        { "EPHY_TOOLBAR_BUTTON", 0, 0 }
};

static void	ephy_editable_toolbar_class_init (EphyEditableToolbarClass *klass);
static void	ephy_editable_toolbar_init	 (EphyEditableToolbar *t);
static void	ephy_editable_toolbar_finalize	 (GObject *object);
static void	do_merge			 (EphyEditableToolbar *t);
static void	setup_editor			 (EphyEditableToolbar *etoolbar);
static void	update_editor_sheet		 (EphyEditableToolbar *etoolbar);
static gboolean update_editor_position		 (EphyEditableToolbar *etoolbar);

enum
{
	PROP_0,
	PROP_MENU_MERGE
};

static GObjectClass *parent_class = NULL;

struct EphyEditableToolbarPrivate
{
	EggMenuMerge *merge;
	GNode *available_actions;
	GNode *toolbars;
	char *filename;

	GtkWidget *editor;
	GtkWidget *main_box;
	GtkWidget *table;
	GtkWidget *label_zone;
	GtkWidget *action_zone;
	GtkWidget *scrolled_window;

	guint ui_id;

	gboolean toolbars_dirty;
	gboolean editor_pos_dirty;
	gboolean editor_sheet_dirty;
	gboolean edit_mode;
};

typedef struct
{
	GtkWidget *widget;
} ToolbarNode;

typedef struct
{
	gboolean separator;
	EggAction *action;
	GtkWidget *widget;
} ItemNode;

GType
ephy_editable_toolbar_get_type (void)
{
        static GType ephy_editable_toolbar_type = 0;

        if (ephy_editable_toolbar_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyEditableToolbarClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_editable_toolbar_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (EphyEditableToolbar),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) ephy_editable_toolbar_init
                };

                ephy_editable_toolbar_type = g_type_register_static (G_TYPE_OBJECT,
						                     "EphyEditableToolbar",
						                     &our_info, 0);
        }

        return ephy_editable_toolbar_type;

}

static ToolbarNode *
toolbar_node_new (void)
{
	ToolbarNode *node;

	node = g_new0 (ToolbarNode, 1);
	node->widget = NULL;

	return node;
}

static ItemNode *
item_node_new (EggAction *action, gboolean separator)
{
	ItemNode *item;

	item = g_new0 (ItemNode, 1);
	item->action = action;
	item->separator = separator;
	item->widget = NULL;

	return item;
}

static void
free_node (GNode *node)
{
	g_free (node->data);
}

static EggAction *
find_action (EphyEditableToolbar *t, const char *name)
{
	GList *l = t->priv->merge->action_groups;
	EggAction *action = NULL;

	for (; l != NULL; l = l->next)
	{
		EggAction *tmp;

		tmp = egg_action_group_get_action
			(EGG_ACTION_GROUP (l->data), name);
		if (tmp) action = tmp;
	}

	return action;
}

static GNode *
find_node_from_action (EphyEditableToolbar *t, EggAction *action)
{
	GNode *n1, *n2;

	for (n1 = t->priv->toolbars->children; n1 != NULL; n1 = n1->next)
	{
		for (n2 = n1->children; n2 != NULL; n2 = n2->next)
		{
			ItemNode *item = (ItemNode *) (n2->data);

			if (!item->separator && item->action == action)
				return n2;
		}
	}

	return NULL;
}

static EggAction *
impl_get_action (EphyEditableToolbar *etoolbar,
		 const char *type,
		 const char *name)
{
	EggAction *action;

	if (type == NULL)
	{
		action = find_action (etoolbar, name);
	}
	else
	{
		action = NULL;
	}

	return action;
}

static void
add_action (EphyEditableToolbar *t,
	    GNode *parent,
	    GNode *sibling,
	    const char *type,
	    const char *name)
{
	EggAction *action = NULL;
	gboolean separator;
	ItemNode *item;
	GNode *node;

	LOG ("Add action, type %s, name %s", type, name)

	separator = (strcmp (name, "separator") == 0);
	if (!separator)
	{
		action = ephy_editable_toolbar_get_action (t, type, name);
		g_return_if_fail (action != NULL);
	}

	item = item_node_new (action, separator);
	node = g_node_new (item);

	g_node_insert_before (parent, sibling, node);
}

static void
parse_item_list (EphyEditableToolbar *t,
		 xmlNodePtr child,
		 GNode *parent)
{
	while (child)
	{
		if (xmlStrEqual (child->name, "toolitem"))
		{
			xmlChar *verb;

			verb = xmlGetProp (child, "verb");
			add_action (t, parent, NULL, NULL, verb);

			xmlFree (verb);
		}
		else if (xmlStrEqual (child->name, "separator"))
		{
			add_action (t, parent, NULL, NULL, "separator");
		}

		child = child->next;
	}
}

static GNode *
add_toolbar (EphyEditableToolbar *t)
{
	ToolbarNode *toolbar;
	GNode *node;

	toolbar = toolbar_node_new ();
	node = g_node_new (toolbar);
	g_node_append (t->priv->toolbars, node);

	return node;
}

static void
parse_toolbars (EphyEditableToolbar *t,
		xmlNodePtr child)
{
	while (child)
	{
		if (xmlStrEqual (child->name, "toolbar"))
		{
			GNode *node;

			node = add_toolbar (t);
			parse_item_list (t, child->children, node);
		}

		child = child->next;
	}
}

static void
load_defaults (EphyEditableToolbar *t)
{
	xmlDocPtr doc;
        xmlNodePtr child;
	xmlNodePtr root;
	const char *xml_filepath;

	LOG ("Load default toolbar info")

	xml_filepath = ephy_file ("epiphany-toolbar.xml");

	doc = xmlParseFile (xml_filepath);
	root = xmlDocGetRootElement (doc);

	child = root->children;
	while (child)
	{
		if (xmlStrEqual (child->name, "available"))
		{
			t->priv->available_actions = g_node_new (NULL);
			parse_item_list (t, child->children,
					 t->priv->available_actions);
		}
		else if (xmlStrEqual (child->name, "default") &&
			 t->priv->toolbars == NULL)
		{
			t->priv->toolbars = g_node_new (NULL);
			parse_toolbars (t, child->children);
		}

		child = child->next;
	}
}

static void
load_toolbar (EphyEditableToolbar *t)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	const char *xml_filepath = t->priv->filename;

	LOG ("Load custom toolbar")

	if (!g_file_test (xml_filepath, G_FILE_TEST_EXISTS)) return;

	doc = xmlParseFile (xml_filepath);
	root = xmlDocGetRootElement (doc);

	t->priv->toolbars = g_node_new (NULL);
	parse_toolbars (t, root->children);
}

static xmlDocPtr
toolbar_list_to_xml (EphyEditableToolbar *t, GNode *tl)
{
	GNode *l1, *l2;
	xmlDocPtr doc;

	xmlIndentTreeOutput = TRUE;
	doc = xmlNewDoc ("1.0");
	doc->children = xmlNewDocNode (doc, NULL, "toolbars", NULL);

	for (l1 = tl->children; l1 != NULL; l1 = l1->next)
	{
		xmlNodePtr tnode;

		tnode = xmlNewChild (doc->children, NULL, "toolbar", NULL);

		for (l2 = l1->children; l2 != NULL; l2 = l2->next)
		{
			xmlNodePtr node;
			ItemNode *item = (ItemNode *) (l2->data);

			if (item->separator)
			{
				 node = xmlNewChild (tnode, NULL, "separator", NULL);
			}
			else
			{
				node = xmlNewChild (tnode, NULL, "toolitem", NULL);
				xmlSetProp (node, "verb", item->action->name);
			}
		}
	}

	return doc;
}

static char *
toolbar_list_to_string (EphyEditableToolbar *t, GNode *tl)
{
	GString *s;
	GNode *l1, *l2;
	char *result;
	int k = 0;

	s = g_string_new (NULL);
	g_string_append (s, "<Root>");
	for (l1 = tl->children; l1 != NULL; l1 = l1->next)
	{
		int i = 0;

		g_string_append_printf
			(s, "<dockitem name=\"Toolbar%d\">\n", k);

		for (l2 = l1->children; l2 != NULL; l2 = l2->next)
		{
			ItemNode *item = (ItemNode *) (l2->data);

			if (item->separator)
			{
				g_string_append_printf
					(s, "<placeholder name=\"PlaceHolder%d\">"
					 "<separator name=\"ToolSeparator\"/>"
					 "</placeholder>\n", i);
			}
			else
			{
				g_string_append_printf
					(s, "<placeholder name=\"PlaceHolder%d\">"
					 "<toolitem name=\"ToolItem\" verb=\"%s\"/>"
					 "</placeholder>\n",
					 i, item->action->name);
			}
			i++;
		}

		g_string_append (s, "</dockitem>\n");

		k++;
	}
	g_string_append (s, "</Root>");

	result = s->str;

	g_string_free (s, FALSE);

	return result;
}

static gboolean
ui_update (gpointer data)
{
	EphyEditableToolbar *etoolbar = EPHY_EDITABLE_TOOLBAR (data);

	if (etoolbar->priv->toolbars_dirty)
	{
		LOG ("Update ui: toolbars")
		do_merge (etoolbar);
		etoolbar->priv->toolbars_dirty = FALSE;
	}

	if (etoolbar->priv->editor_sheet_dirty)
	{
		LOG ("Update ui: editor sheet")
		update_editor_sheet (etoolbar);
		etoolbar->priv->editor_sheet_dirty = FALSE;
	}

	if (etoolbar->priv->editor_pos_dirty)
	{
		LOG ("Update ui: editor position (in idle)")
		g_idle_add ((GSourceFunc)update_editor_position, etoolbar);
		etoolbar->priv->editor_pos_dirty = FALSE;
	}

	return FALSE;
}

static void
queue_ui_update (EphyEditableToolbar *etoolbar)
{
	g_idle_add (ui_update, etoolbar);
}

static void
drag_data_received_cb (GtkWidget *widget,
		       GdkDragContext *context,
		       gint x,
		       gint y,
		       GtkSelectionData *selection_data,
		       guint info,
		       guint time_,
		       EphyEditableToolbar *etoolbar)
{
	GNode *toolbar;
	GNode *parent;
	GNode *sibling;
	const char *type = NULL;
	GdkAtom target;

	toolbar = (GNode *)g_object_get_data (G_OBJECT (widget), "toolbar_node");

	if (!toolbar)
	{
		sibling = (GNode *)g_object_get_data (G_OBJECT (widget), "item_node");
		parent = sibling->parent;
	}
	else
	{
		sibling = NULL;
		parent = toolbar;
	}

	target = gtk_drag_dest_find_target (widget, context, NULL);
	if (target == gdk_atom_intern (EPHY_DND_URL_TYPE, FALSE))
	{
		type = EPHY_DND_URL_TYPE;
	}

	add_action (etoolbar, parent, sibling, type, selection_data->data);

	etoolbar->priv->toolbars_dirty = TRUE;
	queue_ui_update (etoolbar);
}

static void
drag_data_delete_cb (GtkWidget *widget,
                     GdkDragContext *context,
                     EphyEditableToolbar *etoolbar)
{
	GNode *node;

	node = (GNode *)g_object_get_data (G_OBJECT (widget), "item_node");
	free_node (node);
	g_node_destroy (node);

	etoolbar->priv->toolbars_dirty = TRUE;
	queue_ui_update (etoolbar);
}

static void
drag_data_get_cb (GtkWidget *widget,
                  GdkDragContext *context,
                  GtkSelectionData *selection_data,
                  guint info,
                  guint32 time,
                  EphyEditableToolbar *etoolbar)
{
	EggAction *action;
	const char *target;

	action = EGG_ACTION (g_object_get_data (G_OBJECT (widget), "egg-action"));
	target = action->name;

	LOG ("Drag data get %s", action->name);

	gtk_selection_data_set (selection_data,
				selection_data->target,
				8, target, strlen (target));
}

static void
connect_drag_sources (EphyEditableToolbar *etoolbar, GNode *toolbars)
{
	GNode *l1, *l2;

	for (l1 = toolbars->children; l1 != NULL; l1 = l1->next)
	{
		for (l2 = l1->children; l2 != NULL; l2 = l2->next)
		{
			ItemNode *node = (ItemNode *) (l2->data);
			GtkWidget *toolitem;

			toolitem = node->widget;

			if (!g_object_get_data (G_OBJECT (toolitem), "drag_source_set"))
			{
				g_object_set_data (G_OBJECT (toolitem), "drag_source_set",
						   GINT_TO_POINTER (TRUE));

				g_signal_connect (toolitem, "drag_data_get",
						  G_CALLBACK (drag_data_get_cb),
						  etoolbar);
				g_signal_connect (toolitem, "drag_data_delete",
						  G_CALLBACK (drag_data_delete_cb),
						  etoolbar);
			}
		}
	}
}

static void
disconnect_drag_sources (EphyEditableToolbar *etoolbar, GNode *toolbars)
{
	GNode *l1, *l2;

	for (l1 = toolbars->children; l1 != NULL; l1 = l1->next)
	{
		for (l2 = l1->children; l2 != NULL; l2 = l2->next)
		{
			ItemNode *node = (ItemNode *) (l2->data);
			GtkWidget *toolitem;

			toolitem = node->widget;

			if (g_object_get_data (G_OBJECT (toolitem), "drag_source_set"))
			{
				g_object_set_data (G_OBJECT (toolitem), "drag_source_set",
						   GINT_TO_POINTER (FALSE));

				g_signal_handlers_disconnect_by_func
					(toolitem,
					 G_CALLBACK (drag_data_get_cb),
					 etoolbar);
				g_signal_handlers_disconnect_by_func
					(toolitem,
					 G_CALLBACK (drag_data_delete_cb),
					 etoolbar);
			}
		}
	}
}

static void
setup_toolbars (EphyEditableToolbar *etoolbar, GNode *toolbars)
{
	GNode *l1, *l2;
	int k = 0;
	char path[255];

	for (l1 = toolbars->children; l1 != NULL; l1 = l1->next)
	{
		int i = 0;
		GtkWidget *toolbar;
		ToolbarNode *node = (ToolbarNode *)l1->data;

		sprintf (path, "/Toolbar%d", k);
		toolbar = egg_menu_merge_get_widget (etoolbar->priv->merge, path);
		g_object_set_data (G_OBJECT (toolbar), "toolbar_node", l1);
		node->widget = toolbar;

		if (!g_object_get_data (G_OBJECT (toolbar), "drag_dest_set"))
		{
			LOG ("Setup drag dest for toolbar %s", path)
			g_object_set_data (G_OBJECT (toolbar), "drag_dest_set",
					   GINT_TO_POINTER (TRUE));
			gtk_drag_dest_set (toolbar, GTK_DEST_DEFAULT_ALL,
					   dest_drag_types, 2,
					   GDK_ACTION_MOVE | GDK_ACTION_COPY);
			g_signal_connect (toolbar, "drag_data_received",
					  G_CALLBACK (drag_data_received_cb),
					  etoolbar);
		}

		for (l2 = l1->children; l2 != NULL; l2 = l2->next)
		{
			ItemNode *node = (ItemNode *) (l2->data);
			GtkWidget *toolitem;
			const char *type;

			if (node->separator)
			{
				type ="ToolSeparator";
			}
			else
			{
				type ="ToolItem";
			}

			sprintf (path, "/Toolbar%d/PlaceHolder%d/%s", k, i, type);

			toolitem = egg_menu_merge_get_widget (etoolbar->priv->merge, path);
			node->widget = toolitem;

			g_object_set_data (G_OBJECT (toolitem), "item_node", l2);

			LOG ("Setup drag dest for toolbar item %s %p", path, toolitem);

			if (!g_object_get_data (G_OBJECT (toolitem), "drag_dest_set"))
			{
				g_object_set_data (G_OBJECT (toolitem), "drag_dest_set",
						   GINT_TO_POINTER (TRUE));
				gtk_drag_dest_set (toolitem, GTK_DEST_DEFAULT_ALL,
						   dest_drag_types, 2,
						   GDK_ACTION_COPY | GDK_ACTION_MOVE);
				g_signal_connect (toolitem, "drag_data_received",
						  G_CALLBACK (drag_data_received_cb),
						  etoolbar);
			}

			i++;
		}

		k++;
	}
}

static void
ensure_toolbars_min_size (EphyEditableToolbar *t)
{
	GNode *n;
	int i = 0;

	for (n = t->priv->toolbars->children; n != NULL; n = n->next)
	{
		ToolbarNode *node = (ToolbarNode *) (n->data);
		GtkWidget *toolbar;

		toolbar = node->widget;

		if (g_node_n_children (n) == 0)
		{
			gtk_widget_set_size_request (toolbar, -1, 20);
		}
		else
		{
			gtk_widget_set_size_request (toolbar, -1, -1);
		}

		i++;
	}
}

static void
ephy_editable_toolbar_save (EphyEditableToolbar *t)
{
	xmlDocPtr doc;

	doc = toolbar_list_to_xml (t, t->priv->toolbars);
	xmlSaveFormatFile (t->priv->filename, doc, 1);
	xmlFreeDoc (doc);
}


static void
do_merge (EphyEditableToolbar *t)
{
	char *str;
	guint ui_id;

	str = toolbar_list_to_string (t, t->priv->toolbars);

	LOG ("Merge UI\n%s", str)

	ui_id = egg_menu_merge_add_ui_from_string
		(t->priv->merge, str, -1, NULL);

	if (t->priv->ui_id != 0)
	{
		LOG ("Remove old toolbar")

		egg_menu_merge_remove_ui (t->priv->merge,
					  t->priv->ui_id);
	}

	t->priv->ui_id = ui_id;

	egg_menu_merge_ensure_update (t->priv->merge);

	setup_toolbars (t, t->priv->toolbars);

	if (t->priv->edit_mode)
	{
		connect_drag_sources (t, t->priv->toolbars);
	}

	ensure_toolbars_min_size (t);

	ephy_editable_toolbar_save (t);

	g_free (str);
}

static void
ephy_editable_toolbar_set_merge (EphyEditableToolbar *t, EggMenuMerge *merge)
{
	t->priv->merge = merge;

	LOG ("Got MenuMerge")

	load_toolbar (t);
	load_defaults (t);
	do_merge (t);
}

static void
ephy_editable_toolbar_set_property (GObject *object,
				    guint prop_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
        EphyEditableToolbar *t = EPHY_EDITABLE_TOOLBAR (object);

        switch (prop_id)
        {
		case PROP_MENU_MERGE:
		ephy_editable_toolbar_set_merge (t, g_value_get_object (value));
		break;
        }
}

static void
ephy_editable_toolbar_get_property (GObject *object,
                                    guint prop_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
        EphyEditableToolbar *t = EPHY_EDITABLE_TOOLBAR (object);

        switch (prop_id)
        {
                case PROP_MENU_MERGE:
                        g_value_set_object (value, t->priv->merge);
                        break;
        }
}

static void
ephy_editable_toolbar_class_init (EphyEditableToolbarClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_editable_toolbar_finalize;
	object_class->set_property = ephy_editable_toolbar_set_property;
	object_class->get_property = ephy_editable_toolbar_get_property;

	klass->get_action = impl_get_action;

	g_object_class_install_property (object_class,
                                         PROP_MENU_MERGE,
                                         g_param_spec_object ("MenuMerge",
                                                              "MenuMerge",
                                                              "Menu merge",
                                                              EGG_TYPE_MENU_MERGE,
                                                              G_PARAM_READWRITE));
}

static void
ephy_editable_toolbar_init (EphyEditableToolbar *t)
{
        t->priv = g_new0 (EphyEditableToolbarPrivate, 1);

	t->priv->merge = NULL;
	t->priv->available_actions = NULL;
	t->priv->toolbars = NULL;
	t->priv->filename = g_build_filename (ephy_dot_dir (), "toolbar.xml", NULL);
	t->priv->editor = NULL;
	t->priv->ui_id = 0;
	t->priv->toolbars_dirty = FALSE;
	t->priv->editor_pos_dirty = FALSE;
	t->priv->editor_sheet_dirty = FALSE;
	t->priv->edit_mode = FALSE;
}

static void
ephy_editable_toolbar_finalize (GObject *object)
{
	EphyEditableToolbar *t = EPHY_EDITABLE_TOOLBAR (object);

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (object));

	if (t->priv->editor)
	{
		gtk_widget_destroy (t->priv->editor);
	}

	g_node_children_foreach (t->priv->available_actions, G_IN_ORDER,
				 (GNodeForeachFunc)free_node, NULL);
	g_node_children_foreach (t->priv->toolbars, G_IN_ORDER,
				 (GNodeForeachFunc)free_node, NULL);
	g_node_destroy (t->priv->available_actions);
	g_node_destroy (t->priv->toolbars);

	g_free (t->priv->filename);

        g_free (t->priv);

	LOG ("EphyEditableToolbar finalized")

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

EphyEditableToolbar *
ephy_editable_toolbar_new (EggMenuMerge *merge)
{
	EphyEditableToolbar *t;

	t = EPHY_EDITABLE_TOOLBAR (g_object_new (EPHY_EDITABLE_TOOLBAR_TYPE,
				   "MenuMerge", merge,
				   NULL));

	g_return_val_if_fail (t->priv != NULL, NULL);

	return t;
}

static void
editor_get_dimensions (EphyEditableToolbar *etoolbar, GtkWidget *toolbar,
		       int *x, int *y, int *width, int *height)
{
	GtkWidget *vbox;
	GtkBin *popwin;
	GtkWidget *widget;
	GtkWidget *table;
	GtkWidget *label_zone;
	GtkWidget *action_zone;
	GtkWidget *popup;
	GtkRequisition requisition;
	int avail_height;
	int avail_width;
	int work_height;
	int work_width;

	widget = toolbar;
	popup  = etoolbar->priv->scrolled_window;
	popwin = GTK_BIN (etoolbar->priv->editor);
	vbox = etoolbar->priv->main_box;
	table = etoolbar->priv->table;
	action_zone = etoolbar->priv->action_zone;
	label_zone = etoolbar->priv->label_zone;

	gdk_window_get_origin (toolbar->window, x, y);

	*y += toolbar->allocation.y + toolbar->allocation.height;

	avail_height = gdk_screen_height () - *y;
	avail_width = gdk_screen_width () - *x;

	gtk_widget_size_request (table, &requisition);

	*width = MIN (avail_width, requisition.width);
	*height = MIN (avail_height, requisition.height);

	work_width = (2 * GTK_BIN (popwin)->child->style->xthickness +
		      2 * GTK_CONTAINER (popwin)->border_width +
		      2 * GTK_CONTAINER (popwin->child)->border_width +
		      2 * GTK_CONTAINER (vbox)->border_width +
		      2 * GTK_CONTAINER (popup)->border_width +
		      2 * GTK_CONTAINER (GTK_BIN (popup)->child)->border_width +
		      2 * GTK_BIN (popup)->child->style->xthickness);
	*width += work_width;

	work_height = (2 * GTK_BIN (popwin)->child->style->ythickness +
		       2 * GTK_CONTAINER (popwin)->border_width +
		       2 * GTK_CONTAINER (popwin->child)->border_width +
		       2 * GTK_CONTAINER (vbox)->border_width +
		       2 * GTK_CONTAINER (popup)->border_width +
		       2 * GTK_CONTAINER (GTK_BIN (popup)->child)->border_width +
		       2 * GTK_BIN (popup)->child->style->ythickness);
	*height += work_height;

	gtk_widget_size_request (label_zone, &requisition);
	*height += requisition.height;
	gtk_widget_size_request (action_zone, &requisition);
	*height += requisition.height;
	/* Vbox spacing */
	*height += 2 * 12;

	*x += (toolbar->allocation.x + toolbar->allocation.width)/2 - (*width / 2);
}

static GList *
build_to_drag_actions_list (EphyEditableToolbar *etoolbar)
{
	GNode *l;
	GList *result = NULL;

	for (l = etoolbar->priv->available_actions->children; l != NULL; l = l->next)
	{
		ItemNode *item;

		item = (ItemNode *) (l->data);

		if (!find_node_from_action (etoolbar, item->action))
		{
			result = g_list_append (result, item);
		}
	}

	return result;
}

static void
hide_editor (EphyEditableToolbar *etoolbar)
{
	gtk_grab_remove (GTK_WIDGET (etoolbar->priv->editor));
	gtk_widget_hide (GTK_WIDGET (etoolbar->priv->editor));
}

static void
editor_close_cb (GtkWidget *button, EphyEditableToolbar *etoolbar)
{
	etoolbar->priv->edit_mode = FALSE;
	disconnect_drag_sources (etoolbar, etoolbar->priv->toolbars);
	hide_editor (etoolbar);
}

static void
editor_add_toolbar_cb (GtkWidget *button, EphyEditableToolbar *etoolbar)
{
	add_toolbar (etoolbar);

	etoolbar->priv->toolbars_dirty = TRUE;
	etoolbar->priv->editor_pos_dirty = TRUE;
	queue_ui_update (etoolbar);
}

static void
editor_drag_data_received_cb (GtkWidget *widget,
		              GdkDragContext *context,
		              gint x,
		              gint y,
		              GtkSelectionData *selection_data,
		              guint info,
		              guint time_,
		              EphyEditableToolbar *etoolbar)
{
	etoolbar->priv->editor_sheet_dirty = TRUE;
	etoolbar->priv->editor_pos_dirty = TRUE;
	queue_ui_update (etoolbar);
}

static void
editor_drag_data_delete_cb (GtkWidget *widget,
                            GdkDragContext *context,
                            EphyEditableToolbar *etoolbar)
{
	etoolbar->priv->editor_sheet_dirty = TRUE;
	etoolbar->priv->editor_pos_dirty = TRUE;
	queue_ui_update (etoolbar);
}

static void
setup_editor (EphyEditableToolbar *etoolbar)
{
	GtkWidget *editor;
	GtkWidget *scrolled_window;
	GtkWidget *vbox;
	GtkWidget *label_hbox;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *bbox;
	GtkWidget *button;
	GtkWidget *frame;

	editor = gtk_window_new (GTK_WINDOW_POPUP);
	etoolbar->priv->editor = editor;
	frame = gtk_frame_new (NULL);
	gtk_widget_show (frame);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER (editor), frame);
	vbox = gtk_vbox_new (FALSE, 12);
	etoolbar->priv->main_box = vbox;
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	etoolbar->priv->scrolled_window = scrolled_window;
	gtk_widget_show (scrolled_window);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
	label_hbox = gtk_hbox_new (FALSE, 6);
	etoolbar->priv->label_zone = label_hbox;
	gtk_widget_show (label_hbox);
	gtk_box_pack_start (GTK_BOX (vbox), label_hbox, FALSE, FALSE, 0);
	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
	gtk_widget_show (image);
	gtk_box_pack_start (GTK_BOX (label_hbox), image, FALSE, FALSE, 0);
	label = gtk_label_new (_("Drag an item onto the toolbars above to add it, "
				 "from the toolbars in the items table to remove it."));
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (label_hbox), label, FALSE, TRUE, 0);
	bbox = gtk_hbutton_box_new ();
	gtk_box_set_spacing (GTK_BOX (bbox), 10);
	etoolbar->priv->action_zone = bbox;
	gtk_widget_show (bbox);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox),
			           GTK_BUTTONBOX_END);
	gtk_box_pack_start (GTK_BOX (vbox), bbox, FALSE, FALSE, 0);
	button = gtk_button_new_from_stock (GTK_STOCK_ADD);
	gtk_widget_show (button);
	gtk_button_set_label (GTK_BUTTON (button), _("Add Toolbar"));
	g_signal_connect (button, "clicked",
			  G_CALLBACK (editor_add_toolbar_cb),
			  etoolbar);
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
	button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	gtk_widget_show (button);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (editor_close_cb),
			  etoolbar);
	gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

	update_editor_sheet (etoolbar);
	update_editor_position (etoolbar);
}

static void
update_editor_sheet (EphyEditableToolbar *etoolbar)
{
	GList *l;
	GList *to_drag;
	int x, y, height, width;
	GtkWidget *table;
	GtkWidget *viewport;

	viewport = GTK_BIN (etoolbar->priv->scrolled_window)->child;
	if (viewport)
	{
		table = GTK_BIN (viewport)->child;
		gtk_container_remove (GTK_CONTAINER (viewport), table);
	}
	table = gtk_table_new (0, 0, TRUE);
	etoolbar->priv->table = table;
	gtk_container_set_border_width (GTK_CONTAINER (table), 12);
	gtk_table_set_col_spacings (GTK_TABLE (table), 12);
	gtk_table_set_row_spacings (GTK_TABLE (table), 12);
	gtk_widget_show (table);
	gtk_scrolled_window_add_with_viewport
		(GTK_SCROLLED_WINDOW (etoolbar->priv->scrolled_window), table);
	gtk_drag_dest_set (table, GTK_DEST_DEFAULT_ALL,
			   dest_drag_types, 2, GDK_ACTION_MOVE);
	g_signal_connect (table, "drag_data_received",
			  G_CALLBACK (editor_drag_data_received_cb),
			  etoolbar);

	to_drag = build_to_drag_actions_list (etoolbar);

	x = y = 0;
	width = 4;
	height = (g_list_length (to_drag) - 1) / width + 1;
	gtk_table_resize (GTK_TABLE (etoolbar->priv->table), height, width);

	for (l = to_drag; l != NULL; l = l->next)
	{
		GtkWidget *event_box;
		GtkWidget *vbox;
		GtkWidget *icon;
		GtkWidget *label;
		ItemNode *node = (ItemNode *) (l->data);
		EggAction *action = EGG_ACTION (node->action);

		event_box = gtk_event_box_new ();
		gtk_widget_show (event_box);
		gtk_drag_source_set (event_box,
				     GDK_BUTTON1_MASK,
				     source_drag_types, 1,
				     GDK_ACTION_MOVE);
		g_signal_connect (event_box, "drag_data_get",
				  G_CALLBACK (drag_data_get_cb),
				  etoolbar);
		g_signal_connect (event_box, "drag_data_delete",
				  G_CALLBACK (editor_drag_data_delete_cb),
				  etoolbar);

		g_object_set_data (G_OBJECT (event_box), "egg-action", action);

		vbox = gtk_vbox_new (0, FALSE);
		gtk_widget_show (vbox);
		gtk_container_add (GTK_CONTAINER (event_box), vbox);

		icon = gtk_image_new_from_stock
			(action->stock_id ? action->stock_id : GTK_STOCK_DND,
			 GTK_ICON_SIZE_LARGE_TOOLBAR);
		gtk_widget_show (icon);
		gtk_box_pack_start (GTK_BOX (vbox), icon, FALSE, TRUE, 0);

		label = gtk_label_new_with_mnemonic (action->label);
		gtk_widget_show (label);
		gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

		gtk_table_attach_defaults (GTK_TABLE (etoolbar->priv->table),
					   event_box, x, x + 1, y, y + 1);

		x ++;
		if (x >= width)
		{
			x = 0;
			y++;
		}
	}

	g_list_free (to_drag);
}

static gboolean
update_editor_position (EphyEditableToolbar *etoolbar)
{
	ToolbarNode *last_node;
	GtkWidget *last_toolbar;
	GtkWidget *editor;
	int x, y, height, width;

	last_node = (ToolbarNode *)(g_node_last_child (etoolbar->priv->toolbars))->data;
	last_toolbar = last_node->widget;
	g_assert (EGG_IS_TOOLBAR (last_toolbar));

	editor_get_dimensions (etoolbar, last_toolbar,
			       &x, &y, &width, &height);
	editor = etoolbar->priv->editor;
	gtk_widget_set_size_request (GTK_WIDGET (editor), width, height);
	gtk_window_move (GTK_WINDOW (editor), x, y);

	LOG ("Editor moved to %d %d %d %d", x, y, width, height)

	return FALSE;
}

static gboolean
button_press_cb (GtkWidget *w,
		 GdkEvent *event,
		 EphyEditableToolbar *etoolbar)
{
	GtkWidget *widget;
	GtkWidget *toolitem;

	widget = gtk_get_event_widget (event);
	toolitem = gtk_widget_get_ancestor (widget, EGG_TYPE_TOOL_ITEM);

	if (toolitem == NULL) return FALSE;

	switch (event->type)
	{
		case GDK_BUTTON_PRESS:
			gtk_drag_begin (toolitem,
					gtk_target_list_new (source_drag_types, 1),
					GDK_ACTION_MOVE, 1, event);
			return TRUE;
		default:
			break;
	}

	return FALSE;
}

static void
show_editor (EphyEditableToolbar *etoolbar)
{
	GtkWidget *editor = etoolbar->priv->editor;

	g_return_if_fail (editor != NULL);

	gtk_widget_show (GTK_WIDGET (editor));
	gtk_grab_add (editor);

	g_signal_connect (editor, "button_press_event",
			  G_CALLBACK (button_press_cb),
			  etoolbar);
}

static void
set_all_actions_sensitive (EphyEditableToolbar *etoolbar)
{
	GNode *l;

	for (l = etoolbar->priv->available_actions->children; l != NULL; l = l->next)
	{
		ItemNode *node = (ItemNode *) (l->data);

		g_object_set (node->action, "sensitive", TRUE, NULL);
	}
}

void
ephy_editable_toolbar_edit (EphyEditableToolbar *etoolbar)
{
	etoolbar->priv->edit_mode = TRUE;

	connect_drag_sources (etoolbar, etoolbar->priv->toolbars);
	set_all_actions_sensitive (etoolbar);
	setup_editor (etoolbar);
	show_editor (etoolbar);
}

EggAction *
ephy_editable_toolbar_get_action (EphyEditableToolbar *etoolbar,
				  const char *type,
				  const char *name)
{
	 EphyEditableToolbarClass *klass = EPHY_EDITABLE_TOOLBAR_GET_CLASS (etoolbar);
	 return klass->get_action (etoolbar, type, name);
}
