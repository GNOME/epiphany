/*
 *  Copyright (C) 2003-2004 Marco Pesenti Gritti
 *  Copyright (C) 2004 Christian Persch
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktoolbar.h>
#include <gtk/gtktoolitem.h>
#include <gtk/gtkseparatortoolitem.h>
#include <glib/gi18n.h>
#include <string.h>

static void egg_editable_toolbar_class_init	(EggEditableToolbarClass *klass);
static void egg_editable_toolbar_init		(EggEditableToolbar *t);
static void egg_editable_toolbar_finalize	(GObject *object);

#define MIN_TOOLBAR_HEIGHT 20

static GtkTargetEntry dest_drag_types[] = {
  {EGG_TOOLBAR_ITEM_TYPE, GTK_TARGET_SAME_APP, 0},
};
static int n_dest_drag_types = G_N_ELEMENTS (dest_drag_types);

enum
{
  PROP_0,
  PROP_TOOLBARS_MODEL,
  PROP_UI_MANAGER
};

enum
{
  ACTION_REQUEST,
  LAST_SIGNAL
};

static guint egg_editable_toolbar_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

#define EGG_EDITABLE_TOOLBAR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EGG_TYPE_EDITABLE_TOOLBAR, EggEditableToolbarPrivate))

struct EggEditableToolbarPrivate
{
  GtkUIManager *manager;
  EggToolbarsModel *model;
  gboolean edit_mode;
  GtkWidget *selected_toolbar;
  GtkToolItem *fixed;
  GtkWidget *fixed_toolbar;

  gboolean pending;
  GtkToolbar *target_toolbar;
  GtkWidget *dragged_item;
};

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

      egg_editable_toolbar_type = g_type_register_static (GTK_TYPE_VBOX,
							  "EggEditableToolbar",
							  &our_info, 0);
    }

  return egg_editable_toolbar_type;
}

static int
get_toolbar_position (EggEditableToolbar *etoolbar, GtkWidget *toolbar)
{
  GList *l;
  int result;

  l = gtk_container_get_children (GTK_CONTAINER (etoolbar));
  result = g_list_index (l, toolbar->parent);
  g_list_free (l);

  return result;
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

  l = gtk_container_get_children (GTK_CONTAINER (dock));
  result = GTK_WIDGET (l->data);
  g_list_free (l);

  return result;
}

static GtkAction *
find_action (EggEditableToolbar *t,
	     const char         *name)
{
  GList *l;
  GtkAction *action = NULL;

  l = gtk_ui_manager_get_action_groups (t->priv->manager);

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
	gtk_widget_hide (widget);
}

static void
drag_end_cb (GtkWidget          *widget,
	     GdkDragContext     *context,
	     EggEditableToolbar *etoolbar)
{
	gtk_widget_show (widget);
}

static void
drag_data_get_cb (GtkWidget          *widget,
		  GdkDragContext     *context,
		  GtkSelectionData   *selection_data,
		  guint               info,
		  guint32             time,
		  EggEditableToolbar *etoolbar)
{
  const char *id, *type;
  char *target;

  g_return_if_fail (EGG_IS_EDITABLE_TOOLBAR (etoolbar));

  type = g_object_get_data (G_OBJECT (widget), "type");
  id = g_object_get_data (G_OBJECT (widget), "id");
  if (strcmp (id, "separator") == 0)
    {
      target = g_strdup (id);
    }
  else
    {
      target = egg_toolbars_model_get_item_data (etoolbar->priv->model,
						 type, id);
    }

  gtk_selection_data_set (selection_data,
			  selection_data->target, 8, target, strlen (target));

  g_free (target);
}

static void
set_drag_cursor (GtkWidget *widget)
{
  if (widget->window)
    {
      GdkCursor *cursor;
      GdkPixbuf *pixbuf;

      pixbuf = gdk_pixbuf_new_from_file (CURSOR_DIR "/hand-open.png", NULL);
      cursor = gdk_cursor_new_from_pixbuf (gdk_display_get_default (),
					   pixbuf, 12, 12);
      gdk_window_set_cursor (widget->window, cursor);
      gdk_cursor_unref (cursor);
      g_object_unref (pixbuf);
    }
}

static void
unset_drag_cursor (GtkWidget *widget)
{
  if (widget->window)
    {
      gdk_window_set_cursor (widget->window, NULL);
    }
}

static void
set_item_drag_source (EggToolbarsModel *model,
		      GtkWidget        *item,
		      GtkAction        *action,
		      gboolean          is_separator,
		      const char       *type)
{
  GtkTargetEntry target_entry;
  const char *id;

  target_entry.target = (char *)type;
  target_entry.flags = GTK_TARGET_SAME_APP;
  target_entry.info = 0;

  gtk_drag_source_set (item, GDK_BUTTON1_MASK,
                       &target_entry, 1,
                       GDK_ACTION_MOVE);

  if (is_separator)
    {
      GtkWidget *icon;
      GdkPixbuf *pixbuf;

      id = "separator";

      icon = _egg_editable_toolbar_new_separator_image ();
      pixbuf = gtk_image_get_pixbuf (GTK_IMAGE (icon));
      gtk_drag_source_set_icon_pixbuf (item, pixbuf);
    }
  else
    {
      const char *stock_id;
      GValue value = { 0, };
      GdkPixbuf *pixbuf;

      id = gtk_action_get_name (action);

      g_value_init (&value, G_TYPE_STRING);
      g_object_get_property (G_OBJECT (action), "stock_id", &value);
      stock_id = g_value_get_string (&value);

      if (stock_id != NULL)
        {
          pixbuf = gtk_widget_render_icon (item, stock_id,
				           GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
        }
      else
        {
          pixbuf = gtk_widget_render_icon (item, GTK_STOCK_DND,
					   GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
        }

      gtk_drag_source_set_icon_pixbuf (item, pixbuf);
      g_object_unref (pixbuf);

      g_value_unset (&value);
    }

    g_object_set_data_full (G_OBJECT (item), "id",
                            g_strdup (id), g_free);
    g_object_set_data_full (G_OBJECT (item), "type",
                            g_strdup (type), g_free);
}

static GtkWidget *
create_item_from_action (EggEditableToolbar *t,
			 const char *action_name,
			 const char *type,
			 gboolean is_separator,
			 GtkAction **ret_action)
{
  GtkWidget *item;
  GtkAction *action;

  if (is_separator)
    {
      item = GTK_WIDGET (gtk_separator_tool_item_new ());
      action = NULL;
    }
  else
    {
      g_return_val_if_fail (action_name != NULL, NULL);

      g_signal_emit (G_OBJECT (t), egg_editable_toolbar_signals[ACTION_REQUEST],
		     0, action_name);

      action = find_action (t, action_name);
      if (action)
        {
          item = gtk_action_create_tool_item (action);
        }
      else
        {
          return NULL;
        }  
    }

  gtk_widget_show (item);

  g_signal_connect (item, "drag_begin",
		    G_CALLBACK (drag_begin_cb), t);
  g_signal_connect (item, "drag_end",
		    G_CALLBACK (drag_end_cb), t);
  g_signal_connect (item, "drag_data_get",
		    G_CALLBACK (drag_data_get_cb), t);
  g_signal_connect (item, "drag_data_delete",
		    G_CALLBACK (drag_data_delete_cb), t);

  if (t->priv->edit_mode)
    {
      set_drag_cursor (item);
      gtk_widget_set_sensitive (item, TRUE);
      set_item_drag_source (t->priv->model, item, action,
			    is_separator, type);
      gtk_tool_item_set_use_drag_window (GTK_TOOL_ITEM (item), TRUE);
    }

  if (ret_action)
    {
      *ret_action = action;
    }

  return item;
}

static GtkWidget *
create_item (EggEditableToolbar *t,
	     EggToolbarsModel   *model,
	     int                 toolbar_position,
	     int                 position,
             GtkAction         **ret_action)
{
  const char *action_name, *type;
  gboolean is_separator;

  egg_toolbars_model_item_nth (model, toolbar_position, position,
			       &is_separator, &action_name, &type);
  return create_item_from_action (t, action_name, type,
				  is_separator, ret_action);
}

static gboolean
data_is_separator (const char *data)
{
  return strcmp (data, "separator") == 0;
}

static void
drag_data_received_cb (GtkWidget          *widget,
		       GdkDragContext     *context,
		       gint                x,
		       gint                y,
		       GtkSelectionData   *selection_data,
		       guint               info,
		       guint               time,
		       EggEditableToolbar *etoolbar)
{
  char *type;
  char *id;

  GdkAtom target;
	  
  target = gtk_drag_dest_find_target (widget, context, NULL);
  type = egg_toolbars_model_get_item_type (etoolbar->priv->model, target);
  id = egg_toolbars_model_get_item_id (etoolbar->priv->model, type, selection_data->data);

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

  if (id == NULL)
    {
      etoolbar->priv->pending = FALSE;
      g_free (type);
      return;
    }

  if (etoolbar->priv->pending)
    {
      etoolbar->priv->pending = FALSE;
      etoolbar->priv->dragged_item =
        create_item_from_action (etoolbar, id, type,
				 data_is_separator (id), NULL);
      g_object_ref (etoolbar->priv->dragged_item);
      gtk_object_sink (GTK_OBJECT (etoolbar->priv->dragged_item));
    }
  else
    {
      int pos, toolbar_pos;

      pos = gtk_toolbar_get_drop_index (GTK_TOOLBAR (widget), x, y);
      toolbar_pos = get_toolbar_position (etoolbar, widget);

      if (data_is_separator (selection_data->data))
	{
	  egg_toolbars_model_add_separator (etoolbar->priv->model,
					    toolbar_pos, pos);
	}
      else
	{
	  egg_toolbars_model_add_item (etoolbar->priv->model,
				       toolbar_pos, pos, id, type);
	}
      
      gtk_drag_finish (context, TRUE, context->action == GDK_ACTION_MOVE,
		       time);
    }

  g_free (type);
  g_free (id);
}

static void
remove_toolbar_cb (GtkWidget          *menuitem,
		   EggEditableToolbar *etoolbar)
{
  int pos;

  pos = get_toolbar_position (etoolbar, etoolbar->priv->selected_toolbar);
  egg_toolbars_model_remove_toolbar (etoolbar->priv->model, pos);
}

static void
popup_toolbar_context_menu_cb (GtkWidget          *toolbar,
			       gint		   x,
			       gint		   y,
			       gint                button_number,
			       EggEditableToolbar *t)
{
  GtkWidget *menu;
  GtkWidget *item;
  GtkWidget *image;

  if (t->priv->edit_mode)
    {
      EggTbModelFlags flags;
      int position;

      t->priv->selected_toolbar = toolbar;

      menu = gtk_menu_new ();

      item = gtk_image_menu_item_new_with_mnemonic (_("_Remove Toolbar"));
      gtk_widget_show (item);
      image = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
      gtk_widget_show (image);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      g_signal_connect (item, "activate",
	                G_CALLBACK (remove_toolbar_cb),
		        t);

      position = get_toolbar_position (t, toolbar);
      flags = egg_toolbars_model_get_flags (t->priv->model, position);
      if (flags & EGG_TB_MODEL_NOT_REMOVABLE)
        {
          gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
        }

      gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 2,
		      gtk_get_current_event_time ());
    }
}

static void
free_dragged_item (EggEditableToolbar *etoolbar)
{
  if (etoolbar->priv->dragged_item)
    {
      gtk_widget_destroy (etoolbar->priv->dragged_item);
      g_object_unref (etoolbar->priv->dragged_item);
      etoolbar->priv->dragged_item = NULL;
    }
}

static gboolean
toolbar_drag_drop_cb (GtkWidget          *widget,
		      GdkDragContext     *context,
		      gint                x,
		      gint                y,
		      guint               time,
		      EggEditableToolbar *etoolbar)
{
  GdkAtom target;

  target = gtk_drag_dest_find_target (widget, context, NULL);
  if (target != GDK_NONE)
    {
      gtk_drag_get_data (widget, context,
                         target,
                         time);
      return TRUE;
    }
  
  free_dragged_item (etoolbar);
  
  return FALSE;
}

static gboolean
toolbar_drag_motion_cb (GtkWidget          *widget,
		        GdkDragContext     *context,
		        gint                x,
		        gint                y,
		        guint               time,
		        EggEditableToolbar *etoolbar)
{
  GdkAtom target;
  int index;
  GtkToolbar *toolbar = GTK_TOOLBAR (widget);
  GtkToolItem *item;
  GtkWidget *source;

  source = gtk_drag_get_source_widget (context);
  if (source)
    {
      EggTbModelFlags flags;
      int pos;
      gboolean is_item;

      pos = get_toolbar_position (etoolbar, widget);
      flags = egg_toolbars_model_get_flags (etoolbar->priv->model, pos);

      is_item = etoolbar->priv->edit_mode &&
		(gtk_widget_get_ancestor (source, EGG_TYPE_EDITABLE_TOOLBAR) ||
		 gtk_widget_get_ancestor (source, EGG_TYPE_TOOLBAR_EDITOR));

      if ((flags & EGG_TB_MODEL_ACCEPT_ITEMS_ONLY) && !is_item)
        {
          gdk_drag_status (context, 0, time);
          return FALSE;
        }

      if (gtk_widget_is_ancestor (source, widget))
        {
          context->suggested_action = GDK_ACTION_MOVE;
        }
    }

  target = gtk_drag_dest_find_target (widget, context, NULL);
  if (target == GDK_NONE)
    {
      gdk_drag_status (context, 0, time);
      return FALSE;
    }

  if (etoolbar->priv->target_toolbar != toolbar)
    {
      if (etoolbar->priv->target_toolbar)
	gtk_toolbar_set_drop_highlight_item
		(etoolbar->priv->target_toolbar, NULL, 0);
      
      free_dragged_item (etoolbar);
      etoolbar->priv->pending = TRUE;

      etoolbar->priv->target_toolbar = toolbar;

      gtk_drag_get_data (widget, context, target, time);
    }

  if (etoolbar->priv->dragged_item != NULL &&
      etoolbar->priv->edit_mode)
    {
      item = GTK_TOOL_ITEM (etoolbar->priv->dragged_item);

      index = gtk_toolbar_get_drop_index (toolbar, x, y);
      gtk_toolbar_set_drop_highlight_item (toolbar, item, index);
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
  /* This is a workaround for bug 125557. Sometimes
   * we seemingly enter another toolbar *before* leaving
   * the current one.
   *
   * In that case etoolbar->priv->target_toolbar will
   * have been set to something else and the highlighting
   * will already have been turned off
   */
  
  if (etoolbar->priv->target_toolbar == toolbar)
    {
      gtk_toolbar_set_drop_highlight_item (toolbar, NULL, 0);

      etoolbar->priv->target_toolbar = NULL;
      free_dragged_item (etoolbar);
    }
}

static GtkWidget *
create_dock (EggEditableToolbar *t)
{
  GtkWidget *toolbar, *hbox;

  hbox = gtk_hbox_new (0, FALSE);
  gtk_widget_show (hbox);

  toolbar = gtk_toolbar_new ();
  gtk_toolbar_set_show_arrow (GTK_TOOLBAR (toolbar), TRUE);
  gtk_widget_show (toolbar);
  gtk_box_pack_start (GTK_BOX (hbox), toolbar, TRUE, TRUE, 0);

  gtk_drag_dest_set (toolbar, 0,
		     dest_drag_types, n_dest_drag_types,
		     GDK_ACTION_MOVE | GDK_ACTION_COPY);
 
  g_signal_connect (toolbar, "drag_drop",
		    G_CALLBACK (toolbar_drag_drop_cb), t); 
  g_signal_connect (toolbar, "drag_motion",
		    G_CALLBACK (toolbar_drag_motion_cb), t);
  g_signal_connect (toolbar, "drag_leave",
		    G_CALLBACK (toolbar_drag_leave_cb), t);

  g_signal_connect (toolbar, "drag_data_received",
		    G_CALLBACK (drag_data_received_cb), t);
  g_signal_connect (toolbar, "popup_context_menu",
		    G_CALLBACK (popup_toolbar_context_menu_cb), t);

  return hbox;
}

static void
set_fixed_style (EggEditableToolbar *t, GtkToolbarStyle style)
{
  g_return_if_fail (GTK_IS_TOOLBAR (t->priv->fixed_toolbar));
  gtk_toolbar_set_style (GTK_TOOLBAR (t->priv->fixed_toolbar), style);
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
	            EggEditableToolbar *t)
{
  GtkWidget *toolbar;
  EggTbModelFlags flags;
  GtkToolbarStyle style;

  flags = egg_toolbars_model_get_flags (model, position);
  toolbar = get_toolbar_nth (t, position);

  if (flags & EGG_TB_MODEL_ICONS_ONLY)
  {
    style = GTK_TOOLBAR_ICONS;
  }
  else if (flags & EGG_TB_MODEL_TEXT_ONLY)
  {
    style = GTK_TOOLBAR_TEXT;
  }
  else if (flags & EGG_TB_MODEL_ICONS_TEXT)
  {
    style = GTK_TOOLBAR_BOTH;
  }
  else if (flags & EGG_TB_MODEL_ICONS_TEXT_HORIZ)
  {
    style = GTK_TOOLBAR_BOTH_HORIZ;
  }
  else
  {
    gtk_toolbar_unset_style (GTK_TOOLBAR (toolbar));
    if (position == 0 && t->priv->fixed_toolbar)
      {
        unset_fixed_style (t);
      }
    return;
  }

  gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), style);
  if (position == 0 && t->priv->fixed_toolbar)
    {
      set_fixed_style (t, style);
    }
}

static void
unparent_fixed (EggEditableToolbar *t)
{
  GtkWidget *toolbar, *dock;
  g_return_if_fail (GTK_IS_TOOLBAR (t->priv->fixed_toolbar));

  toolbar = t->priv->fixed_toolbar;
  dock = get_dock_nth (t, 0);

  if (dock && toolbar->parent != NULL)
    {
      gtk_container_remove (GTK_CONTAINER (dock), toolbar);
    }
}

static void
update_fixed (EggEditableToolbar *t)
{
  GtkWidget *toolbar, *dock;
  if (!t->priv->fixed_toolbar) return;

  toolbar = t->priv->fixed_toolbar;
  dock = get_dock_nth (t, 0);

  if (dock && toolbar && toolbar->parent == NULL)
    {
      gtk_box_pack_end (GTK_BOX (dock), toolbar, FALSE, TRUE, 0);

      gtk_widget_show_all (toolbar);
    }
}

static void
toolbar_added_cb (EggToolbarsModel   *model,
	          int                 position,
	          EggEditableToolbar *t)
{
  GtkWidget *dock;

  dock = create_dock (t);

  gtk_widget_set_size_request (dock, -1, MIN_TOOLBAR_HEIGHT);

  gtk_box_pack_start (GTK_BOX (t), dock, TRUE, TRUE, 0);

  gtk_box_reorder_child (GTK_BOX (t), dock, position);

  gtk_widget_show_all (dock);
  
  update_fixed (t);
}

static void
toolbar_removed_cb (EggToolbarsModel   *model,
	            int                 position,
	            EggEditableToolbar *t)
{
  GtkWidget *toolbar;

  toolbar = get_dock_nth (t, position);
  gtk_widget_destroy (toolbar);

  update_fixed (t);
}

static void
item_added_cb (EggToolbarsModel   *model,
	       int                 toolbar_position,
	       int                 position,
	       EggEditableToolbar *t)
{
  GtkWidget *dock;
  GtkWidget *toolbar;
  GtkWidget *item;
  GtkAction *action;

  toolbar = get_toolbar_nth (t, toolbar_position);
  item = create_item (t, model, toolbar_position, position, &action);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar),
		      GTK_TOOL_ITEM (item), position);

  dock = get_dock_nth (t, toolbar_position);
  gtk_widget_set_size_request (dock, -1, -1);
  gtk_widget_queue_resize_no_redraw (dock);

  /* FIXME Hack to make tooltip work from gtk */
  if (action)
    {
      g_object_notify (G_OBJECT (action), "tooltip");
    }
}

static void
item_removed_cb (EggToolbarsModel   *model,
	         int                 toolbar_position,
	         int                 position,
	         EggEditableToolbar *t)
{
  GtkWidget *toolbar;
  GtkWidget *item;

  toolbar = get_toolbar_nth (t, toolbar_position);
  item = GTK_WIDGET (gtk_toolbar_get_nth_item
	(GTK_TOOLBAR (toolbar), position));
  g_return_if_fail (item != NULL);
  gtk_container_remove (GTK_CONTAINER (toolbar), item);

  if (egg_toolbars_model_n_items (model, toolbar_position) == 0)
    {
      egg_toolbars_model_remove_toolbar (model, toolbar_position);
    }
}

static void
egg_editable_toolbar_construct (EggEditableToolbar *t)
{
  int i, l, n_items, n_toolbars;
  EggToolbarsModel *model = t->priv->model;

  g_return_if_fail (model != NULL);
  g_return_if_fail (t->priv->manager != NULL);

  n_toolbars = egg_toolbars_model_n_toolbars (model);

  for (i = 0; i < n_toolbars; i++)
    {
      GtkWidget *toolbar, *dock;
      EggTbModelFlags flags;

      dock = create_dock (t);
      gtk_box_pack_start (GTK_BOX (t), dock, TRUE, TRUE, 0);
      toolbar = get_toolbar_nth (t, i);

      flags = egg_toolbars_model_get_flags (model, i);
      if (flags & EGG_TB_MODEL_ICONS_ONLY)
        {
          gtk_toolbar_set_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_ICONS);
          if (i == 0 && t->priv->fixed_toolbar)
            {
	      set_fixed_style (t, GTK_TOOLBAR_ICONS);
            }
        }

      n_items = egg_toolbars_model_n_items (model, i);
      for (l = 0; l < n_items; l++)
        {
          GtkWidget *item;
          GtkAction *action;

          item = create_item (t, model, i, l, &action);
          if (item)
            {
	      gtk_toolbar_insert (GTK_TOOLBAR (toolbar),
			          GTK_TOOL_ITEM (item), l);
              /* FIXME Hack to make tooltip work from gtk */
              if (action)
                {
                  g_object_notify (G_OBJECT (action), "tooltip");
                }
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

  update_fixed (t);
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
}

void
egg_editable_toolbar_set_model (EggEditableToolbar *toolbar,
				EggToolbarsModel   *model)
{
  g_return_if_fail (EGG_IS_TOOLBARS_MODEL (model));
  g_return_if_fail (EGG_IS_EDITABLE_TOOLBAR (toolbar));
  g_return_if_fail (toolbar->priv->manager);

  if (toolbar->priv->model == model) return;

  if (toolbar->priv->model)
    {
      egg_editable_toolbar_disconnect_model (toolbar);
      egg_editable_toolbar_deconstruct (toolbar);

      g_object_unref (toolbar->priv->model);
    }

  toolbar->priv->model = g_object_ref (model);

  egg_editable_toolbar_construct (toolbar);

  g_signal_connect (model, "item_added",
		    G_CALLBACK (item_added_cb), toolbar);
  g_signal_connect (model, "item_removed",
		    G_CALLBACK (item_removed_cb), toolbar);
  g_signal_connect (model, "toolbar_added",
		    G_CALLBACK (toolbar_added_cb), toolbar);
  g_signal_connect (model, "toolbar_removed",
		    G_CALLBACK (toolbar_removed_cb), toolbar);
  g_signal_connect (model, "toolbar_changed",
		    G_CALLBACK (toolbar_changed_cb), toolbar);
}

static void
egg_editable_toolbar_set_ui_manager (EggEditableToolbar *t,
				     GtkUIManager       *manager)
{
  g_return_if_fail (GTK_IS_UI_MANAGER (manager));

  t->priv->manager = g_object_ref (manager);
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
    case PROP_UI_MANAGER:
      egg_editable_toolbar_set_ui_manager (t, g_value_get_object (value));
      break;
    case PROP_TOOLBARS_MODEL:
      egg_editable_toolbar_set_model (t, g_value_get_object (value));
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
  EggEditableToolbar *t = EGG_EDITABLE_TOOLBAR (object);

  switch (prop_id)
    {
    case PROP_UI_MANAGER:
      g_value_set_object (value, t->priv->manager);
      break;
    case PROP_TOOLBARS_MODEL:
      g_value_set_object (value, t->priv->model);
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

  object_class->finalize = egg_editable_toolbar_finalize;
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
							G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_TOOLBARS_MODEL,
				   g_param_spec_object ("model",
							"Model",
							"Toolbars Model",
							EGG_TYPE_TOOLBARS_MODEL,
							G_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (EggEditableToolbarPrivate));
}

static void
egg_editable_toolbar_init (EggEditableToolbar *t)
{
  t->priv = EGG_EDITABLE_TOOLBAR_GET_PRIVATE (t);
}

static void
egg_editable_toolbar_finalize (GObject *object)
{
  EggEditableToolbar *t = EGG_EDITABLE_TOOLBAR (object);

  if (t->priv->fixed_toolbar)
    {
      g_object_unref (t->priv->fixed_toolbar);
    }

  if (t->priv->manager)
    {
      g_object_unref (t->priv->manager);
    }

  if (t->priv->model)
    {
      egg_editable_toolbar_disconnect_model (t);
      g_object_unref (t->priv->model);
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
egg_editable_toolbar_new (GtkUIManager     *manager)
{
  return GTK_WIDGET (g_object_new (EGG_TYPE_EDITABLE_TOOLBAR,
				   "ui-manager", manager,
				   NULL));
}

GtkWidget *
egg_editable_toolbar_new_with_model (GtkUIManager     *manager,
				     EggToolbarsModel *model)
{
  return GTK_WIDGET (g_object_new (EGG_TYPE_EDITABLE_TOOLBAR,
				   "ui-manager", manager,
				   "model", model,
				   NULL));
}

gboolean
egg_editable_toolbar_get_edit_mode (EggEditableToolbar *etoolbar)
{
	return etoolbar->priv->edit_mode;
}

void
egg_editable_toolbar_set_edit_mode (EggEditableToolbar *etoolbar,
				    gboolean            mode)
{
  int i, l, n_toolbars, n_items;

  etoolbar->priv->edit_mode = mode;

  n_toolbars = get_n_toolbars (etoolbar);
  for (i = 0; i < n_toolbars; i++)
    {
      GtkWidget *toolbar;

      toolbar = get_toolbar_nth (etoolbar, i);
      n_items = gtk_toolbar_get_n_items (GTK_TOOLBAR (toolbar));
      for (l = 0; l < n_items; l++)
        {
	  GtkToolItem *item;
	  const char *action_name, *type;
          gboolean is_separator;
	  GtkAction *action = NULL;

          egg_toolbars_model_item_nth (etoolbar->priv->model, i, l,
		 		       &is_separator, &action_name, &type);
	  action = find_action (etoolbar, action_name);

	  item = gtk_toolbar_get_nth_item (GTK_TOOLBAR (toolbar), l);
	  gtk_tool_item_set_use_drag_window (item, mode);

          if (mode)
	    {
              set_drag_cursor (GTK_WIDGET (item));
	      gtk_widget_set_sensitive (GTK_WIDGET (item), TRUE);
              set_item_drag_source (etoolbar->priv->model, GTK_WIDGET (item),
				    action, is_separator, type);
            }
	  else
            {
              unset_drag_cursor (GTK_WIDGET (item));
              gtk_drag_source_unset (GTK_WIDGET (item));

              if (!is_separator)
                {
	          g_object_notify (G_OBJECT (action), "sensitive");
                }
	    }
        }
    }
}

void
egg_editable_toolbar_show (EggEditableToolbar *etoolbar,
			   const char         *name)
{
  int i, n_toolbars;
  EggToolbarsModel *model = etoolbar->priv->model;

  g_return_if_fail (model != NULL);

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
			   const char         *name)
{
  int i, n_toolbars;
  EggToolbarsModel *model = etoolbar->priv->model;

  g_return_if_fail (model != NULL);

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
egg_editable_toolbar_set_fixed (EggEditableToolbar *toolbar,
				GtkToolItem        *fixed)
{
  g_return_if_fail (EGG_IS_EDITABLE_TOOLBAR (toolbar));
  g_return_if_fail (!fixed || GTK_IS_TOOL_ITEM (fixed));

  if (!toolbar->priv->fixed_toolbar)
    {
      toolbar->priv->fixed_toolbar = gtk_toolbar_new ();
      gtk_toolbar_set_show_arrow (GTK_TOOLBAR (toolbar->priv->fixed_toolbar), FALSE);
      g_object_ref (toolbar->priv->fixed_toolbar);
      gtk_object_sink (GTK_OBJECT (toolbar->priv->fixed_toolbar));
    }

  if (toolbar->priv->fixed)
    {
      gtk_container_remove (GTK_CONTAINER (toolbar->priv->fixed_toolbar),
	    		    GTK_WIDGET (toolbar->priv->fixed));
      g_object_unref (toolbar->priv->fixed);
    }

  toolbar->priv->fixed = fixed;

  if (fixed)
    {
      g_object_ref (fixed);
      gtk_object_sink (GTK_OBJECT (fixed));

      gtk_toolbar_insert (GTK_TOOLBAR (toolbar->priv->fixed_toolbar), fixed, 0);
    }
}

void
egg_editable_toolbar_set_drag_dest (EggEditableToolbar   *etoolbar,
				    const GtkTargetEntry *targets,
				    gint                  n_targets,
				    const char           *toolbar_name)
{
  int i, n_toolbars;
  EggToolbarsModel *model = etoolbar->priv->model;

  g_return_if_fail (model != NULL);

  n_toolbars = egg_toolbars_model_n_toolbars (model);
  for (i = 0; i < n_toolbars; i++)
    {
      const char *name;

      name = egg_toolbars_model_toolbar_nth (model, i);
      if (strcmp (toolbar_name, name) == 0)
      {
        GtkWidget *widget = get_toolbar_nth (etoolbar, i);

        gtk_drag_dest_unset (widget);
        gtk_drag_dest_set (widget, 0,
                           targets, n_targets,
                           GDK_ACTION_MOVE | GDK_ACTION_COPY);
      }
    }
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
  pixmap = gdk_pixmap_new (NULL, icon_width, icon_height, gdk_visual_get_best_depth());
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
