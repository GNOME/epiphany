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

#include <libxml/parser.h>
#include <string.h>

/* This is copied from gtkscrollbarwindow.c */
#define DEFAULT_SCROLLBAR_SPACING  3

#define SCROLLBAR_SPACING(w)                                                            \
  (GTK_SCROLLED_WINDOW_GET_CLASS (w)->scrollbar_spacing >= 0 ?                          \
   GTK_SCROLLED_WINDOW_GET_CLASS (w)->scrollbar_spacing : DEFAULT_SCROLLBAR_SPACING)

static GtkTargetEntry drag_types [] =
{
        { "EPHY_TOOLBAR_BUTTON", 0, 0 }
};

static void ephy_editable_toolbar_class_init (EphyEditableToolbarClass *klass);
static void ephy_editable_toolbar_init (EphyEditableToolbar *t);
static void ephy_editable_toolbar_finalize (GObject *object);

enum
{
	PROP_0,
	PROP_MENU_MERGE
};

enum
{
	REQUEST_ACTION,
	LAST_SIGNAL
};

static GObjectClass *parent_class = NULL;

static gint EphyEditableToolbarSignals[LAST_SIGNAL];

struct EphyEditableToolbarPrivate
{
	EggMenuMerge *merge;
	EggAction *separator;
	GList *available_actions;
	GList *toolbars;
	char *filename;

	GtkWidget *editor;
	GtkWidget *table;
};

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

static void
add_action_to_list (EphyEditableToolbar *t, GList **list, const char *name)
{
	EggAction *action = NULL;

	action = find_action (t, name);

	if (!action)
	{
		g_signal_emit (t, EphyEditableToolbarSignals[REQUEST_ACTION], 0, name);
	}

	action = find_action (t, name);

	if (action)
	{
		*list = g_list_append (*list, action);
	}
}

static void
parse_item_list (EphyEditableToolbar *t,
		 xmlNodePtr child,
		 GList **actions)
{
	while (child)
	{
		if (xmlStrEqual (child->name, "toolitem"))
		{
			xmlChar *verb;

			verb = xmlGetProp (child, "verb");
			add_action_to_list (t, actions, verb);

			xmlFree (verb);
		}
		else if (xmlStrEqual (child->name, "separator"))
		{
			*actions = g_list_append
				(*actions, t->priv->separator);
		}

		child = child->next;
	}
}

static void
parse_toolbars (EphyEditableToolbar *t,
		xmlNodePtr child,
		GList **toolbars)
{
	while (child)
	{
		if (xmlStrEqual (child->name, "toolbar"))
		{
			GList *list = NULL;

			parse_item_list (t, child->children, &list);
			*toolbars = g_list_append (*toolbars, list);
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
			parse_item_list (t, child->children,
					 &t->priv->available_actions);
		}
		else if (xmlStrEqual (child->name, "default") &&
			 t->priv->toolbars == NULL)
		{
			parse_toolbars (t, child->children,
					&t->priv->toolbars);
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

	parse_toolbars (t, root->children,
			&t->priv->toolbars);
}

static xmlDocPtr
toolbar_list_to_xml (EphyEditableToolbar *t, GList *tl)
{
	GList *l1, *l2;
	xmlDocPtr doc;

	xmlIndentTreeOutput = TRUE;
	doc = xmlNewDoc ("1.0");
	doc->children = xmlNewDocNode (doc, NULL, "toolbars", NULL);

	for (l1 = tl; l1 != NULL; l1 = l1->next)
	{
		xmlNodePtr tnode;

		tnode = xmlNewChild (doc->children, NULL, "toolbar", NULL);

		for (l2 = l1->data; l2 != NULL; l2 = l2->next)
		{
			xmlNodePtr node;
			EggAction *action = EGG_ACTION (l2->data);

			if (action == t->priv->separator)
			{
				 node = xmlNewChild (tnode, NULL, "separator", NULL);
			}
			else
			{
				node = xmlNewChild (tnode, NULL, "toolitem", NULL);
				xmlSetProp (node, "verb", action->name);
			}
		}
	}

	return doc;
}

static char *
toolbar_list_to_string (EphyEditableToolbar *t, GList *tl)
{
	GString *s;
	GList *l1, *l2;
	char *result;

	s = g_string_new (NULL);
	g_string_append (s, "<Root>");
	for (l1 = tl; l1 != NULL; l1 = l1->next)
	{
		int i = 0;

		g_string_append_printf
			(s, "<dockitem name=\"Toolbar%d\">",
			 g_list_index (tl, l1->data));

		for (l2 = l1->data; l2 != NULL; l2 = l2->next)
		{
			EggAction *action = EGG_ACTION (l2->data);

			if (action == t->priv->separator)
			{
				g_string_append_printf
					(s, "<separator name=\"ToolItem%d\"/>", i);
			}
			else
			{
				g_string_append_printf
					(s, "<toolitem name=\"ToolItem%d\" verb=\"%s\"/>",
					 i, action->name);
			}

			i++;
		}

		g_string_append (s, "</dockitem>");
	}
	g_string_append (s, "</Root>");

	result = s->str;

	g_string_free (s, FALSE);

	return result;
}

static void
drag_data_received_cb (GtkWidget *widget,
		       GdkDragContext *context,
		       gint x,
		       gint y,
		       GtkSelectionData *selection_data,
		       guint info,
		       guint time_)
{
	LOG ("Received data, action is %s", (char *)selection_data->data);
}

static gboolean
drag_motion_cb (GtkWidget *widget,
		GdkDragContext *context,
		gint x,
		gint y,
		guint time)
{
	GdkAtom target;

	LOG ("Motion")

	target = gtk_drag_dest_find_target (widget, context, NULL);
        if (target != GDK_NONE)
	{
		gtk_drag_get_data (widget, context, target, time);
	}

	return TRUE;
}

static void
setup_toolbar_drag (EphyEditableToolbar *etoolbar)
{
	GList *l1, *l2;
	int k = 0;

	for (l1 = etoolbar->priv->toolbars; l1 != NULL; l1 = l1->next)
	{
		int i = 0;

		for (l2 = l1->data; l2 != NULL; l2 = l2->next)
		{
			GtkWidget *toolitem;
			char path[255];

			sprintf (path, "/Toolbar%d/ToolItem%d", k, i);
			LOG ("Setup drag dest for toolbar item %s", path);

			toolitem = egg_menu_merge_get_widget (etoolbar->priv->merge, path);
			gtk_drag_dest_set (GTK_WIDGET (toolitem), 0,
					   drag_types, 1, GDK_ACTION_MOVE);
			g_signal_connect (toolitem, "drag_motion",
					  G_CALLBACK (drag_motion_cb),
					  etoolbar);
			g_signal_connect (toolitem, "drag_data_received",
					  G_CALLBACK (drag_data_received_cb),
					  etoolbar);
			i++;
		}

		k++;
	}
}

static void
do_merge (EphyEditableToolbar *t)
{
	GList *tl;
	char *str;

	tl = t->priv->toolbars;

	str = toolbar_list_to_string (t, tl);

	LOG ("Merge UI\n%s", str)

	egg_menu_merge_add_ui_from_string
		(t->priv->merge, str, -1, NULL);
	egg_menu_merge_ensure_update (t->priv->merge);

	setup_toolbar_drag (t);

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

	EphyEditableToolbarSignals[REQUEST_ACTION] =
		g_signal_new
		("request_action", G_OBJECT_CLASS_TYPE (klass),
		 G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                 G_STRUCT_OFFSET (EphyEditableToolbarClass, request_action),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__STRING,
		 G_TYPE_NONE, 1,
		 G_TYPE_STRING);

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
	t->priv->separator = g_object_new (EGG_TYPE_ACTION, NULL);
	t->priv->editor = NULL;
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
ephy_editable_toolbar_finalize (GObject *object)
{
	EphyEditableToolbar *t = EPHY_EDITABLE_TOOLBAR (object);

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (object));

	ephy_editable_toolbar_save (t);

	g_object_unref (t->priv->separator);
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
editor_get_dimensions (GtkWidget *window, GtkWidget *toolbar, GtkWidget *table,
		       int *x, int *y, int *width, int *height)
{
	GtkBin *popwin;
	GtkWidget *widget;
	GtkScrolledWindow *popup;
	GtkRequisition table_requisition;
	int real_height;
	int real_width;
	int avail_height;
	int avail_width;
	int work_height;
	int work_width;

	widget = toolbar;
	popup  = GTK_SCROLLED_WINDOW (GTK_BIN (window)->child);
	popwin = GTK_BIN (window);

	gdk_window_get_origin (widget->window, x, y);
	real_height = MIN (widget->requisition.height,
			   widget->allocation.height);
	real_width = MIN (widget->requisition.width,
			  widget->allocation.width);
	*y += real_height;
	avail_height = gdk_screen_height () - *y;
	avail_width = gdk_screen_width () - *x;

	gtk_widget_size_request (table, &table_requisition);

	*width = MIN (avail_width, table_requisition.width);
	*height = MIN (avail_height, table_requisition.height);

	work_width = (2 * popwin->child->style->xthickness +
		      2 * GTK_CONTAINER (window)->border_width +
		      2 * GTK_CONTAINER (popwin->child)->border_width +
		      2 * GTK_CONTAINER (popup)->border_width +
		      2 * GTK_CONTAINER (GTK_BIN (popup)->child)->border_width +
		      2 * GTK_BIN (popup)->child->style->xthickness);
	*width += work_width;

	work_height = (2 * popwin->child->style->ythickness +
		       2 * GTK_CONTAINER (window)->border_width +
		       2 * GTK_CONTAINER (popwin->child)->border_width +
		       2 * GTK_CONTAINER (popup)->border_width +
		       2 * GTK_CONTAINER (GTK_BIN (popup)->child)->border_width +
		       2 * GTK_BIN (popup)->child->style->ythickness);
	*height += work_height;

	*x = *x + (real_width/2 - *width/2);
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
/*	gtk_selection_data_set (selection_data,
				selection_data->target,
				8, target, strlen (target));*/
}

static void
setup_editor (EphyEditableToolbar *etoolbar)
{
	GList *l;
	int x, y, height, width;

	if (etoolbar->priv->editor == NULL)
	{
		GtkWidget *editor;
		GtkWidget *scrolled_window;
		GtkWidget *table;

		editor = gtk_window_new (GTK_WINDOW_POPUP);
		gtk_container_set_border_width (GTK_CONTAINER (editor), 12);
		scrolled_window = gtk_scrolled_window_new (NULL, NULL);
		gtk_widget_show (scrolled_window);
		gtk_scrolled_window_set_shadow_type
			(GTK_SCROLLED_WINDOW(scrolled_window), GTK_SHADOW_IN);
		gtk_scrolled_window_set_policy
			(GTK_SCROLLED_WINDOW (scrolled_window),
                         GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_container_add (GTK_CONTAINER (editor), scrolled_window);
		table = gtk_table_new (0, 0, TRUE);
		gtk_table_set_col_spacings (GTK_TABLE (table), 12);
		gtk_table_set_row_spacings (GTK_TABLE (table), 12);
		gtk_widget_show (table);
		gtk_scrolled_window_add_with_viewport
			(GTK_SCROLLED_WINDOW (scrolled_window), table);

		etoolbar->priv->editor = editor;
		etoolbar->priv->table = table;
	}

	x = y = 0;
	width = 4;
	height = (g_list_length (etoolbar->priv->available_actions) - 1) / width + 1;
	gtk_table_resize (GTK_TABLE (etoolbar->priv->table), height, width);

	for (l = etoolbar->priv->available_actions; l != NULL; l = l->next)
	{
		GtkWidget *event_box;
		GtkWidget *vbox;
		GtkWidget *icon;
		GtkWidget *label;
		EggAction *action = EGG_ACTION (l->data);

		event_box = gtk_event_box_new ();
		gtk_widget_show (event_box);
		gtk_drag_source_set (event_box,
				     GDK_BUTTON1_MASK,
				     drag_types, 1,
				     GDK_ACTION_MOVE);
		g_signal_connect (event_box, "drag_data_get",
				  G_CALLBACK (drag_data_get_cb),
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
}

static void
show_editor (EphyEditableToolbar *etoolbar, GtkWidget *toolbar)
{
	GtkWidget *editor = etoolbar->priv->editor;
	int x, y, width, height;

	g_return_if_fail (editor != NULL);

	editor_get_dimensions (GTK_WIDGET (editor), toolbar,
			       etoolbar->priv->table, &x, &y, &width, &height);
	gtk_widget_set_size_request (GTK_WIDGET (editor), width, height);
	gtk_window_move (GTK_WINDOW (editor), x, y);
	gtk_widget_show (GTK_WIDGET (editor));
	gtk_grab_add (editor);
}

void
ephy_editable_toolbar_edit (EphyEditableToolbar *etoolbar)
{
	GtkWidget *last_toolbar;
	char path[255];

	sprintf (path, "/Toolbar%d", g_list_length (etoolbar->priv->toolbars) - 1);
	last_toolbar = egg_menu_merge_get_widget (etoolbar->priv->merge, path);

	setup_editor (etoolbar);
	show_editor (etoolbar, last_toolbar);
}
