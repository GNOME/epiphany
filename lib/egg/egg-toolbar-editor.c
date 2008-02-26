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
 *
 *  $Id$
 */

#include "config.h"

#include "egg-toolbar-editor.h"
#include "egg-editable-toolbar.h"

#include <string.h>
#include <libxml/tree.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktable.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

static const GtkTargetEntry dest_drag_types[] = {
  {EGG_TOOLBAR_ITEM_TYPE, GTK_TARGET_SAME_APP, 0},
};

static const GtkTargetEntry source_drag_types[] = {
  {EGG_TOOLBAR_ITEM_TYPE, GTK_TARGET_SAME_APP, 0},
};


static void egg_toolbar_editor_finalize         (GObject *object);
static void update_editor_sheet                 (EggToolbarEditor *editor);

enum
{
  PROP_0,
  PROP_UI_MANAGER,
  PROP_TOOLBARS_MODEL
};

enum
{
  SIGNAL_HANDLER_ITEM_ADDED,
  SIGNAL_HANDLER_ITEM_REMOVED,
  SIGNAL_HANDLER_TOOLBAR_REMOVED,
  SIGNAL_HANDLER_LIST_SIZE  /* Array size */
};

#define EGG_TOOLBAR_EDITOR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EGG_TYPE_TOOLBAR_EDITOR, EggToolbarEditorPrivate))

struct EggToolbarEditorPrivate
{
  GtkUIManager *manager;
  EggToolbarsModel *model;

  GtkWidget *table;
  GtkWidget *scrolled_window;
  GList     *actions_list;
  GList     *factory_list;

  /* These handlers need to be sanely disconnected when switching models */
  gulong     sig_handlers[SIGNAL_HANDLER_LIST_SIZE];
};

G_DEFINE_TYPE (EggToolbarEditor, egg_toolbar_editor, GTK_TYPE_VBOX);

static gint
compare_items (gconstpointer a,
               gconstpointer b)
{
  const GtkWidget *item1 = a;
  const GtkWidget *item2 = b;

  char *key1 = g_object_get_data (G_OBJECT (item1),
                                  "egg-collate-key");
  char *key2 = g_object_get_data (G_OBJECT (item2),
                                  "egg-collate-key");
  
  return strcmp (key1, key2);
}

static GtkAction *
find_action (EggToolbarEditor *t,
	     const char       *name)
{
  GList *l;
  GtkAction *action = NULL;

  l = gtk_ui_manager_get_action_groups (t->priv->manager);

  g_return_val_if_fail (EGG_IS_TOOLBAR_EDITOR (t), NULL);
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
egg_toolbar_editor_set_ui_manager (EggToolbarEditor *t,
				   GtkUIManager     *manager)
{
  g_return_if_fail (GTK_IS_UI_MANAGER (manager));

  t->priv->manager = g_object_ref (manager);
}

static void
item_added_or_removed_cb (EggToolbarsModel   *model,
                          int                 tpos,
                          int                 ipos,
                          EggToolbarEditor   *editor)
{
  update_editor_sheet (editor);
}

static void
toolbar_removed_cb (EggToolbarsModel   *model,
	            int                 position,
	            EggToolbarEditor   *editor)
{
  update_editor_sheet (editor);
}

static void
egg_toolbar_editor_disconnect_model (EggToolbarEditor *t)
{
  EggToolbarEditorPrivate *priv = t->priv;
  EggToolbarsModel *model = priv->model;
  gulong handler;
  int i;

  for (i = 0; i < SIGNAL_HANDLER_LIST_SIZE; i++)
    {
      handler = priv->sig_handlers[i];

      if (handler != 0)
        {
	  if (g_signal_handler_is_connected (model, handler))
	    {
	      g_signal_handler_disconnect (model, handler);
	    }

	  priv->sig_handlers[i] = 0;
        }
    }
}

void
egg_toolbar_editor_set_model (EggToolbarEditor *t,
			      EggToolbarsModel *model)
{
  EggToolbarEditorPrivate *priv;

  g_return_if_fail (EGG_IS_TOOLBAR_EDITOR (t));
  g_return_if_fail (model != NULL);

  priv = t->priv;

  if (priv->model)
    {
      if (G_UNLIKELY (priv->model == model)) return;

      egg_toolbar_editor_disconnect_model (t);
      g_object_unref (priv->model); 
    }

  priv->model = g_object_ref (model);

  update_editor_sheet (t);
  
  priv->sig_handlers[SIGNAL_HANDLER_ITEM_ADDED] = 
    g_signal_connect_object (model, "item_added",
			     G_CALLBACK (item_added_or_removed_cb), t, 0);
  priv->sig_handlers[SIGNAL_HANDLER_ITEM_REMOVED] =
    g_signal_connect_object (model, "item_removed",
			     G_CALLBACK (item_added_or_removed_cb), t, 0);
  priv->sig_handlers[SIGNAL_HANDLER_TOOLBAR_REMOVED] =
    g_signal_connect_object (model, "toolbar_removed",
			     G_CALLBACK (toolbar_removed_cb), t, 0);
}

static void
egg_toolbar_editor_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
  EggToolbarEditor *t = EGG_TOOLBAR_EDITOR (object);

  switch (prop_id)
    {
    case PROP_UI_MANAGER:
      egg_toolbar_editor_set_ui_manager (t, g_value_get_object (value));
      break;
    case PROP_TOOLBARS_MODEL:
      egg_toolbar_editor_set_model (t, g_value_get_object (value));
      break;
    }
}

static void
egg_toolbar_editor_get_property (GObject    *object,
				 guint       prop_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
  EggToolbarEditor *t = EGG_TOOLBAR_EDITOR (object);

  switch (prop_id)
    {
    case PROP_UI_MANAGER:
      g_value_set_object (value, t->priv->manager);
      break;
    case PROP_TOOLBARS_MODEL:
      g_value_set_object (value, t->priv->model);
      break;
    }
}

static void
egg_toolbar_editor_class_init (EggToolbarEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = egg_toolbar_editor_finalize;
  object_class->set_property = egg_toolbar_editor_set_property;
  object_class->get_property = egg_toolbar_editor_get_property;

  g_object_class_install_property (object_class,
				   PROP_UI_MANAGER,
				   g_param_spec_object ("ui-manager",
							"UI-Manager",
							"UI Manager",
							GTK_TYPE_UI_MANAGER,
							G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
							G_PARAM_CONSTRUCT_ONLY));
 g_object_class_install_property (object_class,
				  PROP_TOOLBARS_MODEL,
				  g_param_spec_object ("model",
						       "Model",
						       "Toolbars Model",
						       EGG_TYPE_TOOLBARS_MODEL,
						       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB |
						       G_PARAM_CONSTRUCT));

  g_type_class_add_private (object_class, sizeof (EggToolbarEditorPrivate));
}

static void
egg_toolbar_editor_finalize (GObject *object)
{
  EggToolbarEditor *editor = EGG_TOOLBAR_EDITOR (object);

  if (editor->priv->manager)
    {
      g_object_unref (editor->priv->manager);
    }

  if (editor->priv->model)
    {
      egg_toolbar_editor_disconnect_model (editor);
      g_object_unref (editor->priv->model);
    }

  g_list_free (editor->priv->actions_list);
  g_list_free (editor->priv->factory_list);

  G_OBJECT_CLASS (egg_toolbar_editor_parent_class)->finalize (object);
}

GtkWidget *
egg_toolbar_editor_new (GtkUIManager *manager,
			EggToolbarsModel *model)
{
  return GTK_WIDGET (g_object_new (EGG_TYPE_TOOLBAR_EDITOR,
				   "ui-manager", manager,
				   "model", model,
				   NULL));
}

static void
drag_begin_cb (GtkWidget          *widget,
	       GdkDragContext     *context)
{
  gtk_widget_hide (widget);
}

static void
drag_end_cb (GtkWidget          *widget,
	     GdkDragContext     *context)
{
  gtk_widget_show (widget);
}

static void
drag_data_get_cb (GtkWidget          *widget,
		  GdkDragContext     *context,
		  GtkSelectionData   *selection_data,
		  guint               info,
		  guint32             time,
		  EggToolbarEditor   *editor)
{
  const char *target;

  target = g_object_get_data (G_OBJECT (widget), "egg-item-name");
  g_return_if_fail (target != NULL);
  
  gtk_selection_data_set (selection_data, selection_data->target, 8,
			  (const guchar *) target, strlen (target));
}

static gchar *
elide_underscores (const gchar *original)
{
  gchar *q, *result;
  const gchar *p;
  gboolean last_underscore;

  q = result = g_malloc (strlen (original) + 1);
  last_underscore = FALSE;

  for (p = original; *p; p++)
    {
      if (!last_underscore && *p == '_')
	last_underscore = TRUE;
      else
	{
	  last_underscore = FALSE;
	  *q++ = *p;
	}
    }

  *q = '\0';

  return result;
}

static void
set_drag_cursor (GtkWidget *widget)
{
  GdkCursor *cursor;
  GdkScreen *screen;
  
  screen = gtk_widget_get_screen (widget);
  
  cursor = gdk_cursor_new_for_display (gdk_screen_get_display (screen),
				       GDK_HAND2);
  gdk_window_set_cursor (widget->window, cursor);
  gdk_cursor_unref (cursor);
}

static void
event_box_realize_cb (GtkWidget *widget, GtkImage *icon)
{
  GtkImageType type;

  set_drag_cursor (widget);

  type = gtk_image_get_storage_type (icon);
  if (type == GTK_IMAGE_STOCK)
    {
      gchar *stock_id;
      GdkPixbuf *pixbuf;

      gtk_image_get_stock (icon, &stock_id, NULL);
      pixbuf = gtk_widget_render_icon (widget, stock_id,
	                               GTK_ICON_SIZE_LARGE_TOOLBAR, NULL);
      gtk_drag_source_set_icon_pixbuf (widget, pixbuf);
      g_object_unref (pixbuf);
    }
  else if (type == GTK_IMAGE_ICON_NAME)
    {
      const gchar *icon_name;
      GdkScreen *screen;
      GtkIconTheme *icon_theme;
      GtkSettings *settings;
      gint width, height;
      GdkPixbuf *pixbuf;

      gtk_image_get_icon_name (icon, &icon_name, NULL);
      screen = gtk_widget_get_screen (widget);
      icon_theme = gtk_icon_theme_get_for_screen (screen);
      settings = gtk_settings_get_for_screen (screen);

      if (!gtk_icon_size_lookup_for_settings (settings,
                                              GTK_ICON_SIZE_LARGE_TOOLBAR,
					      &width, &height))
        {
	  width = height = 24;
	}

      pixbuf = gtk_icon_theme_load_icon (icon_theme, icon_name,
                                         MIN (width, height), 0, NULL);
      if (G_UNLIKELY (!pixbuf))
        return;

      gtk_drag_source_set_icon_pixbuf (widget, pixbuf);
      g_object_unref (pixbuf);

    }
  else if (type == GTK_IMAGE_PIXBUF)
    {
      GdkPixbuf *pixbuf = gtk_image_get_pixbuf (icon);
      gtk_drag_source_set_icon_pixbuf (widget, pixbuf);
    }
}

static GtkWidget *
editor_create_item (EggToolbarEditor *editor,
		    GtkImage	     *icon,
		    const char       *label_text,
		    GdkDragAction     action)
{
  GtkWidget *event_box;
  GtkWidget *vbox;
  GtkWidget *label;
  gchar *label_no_mnemonic = NULL;

  event_box = gtk_event_box_new ();
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box), FALSE);
  gtk_widget_show (event_box);
  gtk_drag_source_set (event_box,
		       GDK_BUTTON1_MASK,
		       source_drag_types, G_N_ELEMENTS (source_drag_types), action);
  g_signal_connect (event_box, "drag_data_get",
		    G_CALLBACK (drag_data_get_cb), editor);
  g_signal_connect_after (event_box, "realize",
		          G_CALLBACK (event_box_realize_cb), icon);

  if (action == GDK_ACTION_MOVE)
    {
      g_signal_connect (event_box, "drag_begin",
		        G_CALLBACK (drag_begin_cb), NULL);
      g_signal_connect (event_box, "drag_end",
		        G_CALLBACK (drag_end_cb), NULL);
    }

  vbox = gtk_vbox_new (0, FALSE);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (event_box), vbox);

  gtk_widget_show (GTK_WIDGET (icon));
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (icon), FALSE, TRUE, 0);
  label_no_mnemonic = elide_underscores (label_text);
  label = gtk_label_new (label_no_mnemonic);
  g_free (label_no_mnemonic);
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

  return event_box;
}

static GtkWidget *
editor_create_item_from_name (EggToolbarEditor *editor,
                              const char *      name,
                              GdkDragAction     drag_action)
{
  GtkWidget *item;
  const char *item_name;
  char *short_label;
  const char *collate_key;
  
  if (strcmp (name, "_separator") == 0)
    {
      GtkWidget *icon;
      
      icon = _egg_editable_toolbar_new_separator_image ();
      short_label = _("Separator");
      item_name = g_strdup (name);
      collate_key = g_utf8_collate_key (short_label, -1);
      item = editor_create_item (editor, GTK_IMAGE (icon), 
                                 short_label, drag_action);
    }
  else
    {
      GtkAction *action;
      GtkWidget *icon;
      char *stock_id, *icon_name = NULL;
      
      action = find_action (editor, name);
      g_return_val_if_fail (action != NULL, NULL);

      g_object_get (action,
                    "icon-name", &icon_name,
                    "stock-id", &stock_id,
		    "short-label", &short_label,
		    NULL);

      /* This is a workaround to catch named icons. */
      if (icon_name)
        icon = gtk_image_new_from_icon_name (icon_name,
	                                     GTK_ICON_SIZE_LARGE_TOOLBAR);
      else
        icon = gtk_image_new_from_stock (stock_id ? stock_id : GTK_STOCK_DND,
                                         GTK_ICON_SIZE_LARGE_TOOLBAR);

      item_name = g_strdup (name);
      collate_key = g_utf8_collate_key (short_label, -1);
      item = editor_create_item (editor, GTK_IMAGE (icon),
                                 short_label, drag_action);

      g_free (short_label);
      g_free (stock_id);
      g_free (icon_name);
    }
  
  g_object_set_data_full (G_OBJECT (item), "egg-collate-key",
                          (gpointer) collate_key, g_free);
  g_object_set_data_full (G_OBJECT (item), "egg-item-name",
                          (gpointer) item_name, g_free);
  
  return item;
}

static gint
append_table (GtkTable *table, GList *items, gint y, gint width)
{
  if (items != NULL)
    {
      gint x = 0, height;
      GtkWidget *alignment;
      GtkWidget *item;
  
      height = g_list_length (items) / width + 1;
      gtk_table_resize (table, height, width);
      
      if (y > 0)
        {
          item = gtk_hseparator_new ();
          alignment = gtk_alignment_new (0.5, 0.5, 1.0, 0.0);
          gtk_container_add (GTK_CONTAINER (alignment), item);
          gtk_widget_show (alignment);
          gtk_widget_show (item);
          
          gtk_table_attach_defaults (table, alignment, 0, width, y-1, y+1);
        }
      
      for (; items != NULL; items = items->next)
        {
          item = items->data;
          alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
          gtk_container_add (GTK_CONTAINER (alignment), item);
          gtk_widget_show (alignment);
          gtk_widget_show (item);
          
          if (x >= width)
            {
              x = 0;
              y++;
            }
          gtk_table_attach_defaults (table, alignment, x, x+1, y, y+1);
          x++;
        }
      
      y++;
    }
  return y;
}

static void
update_editor_sheet (EggToolbarEditor *editor)
{
  gint y;
  GPtrArray *items;
  GList *to_move = NULL, *to_copy = NULL;
  GtkWidget *table;
  GtkWidget *viewport;

  g_return_if_fail (EGG_IS_TOOLBAR_EDITOR (editor));
  
  /* Create new table. */
  table = gtk_table_new (0, 0, TRUE);
  editor->priv->table = table;
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_table_set_row_spacings (GTK_TABLE (table), 24);
  gtk_widget_show (table);
  gtk_drag_dest_set (table, GTK_DEST_DEFAULT_ALL,
		     dest_drag_types, G_N_ELEMENTS (dest_drag_types),
                     GDK_ACTION_MOVE | GDK_ACTION_COPY);
  
  /* Build two lists of items (one for copying, one for moving). */
  items = egg_toolbars_model_get_name_avail (editor->priv->model);
  while (items->len > 0)
    {
      GtkWidget *item;
      const char *name;
      gint flags;
      
      name = g_ptr_array_index (items, 0);
      g_ptr_array_remove_index_fast (items, 0);
      
      flags = egg_toolbars_model_get_name_flags (editor->priv->model, name);
      if ((flags & EGG_TB_MODEL_NAME_INFINITE) == 0)
        {
          item = editor_create_item_from_name (editor, name, GDK_ACTION_MOVE);
          if (item != NULL)
            to_move = g_list_insert_sorted (to_move, item, compare_items);
        }
      else
        {
          item = editor_create_item_from_name (editor, name, GDK_ACTION_COPY);
          if (item != NULL)
            to_copy = g_list_insert_sorted (to_copy, item, compare_items);
        }
    }

  /* Add them to the sheet. */
  y = 0;
  y = append_table (GTK_TABLE (table), to_move, y, 4);
  y = append_table (GTK_TABLE (table), to_copy, y, 4);
  
  g_list_free (to_move);
  g_list_free (to_copy);
  g_ptr_array_free (items, TRUE);
  
  /* Delete old table. */
  viewport = GTK_BIN (editor->priv->scrolled_window)->child;
  if (viewport)
    {
      gtk_container_remove (GTK_CONTAINER (viewport),
                            GTK_BIN (viewport)->child);
    }
  
  /* Add table to window. */
  gtk_scrolled_window_add_with_viewport
    (GTK_SCROLLED_WINDOW (editor->priv->scrolled_window), table);

}

static void
setup_editor (EggToolbarEditor *editor)
{
  GtkWidget *scrolled_window;

  gtk_container_set_border_width (GTK_CONTAINER (editor), 12);
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  editor->priv->scrolled_window = scrolled_window;
  gtk_widget_show (scrolled_window);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (editor), scrolled_window, TRUE, TRUE, 0);
}

static void
egg_toolbar_editor_init (EggToolbarEditor *t)
{
  t->priv = EGG_TOOLBAR_EDITOR_GET_PRIVATE (t);

  t->priv->manager = NULL;
  t->priv->actions_list = NULL;

  setup_editor (t);
}

