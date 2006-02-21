/*
 *  Copyright (C) 2003, 2004  Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004, 2005  Christian Persch
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

#include "egg-editable-toolbar.h"
#include "egg-toolbars-model.h"
#include "egg-toolbar-editor.h"

#include <gtk/gtkvseparator.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtktoggleaction.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkseparatormenuitem.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkstock.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtktoolbar.h>
#include <gtk/gtktoolitem.h>
#include <gtk/gtktoolbutton.h>
#include <gtk/gtkseparatortoolitem.h>
#include <glib/gi18n.h>
#include <string.h>

static void egg_editable_toolbar_class_init	(EggEditableToolbarClass *klass);
static void egg_editable_toolbar_init		(EggEditableToolbar *etoolbar);

#define MIN_TOOLBAR_HEIGHT 20
#define EGG_ITEM_NAME      "egg-item-name"
#define STOCK_DRAG_MODE    "stock_drag-mode"

static const GtkTargetEntry dest_drag_types[] = {
  {EGG_TOOLBAR_ITEM_TYPE, GTK_TARGET_SAME_APP, 0},
};

enum
{
  PROP_0,
  PROP_TOOLBARS_MODEL,
  PROP_UI_MANAGER,
  PROP_POPUP_PATH,
  PROP_SELECTED,
  PROP_EDIT_MODE
};

enum
{
  ACTION_REQUEST,
  LAST_SIGNAL
};

static guint egg_editable_toolbar_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

#define EGG_EDITABLE_TOOLBAR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EGG_TYPE_EDITABLE_TOOLBAR, EggEditableToolbarPrivate))

struct _EggEditableToolbarPrivate
{
  GtkUIManager *manager;
  EggToolbarsModel *model;
  guint edit_mode;
  gboolean save_hidden;
  GtkWidget *fixed_toolbar;
  
  GtkWidget *selected;
  GtkActionGroup *actions;
  
  guint visibility_id;
  GList *visibility_paths;
  GPtrArray *visibility_actions;

  char *popup_path;

  guint        dnd_pending;
  GtkToolbar  *dnd_toolbar;
  GtkToolItem *dnd_toolitem;
};

GType
egg_editable_toolbar_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
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

      type = g_type_register_static (GTK_TYPE_VBOX,
				     "EggEditableToolbar",
				     &our_info, 0);
    }

  return type;
}

static int
get_dock_position (EggEditableToolbar *etoolbar,
                   GtkWidget *dock)
{
  GList *l;
  int result;

  l = gtk_container_get_children (GTK_CONTAINER (etoolbar));
  result = g_list_index (l, dock);
  g_list_free (l);

  return result;
}

static int
get_toolbar_position (EggEditableToolbar *etoolbar, GtkWidget *toolbar)
{
  return get_dock_position (etoolbar, toolbar->parent);
}

static int
get_n_toolbars (EggEditableToolbar *etoolbar)
{
  GList *l;
  int result;

  l = gtk_container_get_children (GTK_CONTAINER (etoolbar));
  result = g_list_length (l);
  g_list_free (l);

  return result;
}

static GtkWidget *
get_dock_nth (EggEditableToolbar *etoolbar,
	      int                 position)
{
  GList *l;
  GtkWidget *result;

  l = gtk_container_get_children (GTK_CONTAINER (etoolbar));
  result = g_list_nth_data (l, position);
  g_list_free (l);

  return result;
}

static GtkWidget *
get_toolbar_nth (EggEditableToolbar *etoolbar,
		 int                 position)
{
  GList *l;
  GtkWidget *dock;
  GtkWidget *result;

  dock = get_dock_nth (etoolbar, position);
  g_return_val_if_fail (dock != NULL, NULL);

  l = gtk_container_get_children (GTK_CONTAINER (dock));
  result = GTK_WIDGET (l->data);
  g_list_free (l);

  return result;
}

static GtkAction *
find_action (EggEditableToolbar *etoolbar,
	     const char         *name)
{
  GList *l;
  GtkAction *action = NULL;

  l = gtk_ui_manager_get_action_groups (etoolbar->priv->manager);

  g_return_val_if_fail (name != NULL, NULL);

  for (; l != NULL; l = l->next)
    {
      GtkAction *tmp;

      tmp = gtk_action_group_get_action (GTK_ACTION_GROUP (l->data), name);
      if (tmp)
	action = tmp;
    }

  return action;
}

static void
drag_data_delete_cb (GtkWidget          *widget,
		     GdkDragContext     *context,
		     EggEditableToolbar *etoolbar)
{
  int pos, toolbar_pos;

  widget = gtk_widget_get_ancestor (widget, GTK_TYPE_TOOL_ITEM);
  g_return_if_fail (widget != NULL);
  g_return_if_fail (EGG_IS_EDITABLE_TOOLBAR (etoolbar));

  pos = gtk_toolbar_get_item_index (GTK_TOOLBAR (widget->parent),
				    GTK_TOOL_ITEM (widget));
  toolbar_pos = get_toolbar_position (etoolbar, widget->parent);

  egg_toolbars_model_remove_item (etoolbar->priv->model,
				  toolbar_pos, pos);
}

static void
drag_begin_cb (GtkWidget          *widget,
	       GdkDragContext     *context,
	       EggEditableToolbar *etoolbar)
{
  GtkAction *action;
  gint flags;
  
  gtk_widget_hide (widget);

  action = g_object_get_data (G_OBJECT (widget), "gtk-action");
  if (action == NULL) return;
  
  flags = egg_toolbars_model_get_name_flags (etoolbar->priv->model,
					     gtk_action_get_name (action));
  if (!(flags & EGG_TB_MODEL_NAME_INFINITE))
    {
      flags &= ~EGG_TB_MODEL_NAME_USED;
      egg_toolbars_model_set_name_flags (etoolbar->priv->model,
					 gtk_action_get_name (action),
					 flags);
    }
}

static void
drag_end_cb (GtkWidget          *widget,
	     GdkDragContext     *context,
	     EggEditableToolbar *etoolbar)
{
  GtkAction *action;
  gint flags;
 
  if (gtk_widget_get_parent (widget) != NULL)
    {
      gtk_widget_show (widget);

      action = g_object_get_data (G_OBJECT (widget), "gtk-action");
      if (action == NULL) return;
      
      flags = egg_toolbars_model_get_name_flags (etoolbar->priv->model,
						 gtk_action_get_name (action));
      if (!(flags & EGG_TB_MODEL_NAME_INFINITE))
        {
	  flags |= EGG_TB_MODEL_NAME_USED;
	  egg_toolbars_model_set_name_flags (etoolbar->priv->model,
					     gtk_action_get_name (action),
					     flags);
	}
    }
}

static void
drag_data_get_cb (GtkWidget          *widget,
		  GdkDragContext     *context,
		  GtkSelectionData   *selection_data,
		  guint               info,
		  guint32             time,
		  EggEditableToolbar *etoolbar)
{
  EggToolbarsModel *model;
  const char *name;
  char *data;

  g_return_if_fail (EGG_IS_EDITABLE_TOOLBAR (etoolbar));
  model = egg_editable_toolbar_get_model (etoolbar);
  
  name = g_object_get_data (G_OBJECT (widget), EGG_ITEM_NAME);
  if (name == NULL)
    {
      name = g_object_get_data (G_OBJECT (gtk_widget_get_parent (widget)), EGG_ITEM_NAME);
      g_return_if_fail (name != NULL);
    }
  
  data = egg_toolbars_model_get_data (model, selection_data->target, name);
  if (data != NULL)
    {
      gtk_selection_data_set (selection_data, selection_data->target, 8, (unsigned char *)data, strlen (data));
      g_free (data);
    }
}

static void
move_item_cb (GtkAction          *action,
              EggEditableToolbar *etoolbar)
{
  GtkWidget *toolitem = gtk_widget_get_ancestor (egg_editable_toolbar_get_selected (etoolbar), GTK_TYPE_TOOL_ITEM);
  GtkTargetList *list = gtk_target_list_new (dest_drag_types, G_N_ELEMENTS (dest_drag_types));

  GdkEvent *realevent = gtk_get_current_event();
  GdkEventMotion event;
  event.type = GDK_MOTION_NOTIFY;
  event.window = realevent->any.window;
  event.send_event = FALSE;
  event.axes = NULL;
  event.time = gdk_event_get_time (realevent);
  gdk_event_get_state (realevent, &event.state);
  gdk_event_get_coords (realevent, &event.x, &event.y);
  gdk_event_get_root_coords (realevent, &event.x_root, &event.y_root);
    
  gtk_drag_begin (toolitem, list, GDK_ACTION_MOVE, 1, (GdkEvent *)&event);
  gtk_target_list_unref (list);
}

static void
remove_item_cb (GtkAction          *action,
                EggEditableToolbar *etoolbar)
{
  GtkWidget *toolitem = gtk_widget_get_ancestor (egg_editable_toolbar_get_selected (etoolbar), GTK_TYPE_TOOL_ITEM);
  int pos, toolbar_pos;
      
  toolbar_pos = get_toolbar_position (etoolbar, toolitem->parent);
  pos = gtk_toolbar_get_item_index (GTK_TOOLBAR (toolitem->parent), 
				    GTK_TOOL_ITEM (toolitem));

  egg_toolbars_model_remove_item (etoolbar->priv->model,
			          toolbar_pos, pos);

  if (egg_toolbars_model_n_items (etoolbar->priv->model, toolbar_pos) == 0)
    {
      egg_toolbars_model_remove_toolbar (etoolbar->priv->model, toolbar_pos);
    }
}

static void
remove_toolbar_cb (GtkAction          *action,
		   EggEditableToolbar *etoolbar)
{
  GtkWidget *selected = egg_editable_toolbar_get_selected (etoolbar);
  GtkWidget *toolbar = gtk_widget_get_ancestor (selected, GTK_TYPE_TOOLBAR);
  int toolbar_pos;

  toolbar_pos = get_toolbar_position (etoolbar, toolbar);
  egg_toolbars_model_remove_toolbar (etoolbar->priv->model, toolbar_pos);
}

static void
popup_context_deactivate (GtkMenuShell *menu,
			  EggEditableToolbar *etoolbar)
{
  egg_editable_toolbar_set_selected (etoolbar, NULL);
  g_object_notify (G_OBJECT (etoolbar), "selected");
}

static void
popup_context_menu_cb (GtkWidget          *toolbar,
                       gint		   x,
                       gint		   y,
                       gint                button_number,
                       EggEditableToolbar *etoolbar)
{
  if (etoolbar->priv->popup_path != NULL)
    {
      GtkMenu *menu;
      
      egg_editable_toolbar_set_selected (etoolbar, toolbar);
      g_object_notify (G_OBJECT (etoolbar), "selected");
      
      menu = GTK_MENU (gtk_ui_manager_get_widget (etoolbar->priv->manager, 
						  etoolbar->priv->popup_path));
      g_return_if_fail (menu != NULL);
      gtk_menu_popup (menu, NULL, NULL, NULL, NULL, button_number, gtk_get_current_event_time ());
      g_signal_connect_object (menu, "selection-done",
			       G_CALLBACK (popup_context_deactivate),
			       etoolbar, 0);
    }
}

static gboolean
button_press_event_cb (GtkWidget *widget,
                       GdkEventButton *event,
                       EggEditableToolbar *etoolbar)
{
  if (event->button == 3 && etoolbar->priv->popup_path != NULL)
    {
      GtkMenu *menu;
      
      egg_editable_toolbar_set_selected (etoolbar, widget);
      g_object_notify (G_OBJECT (etoolbar), "selected");
	
      menu = GTK_MENU (gtk_ui_manager_get_widget (etoolbar->priv->manager, 
						  etoolbar->priv->popup_path));
      g_return_val_if_fail (menu != NULL, FALSE);
      gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button, event->time);
      g_signal_connect_object (menu, "selection-done",
			       G_CALLBACK (popup_context_deactivate),
			       etoolbar, 0);
      
      return TRUE;
    }
    
  return FALSE;
}

static void
configure_item_sensitivity (GtkToolItem *item, EggEditableToolbar *etoolbar)
{
  GtkAction *action;
  char *name;
  
  name = g_object_get_data (G_OBJECT (item), EGG_ITEM_NAME);
  action = name ? find_action (etoolbar, name) : NULL;
  
  if (action)
    {
      g_object_notify (G_OBJECT (action), "sensitive");
    }

  gtk_tool_item_set_use_drag_window (item,
				     (etoolbar->priv->edit_mode > 0) || 
				     GTK_IS_SEPARATOR_TOOL_ITEM (item));
  
}

static void
configure_item_cursor (GtkToolItem *item,
		       EggEditableToolbar *etoolbar)
{
  EggEditableToolbarPrivate *priv = etoolbar->priv;
  GtkWidget *widget = GTK_WIDGET (item);

  if (widget->window != NULL)
    {
      if (priv->edit_mode > 0)
        {
          GdkCursor *cursor;
          
          cursor = gdk_cursor_new (GDK_HAND2);
          gdk_window_set_cursor (widget->window, cursor);
          gdk_cursor_unref (cursor);

          gtk_drag_source_set (widget, GDK_BUTTON1_MASK, dest_drag_types,
                               G_N_ELEMENTS (dest_drag_types), GDK_ACTION_MOVE);
        }
      else
        {
          gdk_window_set_cursor (GTK_WIDGET(item)->window, NULL);
        }
    }
}


static void
configure_item_tooltip (GtkToolItem *item)
{
  GtkAction *action = g_object_get_data (G_OBJECT (item),
					 "gtk-action");
  
  if (action != NULL)
    {
      g_object_notify (G_OBJECT (action), "tooltip");
    }
}


static void
connect_widget_signals (GtkWidget *proxy, EggEditableToolbar *etoolbar)
{
  if (GTK_IS_CONTAINER (proxy))
    {
       gtk_container_forall (GTK_CONTAINER (proxy),
			     (GtkCallback) connect_widget_signals,
			     (gpointer) etoolbar);
    }

  if (GTK_IS_TOOL_ITEM (proxy))
    {
      g_signal_connect_object (proxy, "drag_begin",
			       G_CALLBACK (drag_begin_cb), 
			       etoolbar, 0);
      g_signal_connect_object (proxy, "drag_end",
			       G_CALLBACK (drag_end_cb),
			       etoolbar, 0);
      g_signal_connect_object (proxy, "drag_data_get",
			       G_CALLBACK (drag_data_get_cb), 
			       etoolbar, 0);
      g_signal_connect_object (proxy, "drag_data_delete",
			       G_CALLBACK (drag_data_delete_cb),
			       etoolbar, 0);
    }
    
  if (GTK_IS_BUTTON (proxy) || GTK_IS_TOOL_ITEM (proxy))
    {
      g_signal_connect_object (proxy, "button-press-event",
			       G_CALLBACK (button_press_event_cb),
			       etoolbar, 0);
    }
}

static void
action_sensitive_cb (GtkAction   *action, 
                     GParamSpec  *pspec,
                     GtkToolItem *item)
{
  EggEditableToolbar *etoolbar = EGG_EDITABLE_TOOLBAR
    (gtk_widget_get_ancestor (GTK_WIDGET (item), EGG_TYPE_EDITABLE_TOOLBAR));

  if (etoolbar->priv->edit_mode > 0)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (item), TRUE);
    }
}

static GtkToolItem *
create_item_from_action (EggEditableToolbar *etoolbar,
			 const char *name)
{
  GtkToolItem *item;

  g_return_val_if_fail (name != NULL, NULL);
  
  if (strcmp (name, "_separator") == 0)
    {
      item = gtk_separator_tool_item_new ();
    }
  else
    {
      GtkAction *action = find_action (etoolbar, name);
      if (action == NULL) return NULL;
	
      item = GTK_TOOL_ITEM (gtk_action_create_tool_item (action));

      /* Normally done on-demand by the GtkUIManager, but no
       * such demand may have been made yet, so do it ourselves.
       */
      gtk_action_set_accel_group
        (action, gtk_ui_manager_get_accel_group(etoolbar->priv->manager));
     
      g_signal_connect_object (action, "notify::sensitive",
                               G_CALLBACK (action_sensitive_cb), item, 0);
    }

  gtk_widget_show (GTK_WIDGET (item));

  g_object_set_data_full (G_OBJECT (item), EGG_ITEM_NAME,
                          g_strdup (name), g_free);  
  
  return item;
}

static GtkToolItem *
create_item_from_position (EggEditableToolbar *etoolbar,
                           int                 toolbar_position,
                           int                 position)
{
  GtkToolItem *item;
  const char *name;

  name = egg_toolbars_model_item_nth (etoolbar->priv->model, toolbar_position, position);
  item = create_item_from_action (etoolbar, name);

  return item;
}

static void
toolbar_drag_data_received_cb (GtkToolbar         *toolbar,
                               GdkDragContext     *context,
                               gint                x,
                               gint                y,
                               GtkSelectionData   *selection_data,
                               guint               info,
                               guint               time,
                               EggEditableToolbar *etoolbar)
{
  /* This function can be called for two reasons
   *
   *  (1) drag_motion() needs an item to pass to
   *      gtk_toolbar_set_drop_highlight_item(). We can
   *      recognize this case by etoolbar->priv->pending being TRUE
   *      We should just create an item and return.
   *
   *  (2) The drag has finished, and drag_drop() wants us to
   *      actually add a new item to the toolbar.
   */

  GdkAtom type = selection_data->type;
  const char *data = (char *)selection_data->data;
  
  int ipos = -1;
  char *name = NULL;
  gboolean used = FALSE;
  
  /* Find out where the drop is occuring, and the name of what is being dropped. */
  if (selection_data->length >= 0)
    {
      ipos = gtk_toolbar_get_drop_index (toolbar, x, y);
      name = egg_toolbars_model_get_name (etoolbar->priv->model, type, data, FALSE);
      if (name != NULL)
	{
	  used = ((egg_toolbars_model_get_name_flags (etoolbar->priv->model, name) & EGG_TB_MODEL_NAME_USED) != 0);
        }
    }

  /* If we just want a highlight item, then . */
  if (etoolbar->priv->dnd_pending > 0)
    {
      etoolbar->priv->dnd_pending--;
      
      if (name != NULL && etoolbar->priv->dnd_toolbar == toolbar && !used)
        {
          etoolbar->priv->dnd_toolitem = create_item_from_action (etoolbar, name);
          gtk_toolbar_set_drop_highlight_item (etoolbar->priv->dnd_toolbar,
                                               etoolbar->priv->dnd_toolitem, ipos);
        }
    }
  else
    {
      gtk_toolbar_set_drop_highlight_item (toolbar, NULL, 0);
      etoolbar->priv->dnd_toolbar = NULL;
      etoolbar->priv->dnd_toolitem = NULL;
  
      /* If we don't have a name to use yet, try to create one. */
      if (name == NULL && selection_data->length >= 0)
        {
          name = egg_toolbars_model_get_name (etoolbar->priv->model, type, data, TRUE);
        }
  
      if (name != NULL && !used)
        {
          gint tpos = get_toolbar_position (etoolbar, GTK_WIDGET (toolbar));
          egg_toolbars_model_add_item (etoolbar->priv->model, tpos, ipos, name);
          gtk_drag_finish (context, TRUE, context->action == GDK_ACTION_MOVE, time);
        }
      else
        {  
          gtk_drag_finish (context, FALSE, context->action == GDK_ACTION_MOVE, time);
        }
    }
        
  g_free (name);
}

static gboolean
toolbar_drag_drop_cb (GtkToolbar         *toolbar,
		      GdkDragContext     *context,
		      gint                x,
		      gint                y,
		      guint               time,
		      EggEditableToolbar *etoolbar)
{
  GdkAtom target;

  target = gtk_drag_dest_find_target (GTK_WIDGET (toolbar), context, NULL);
  if (target != GDK_NONE)
    {
      gtk_drag_get_data (GTK_WIDGET (toolbar), context, target, time);
      return TRUE;
    }
  
  return FALSE;
}

static gboolean
toolbar_drag_motion_cb (GtkToolbar         *toolbar,
		        GdkDragContext     *context,
		        gint                x,
		        gint                y,
		        guint               time,
		        EggEditableToolbar *etoolbar)
{
  GdkAtom target = gtk_drag_dest_find_target (GTK_WIDGET (toolbar), context, NULL);
  if (target == GDK_NONE)
    {
      gdk_drag_status (context, 0, time);
      return FALSE;
    }

  /* Make ourselves the current dnd toolbar, and request a highlight item. */
  if (etoolbar->priv->dnd_toolbar != toolbar)
    {
      etoolbar->priv->dnd_toolbar = toolbar;
      etoolbar->priv->dnd_toolitem = NULL;
      etoolbar->priv->dnd_pending++;
      gtk_drag_get_data (GTK_WIDGET (toolbar), context, target, time);
    }
  
  /* If a highlight item is available, use it. */
  else if (etoolbar->priv->dnd_toolitem)
    {
      gint ipos = gtk_toolbar_get_drop_index (etoolbar->priv->dnd_toolbar, x, y);
      gtk_toolbar_set_drop_highlight_item (etoolbar->priv->dnd_toolbar,
                                           etoolbar->priv->dnd_toolitem, ipos);
    }

  gdk_drag_status (context, context->suggested_action, time);

  return TRUE;
}

static void
toolbar_drag_leave_cb (GtkToolbar         *toolbar,
		       GdkDragContext     *context,
		       guint               time,
		       EggEditableToolbar *etoolbar)
{
  gtk_toolbar_set_drop_highlight_item (toolbar, NULL, 0);

  /* If we were the current dnd toolbar target, remove the item. */
  if (etoolbar->priv->dnd_toolbar == toolbar)
    {
      etoolbar->priv->dnd_toolbar = NULL;
      etoolbar->priv->dnd_toolitem = NULL;
    }
}

static void
configure_drag_dest (EggEditableToolbar *etoolbar,
                     GtkToolbar         *toolbar)
{
  EggToolbarsItemType *type;
  GtkTargetList *targets;
  GList *list;

  /* Make every toolbar able to receive drag-drops. */
  gtk_drag_dest_set (GTK_WIDGET (toolbar), 0,
		     dest_drag_types, G_N_ELEMENTS (dest_drag_types),
		     GDK_ACTION_MOVE | GDK_ACTION_COPY);
 
  /* Add any specialist drag-drop abilities. */
  targets = gtk_drag_dest_get_target_list (GTK_WIDGET (toolbar));
  list = egg_toolbars_model_get_types (etoolbar->priv->model);
  while (list)
  {
    type = list->data;
    if (type->new_name != NULL || type->get_name != NULL)
      gtk_target_list_add (targets, type->type, 0, 0);
    list = list->next;
  }
}

static void
toggled_visibility_cb (GtkToggleAction *action,
		       EggEditableToolbar *etoolbar)
{
  EggEditableToolbarPrivate *priv = etoolbar->priv;
  GtkWidget *dock;
  EggTbModelFlags flags;
  gboolean visible;
  gint i;
  
  visible = gtk_toggle_action_get_active (action);
  for (i = 0; i < priv->visibility_actions->len; i++)
    if (g_ptr_array_index (priv->visibility_actions, i) == action)
      break;
  
  g_return_if_fail (i < priv->visibility_actions->len);
  
  dock = get_dock_nth (etoolbar, i);  
  if (visible)
    {
      gtk_widget_show (dock);
    }
  else
    {
      gtk_widget_hide (dock);
    }
  
  if (priv->save_hidden)
    {      
      flags = egg_toolbars_model_get_flags (priv->model, i);
      
      if (visible)
        {
	  flags &= ~(EGG_TB_MODEL_HIDDEN);
	}
      else
	{
	  flags |=  (EGG_TB_MODEL_HIDDEN);
	}
      
      egg_toolbars_model_set_flags (priv->model, i, flags);
    }
}

static void
toolbar_visibility_refresh (EggEditableToolbar *etoolbar)
{
  EggEditableToolbarPrivate *priv = etoolbar->priv;
  gint n_toolbars, n_items, i, j, k;
  GtkToggleAction *action;
  GList *list;
  GString *string;
  char action_name[40];
  char *action_label;
  char *tmp;    	
  
  if (priv == NULL || priv->model == NULL || priv->manager == NULL ||
      priv->visibility_paths == NULL || priv->actions == NULL)
    {
      return;
    }

  if (priv->visibility_actions == NULL)
    {
      priv->visibility_actions = g_ptr_array_new ();
    }
  
  if (priv->visibility_id != 0)
    {
      gtk_ui_manager_remove_ui (priv->manager, priv->visibility_id);
    }  
  
  priv->visibility_id = gtk_ui_manager_new_merge_id (priv->manager);
  
  n_toolbars = egg_toolbars_model_n_toolbars (priv->model);
  for (i = 0; i < n_toolbars; i++)
    {
      string = g_string_sized_new (0);
      n_items = egg_toolbars_model_n_items (priv->model, i);
      for (k = 0, j = 0; j < n_items; j++)
        {
          GValue value = { 0, };
          GtkAction *action;
          const char *name;

          name = egg_toolbars_model_item_nth (priv->model, i, j);
          if (name == NULL) continue;
          action = find_action (etoolbar, name);
          if (action == NULL) continue;

          g_value_init (&value, G_TYPE_STRING);
          g_object_get_property (G_OBJECT (action), "label", &value);
          name = g_value_get_string (&value);
          if (name == NULL)
	    {
		g_value_unset (&value);
		continue;
	    }
	  k += g_utf8_strlen (name, -1) + 2;
	  if (j > 0)
	    {
	      g_string_append (string, ", ");
	      if (j > 1 && k > 25)
		{
		  g_value_unset (&value);
		  break;
		}
	    }
	  g_string_append (string, name);
	  g_value_unset (&value);
	}
      if (j < n_items)
        {
	  g_string_append (string, " ...");
        }
      
      tmp = g_string_free (string, FALSE);
      for (j = 0, k = 0; tmp[j]; j++)
      {
	if (tmp[j] == '_') continue;
	tmp[k] = tmp[j];
	k++;
      }
      tmp[k] = 0;
      action_label = g_strdup_printf (_("Show “_%s”"), tmp);
      g_free (tmp);
      
      sprintf(action_name, "ToolbarToggle%d", i);
      
      if (i >= priv->visibility_actions->len)
        {
	  action = gtk_toggle_action_new (action_name, action_label, NULL, NULL);
	  g_ptr_array_add (priv->visibility_actions, action);
	  g_signal_connect_object (action, "toggled",
				   G_CALLBACK (toggled_visibility_cb),
				   etoolbar, 0);
	  gtk_action_group_add_action (priv->actions, GTK_ACTION (action));
	}
      else
        {
	  action = g_ptr_array_index (priv->visibility_actions, i);
	  g_object_set (action, "label", action_label, NULL);
        }

      gtk_action_set_visible (GTK_ACTION (action), (egg_toolbars_model_get_flags (priv->model, i) 
						    & EGG_TB_MODEL_NOT_REMOVABLE) == 0);
      gtk_toggle_action_set_active (action, GTK_WIDGET_VISIBLE
				    (get_dock_nth (etoolbar, i)));
      
      for (list = priv->visibility_paths; list != NULL; list = g_list_next (list))
        {
	  gtk_ui_manager_add_ui (priv->manager, priv->visibility_id,
				 (const char *)list->data, action_name, action_name,
				 GTK_UI_MANAGER_MENUITEM, FALSE);
	}
	    
      g_free (action_label);
    }
  
  gtk_ui_manager_ensure_update (priv->manager);
  
  while (i < priv->visibility_actions->len)
    {
      action = g_ptr_array_index (priv->visibility_actions, i);
      g_ptr_array_remove_index_fast (priv->visibility_actions, i);
      gtk_action_group_remove_action (priv->actions, GTK_ACTION (action));
      i++;
    }
}

static GtkWidget *
create_dock (EggEditableToolbar *etoolbar)
{
  GtkWidget *toolbar, *hbox;

  hbox = gtk_hbox_new (0, FALSE);

  toolbar = gtk_toolbar_new ();
  gtk_toolbar_set_show_arrow (GTK_TOOLBAR (toolbar), TRUE);
  gtk_widget_show (toolbar);
  gtk_box_pack_start (GTK_BOX (hbox), toolbar, TRUE, TRUE, 0);

  g_signal_connect (toolbar, "drag_drop",
		    G_CALLBACK (toolbar_drag_drop_cb), etoolbar); 
  g_signal_connect (toolbar, "drag_motion",
		    G_CALLBACK (toolbar_drag_motion_cb), etoolbar);
  g_signal_connect (toolbar, "drag_leave",
		    G_CALLBACK (toolbar_drag_leave_cb), etoolbar);

  g_signal_connect (toolbar, "drag_data_received",
		    G_CALLBACK (toolbar_drag_data_received_cb), etoolbar);
  g_signal_connect (toolbar, "popup_context_menu",
		    G_CALLBACK (popup_context_menu_cb), etoolbar);

  configure_drag_dest (etoolbar, GTK_TOOLBAR (toolbar));
  
  return hbox;
}

static void
set_fixed_style (EggEditableToolbar *t, GtkToolbarStyle style)
{
  g_return_if_fail (GTK_IS_TOOLBAR (t->priv->fixed_toolbar));
  gtk_toolbar_set_style (GTK_TOOLBAR (t->priv->fixed_toolbar),
  			 style == GTK_TOOLBAR_ICONS ? GTK_TOOLBAR_BOTH_HORIZ : style);
}

static void
unset_fixed_style (EggEditableToolbar *t)
{
  g_return_if_fail (GTK_IS_TOOLBAR (t->priv->fixed_toolbar));
  gtk_toolbar_unset_style (GTK_TOOLBAR (t->priv->fixed_toolbar));
}

static void
toolbar_changed_cb (EggToolbarsModel   *model,
	            int                 position,
	            EggEditableToolbar *etoolbar)
{
  GtkWidget *toolbar;
  EggTbModelFlags flags;
  GtkToolbarStyle style;

  flags = egg_toolbars_model_get_flags (model, position);
  toolbar = get_toolbar_nth (etoolbar, position);

  if (flags & EGG_TB_MODEL_ICONS)
  {
    style = GTK_TOOLBAR_ICONS;
  }
  else if (flags & EGG_TB_MODEL_TEXT)
  {
    style = GTK_TOOLBAR_TEXT;
  }
  else if (flags & EGG_TB_MODEL_BOTH)
  {
    style = GTK_TOOLBAR_BOTH;
  }
  else if (flags & EGG_TB_MODEL_BOTH_HORIZ)
  {
    style = GTK_TOOLBAR_BOTH_HORIZ;
  }
  else
  {
    gtk_toolbar_unset_style (GTK_TOOLBAR (toolbar));
    if (position == 0 && etoolbar->priv->fixed_toolbar)
      {
        unset_fixed_style (etoolbar);
      }
    return;
  }

  gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), style);
  if (position == 0 && etoolbar->priv->fixed_toolbar)
    {
      set_fixed_style (etoolbar, style);
    }

  toolbar_visibility_refresh (etoolbar);
}

static void
unparent_fixed (EggEditableToolbar *etoolbar)
{
  GtkWidget *toolbar, *dock;
  g_return_if_fail (GTK_IS_TOOLBAR (etoolbar->priv->fixed_toolbar));

  toolbar = etoolbar->priv->fixed_toolbar;
  dock = get_dock_nth (etoolbar, 0);

  if (dock && toolbar->parent != NULL)
    {
      gtk_container_remove (GTK_CONTAINER (dock), toolbar);
    }
}

static void
update_fixed (EggEditableToolbar *etoolbar)
{
  GtkWidget *toolbar, *dock;
  if (!etoolbar->priv->fixed_toolbar) return;

  toolbar = etoolbar->priv->fixed_toolbar;
  dock = get_dock_nth (etoolbar, 0);

  if (dock && toolbar && toolbar->parent == NULL)
    {
      gtk_box_pack_end (GTK_BOX (dock), toolbar, FALSE, TRUE, 0);

      gtk_widget_show (toolbar);
  
      gtk_widget_set_size_request (dock, -1, -1);
      gtk_widget_queue_resize_no_redraw (dock);
    }
}

static void
toolbar_added_cb (EggToolbarsModel   *model,
	          int                 position,
	          EggEditableToolbar *etoolbar)
{
  GtkWidget *dock;

  dock = create_dock (etoolbar);
  if ((egg_toolbars_model_get_flags (model, position) & EGG_TB_MODEL_HIDDEN) == 0)
    gtk_widget_show (dock);

  gtk_widget_set_size_request (dock, -1, MIN_TOOLBAR_HEIGHT);

  gtk_box_pack_start (GTK_BOX (etoolbar), dock, TRUE, TRUE, 0);

  gtk_box_reorder_child (GTK_BOX (etoolbar), dock, position);

  gtk_widget_show_all (dock);
  
  update_fixed (etoolbar);

  toolbar_visibility_refresh (etoolbar);
}

static void
toolbar_removed_cb (EggToolbarsModel   *model,
	            int                 position,
	            EggEditableToolbar *etoolbar)
{
  GtkWidget *dock;

  if (position == 0 && etoolbar->priv->fixed_toolbar != NULL)
    {
      unparent_fixed (etoolbar);
    }

  dock = get_dock_nth (etoolbar, position);
  gtk_widget_destroy (dock);

  update_fixed (etoolbar);
  
  toolbar_visibility_refresh (etoolbar);
}

static void
item_added_cb (EggToolbarsModel   *model,
	       int                 tpos,
	       int                 ipos,
	       EggEditableToolbar *etoolbar)
{
  GtkWidget *dock;
  GtkWidget *toolbar;
  GtkToolItem *item;

  toolbar = get_toolbar_nth (etoolbar, tpos);
  item = create_item_from_position (etoolbar, tpos, ipos);
  if (item == NULL) return;
    
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, ipos);
  
  connect_widget_signals (GTK_WIDGET (item), etoolbar);
  configure_item_tooltip (item);
  configure_item_cursor (item, etoolbar);
  configure_item_sensitivity (item, etoolbar);
  
  dock = get_dock_nth (etoolbar, tpos);
  gtk_widget_set_size_request (dock, -1, -1);
  gtk_widget_queue_resize_no_redraw (dock);

  toolbar_visibility_refresh (etoolbar);
}

static void
item_removed_cb (EggToolbarsModel   *model,
	         int                 toolbar_position,
	         int                 position,
	         EggEditableToolbar *etoolbar)
{
  EggEditableToolbarPrivate *priv = etoolbar->priv;
  
  GtkWidget *toolbar;
  GtkWidget *item;

  toolbar = get_toolbar_nth (etoolbar, toolbar_position);
  item = GTK_WIDGET (gtk_toolbar_get_nth_item
	(GTK_TOOLBAR (toolbar), position));
  g_return_if_fail (item != NULL);

  if (item == priv->selected)
    {
      /* FIXME */
    }

  gtk_container_remove (GTK_CONTAINER (toolbar), item);

  toolbar_visibility_refresh (etoolbar);
}

static void
egg_editable_toolbar_build (EggEditableToolbar *etoolbar)
{
  int i, l, n_items, n_toolbars;
  EggToolbarsModel *model = etoolbar->priv->model;

  g_return_if_fail (model != NULL);
  g_return_if_fail (etoolbar->priv->manager != NULL);

  n_toolbars = egg_toolbars_model_n_toolbars (model);

  for (i = 0; i < n_toolbars; i++)
    {
      GtkWidget *toolbar, *dock;

      dock = create_dock (etoolbar);
      if ((egg_toolbars_model_get_flags (model, i) & EGG_TB_MODEL_HIDDEN) == 0)
        gtk_widget_show (dock);
      gtk_box_pack_start (GTK_BOX (etoolbar), dock, TRUE, TRUE, 0);
      toolbar = get_toolbar_nth (etoolbar, i);

      n_items = egg_toolbars_model_n_items (model, i);
      for (l = 0; l < n_items; l++)
        {
          GtkToolItem *item;

          item = create_item_from_position (etoolbar, i, l);
          if (item)
            {
	      gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, l);
              
              connect_widget_signals (GTK_WIDGET (item), etoolbar);
	      configure_item_tooltip (item);
              configure_item_sensitivity (item, etoolbar);
            }
          else
            {
              egg_toolbars_model_remove_item (model, i, l);
              l--;
              n_items--;
            }
        }

      if (n_items == 0)
        {
            gtk_widget_set_size_request (dock, -1, MIN_TOOLBAR_HEIGHT);
        }
    }

  update_fixed (etoolbar);

  /* apply styles */
  for (i = 0; i < n_toolbars; i ++)
    {
      toolbar_changed_cb (model, i, etoolbar);
    }
}

static void
egg_editable_toolbar_disconnect_model (EggEditableToolbar *toolbar)
{
  EggToolbarsModel *model = toolbar->priv->model;

  g_signal_handlers_disconnect_by_func
    (model, G_CALLBACK (item_added_cb), toolbar);
  g_signal_handlers_disconnect_by_func
    (model, G_CALLBACK (item_removed_cb), toolbar);
  g_signal_handlers_disconnect_by_func
    (model, G_CALLBACK (toolbar_added_cb), toolbar);
  g_signal_handlers_disconnect_by_func
    (model, G_CALLBACK (toolbar_removed_cb), toolbar);
  g_signal_handlers_disconnect_by_func
    (model, G_CALLBACK (toolbar_changed_cb), toolbar);
}

static void
egg_editable_toolbar_deconstruct (EggEditableToolbar *toolbar)
{
  EggToolbarsModel *model = toolbar->priv->model;
  GList *children;

  g_return_if_fail (model != NULL);

  if (toolbar->priv->fixed_toolbar)
    {
       unset_fixed_style (toolbar);
       unparent_fixed (toolbar);
    }

  children = gtk_container_get_children (GTK_CONTAINER (toolbar));
  g_list_foreach (children, (GFunc) gtk_widget_destroy, NULL);
  g_list_free (children);
  
  children = toolbar->priv->visibility_paths;
  g_list_foreach (children, (GFunc) g_free, NULL);
  g_list_free (children);
}

void
egg_editable_toolbar_set_model (EggEditableToolbar *etoolbar,
				EggToolbarsModel   *model)
{
  EggEditableToolbarPrivate *priv = etoolbar->priv;

  if (priv->model == model) return;

  if (priv->model)
    {
      egg_editable_toolbar_disconnect_model (etoolbar);
      egg_editable_toolbar_deconstruct (etoolbar);

      g_object_unref (priv->model);
    }

  priv->model = g_object_ref (model);

  egg_editable_toolbar_build (etoolbar);

  toolbar_visibility_refresh (etoolbar);

  g_signal_connect (model, "item_added",
		    G_CALLBACK (item_added_cb), etoolbar);
  g_signal_connect (model, "item_removed",
		    G_CALLBACK (item_removed_cb), etoolbar);
  g_signal_connect (model, "toolbar_added",
		    G_CALLBACK (toolbar_added_cb), etoolbar);
  g_signal_connect (model, "toolbar_removed",
		    G_CALLBACK (toolbar_removed_cb), etoolbar);
  g_signal_connect (model, "toolbar_changed",
		    G_CALLBACK (toolbar_changed_cb), etoolbar);
}

static void
egg_editable_toolbar_init (EggEditableToolbar *etoolbar)
{
  EggEditableToolbarPrivate *priv;

  priv = etoolbar->priv = EGG_EDITABLE_TOOLBAR_GET_PRIVATE (etoolbar);

  priv->save_hidden = TRUE;
}

static void
egg_editable_toolbar_dispose (GObject *object)
{
  EggEditableToolbar *etoolbar = EGG_EDITABLE_TOOLBAR (object);
  EggEditableToolbarPrivate *priv = etoolbar->priv;

  if (priv->fixed_toolbar != NULL)
    {
      g_object_unref (priv->fixed_toolbar);
      priv->fixed_toolbar = NULL;
    }

  if (priv->manager != NULL)
    {
      g_object_unref (priv->manager);
      priv->manager = NULL;
    }

  if (priv->model)
    {
      egg_editable_toolbar_disconnect_model (etoolbar);
      g_object_unref (priv->model);
      priv->model = NULL;
    }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
egg_editable_toolbar_set_ui_manager (EggEditableToolbar *etoolbar,
				     GtkUIManager       *manager)
{
  static const GtkActionEntry actions[] = {
    { "MoveToolItem", STOCK_DRAG_MODE, N_("_Move on Toolbar"), NULL,
      N_("Move the selected item on the toolbar"), G_CALLBACK (move_item_cb) },
    { "RemoveToolItem", GTK_STOCK_REMOVE, N_("_Remove from Toolbar"), NULL,
      N_("Remove the selected item from the toolbar"), G_CALLBACK (remove_item_cb) },
    { "RemoveToolbar", GTK_STOCK_DELETE, N_("_Delete Toolbar"), NULL,
      N_("Remove the selected toolbar"), G_CALLBACK (remove_toolbar_cb) },
  };
  
  etoolbar->priv->manager = g_object_ref (manager);

  etoolbar->priv->actions = gtk_action_group_new ("ToolbarActions");
  gtk_action_group_set_translation_domain (etoolbar->priv->actions, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (etoolbar->priv->actions, actions,
		 		G_N_ELEMENTS (actions), etoolbar);
  gtk_ui_manager_insert_action_group (manager, etoolbar->priv->actions, -1);
  g_object_unref (etoolbar->priv->actions);

  toolbar_visibility_refresh (etoolbar);
}

GtkWidget * egg_editable_toolbar_get_selected (EggEditableToolbar   *etoolbar)
{
  return etoolbar->priv->selected;
}

void
egg_editable_toolbar_set_selected (EggEditableToolbar *etoolbar,
				   GtkWidget          *widget)
{
  GtkWidget *toolbar, *toolitem;
  gboolean editable;

  etoolbar->priv->selected = widget;
  
  toolbar = (widget != NULL) ? gtk_widget_get_ancestor (widget, GTK_TYPE_TOOLBAR) : NULL;
  toolitem = (widget != NULL) ? gtk_widget_get_ancestor (widget, GTK_TYPE_TOOL_ITEM) : NULL;
  
  if(toolbar != NULL)
    {
      gint tpos = get_toolbar_position (etoolbar, toolbar);
      editable = ((egg_toolbars_model_get_flags (etoolbar->priv->model, tpos) & EGG_TB_MODEL_NOT_EDITABLE) == 0);
    }
  else
    {
      editable = FALSE;
    }
  
  gtk_action_set_visible (find_action (etoolbar, "RemoveToolbar"), (toolbar != NULL) && (etoolbar->priv->edit_mode > 0));
  gtk_action_set_visible (find_action (etoolbar, "RemoveToolItem"), (toolitem != NULL) && editable);
  gtk_action_set_visible (find_action (etoolbar, "MoveToolItem"), (toolitem != NULL) && editable);
}

static void
set_edit_mode (EggEditableToolbar *etoolbar,
	       gboolean mode)
{
  EggEditableToolbarPrivate *priv = etoolbar->priv;
  int i, l, n_items;

  i = priv->edit_mode;
  if (mode)
    {
      priv->edit_mode++;
    }
  else
    {
      g_return_if_fail (priv->edit_mode > 0);
      priv->edit_mode--;
    }
  i *= priv->edit_mode;
  
  if (i == 0)
    {
      for (i = get_n_toolbars (etoolbar)-1; i >= 0; i--)
        {
          GtkWidget *toolbar;
          
          toolbar = get_toolbar_nth (etoolbar, i);
          n_items = gtk_toolbar_get_n_items (GTK_TOOLBAR (toolbar));

          if (n_items == 0 && priv->edit_mode == 0)
            {
              egg_toolbars_model_remove_toolbar (priv->model, i);
            }
          else
            {          
              for (l = 0; l < n_items; l++)
                {
                  GtkToolItem *item;
              
                  item = gtk_toolbar_get_nth_item (GTK_TOOLBAR (toolbar), l);
                  
                  configure_item_cursor (item, etoolbar);
                  configure_item_sensitivity (item, etoolbar);
                }
            }
        }
    }
}

static void
egg_editable_toolbar_set_property (GObject      *object,
				   guint         prop_id,
				   const GValue *value,
				   GParamSpec   *pspec)
{
  EggEditableToolbar *etoolbar = EGG_EDITABLE_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_UI_MANAGER:
      egg_editable_toolbar_set_ui_manager (etoolbar, g_value_get_object (value));
      break;
    case PROP_TOOLBARS_MODEL:
      egg_editable_toolbar_set_model (etoolbar, g_value_get_object (value));
      break;
    case PROP_SELECTED:
      egg_editable_toolbar_set_selected (etoolbar, g_value_get_object (value));
      break;
    case PROP_POPUP_PATH:
      etoolbar->priv->popup_path = g_strdup (g_value_get_string (value));
      break;
    case PROP_EDIT_MODE:
      set_edit_mode (etoolbar, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
egg_editable_toolbar_get_property (GObject    *object,
				   guint       prop_id,
				   GValue     *value,
				   GParamSpec *pspec)
{
  EggEditableToolbar *etoolbar = EGG_EDITABLE_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_UI_MANAGER:
      g_value_set_object (value, etoolbar->priv->manager);
      break;
    case PROP_TOOLBARS_MODEL:
      g_value_set_object (value, etoolbar->priv->model);
      break;
    case PROP_SELECTED:
      g_value_set_object (value, etoolbar->priv->selected);
      break;
    case PROP_EDIT_MODE:
      g_value_set_boolean (value, etoolbar->priv->edit_mode>0);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
egg_editable_toolbar_class_init (EggEditableToolbarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->dispose = egg_editable_toolbar_dispose;
  object_class->set_property = egg_editable_toolbar_set_property;
  object_class->get_property = egg_editable_toolbar_get_property;

  egg_editable_toolbar_signals[ACTION_REQUEST] =
    g_signal_new ("action_request",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggEditableToolbarClass, action_request),
		  NULL, NULL, g_cclosure_marshal_VOID__STRING,
		  G_TYPE_NONE, 1, G_TYPE_STRING);

  g_object_class_install_property (object_class,
				   PROP_UI_MANAGER,
				   g_param_spec_object ("ui-manager",
							"UI-Mmanager",
							"UI Manager",
							GTK_TYPE_UI_MANAGER,
							G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class,
				   PROP_TOOLBARS_MODEL,
				   g_param_spec_object ("model",
							"Model",
							"Toolbars Model",
							EGG_TYPE_TOOLBARS_MODEL,
							G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
  g_object_class_install_property (object_class,
				   PROP_SELECTED,
				   g_param_spec_object ("selected",
							"Selected",
							"Selected toolitem",
							GTK_TYPE_TOOL_ITEM,
							G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
				   PROP_POPUP_PATH,
				   g_param_spec_string ("popup-path",
							"popup-path",
							"popup-path",
							NULL,
							G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
				   PROP_EDIT_MODE,
				   g_param_spec_boolean ("edit-mode",
							 "Edit-Mode",
							 "Edit Mode",
							 FALSE,
							 G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (object_class, sizeof (EggEditableToolbarPrivate));
}

GtkWidget *
egg_editable_toolbar_new (GtkUIManager *manager,
                          const char *popup_path)
{
    return GTK_WIDGET (g_object_new (EGG_TYPE_EDITABLE_TOOLBAR,
                                     "ui-manager", manager,
                                     "popup-path", popup_path,
                                     NULL));
}

GtkWidget *
egg_editable_toolbar_new_with_model (GtkUIManager *manager,
 				     EggToolbarsModel *model,
                                     const char *popup_path)
{
  return GTK_WIDGET (g_object_new (EGG_TYPE_EDITABLE_TOOLBAR,
                                   "ui-manager", manager,
                                   "model", model,
                                   "popup-path", popup_path,
				   NULL));
}

gboolean
egg_editable_toolbar_get_edit_mode (EggEditableToolbar *etoolbar)
{
  EggEditableToolbarPrivate *priv = etoolbar->priv;

  return priv->edit_mode > 0;
}

void
egg_editable_toolbar_set_edit_mode (EggEditableToolbar *etoolbar,
				    gboolean mode)
{
  set_edit_mode (etoolbar, mode);
  g_object_notify (G_OBJECT (etoolbar), "edit-mode");
}

void
egg_editable_toolbar_add_visibility (EggEditableToolbar *etoolbar,
				     const char *path)
{
  etoolbar->priv->visibility_paths = g_list_prepend
	  (etoolbar->priv->visibility_paths, g_strdup (path));
}

void
egg_editable_toolbar_show (EggEditableToolbar *etoolbar,
			   const char *name)
{
  EggEditableToolbarPrivate *priv = etoolbar->priv;
  EggToolbarsModel *model = priv->model;
  int i, n_toolbars;

  n_toolbars = egg_toolbars_model_n_toolbars (model);
  for (i = 0; i < n_toolbars; i++)
    {
      const char *toolbar_name;

      toolbar_name = egg_toolbars_model_toolbar_nth (model, i);
      if (strcmp (toolbar_name, name) == 0)
        {
          gtk_widget_show (get_dock_nth (etoolbar, i));
        }
    }
}

void
egg_editable_toolbar_hide (EggEditableToolbar *etoolbar,
			   const char *name)
{
  EggEditableToolbarPrivate *priv = etoolbar->priv;
  EggToolbarsModel *model = priv->model;
  int i, n_toolbars;

  n_toolbars = egg_toolbars_model_n_toolbars (model);
  for (i = 0; i < n_toolbars; i++)
    {
      const char *toolbar_name;

      toolbar_name = egg_toolbars_model_toolbar_nth (model, i);
      if (strcmp (toolbar_name, name) == 0)
      {
        gtk_widget_hide (get_dock_nth (etoolbar, i));
      }
    }
}

void
egg_editable_toolbar_set_fixed (EggEditableToolbar *etoolbar,
				GtkToolbar *toolbar)
{
  EggEditableToolbarPrivate *priv = etoolbar->priv;

  g_return_if_fail (!toolbar || GTK_IS_TOOLBAR (toolbar));

  if (priv->fixed_toolbar)
    {
      unparent_fixed (etoolbar);
      g_object_unref (priv->fixed_toolbar);
      priv->fixed_toolbar = NULL;
    }

  if (toolbar)
    {
      priv->fixed_toolbar = GTK_WIDGET (toolbar);
      gtk_toolbar_set_show_arrow (toolbar, FALSE);
      g_object_ref (toolbar);
      gtk_object_sink (GTK_OBJECT (toolbar));
    }

  update_fixed (etoolbar);
}

#define DEFAULT_ICON_HEIGHT 20
#define DEFAULT_ICON_WIDTH 0

static void
fake_expose_widget (GtkWidget *widget,
		    GdkPixmap *pixmap)
{
  GdkWindow *tmp_window;
  GdkEventExpose event;

  event.type = GDK_EXPOSE;
  event.window = pixmap;
  event.send_event = FALSE;
  event.area = widget->allocation;
  event.region = NULL;
  event.count = 0;

  tmp_window = widget->window;
  widget->window = pixmap;
  gtk_widget_send_expose (widget, (GdkEvent *) &event);
  widget->window = tmp_window;
}

/* We should probably experiment some more with this.
 * Right now the rendered icon is pretty good for most
 * themes. However, the icon is slightly large for themes
 * with large toolbar icons.
 */
static GdkPixbuf *
new_pixbuf_from_widget (GtkWidget *widget)
{
  GtkWidget *window;
  GdkPixbuf *pixbuf;
  GtkRequisition requisition;
  GtkAllocation allocation;
  GdkPixmap *pixmap;
  GdkVisual *visual;
  gint icon_width;
  gint icon_height;

  icon_width = DEFAULT_ICON_WIDTH;

  if (!gtk_icon_size_lookup_for_settings (gtk_settings_get_default (), 
					  GTK_ICON_SIZE_LARGE_TOOLBAR,
					  NULL, 
					  &icon_height))
    {
      icon_height = DEFAULT_ICON_HEIGHT;
    }

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  
  gtk_container_add (GTK_CONTAINER (window), widget);
  gtk_widget_realize (window);
  gtk_widget_show (widget);
  gtk_widget_realize (widget);
  gtk_widget_map (widget);

  /* Gtk will never set the width or height of a window to 0. So setting the width to
   * 0 and than getting it will provide us with the minimum width needed to render
   * the icon correctly, without any additional window background noise.
   * This is needed mostly for pixmap based themes.
   */
  gtk_window_set_default_size (GTK_WINDOW (window), icon_width, icon_height);
  gtk_window_get_size (GTK_WINDOW (window),&icon_width, &icon_height);

  gtk_widget_size_request (window, &requisition);
  allocation.x = 0;
  allocation.y = 0;
  allocation.width = icon_width;
  allocation.height = icon_height;
  gtk_widget_size_allocate (window, &allocation);
  gtk_widget_size_request (window, &requisition);
  
  /* Create a pixmap */
  visual = gtk_widget_get_visual (window);
  pixmap = gdk_pixmap_new (NULL, icon_width, icon_height, visual->depth);
  gdk_drawable_set_colormap (GDK_DRAWABLE (pixmap), gtk_widget_get_colormap (window));

  /* Draw the window */
  gtk_widget_ensure_style (window);
  g_assert (window->style);
  g_assert (window->style->font_desc);
  
  fake_expose_widget (window, pixmap);
  fake_expose_widget (widget, pixmap);
  
  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, icon_width, icon_height);
  gdk_pixbuf_get_from_drawable (pixbuf, pixmap, NULL, 0, 0, 0, 0, icon_width, icon_height);

  gtk_widget_destroy (window);

  return pixbuf;
}

static GdkPixbuf *
new_separator_pixbuf ()
{
  GtkWidget *separator;
  GdkPixbuf *pixbuf;

  separator = gtk_vseparator_new ();
  pixbuf = new_pixbuf_from_widget (separator);
  return pixbuf;
}

static void
update_separator_image (GtkImage *image)
{
  GdkPixbuf *pixbuf = new_separator_pixbuf ();
  gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
  g_object_unref (pixbuf);
}

static gboolean
style_set_cb (GtkWidget *widget,
              GtkStyle *previous_style,
              GtkImage *image)
{

  update_separator_image (image);
  return FALSE;
}

GtkWidget *
_egg_editable_toolbar_new_separator_image (void)
{
  GtkWidget *image = gtk_image_new ();
  update_separator_image (GTK_IMAGE (image));
  g_signal_connect (G_OBJECT (image), "style_set",
		    G_CALLBACK (style_set_cb), GTK_IMAGE (image));

  return image;
}

EggToolbarsModel *
egg_editable_toolbar_get_model (EggEditableToolbar *etoolbar)
{
  return etoolbar->priv->model;
}
