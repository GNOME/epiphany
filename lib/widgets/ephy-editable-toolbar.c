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
#include "ephy-toolbars-group.h"
#include "ephy-debug.h"
#include "ephy-dnd.h"
#include "eggtoolitem.h"
#include "eggtoolbar.h"
#include "eggseparatortoolitem.h"

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

enum
{
	RESPONSE_ADD_TOOLBAR
};

static void	ephy_editable_toolbar_class_init (EphyEditableToolbarClass *klass);
static void	ephy_editable_toolbar_init	 (EphyEditableToolbar *t);
static void	ephy_editable_toolbar_finalize	 (GObject *object);
static void	do_merge			 (EphyEditableToolbar *t);
static void	setup_editor			 (EphyEditableToolbar *etoolbar,
						  GtkWidget *window);
static void	update_editor_sheet		 (EphyEditableToolbar *etoolbar);

enum
{
	PROP_0,
	PROP_TOOLBARS_GROUP,
	PROP_MENU_MERGE
};

static GObjectClass *parent_class = NULL;

struct EphyEditableToolbarPrivate
{
	EggMenuMerge *merge;

	GtkWidget *editor;
	GtkWidget *table;
	GtkWidget *scrolled_window;

	GtkWidget *last_toolbar;

	guint ui_id;

	gboolean toolbars_dirty;
	gboolean editor_sheet_dirty;
	gboolean edit_mode;

	EphyToolbarsGroup *group;

	GList *actions_list;
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

	g_return_val_if_fail (IS_EPHY_EDITABLE_TOOLBAR (t), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	for (; l != NULL; l = l->next)
	{
		EggAction *tmp;

		tmp = egg_action_group_get_action
			(EGG_ACTION_GROUP (l->data), name);
		if (tmp) action = tmp;
	}

	return action;
}

static EggAction *
impl_get_action (EphyEditableToolbar *etoolbar,
		 const char *type,
		 const char *name)
{
	EggAction *action;

	g_return_val_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar), NULL);

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

static gboolean
ui_update (gpointer data)
{
	EphyEditableToolbar *etoolbar = EPHY_EDITABLE_TOOLBAR (data);

	g_return_val_if_fail (etoolbar != NULL, FALSE);

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

	return FALSE;
}

static void
queue_ui_update (EphyEditableToolbar *etoolbar)
{
	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));

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
	EphyToolbarsToolbar *toolbar;
	EphyToolbarsToolbar *parent;
	EphyToolbarsItem *sibling;
	const char *type = NULL;
	GdkAtom target;
	EggAction *action;

	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));

	toolbar = (EphyToolbarsToolbar *)g_object_get_data (G_OBJECT (widget), "toolbar_data");

	if (!toolbar)
	{
		sibling = (EphyToolbarsItem *)g_object_get_data (G_OBJECT (widget), "item_data");
		g_return_if_fail (sibling != NULL);
		parent = sibling->parent;
	}
	else
	{
		sibling = NULL;
		parent = toolbar;
	}

	g_return_if_fail (parent != NULL);

	target = gtk_drag_dest_find_target (widget, context, NULL);
	if (target == gdk_atom_intern (EPHY_DND_URL_TYPE, FALSE))
	{
		type = EPHY_DND_URL_TYPE;
	}

	action = ephy_editable_toolbar_get_action (etoolbar, type, selection_data->data);
	if (action)
	{
		ephy_toolbars_group_add_item (etoolbar->priv->group, parent, sibling,
					      action->name);
		etoolbar->priv->toolbars_dirty = TRUE;
		queue_ui_update (etoolbar);
	}
}

static void
drag_data_delete_cb (GtkWidget *widget,
                     GdkDragContext *context,
                     EphyEditableToolbar *etoolbar)
{
	EphyToolbarsItem *node;

	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));

	node = (EphyToolbarsItem *)g_object_get_data (G_OBJECT (widget), "item_data");
	g_return_if_fail (node != NULL);
	ephy_toolbars_group_remove_item (etoolbar->priv->group, node);

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

	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));

	action = EGG_ACTION (g_object_get_data (G_OBJECT (widget), "egg-action"));
	target = action->name;

	LOG ("Drag data get %s", action->name);

	gtk_selection_data_set (selection_data,
				selection_data->target,
				8, target, strlen (target));
}

static GtkWidget *
get_item_widget (EphyEditableToolbar *t, gpointer data)
{
	GtkWidget *widget;
	char *path;

	path = ephy_toolbars_group_get_path (t->priv->group, data);
	g_return_val_if_fail (path != NULL, NULL);

	widget = egg_menu_merge_get_widget (t->priv->merge, path);
	g_free (path);

	return widget;
}

static void
connect_item_drag_source (EphyToolbarsItem *item, EphyEditableToolbar *etoolbar)
{
	GtkWidget *toolitem;

	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));
	g_return_if_fail (item != NULL);

	toolitem = get_item_widget (etoolbar, item);

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

static void
disconnect_item_drag_source (EphyToolbarsItem *item, EphyEditableToolbar *etoolbar)
{
	GtkWidget *toolitem;

	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));
	g_return_if_fail (item != NULL);

	toolitem = get_item_widget (etoolbar, item);

	if (g_object_get_data (G_OBJECT (toolitem), "drag_source_set"))
	{
		g_object_set_data (G_OBJECT (toolitem), "drag_source_set",
				   GINT_TO_POINTER (FALSE));

		g_signal_handlers_disconnect_by_func (toolitem,
						      G_CALLBACK (drag_data_get_cb),
						      etoolbar);
		g_signal_handlers_disconnect_by_func (toolitem,
						      G_CALLBACK (drag_data_delete_cb),
						      etoolbar);
	}
}

static void
setup_toolbar (EphyToolbarsToolbar *toolbar, EphyEditableToolbar *etoolbar)
{
	GtkWidget *widget;

	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));
	g_return_if_fail (toolbar != NULL);

	widget = get_item_widget (etoolbar, toolbar);
	g_object_set_data (G_OBJECT (widget), "toolbar_data", toolbar);

	if (!g_object_get_data (G_OBJECT (widget), "drag_dest_set"))
	{
		LOG ("Setup drag dest for toolbar %s", toolbar->id)
		g_object_set_data (G_OBJECT (widget), "drag_dest_set",
				   GINT_TO_POINTER (TRUE));
		gtk_drag_dest_set (widget, GTK_DEST_DEFAULT_ALL,
				   dest_drag_types, 2,
				   GDK_ACTION_MOVE | GDK_ACTION_COPY);
		g_signal_connect (widget, "drag_data_received",
				  G_CALLBACK (drag_data_received_cb),
				  etoolbar);
	}

	etoolbar->priv->last_toolbar = widget;
}

static void
setup_item (EphyToolbarsItem *item, EphyEditableToolbar *etoolbar)
{
	GtkWidget *toolitem;
	char *path;

	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));
	g_return_if_fail (item != NULL);

	path = ephy_toolbars_group_get_path (etoolbar->priv->group, item);
	g_return_if_fail (path != NULL);

	toolitem = get_item_widget (etoolbar, item);
	g_object_set_data (G_OBJECT (toolitem), "item_data", item);

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

	g_free (path);
}

static void
ensure_toolbar_min_size (EphyToolbarsToolbar *toolbar, EphyEditableToolbar *t)
{
	GtkWidget *widget;

	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (t));
	g_return_if_fail (toolbar != NULL);

	widget = get_item_widget (t, toolbar);

	if (EGG_TOOLBAR (widget)->num_children == 0)
	{
		gtk_widget_set_size_request (widget, -1, 20);
	}
	else
	{
		gtk_widget_set_size_request (widget, -1, -1);
	}
}

static void
do_merge (EphyEditableToolbar *t)
{
	char *str;
	guint ui_id;

	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (t));

	str = ephy_toolbars_group_to_string (t->priv->group);
	g_return_if_fail (str != NULL);

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

	ephy_toolbars_group_foreach_toolbar (t->priv->group,
					     (EphyToolbarsGroupForeachToolbarFunc)
					     setup_toolbar, t);
	ephy_toolbars_group_foreach_item (t->priv->group,
					  (EphyToolbarsGroupForeachItemFunc)
					  setup_item, t);

	if (t->priv->edit_mode)
	{
		ephy_toolbars_group_foreach_item (t->priv->group,
						  (EphyToolbarsGroupForeachItemFunc)
					          connect_item_drag_source, t);
	}

	ephy_toolbars_group_foreach_toolbar (t->priv->group,
					     (EphyToolbarsGroupForeachToolbarFunc)
					     ensure_toolbar_min_size, t);

	g_free (str);
}

static void
ensure_action (EphyToolbarsItem *item, EphyEditableToolbar *t)
{
	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (t));
	g_return_if_fail (item != NULL);

	ephy_editable_toolbar_get_action (t, NULL, item->action);
}

static void
group_changed_cb (EphyToolbarsGroup *group, EphyEditableToolbar *t)
{
	g_return_if_fail (IS_EPHY_TOOLBARS_GROUP (group));
	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (t));

	t->priv->toolbars_dirty = TRUE;

	ephy_toolbars_group_foreach_item (t->priv->group,
					  (EphyToolbarsGroupForeachItemFunc)
				          ensure_action, t);

	queue_ui_update (t);
}

static void
ephy_editable_toolbar_set_group (EphyEditableToolbar *t, EphyToolbarsGroup *group)
{
	g_return_if_fail (IS_EPHY_TOOLBARS_GROUP (group));
	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (t));

	t->priv->group = group;

	g_signal_connect_object (group, "changed",
			         G_CALLBACK (group_changed_cb),
			         t, 0);
}

static void
ephy_editable_toolbar_set_merge (EphyEditableToolbar *t, EggMenuMerge *merge)
{
	g_return_if_fail (EGG_IS_MENU_MERGE (merge));
	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (t));

	t->priv->merge = merge;

	LOG ("Got MenuMerge")

	ephy_toolbars_group_foreach_item (t->priv->group,
					  (EphyToolbarsGroupForeachItemFunc)
				          ensure_action, t);

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
		case PROP_TOOLBARS_GROUP:
		ephy_editable_toolbar_set_group (t, g_value_get_object (value));
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
                case PROP_TOOLBARS_GROUP:
                        g_value_set_object (value, t->priv->group);
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
	g_object_class_install_property (object_class,
                                         PROP_TOOLBARS_GROUP,
                                         g_param_spec_object ("ToolbarsGroup",
                                                              "ToolbarsGroup",
                                                              "Toolbars Group",
                                                              EPHY_TOOLBARS_GROUP_TYPE,
                                                              G_PARAM_READWRITE));
}

static void
ephy_editable_toolbar_init (EphyEditableToolbar *t)
{
        t->priv = g_new0 (EphyEditableToolbarPrivate, 1);

	t->priv->merge = NULL;
	t->priv->editor = NULL;
	t->priv->ui_id = 0;
	t->priv->toolbars_dirty = FALSE;
	t->priv->editor_sheet_dirty = FALSE;
	t->priv->edit_mode = FALSE;
	t->priv->actions_list = NULL;
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
hide_editor (EphyEditableToolbar *etoolbar)
{
	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));

	gtk_grab_remove (GTK_WIDGET (etoolbar->priv->editor));
	gtk_widget_hide (GTK_WIDGET (etoolbar->priv->editor));
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
	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));

	etoolbar->priv->editor_sheet_dirty = TRUE;
	queue_ui_update (etoolbar);
}

static void
editor_drag_data_delete_cb (GtkWidget *widget,
                            GdkDragContext *context,
                            EphyEditableToolbar *etoolbar)
{
	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));

	etoolbar->priv->editor_sheet_dirty = TRUE;
	queue_ui_update (etoolbar);
}

static void
editor_close (EphyEditableToolbar *etoolbar)
{
	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));

	etoolbar->priv->edit_mode = FALSE;
	ephy_toolbars_group_foreach_item (etoolbar->priv->group,
					  (EphyToolbarsGroupForeachItemFunc)
				          disconnect_item_drag_source,
					  etoolbar);
	hide_editor (etoolbar);
}

static void
editor_add_toolbar (EphyEditableToolbar *etoolbar)
{
	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));

	ephy_toolbars_group_add_toolbar (etoolbar->priv->group);

	etoolbar->priv->toolbars_dirty = TRUE;
	queue_ui_update (etoolbar);
}

static void
dialog_response_cb (GtkDialog *dialog, gint response_id,
		    EphyEditableToolbar *etoolbar)
{
	switch (response_id)
	{
		case RESPONSE_ADD_TOOLBAR:
			editor_add_toolbar (etoolbar);
			break;
		case GTK_RESPONSE_CLOSE:
			editor_close (etoolbar);
			break;
	}
}

static void
setup_editor (EphyEditableToolbar *etoolbar, GtkWidget *window)
{
	GtkWidget *editor;
	GtkWidget *scrolled_window;
	GtkWidget *vbox;
	GtkWidget *label_hbox;
	GtkWidget *image;
	GtkWidget *label;

	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));

	editor = gtk_dialog_new (),
	gtk_dialog_set_has_separator (GTK_DIALOG (editor), FALSE);
	gtk_widget_set_size_request (GTK_WIDGET (editor), 500, 330);
	gtk_window_set_transient_for (GTK_WINDOW (editor), GTK_WINDOW (window));
	gtk_window_set_title (GTK_WINDOW (editor), "Toolbar editor");
	etoolbar->priv->editor = editor;

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (editor)->vbox), vbox);
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	etoolbar->priv->scrolled_window = scrolled_window;
	gtk_widget_show (scrolled_window);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
	label_hbox = gtk_hbox_new (FALSE, 6);
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

	gtk_dialog_add_button (GTK_DIALOG (editor),
			       _("Add Toolbar"),
			       RESPONSE_ADD_TOOLBAR);
	gtk_dialog_add_button (GTK_DIALOG (editor),
			       GTK_STOCK_CLOSE,
			       GTK_RESPONSE_CLOSE);

	g_signal_connect (editor, "response",
			  G_CALLBACK (dialog_response_cb),
			  etoolbar);

	update_editor_sheet (etoolbar);
}

static void
add_to_list (EphyToolbarsItem *item, GList **l)
{
	g_return_if_fail (item != NULL);

	*l = g_list_append (*l, item);
}

static void
update_editor_sheet (EphyEditableToolbar *etoolbar)
{
	GList *l;
	GList *to_drag = NULL;
	int x, y, height, width;
	GtkWidget *table;
	GtkWidget *viewport;

	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));

	viewport = GTK_BIN (etoolbar->priv->scrolled_window)->child;
	if (viewport)
	{
		table = GTK_BIN (viewport)->child;
		gtk_container_remove (GTK_CONTAINER (viewport), table);
	}
	table = gtk_table_new (0, 0, TRUE);
	etoolbar->priv->table = table;
	gtk_container_set_border_width (GTK_CONTAINER (table), 12);
	gtk_widget_show (table);
	gtk_scrolled_window_add_with_viewport
		(GTK_SCROLLED_WINDOW (etoolbar->priv->scrolled_window), table);
	gtk_drag_dest_set (table, GTK_DEST_DEFAULT_ALL,
			   dest_drag_types, 2, GDK_ACTION_MOVE);
	g_signal_connect (table, "drag_data_received",
			  G_CALLBACK (editor_drag_data_received_cb),
			  etoolbar);

	ephy_toolbars_group_foreach_available (etoolbar->priv->group,
					       (EphyToolbarsGroupForeachItemFunc)
					       add_to_list, &to_drag);

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
		EphyToolbarsItem *node = (EphyToolbarsItem *) (l->data);
		EggAction *action;

		action = ephy_editable_toolbar_get_action
			(etoolbar, NULL, node->action);
		g_return_if_fail (action != NULL);

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
button_press_cb (GtkWidget *w,
		 GdkEvent *event,
		 EphyEditableToolbar *etoolbar)
{
	GtkWidget *widget;
	GtkWidget *toolitem;

	g_return_val_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar), FALSE);

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

	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));
	g_return_if_fail (editor != NULL);

	gtk_widget_show (GTK_WIDGET (editor));
	gtk_grab_add (editor);

	g_signal_connect (editor, "button_press_event",
			  G_CALLBACK (button_press_cb),
			  etoolbar);
}

static void
set_action_sensitive (EphyToolbarsItem *item, EphyEditableToolbar *etoolbar)
{
	EggAction *action;

	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));
	g_return_if_fail (item != NULL);

	if (!item->separator)
	{
		action = find_action (etoolbar, item->action);
		g_object_set (action, "sensitive", TRUE, NULL);
	}
}

void
ephy_editable_toolbar_edit (EphyEditableToolbar *etoolbar, GtkWidget *window)
{
	g_return_if_fail (IS_EPHY_EDITABLE_TOOLBAR (etoolbar));

	etoolbar->priv->edit_mode = TRUE;
	ephy_toolbars_group_foreach_item (etoolbar->priv->group,
					  (EphyToolbarsGroupForeachItemFunc)
				          connect_item_drag_source, etoolbar);
	ephy_toolbars_group_foreach_item (etoolbar->priv->group,
					  (EphyToolbarsGroupForeachItemFunc)
				          set_action_sensitive, etoolbar);
	setup_editor (etoolbar, window);
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
