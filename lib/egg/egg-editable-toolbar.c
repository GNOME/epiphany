/*
 *  Copyright (C) 2003 Marco Pesenti Gritti
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

#include "egg-editable-toolbar.h"
#include "egg-toolbars-group.h"
#include "eggtoolitem.h"
#include "eggtoolbar.h"
#include "eggseparatortoolitem.h"
#include "eggintl.h"

#include <string.h>

#define EGG_TOOLBAR_ITEM_TYPE "application/x-toolbar-item"

enum
{
  X_TOOLBAR_ITEM
};

static GtkTargetEntry dest_drag_types[] = {
  {EGG_TOOLBAR_ITEM_TYPE, 0, X_TOOLBAR_ITEM},
};

static int n_dest_drag_types = G_N_ELEMENTS (dest_drag_types);

static GtkTargetEntry source_drag_types[] = {
  {EGG_TOOLBAR_ITEM_TYPE, 0, X_TOOLBAR_ITEM},
};

static int n_source_drag_types = G_N_ELEMENTS (source_drag_types);

enum
{
  RESPONSE_ADD_TOOLBAR
};

static void egg_editable_toolbar_class_init	(EggEditableToolbarClass *klass);
static void egg_editable_toolbar_init		(EggEditableToolbar *t);
static void egg_editable_toolbar_finalize	(GObject *object);
static void do_merge				(EggEditableToolbar *t);
static void setup_editor			(EggEditableToolbar *etoolbar,
						 GtkWidget          *window);
static void update_editor_sheet			(EggEditableToolbar *etoolbar);
static void egg_editable_toolbar_remove_cb	(EggAction          *action,
						 EggEditableToolbar *etoolbar);
static void egg_editable_toolbar_edit_cb	(EggAction          *action,
						 EggEditableToolbar *etoolbar);

static EggActionGroupEntry egg_toolbar_popups[] = {
  /* Toplevel */
  {"FakeToplevel", (""), NULL, NULL, NULL, NULL, NULL},

  /* Popups */
  {"RemoveToolbarPopup", N_("_Remove Toolbar"), GTK_STOCK_REMOVE, NULL,
   NULL, G_CALLBACK (egg_editable_toolbar_remove_cb), NULL},
  {"EditToolbarPopup", N_("_Edit Toolbars..."), GTK_STOCK_PREFERENCES, NULL,
   NULL, G_CALLBACK (egg_editable_toolbar_edit_cb), NULL},
};

static guint egg_toolbar_popups_n_entries = G_N_ELEMENTS (egg_toolbar_popups);

enum
{
  PROP_0,
  PROP_TOOLBARS_GROUP,
  PROP_MENU_MERGE
};

static GObjectClass *parent_class = NULL;

struct EggEditableToolbarPrivate
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

  EggToolbarsGroup *group;

  EggMenuMerge *popup_merge;
  EggActionGroup *popup_action_group;

  GList *actions_list;

  GList *drag_types;
};

typedef struct
{
  EggEditableToolbar *etoolbar;
  EggToolbarsToolbar *t;
} ContextMenuData;

GType
egg_editable_toolbar_get_type (void)
{
  static GType egg_editable_toolbar_type = 0;

  if (egg_editable_toolbar_type == 0)
    {
      static const GTypeInfo our_info = {
	sizeof (EggEditableToolbarClass),
	NULL,			/* base_init */
	NULL,			/* base_finalize */
	(GClassInitFunc) egg_editable_toolbar_class_init,
	NULL,
	NULL,			/* class_data */
	sizeof (EggEditableToolbar),
	0,			/* n_preallocs */
	(GInstanceInitFunc) egg_editable_toolbar_init
      };

      egg_editable_toolbar_type = g_type_register_static (G_TYPE_OBJECT,
							  "EggEditableToolbar",
							  &our_info, 0);
    }

  return egg_editable_toolbar_type;
}

static void
update_popup_menu (EggEditableToolbar *t)
{
  EggAction *action;

  action = egg_action_group_get_action (t->priv->popup_action_group,
					"EditToolbarPopup");
  g_object_set (G_OBJECT (action), "visible", !t->priv->edit_mode, NULL);

  action = egg_action_group_get_action (t->priv->popup_action_group,
					"RemoveToolbarPopup");
  g_object_set (G_OBJECT (action), "visible", t->priv->edit_mode, NULL);
}

static EggAction *
find_action (EggEditableToolbar *t,
	     const char         *name)
{
  GList *l = t->priv->merge->action_groups;
  EggAction *action = NULL;

  g_return_val_if_fail (IS_EGG_EDITABLE_TOOLBAR (t), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  for (; l != NULL; l = l->next)
    {
      EggAction *tmp;

      tmp = egg_action_group_get_action (EGG_ACTION_GROUP (l->data), name);
      if (tmp)
	action = tmp;
    }

  return action;
}

static char *
impl_get_action_name (EggEditableToolbar *etoolbar,
		      const char         *drag_type,
		      const char	 *data)
{
  return NULL;
}

static EggAction *
impl_get_action (EggEditableToolbar *etoolbar,
		 const char	    *name)
{
  EggAction *action;

  g_return_val_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar), NULL);

  action = find_action (etoolbar, name);

  return action;
}

static gboolean
ui_update (gpointer data)
{
  EggEditableToolbar *etoolbar = EGG_EDITABLE_TOOLBAR (data);

  g_return_val_if_fail (etoolbar != NULL, FALSE);

  if (etoolbar->priv->toolbars_dirty)
    {
      do_merge (etoolbar);
      etoolbar->priv->toolbars_dirty = FALSE;
    }

  if (etoolbar->priv->editor_sheet_dirty)
    {
      update_editor_sheet (etoolbar);
      etoolbar->priv->editor_sheet_dirty = FALSE;
    }

  return FALSE;
}

static void
queue_ui_update (EggEditableToolbar *etoolbar)
{
  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));

  g_idle_add (ui_update, etoolbar);
}

static void
drag_data_received_cb (GtkWidget          *widget,
		       GdkDragContext     *context,
		       gint                x,
		       gint                y,
		       GtkSelectionData   *selection_data,
		       guint               info,
		       guint               time_,
		       EggEditableToolbar *etoolbar)
{
  EggToolbarsToolbar *toolbar;
  const char *type = NULL;
  EggAction *action = NULL;
  int pos;
  GdkAtom target;
  GList *l;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));

  toolbar =
    (EggToolbarsToolbar *) g_object_get_data (G_OBJECT (widget),
					      "toolbar_data");
  pos = egg_toolbar_get_drop_index (EGG_TOOLBAR (widget), x, y);

  /* HACK placeholder are implemented as separators */
  pos = pos / 3 + 1;

  target = gtk_drag_dest_find_target (widget, context, NULL);

  for (l = etoolbar->priv->drag_types; l != NULL; l = l->next)
    {
      char *drag_type = (char *)l->data;

      if (gdk_atom_intern (drag_type, FALSE) == target)
        {
          type = drag_type;
        }
    }

  if (strcmp (type, EGG_TOOLBAR_ITEM_TYPE) != 0)
    {
      char *name;

      name = egg_editable_toolbar_get_action_name
	(etoolbar, type, selection_data->data);
      if (name != NULL)
	{
	  action = egg_editable_toolbar_get_action (etoolbar, name);
	  g_free (name);
	}
    }
  else
    {
      action =
	egg_editable_toolbar_get_action (etoolbar, selection_data->data);
    }

  if (action)
    {
      egg_toolbars_group_add_item (etoolbar->priv->group, toolbar, pos,
				   action->name);
      etoolbar->priv->toolbars_dirty = TRUE;
    }
  else if (strcmp (selection_data->data, "separator") == 0)
    {
      egg_toolbars_group_add_item (etoolbar->priv->group, toolbar, pos,
				   "separator");
      etoolbar->priv->toolbars_dirty = TRUE;
    }

  queue_ui_update (etoolbar);
}

static void
drag_data_delete_cb (GtkWidget          *widget,
		     GdkDragContext     *context,
		     EggEditableToolbar *etoolbar)
{
  EggToolbarsItem *node;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));

  node =
    (EggToolbarsItem *) g_object_get_data (G_OBJECT (widget), "item_data");
  g_return_if_fail (node != NULL);
  egg_toolbars_group_remove_item (etoolbar->priv->group, node);

  etoolbar->priv->toolbars_dirty = TRUE;
  queue_ui_update (etoolbar);
}

static void
drag_data_get_cb (GtkWidget          *widget,
		  GdkDragContext     *context,
		  GtkSelectionData   *selection_data,
		  guint               info,
		  guint32             time,
		  EggEditableToolbar *etoolbar)
{
  EggAction *action;
  const char *target;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));

  action = EGG_ACTION (g_object_get_data (G_OBJECT (widget), "egg-action"));

  if (action)
    {
      target = action->name;
    }
  else
    {
      target = "separator";
    }

  gtk_selection_data_set (selection_data,
			  selection_data->target, 8, target, strlen (target));
}

static void
toolbar_drag_data_delete_cb (GtkWidget          *widget,
			     GdkDragContext     *context,
			     EggEditableToolbar *etoolbar)
{
  EggToolbarsToolbar *t;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));

  t = (EggToolbarsToolbar *) g_object_get_data (G_OBJECT (widget),
						"toolbar_drag_data");
  g_return_if_fail (t != NULL);

  egg_toolbars_group_remove_toolbar (etoolbar->priv->group, t);

  etoolbar->priv->toolbars_dirty = TRUE;
  queue_ui_update (etoolbar);
}

static void
toolbar_drag_data_get_cb (GtkWidget          *widget,
			  GdkDragContext     *context,
			  GtkSelectionData   *selection_data,
			  guint               info,
			  guint32             time,
			  EggEditableToolbar *etoolbar)
{
  EggToolbarsToolbar *t;
  char *target;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));

  t = (EggToolbarsToolbar *) g_object_get_data (G_OBJECT (widget),
						"toolbar_drag_data");

  target = t->id;

  gtk_selection_data_set (selection_data,
			  selection_data->target, 8, target, strlen (target));
}

static void
egg_editable_toolbar_remove_cb (EggAction          *action,
				EggEditableToolbar *etoolbar)
{
  EggToolbarsToolbar *t;

  t = g_object_get_data (G_OBJECT (etoolbar), "popup_toolbar");

  egg_toolbars_group_remove_toolbar (etoolbar->priv->group, t);
}

static void
egg_editable_toolbar_edit_cb (EggAction          *action,
			      EggEditableToolbar *etoolbar)
{
  egg_editable_toolbar_edit (etoolbar, NULL);
}

static GtkWidget *
get_item_widget (EggEditableToolbar *t,
		 gpointer            data)
{
  GtkWidget *widget;
  char *path;

  path = egg_toolbars_group_get_path (t->priv->group, data);
  g_return_val_if_fail (path != NULL, NULL);

  widget = egg_menu_merge_get_widget (t->priv->merge, path);
  g_free (path);

  return widget;
}

static void
connect_item_drag_source (EggToolbarsItem    *item,
			  EggEditableToolbar *etoolbar)
{
  GtkWidget *toolitem;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));
  g_return_if_fail (item != NULL);

  toolitem = get_item_widget (etoolbar, item);

  if (!g_object_get_data (G_OBJECT (toolitem), "drag_source_set"))
    {
      g_object_set_data (G_OBJECT (toolitem), "drag_source_set",
			 GINT_TO_POINTER (TRUE));

      egg_tool_item_set_use_drag_window (EGG_TOOL_ITEM (toolitem), TRUE);

      g_object_set_data (G_OBJECT (toolitem), "item_data", item);

      gtk_drag_source_set (toolitem, GDK_BUTTON1_MASK,
			   source_drag_types, n_source_drag_types,
			   GDK_ACTION_MOVE);
      g_signal_connect (toolitem, "drag_data_get",
			G_CALLBACK (drag_data_get_cb), etoolbar);
      g_signal_connect (toolitem, "drag_data_delete",
			G_CALLBACK (drag_data_delete_cb), etoolbar);
    }
}

static void
disconnect_item_drag_source (EggToolbarsItem    *item,
			     EggEditableToolbar *etoolbar)
{
  GtkWidget *toolitem;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));
  g_return_if_fail (item != NULL);

  toolitem = get_item_widget (etoolbar, item);

  if (g_object_get_data (G_OBJECT (toolitem), "drag_source_set"))
    {
      g_object_set_data (G_OBJECT (toolitem), "drag_source_set",
			 GINT_TO_POINTER (FALSE));

      egg_tool_item_set_use_drag_window (EGG_TOOL_ITEM (toolitem), FALSE);

      gtk_drag_source_unset (toolitem);

      g_signal_handlers_disconnect_by_func (toolitem,
					    G_CALLBACK (drag_data_get_cb),
					    etoolbar);
      g_signal_handlers_disconnect_by_func (toolitem,
					    G_CALLBACK (drag_data_delete_cb),
					    etoolbar);
    }
}

static void
connect_toolbar_drag_source (EggToolbarsToolbar *t,
			     EggEditableToolbar *etoolbar)
{
  GtkWidget *tb;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));
  g_return_if_fail (t != NULL);

  tb = get_item_widget (etoolbar, t);

  g_return_if_fail (tb != NULL);

  if (!g_object_get_data (G_OBJECT (tb), "drag_source_set"))
    {
      g_object_set_data (G_OBJECT (tb), "drag_source_set",
			 GINT_TO_POINTER (TRUE));

      g_object_set_data (G_OBJECT (tb), "toolbar_drag_data", t);

      g_signal_connect (tb, "drag_data_get",
			G_CALLBACK (toolbar_drag_data_get_cb), etoolbar);
      g_signal_connect (tb, "drag_data_delete",
			G_CALLBACK (toolbar_drag_data_delete_cb), etoolbar);
    }
}

static void
disconnect_toolbar_drag_source (EggToolbarsToolbar *t,
				EggEditableToolbar *etoolbar)
{
  GtkWidget *tb;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));
  g_return_if_fail (t != NULL);

  tb = get_item_widget (etoolbar, t);

  g_return_if_fail (tb != NULL);

  if (g_object_get_data (G_OBJECT (tb), "drag_source_set"))
    {
      g_object_set_data (G_OBJECT (tb), "drag_source_set",
			 GINT_TO_POINTER (FALSE));

      g_signal_handlers_disconnect_by_func (tb,
					    G_CALLBACK
					    (toolbar_drag_data_get_cb),
					    etoolbar);
      g_signal_handlers_disconnect_by_func (tb,
					    G_CALLBACK
					    (toolbar_drag_data_delete_cb),
					    etoolbar);
    }
}

static void
popup_toolbar_context_menu (EggToolbar      *toolbar,
			    ContextMenuData *data)
{
  GtkWidget *widget;

  widget = egg_menu_merge_get_widget (data->etoolbar->priv->popup_merge,
				      "/popups/EggToolbarPopup");

  g_object_set_data (G_OBJECT (data->etoolbar), "popup_toolbar", data->t);

  g_return_if_fail (widget != NULL);

  gtk_menu_popup (GTK_MENU (widget), NULL, NULL, NULL, NULL, 2,
		  gtk_get_current_event_time ());
}

static GtkTargetList *
get_dest_targets (EggEditableToolbar *etoolbar)
{
  GList *l;
  GtkTargetList *targets;
  int i = 0;

  targets = gtk_target_list_new (NULL, 0);

  for (l = etoolbar->priv->drag_types; l != NULL; l = l->next)
    {
      char *type = (char *)l->data;
      gtk_target_list_add (targets, gdk_atom_intern (type, FALSE), 0, i);
      i++;
    }

  return targets;
}

static void
setup_toolbar (EggToolbarsToolbar *toolbar,
	       EggEditableToolbar *etoolbar)
{
  GtkWidget *widget;
  ContextMenuData *data;
  int signal_id;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));
  g_return_if_fail (toolbar != NULL);

  widget = get_item_widget (etoolbar, toolbar);
  g_object_set_data (G_OBJECT (widget), "toolbar_data", toolbar);

  if (!g_object_get_data (G_OBJECT (widget), "drag_dest_set"))
    {
      GtkTargetList *targets;

      g_object_set_data (G_OBJECT (widget), "drag_dest_set",
			 GINT_TO_POINTER (TRUE));
      gtk_drag_dest_set (widget, GTK_DEST_DEFAULT_ALL,
			 NULL, 0,
			 GDK_ACTION_MOVE | GDK_ACTION_COPY);
      targets = get_dest_targets (etoolbar);
      gtk_drag_dest_set_target_list (widget, targets);
      gtk_target_list_unref (targets);
      g_signal_connect (widget, "drag_data_received",
			G_CALLBACK (drag_data_received_cb), etoolbar);
    }

  if (!g_object_get_data (G_OBJECT (widget), "popup_signal_id"))
    {
      data = g_new0 (ContextMenuData, 1);
      data->etoolbar = etoolbar;
      data->t = toolbar;

      signal_id = g_signal_connect_data (widget,
					 "popup_context_menu",
					 G_CALLBACK
					 (popup_toolbar_context_menu), data,
					 (GClosureNotify) g_free, 0);

      g_object_set_data (G_OBJECT (widget), "popup_signal_id",
			 GINT_TO_POINTER (signal_id));
    }

  etoolbar->priv->last_toolbar = widget;
}

static void
ensure_toolbar_min_size (EggToolbarsToolbar *toolbar,
			 EggEditableToolbar *t)
{
  GtkWidget *widget;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (t));
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
do_merge (EggEditableToolbar *t)
{
  char *str;
  guint ui_id;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (t));

  str = egg_toolbars_group_to_string (t->priv->group);
  g_return_if_fail (str != NULL);

  ui_id = egg_menu_merge_add_ui_from_string (t->priv->merge, str, -1, NULL);

  if (t->priv->ui_id != 0)
    {
      egg_menu_merge_remove_ui (t->priv->merge, t->priv->ui_id);
    }

  t->priv->ui_id = ui_id;

  egg_menu_merge_ensure_update (t->priv->merge);

  egg_toolbars_group_foreach_toolbar (t->priv->group,
				      (EggToolbarsGroupForeachToolbarFunc)
				      setup_toolbar, t);

  if (t->priv->edit_mode)
    {
      egg_toolbars_group_foreach_item (t->priv->group,
				       (EggToolbarsGroupForeachItemFunc)
				       connect_item_drag_source, t);

      egg_toolbars_group_foreach_toolbar (t->priv->group,
					  (EggToolbarsGroupForeachToolbarFunc)
					  connect_toolbar_drag_source, t);
    }

  egg_toolbars_group_foreach_toolbar (t->priv->group,
				      (EggToolbarsGroupForeachToolbarFunc)
				      ensure_toolbar_min_size, t);

  g_free (str);
}

static void
ensure_action (EggToolbarsItem *item,
	       EggEditableToolbar *t)
{
  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (t));
  g_return_if_fail (item != NULL);

  egg_editable_toolbar_get_action (t, item->action);
}

static void
group_changed_cb (EggToolbarsGroup   *group,
		  EggEditableToolbar *t)
{
  g_return_if_fail (IS_EGG_TOOLBARS_GROUP (group));
  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (t));

  t->priv->toolbars_dirty = TRUE;

  egg_toolbars_group_foreach_item (t->priv->group,
				   (EggToolbarsGroupForeachItemFunc)
				   ensure_action, t);

  queue_ui_update (t);
}

static void
egg_editable_toolbar_set_group (EggEditableToolbar *t,
				EggToolbarsGroup   *group)
{
  g_return_if_fail (IS_EGG_TOOLBARS_GROUP (group));
  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (t));

  t->priv->group = group;

  g_signal_connect_object (group, "changed",
			   G_CALLBACK (group_changed_cb), t, 0);
}

static void
egg_editable_toolbar_set_merge (EggEditableToolbar *t,
				EggMenuMerge       *merge)
{
  g_return_if_fail (EGG_IS_MENU_MERGE (merge));
  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (t));

  t->priv->merge = merge;

  egg_toolbars_group_foreach_item (t->priv->group,
				   (EggToolbarsGroupForeachItemFunc)
				   ensure_action, t);

  do_merge (t);
}

static void
egg_editable_toolbar_set_property (GObject      *object,
				   guint         prop_id,
				   const GValue *value,
				   GParamSpec   *pspec)
{
  EggEditableToolbar *t = EGG_EDITABLE_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_MENU_MERGE:
      egg_editable_toolbar_set_merge (t, g_value_get_object (value));
      break;
    case PROP_TOOLBARS_GROUP:
      egg_editable_toolbar_set_group (t, g_value_get_object (value));
      break;
    }
}

static void
egg_editable_toolbar_get_property (GObject    *object,
				   guint       prop_id,
				   GValue     *value,
				   GParamSpec *pspec)
{
  EggEditableToolbar *t = EGG_EDITABLE_TOOLBAR (object);

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
egg_editable_toolbar_class_init (EggEditableToolbarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = egg_editable_toolbar_finalize;
  object_class->set_property = egg_editable_toolbar_set_property;
  object_class->get_property = egg_editable_toolbar_get_property;

  klass->get_action = impl_get_action;
  klass->get_action_name = impl_get_action_name;

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
							EGG_TOOLBARS_GROUP_TYPE,
							G_PARAM_READWRITE));
}

static void
egg_editable_toolbar_init (EggEditableToolbar *t)
{
  int i;

  t->priv = g_new0 (EggEditableToolbarPrivate, 1);

  t->priv->merge = NULL;
  t->priv->editor = NULL;
  t->priv->ui_id = 0;
  t->priv->toolbars_dirty = FALSE;
  t->priv->editor_sheet_dirty = FALSE;
  t->priv->edit_mode = FALSE;
  t->priv->actions_list = NULL;
  t->priv->drag_types = NULL;

  egg_editable_toolbar_add_drag_type (t, EGG_TOOLBAR_ITEM_TYPE);

  for (i = 0; i < egg_toolbar_popups_n_entries; i++)
    {
      egg_toolbar_popups[i].user_data = t;
    }

  t->priv->popup_merge = egg_menu_merge_new ();

  t->priv->popup_action_group = egg_action_group_new ("ToolbarPopupActions");
  egg_action_group_add_actions (t->priv->popup_action_group,
				egg_toolbar_popups,
				egg_toolbar_popups_n_entries);
  egg_menu_merge_insert_action_group (t->priv->popup_merge,
				      t->priv->popup_action_group, 0);
/* FIXME
	egg_menu_merge_add_ui_from_file (t->priv->popup_merge,
				egg_file ("epiphany-toolbar-popup-ui.xml"),
				NULL);
*/
  update_popup_menu (t);
}

static void
egg_editable_toolbar_finalize (GObject *object)
{
  EggEditableToolbar *t = EGG_EDITABLE_TOOLBAR (object);

  g_return_if_fail (object != NULL);
  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (object));

  if (t->priv->editor)
    {
      gtk_widget_destroy (t->priv->editor);
    }

  if (t->priv->drag_types)
    {
      g_list_foreach (t->priv->drag_types, (GFunc)g_free, NULL);
      g_list_free (t->priv->drag_types);
    }

  g_object_unref (t->priv->popup_action_group);
  egg_menu_merge_remove_action_group (t->priv->popup_merge,
				      t->priv->popup_action_group);
  g_object_unref (t->priv->popup_merge);

  g_free (t->priv);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

EggEditableToolbar *
egg_editable_toolbar_new (EggMenuMerge     *merge,
			  EggToolbarsGroup *group)
{
  EggEditableToolbar *t;

  t = EGG_EDITABLE_TOOLBAR (g_object_new (EGG_EDITABLE_TOOLBAR_TYPE,
					  "ToolbarsGroup", group,
					  "MenuMerge", merge, NULL));

  g_return_val_if_fail (t->priv != NULL, NULL);

  return t;
}

static void
hide_editor (EggEditableToolbar *etoolbar)
{
  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));

  gtk_widget_hide (GTK_WIDGET (etoolbar->priv->editor));
}

static void
editor_drag_data_received_cb (GtkWidget          *widget,
			      GdkDragContext     *context,
			      gint                x,
			      gint                y,
			      GtkSelectionData   *selection_data,
			      guint               info,
			      guint               time_,
			      EggEditableToolbar *etoolbar)
{
  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));

  etoolbar->priv->editor_sheet_dirty = TRUE;
  queue_ui_update (etoolbar);
}

static void
editor_drag_data_delete_cb (GtkWidget          *widget,
			    GdkDragContext     *context,
			    EggEditableToolbar *etoolbar)
{
  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));

  etoolbar->priv->editor_sheet_dirty = TRUE;
  queue_ui_update (etoolbar);
}

static void
editor_close (EggEditableToolbar * etoolbar)
{
  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));

  etoolbar->priv->edit_mode = FALSE;
  egg_toolbars_group_foreach_item (etoolbar->priv->group,
				   (EggToolbarsGroupForeachItemFunc)
				   disconnect_item_drag_source, etoolbar);
  egg_toolbars_group_foreach_toolbar (etoolbar->priv->group,
				      (EggToolbarsGroupForeachToolbarFunc)
				      disconnect_toolbar_drag_source,
				      etoolbar);

  update_popup_menu (etoolbar);
  hide_editor (etoolbar);
}

static void
editor_add_toolbar (EggEditableToolbar *etoolbar)
{
  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));

  egg_toolbars_group_add_toolbar (etoolbar->priv->group);

  etoolbar->priv->toolbars_dirty = TRUE;
  queue_ui_update (etoolbar);
}

static void
dialog_response_cb (GtkDialog          *dialog,
		    gint                response_id,
		    EggEditableToolbar *etoolbar)
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
setup_editor (EggEditableToolbar *etoolbar,
	      GtkWidget          *window)
{
  GtkWidget *editor;
  GtkWidget *scrolled_window;
  GtkWidget *vbox;
  GtkWidget *label_hbox;
  GtkWidget *image;
  GtkWidget *label;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));

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
  image =
    gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
  gtk_widget_show (image);
  gtk_box_pack_start (GTK_BOX (label_hbox), image, FALSE, FALSE, 0);
  label = gtk_label_new (_("Drag an item onto the toolbars above to add it, "
			   "from the toolbars in the items table to remove it."));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (label_hbox), label, FALSE, TRUE, 0);

  gtk_dialog_add_button (GTK_DIALOG (editor),
			 _("Add Toolbar"), RESPONSE_ADD_TOOLBAR);
  gtk_dialog_add_button (GTK_DIALOG (editor),
			 GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

  g_signal_connect (editor, "response",
		    G_CALLBACK (dialog_response_cb), etoolbar);

  update_editor_sheet (etoolbar);
}

static void
add_to_list (EggToolbarsItem  *item,
	     GList           **l)
{
  g_return_if_fail (item != NULL);

  *l = g_list_append (*l, item);
}

static GtkWidget *
editor_create_item (EggEditableToolbar *etoolbar,
		    const char         *stock_id,
		    const char         *label_text,
		    GdkDragAction	action)
{
  GtkWidget *event_box;
  GtkWidget *vbox;
  GtkWidget *icon;
  GtkWidget *label;

  event_box = gtk_event_box_new ();
  gtk_widget_show (event_box);
  gtk_drag_source_set (event_box,
		       GDK_BUTTON1_MASK,
		       source_drag_types, 1, action);
  g_signal_connect (event_box, "drag_data_get",
		    G_CALLBACK (drag_data_get_cb), etoolbar);
  g_signal_connect (event_box, "drag_data_delete",
		    G_CALLBACK (editor_drag_data_delete_cb), etoolbar);

  vbox = gtk_vbox_new (0, FALSE);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (event_box), vbox);

  icon = gtk_image_new_from_stock
	(stock_id ? stock_id : GTK_STOCK_DND,
	 GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_show (icon);
  gtk_box_pack_start (GTK_BOX (vbox), icon, FALSE, TRUE, 0);

  label = gtk_label_new_with_mnemonic (label_text);
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

  return event_box;
}

static void
update_editor_sheet (EggEditableToolbar *etoolbar)
{
  GList *l;
  GList *to_drag = NULL;
  int x, y, height, width;
  GtkWidget *table;
  GtkWidget *viewport;
  GtkWidget *item;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));

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
		     dest_drag_types, n_dest_drag_types, GDK_ACTION_MOVE);
  g_signal_connect (table, "drag_data_received",
		    G_CALLBACK (editor_drag_data_received_cb), etoolbar);

  egg_toolbars_group_foreach_available (etoolbar->priv->group,
					(EggToolbarsGroupForeachItemFunc)
					add_to_list, &to_drag);

  x = y = 0;
  width = 4;
  height = (g_list_length (to_drag) - 1) / width + 1;
  gtk_table_resize (GTK_TABLE (etoolbar->priv->table), height, width);

  for (l = to_drag; l != NULL; l = l->next)
    {
      EggToolbarsItem *node = (EggToolbarsItem *) (l->data);
      EggAction *action;

      action = egg_editable_toolbar_get_action (etoolbar, node->action);
      g_return_if_fail (action != NULL);

      item = editor_create_item (etoolbar, action->stock_id,
				 action->label, GDK_ACTION_MOVE);
      g_object_set_data (G_OBJECT (item), "egg-action", action);
      gtk_table_attach_defaults (GTK_TABLE (etoolbar->priv->table),
		                 item, x, x + 1, y, y + 1);

      x++;
      if (x >= width)
	{
	  x = 0;
	  y++;
	}
    }

  item = editor_create_item (etoolbar, NULL, _("Separator"),
			     GDK_ACTION_COPY);
  gtk_table_attach_defaults (GTK_TABLE (etoolbar->priv->table),
		             item, x, x + 1, y, y + 1);

  g_list_free (to_drag);
}

static void
show_editor (EggEditableToolbar *etoolbar)
{
  GtkWidget *editor = etoolbar->priv->editor;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));
  g_return_if_fail (editor != NULL);

  gtk_widget_show (GTK_WIDGET (editor));
}

static void
set_action_sensitive (EggToolbarsItem    *item,
		      EggEditableToolbar *etoolbar)
{
  EggAction *action;

  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));
  g_return_if_fail (item != NULL);

  if (!item->separator)
    {
      action = find_action (etoolbar, item->action);
      g_object_set (action, "sensitive", TRUE, NULL);
    }
}

void
egg_editable_toolbar_edit (EggEditableToolbar *etoolbar,
			   GtkWidget          *window)
{
  g_return_if_fail (IS_EGG_EDITABLE_TOOLBAR (etoolbar));

  etoolbar->priv->edit_mode = TRUE;
  egg_toolbars_group_foreach_item (etoolbar->priv->group,
				   (EggToolbarsGroupForeachItemFunc)
				   connect_item_drag_source, etoolbar);
  egg_toolbars_group_foreach_item (etoolbar->priv->group,
				   (EggToolbarsGroupForeachItemFunc)
				   set_action_sensitive, etoolbar);
  egg_toolbars_group_foreach_toolbar (etoolbar->priv->group,
				      (EggToolbarsGroupForeachToolbarFunc)
				      connect_toolbar_drag_source, etoolbar);

  update_popup_menu (etoolbar);

  setup_editor (etoolbar, window);
  show_editor (etoolbar);
}

void
egg_editable_toolbar_add_drag_type (EggEditableToolbar *etoolbar,
				    const char         *drag_type)
{
  etoolbar->priv->drag_types = g_list_append
	(etoolbar->priv->drag_types, g_strdup (drag_type));
}

char *
egg_editable_toolbar_get_action_name (EggEditableToolbar *etoolbar,
				      const char         *drag_type,
				      const char         *data)
{
  EggEditableToolbarClass *klass = EGG_EDITABLE_TOOLBAR_GET_CLASS (etoolbar);
  return klass->get_action_name (etoolbar, drag_type, data);
}

EggAction *
egg_editable_toolbar_get_action (EggEditableToolbar *etoolbar,
				 const char         *name)
{
  EggEditableToolbarClass *klass = EGG_EDITABLE_TOOLBAR_GET_CLASS (etoolbar);
  return klass->get_action (etoolbar, name);
}
