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

#include "egg-toolbar-editor.h"
#include "egg-editable-toolbar.h"
#include "eggintl.h"

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

static GtkTargetEntry dest_drag_types[] = {
  {EGG_TOOLBAR_ITEM_TYPE, GTK_TARGET_SAME_APP, 0},
};
static int n_dest_drag_types = G_N_ELEMENTS (dest_drag_types);

static GtkTargetEntry source_drag_types[] = {
  {EGG_TOOLBAR_ITEM_TYPE, GTK_TARGET_SAME_APP, 0},
};
static int n_source_drag_types = G_N_ELEMENTS (source_drag_types);

static void egg_toolbar_editor_class_init	(EggToolbarEditorClass *klass);
static void egg_toolbar_editor_init		(EggToolbarEditor *t);
static void egg_toolbar_editor_finalize         (GObject *object);
static void update_actions_list			(EggToolbarEditor *editor);
static void update_editor_sheet                 (EggToolbarEditor *editor);

enum
{
  PROP_0,
  PROP_MENU_MERGE,
  PROP_TOOLBARS_MODEL
};

static GObjectClass *parent_class = NULL;

#define EGG_TOOLBAR_EDITOR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EGG_TYPE_TOOLBAR_EDITOR, EggToolbarEditorPrivate))

struct EggToolbarEditorPrivate
{
  GtkUIManager *merge;
  EggToolbarsModel *model;

  GtkWidget *table;
  GtkWidget *scrolled_window;

  GList *default_actions_list;
  GList *actions_list;
};

GType
egg_toolbar_editor_get_type (void)
{
  static GType egg_toolbar_editor_type = 0;

  if (egg_toolbar_editor_type == 0)
    {
      static const GTypeInfo our_info = {
	sizeof (EggToolbarEditorClass),
	NULL,			/* base_init */
	NULL,			/* base_finalize */
	(GClassInitFunc) egg_toolbar_editor_class_init,
	NULL,
	NULL,			/* class_data */
	sizeof (EggToolbarEditor),
	0,			/* n_preallocs */
	(GInstanceInitFunc) egg_toolbar_editor_init
      };

      egg_toolbar_editor_type = g_type_register_static (GTK_TYPE_VBOX,
							"EggToolbarEditor",
							&our_info, 0);
    }

  return egg_toolbar_editor_type;
}

static gint
compare_actions (gconstpointer a, gconstpointer b)
{
  g_return_val_if_fail (GTK_IS_ACTION (a), 0);
  g_return_val_if_fail (GTK_IS_ACTION (b), 0);

  GValue value_a = { 0, }, value_b = { 0, };
  const char *short_label_a, *short_label_b;
  int ret;

  g_value_init (&value_a, G_TYPE_STRING);
  g_object_get_property (G_OBJECT (a), "short_label", &value_a);
  short_label_a = g_value_get_string (&value_a);

  g_value_init (&value_b, G_TYPE_STRING);
  g_object_get_property (G_OBJECT (b), "short_label", &value_b);
  short_label_b = g_value_get_string (&value_b);

  ret = g_utf8_collate (short_label_a, short_label_b);

  g_value_unset (&value_a);
  g_value_unset (&value_b);

  return ret;
}

static GtkAction *
find_action (EggToolbarEditor *t,
	     const char       *name)
{
  GList *l;
  GtkAction *action = NULL;

  l = gtk_ui_manager_get_action_groups (t->priv->merge);

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
egg_toolbar_editor_set_merge (EggToolbarEditor *t,
			      GtkUIManager     *merge)
{
  g_return_if_fail (GTK_IS_UI_MANAGER (merge));
  g_return_if_fail (EGG_IS_TOOLBAR_EDITOR (t));

  t->priv->merge = g_object_ref (merge);
}

static void
toolbar_removed_cb (EggToolbarsModel   *model,
	            int                 position,
	            EggToolbarEditor   *editor)
{
  update_actions_list (editor);
  update_editor_sheet (editor);
}

static void
egg_toolbar_editor_set_model (EggToolbarEditor *t,
			      EggToolbarsModel *model)
{
  g_return_if_fail (EGG_IS_TOOLBAR_EDITOR (t));

  t->priv->model = g_object_ref (model);

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
    case PROP_MENU_MERGE:
      egg_toolbar_editor_set_merge (t, g_value_get_object (value));
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
    case PROP_MENU_MERGE:
      g_value_set_object (value, t->priv->merge);
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

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = egg_toolbar_editor_finalize;
  object_class->set_property = egg_toolbar_editor_set_property;
  object_class->get_property = egg_toolbar_editor_get_property;

  g_object_class_install_property (object_class,
				   PROP_MENU_MERGE,
				   g_param_spec_object ("MenuMerge",
							"MenuMerge",
							"Menu merge",
							GTK_TYPE_UI_MANAGER,
							G_PARAM_READWRITE |
							G_PARAM_CONSTRUCT_ONLY));
 g_object_class_install_property (object_class,
				  PROP_TOOLBARS_MODEL,
				  g_param_spec_object ("ToolbarsModel",
						       "ToolbarsModel",
						       "Toolbars Model",
						       EGG_TYPE_TOOLBARS_MODEL,
						       G_PARAM_READWRITE |
						       G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (object_class, sizeof (EggToolbarEditorPrivate));
}

static void
egg_toolbar_editor_finalize (GObject *object)
{
  EggToolbarEditor *editor = EGG_TOOLBAR_EDITOR (object);

  if (editor->priv->merge)
    {
      g_object_unref (editor->priv->merge);
    }

  if (editor->priv->model)
    {
      g_object_unref (editor->priv->model);
    }

  g_list_free (editor->priv->default_actions_list);
  g_list_free (editor->priv->actions_list);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
egg_toolbar_editor_new (GtkUIManager *merge,
			EggToolbarsModel *model)
{
  return GTK_WIDGET (g_object_new (EGG_TYPE_TOOLBAR_EDITOR,
				   "MenuMerge", merge,
				   "ToolbarsModel", model,
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
editor_drag_data_received_cb (GtkWidget          *widget,
			      GdkDragContext     *context,
			      gint                x,
			      gint                y,
			      GtkSelectionData   *selection_data,
			      guint               info,
			      guint               time_,
			      EggToolbarEditor *editor)
{
  GtkAction *action;
  const char *data;

  g_return_if_fail (EGG_IS_TOOLBAR_EDITOR (editor));
  g_return_if_fail (selection_data != NULL);

  if (selection_data->length <= 0 || selection_data->data == NULL) return;

  data = (const char *) selection_data->data;

  if (strcmp (data, "separator") == 0) return;

  action = find_action (editor, data);
  g_return_if_fail (action != NULL);

  if (g_list_find (editor->priv->default_actions_list, action))
    {
      editor->priv->actions_list = g_list_insert_sorted
	    (editor->priv->actions_list, action, compare_actions);
    }

  update_editor_sheet (editor);
}

static void
editor_drag_data_delete_cb (GtkWidget          *widget,
			    GdkDragContext     *context,
			    EggToolbarEditor *editor)
{
  GtkAction *action;
  g_return_if_fail (EGG_IS_TOOLBAR_EDITOR (editor));

  action = GTK_ACTION (g_object_get_data (G_OBJECT (widget), "egg-action"));
  if (action)
    {
      editor->priv->actions_list = g_list_remove
	    (editor->priv->actions_list, action);
    }

  update_editor_sheet (editor);
}

static void
drag_data_get_cb (GtkWidget          *widget,
		  GdkDragContext     *context,
		  GtkSelectionData   *selection_data,
		  guint               info,
		  guint32             time,
		  EggToolbarEditor   *editor)
{
  GtkAction *action;
  const char *target;

  action = GTK_ACTION (g_object_get_data (G_OBJECT (widget), "egg-action"));

  if (action)
    {
      target = gtk_action_get_name (action);
    }
  else
    {
      target = "separator";
    }

  gtk_selection_data_set (selection_data,
			  selection_data->target, 8, target, strlen (target));
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
  GdkPixbuf *pixbuf;

  pixbuf = gdk_pixbuf_new_from_file (CURSOR_DIR "/hand-open.png", NULL);
  cursor = gdk_cursor_new_from_pixbuf (gdk_display_get_default (), pixbuf, 12, 12);

  gdk_window_set_cursor (widget->window, cursor);
  gdk_cursor_unref (cursor);
  g_object_unref (pixbuf);
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
		       source_drag_types, n_source_drag_types, action);
  g_signal_connect (event_box, "drag_data_get",
		    G_CALLBACK (drag_data_get_cb), editor);
  g_signal_connect (event_box, "drag_data_delete",
		    G_CALLBACK (editor_drag_data_delete_cb), editor);
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

static void
update_editor_sheet (EggToolbarEditor *editor)
{
  GList *l;
  GList *to_drag;
  int x, y, height, width;
  GtkWidget *table;
  GtkWidget *viewport;
  GtkWidget *item;
  GtkWidget *icon;

  g_return_if_fail (EGG_IS_TOOLBAR_EDITOR (editor));

  viewport = GTK_BIN (editor->priv->scrolled_window)->child;
  if (viewport)
    {
      table = GTK_BIN (viewport)->child;
      gtk_container_remove (GTK_CONTAINER (viewport), table);
    }
  table = gtk_table_new (0, 0, TRUE);
  editor->priv->table = table;
  gtk_container_set_border_width (GTK_CONTAINER (table), 12);
  gtk_widget_show (table);
  gtk_scrolled_window_add_with_viewport
    (GTK_SCROLLED_WINDOW (editor->priv->scrolled_window), table);
  gtk_drag_dest_set (table, GTK_DEST_DEFAULT_ALL,
		     dest_drag_types, n_dest_drag_types, GDK_ACTION_MOVE);
  g_signal_connect (table, "drag_data_received",
		    G_CALLBACK (editor_drag_data_received_cb), editor);

  to_drag = editor->priv->actions_list;

  x = y = 0;
  width = 4;
  height = (g_list_length (to_drag)) / width + 1;
  gtk_table_resize (GTK_TABLE (editor->priv->table), height, width);

  for (l = to_drag; l != NULL; l = l->next)
    {
      GtkAction *action = (l->data);
      const char *stock_id, *short_label;
      GValue value = { 0, };

      g_value_init (&value, G_TYPE_STRING);
      g_object_get_property (G_OBJECT (action), "stock_id", &value);
      stock_id = g_value_get_string (&value);
      icon = gtk_image_new_from_stock
		(stock_id ? stock_id : GTK_STOCK_DND,
		 GTK_ICON_SIZE_LARGE_TOOLBAR);
      g_value_unset (&value);

      g_value_init (&value, G_TYPE_STRING);
      g_object_get_property (G_OBJECT (action), "short_label", &value);
      short_label = g_value_get_string (&value);
      item = editor_create_item (editor, GTK_IMAGE (icon),
				 short_label, GDK_ACTION_MOVE);
      g_value_unset (&value);
      g_object_set_data (G_OBJECT (item), "egg-action", action);
      gtk_table_attach_defaults (GTK_TABLE (editor->priv->table),
		                 item, x, x + 1, y, y + 1);

      x++;
      if (x >= width)
	{
	  x = 0;
	  y++;
	}
    }

  icon = _egg_editable_toolbar_new_separator_image ();
  item = editor_create_item (editor, GTK_IMAGE (icon), _("Separator"),
			     GDK_ACTION_COPY);
  gtk_table_attach_defaults (GTK_TABLE (editor->priv->table),
		             item, x, x + 1, y, y + 1);
}

static void
setup_editor (EggToolbarEditor *editor)
{
  GtkWidget *scrolled_window;
  GtkWidget *label_hbox;
  GtkWidget *image;
  GtkWidget *label;

  g_return_if_fail (EGG_IS_TOOLBAR_EDITOR (editor));

  gtk_container_set_border_width (GTK_CONTAINER (editor), 12);
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  editor->priv->scrolled_window = scrolled_window;
  gtk_widget_show (scrolled_window);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (editor), scrolled_window, TRUE, TRUE, 0);
  label_hbox = gtk_hbox_new (FALSE, 6);
  gtk_widget_show (label_hbox);
  gtk_box_pack_start (GTK_BOX (editor), label_hbox, FALSE, FALSE, 0);
  image =
    gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
  gtk_widget_show (image);
  gtk_box_pack_start (GTK_BOX (label_hbox), image, FALSE, FALSE, 0);
  label = gtk_label_new (_("Drag an item onto the toolbars above to add it, "
			   "from the toolbars in the items table to remove it."));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (label_hbox), label, FALSE, TRUE, 0);
}

static void
egg_toolbar_editor_init (EggToolbarEditor *t)
{
  t->priv = EGG_TOOLBAR_EDITOR_GET_PRIVATE (t);

  t->priv->merge = NULL;
  t->priv->default_actions_list = NULL;
  t->priv->actions_list = NULL;

  setup_editor (t);
}

static void
egg_toolbar_editor_add_action (EggToolbarEditor *editor,
			       const char       *action_name)
{
	GtkAction *action;

	action = find_action (editor, action_name);
	g_return_if_fail (action != NULL);

	editor->priv->default_actions_list = g_list_insert_sorted
		(editor->priv->default_actions_list, action, compare_actions);
}

static void
parse_item_list (EggToolbarEditor *t,
		 xmlNodePtr        child)
{
  while (child)
    {
      if (xmlStrEqual (child->name, "toolitem"))
	{
	  xmlChar *name;

	  name = xmlGetProp (child, "name");
	  egg_toolbar_editor_add_action (t, name);
	  xmlFree (name);
	}
      child = child->next;
    }
}

static gboolean
model_has_action (EggToolbarsModel *model, GtkAction *action)
{
  int i, l, n_items, n_toolbars;

  n_toolbars = egg_toolbars_model_n_toolbars (model);
  for (i = 0; i < n_toolbars; i++)
    {
      n_items = egg_toolbars_model_n_items (model, i);
      for (l = 0; l < n_items; l++)
        {
          const char *name;
	  const char *action_name;
          gboolean sep;

          egg_toolbars_model_item_nth (model, i, l, &sep, &name, NULL);
          action_name = gtk_action_get_name (action);
          if (!sep && strcmp (name, action_name) == 0) return TRUE;
        }
    }

  return FALSE;
}

static void
update_actions_list (EggToolbarEditor *editor)
{
  GList *l;

  if (editor->priv->actions_list)
    g_list_free (editor->priv->actions_list);

  /* Remove the already used items */
  editor->priv->actions_list = NULL;

  for (l = editor->priv->default_actions_list; l != NULL; l = l->next)
    {
      GtkAction *action = GTK_ACTION (l->data);

      if (!model_has_action (editor->priv->model, action))
        {
          editor->priv->actions_list = g_list_insert_sorted
		(editor->priv->actions_list, action, compare_actions);
        }
    }
}

void
egg_toolbar_editor_load_actions (EggToolbarEditor *editor,
				 const char       *xml_file)
{
  xmlDocPtr doc;
  xmlNodePtr root;
  xmlNodePtr child;

  doc = xmlParseFile (xml_file);
  root = xmlDocGetRootElement (doc);
  child = root->children;

  while (child)
    {
      if (xmlStrEqual (child->name, "available"))
	{
	  parse_item_list (editor, child->children);
	}
      child = child->next;
    }

  xmlFreeDoc (doc);

  update_actions_list (editor);
  update_editor_sheet (editor);
}
