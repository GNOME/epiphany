/*
 * Copyright (c) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public 
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License 
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "gd-main-view.h"

#include "gd-main-view-generic.h"
#include "gd-main-icon-view.h"
#include "gd-main-list-view.h"

#define MAIN_VIEW_TYPE_INITIAL -1
#define MAIN_VIEW_DND_ICON_OFFSET 20

struct _GdMainViewPrivate {
  GdMainViewType current_type;
  gboolean selection_mode;

  GtkWidget *current_view;
  GtkTreeModel *model;

  gchar *button_press_item_path;
};

enum {
  PROP_VIEW_TYPE = 1,
  PROP_SELECTION_MODE,
  PROP_MODEL,
  NUM_PROPERTIES
};

enum {
  ITEM_ACTIVATED = 1,
  SELECTION_MODE_REQUEST,
  VIEW_SELECTION_CHANGED,
  NUM_SIGNALS
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };
static guint signals[NUM_SIGNALS] = { 0, };

G_DEFINE_TYPE (GdMainView, gd_main_view, GTK_TYPE_SCROLLED_WINDOW)

static void
gd_main_view_dispose (GObject *obj)
{
  GdMainView *self = GD_MAIN_VIEW (obj);

  g_clear_object (&self->priv->model);

  G_OBJECT_CLASS (gd_main_view_parent_class)->dispose (obj);
}

static void
gd_main_view_finalize (GObject *obj)
{
  GdMainView *self = GD_MAIN_VIEW (obj);

  g_free (self->priv->button_press_item_path);

  G_OBJECT_CLASS (gd_main_view_parent_class)->finalize (obj);
}

static void
gd_main_view_init (GdMainView *self)
{
  GtkStyleContext *context;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GD_TYPE_MAIN_VIEW, GdMainViewPrivate);

  /* so that we get constructed with the right view even at startup */
  self->priv->current_type = MAIN_VIEW_TYPE_INITIAL;

  gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (self), GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, "documents-scrolledwin");
}

static void
gd_main_view_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GdMainView *self = GD_MAIN_VIEW (object);

  switch (property_id)
    {
    case PROP_VIEW_TYPE:
      g_value_set_int (value, gd_main_view_get_view_type (self));
      break;
    case PROP_SELECTION_MODE:
      g_value_set_boolean (value, gd_main_view_get_selection_mode (self));
      break;
    case PROP_MODEL:
      g_value_set_object (value, gd_main_view_get_model (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gd_main_view_set_property (GObject    *object,
                           guint       property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
  GdMainView *self = GD_MAIN_VIEW (object);

  switch (property_id)
    {
    case PROP_VIEW_TYPE:
      gd_main_view_set_view_type (self, g_value_get_int (value));
      break;
    case PROP_SELECTION_MODE:
      gd_main_view_set_selection_mode (self, g_value_get_boolean (value));
      break;
    case PROP_MODEL:
      gd_main_view_set_model (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gd_main_view_class_init (GdMainViewClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->get_property = gd_main_view_get_property;
  oclass->set_property = gd_main_view_set_property;
  oclass->dispose = gd_main_view_dispose;
  oclass->finalize = gd_main_view_finalize;

  properties[PROP_VIEW_TYPE] =
    g_param_spec_int ("view-type",
                      "View type",
                      "View type",
                      GD_MAIN_VIEW_ICON,
                      GD_MAIN_VIEW_LIST,
                      GD_MAIN_VIEW_ICON,
                      G_PARAM_READWRITE |
                      G_PARAM_STATIC_STRINGS);

  properties[PROP_SELECTION_MODE] =
    g_param_spec_boolean ("selection-mode",
                          "Selection mode",
                          "Whether the view is in selection mode",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_STATIC_STRINGS);

  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                         "Model",
                         "The GtkTreeModel",
                         GTK_TYPE_TREE_MODEL,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_STATIC_STRINGS);

  signals[ITEM_ACTIVATED] =
    g_signal_new ("item-activated",
                  GD_TYPE_MAIN_VIEW,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_STRING, 
                  GTK_TYPE_TREE_PATH);

  signals[SELECTION_MODE_REQUEST] =
    g_signal_new ("selection-mode-request",
                  GD_TYPE_MAIN_VIEW,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[VIEW_SELECTION_CHANGED] = 
    g_signal_new ("view-selection-changed",
                  GD_TYPE_MAIN_VIEW,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  g_type_class_add_private (klass, sizeof (GdMainViewPrivate));
  g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

static GdkPixbuf *
gd_main_view_get_counter_icon (GdMainView *self,
                               GdkPixbuf *base,
                               gint number)
{
  GtkStyleContext *context;
  cairo_t *cr, *emblem_cr;
  cairo_surface_t *surface, *emblem_surface;
  GdkPixbuf *retval;
  gint width, height;
  gint layout_width, layout_height;
  gint emblem_size;
  gdouble scale;
  gchar *str;
  PangoLayout *layout;
  PangoAttrList *attr_list;
  PangoAttribute *attr;
  const PangoFontDescription *desc;
  GdkRGBA color;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "documents-counter");

  width = gdk_pixbuf_get_width (base);
  height = gdk_pixbuf_get_height (base);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        width, height);
  cr = cairo_create (surface);
  gdk_cairo_set_source_pixbuf (cr, base, 0, 0);
  cairo_paint (cr);

  emblem_size = MIN (width / 2, height / 2);
  emblem_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                               emblem_size, emblem_size);
  emblem_cr = cairo_create (emblem_surface);
  gtk_render_background (context, emblem_cr,
                         0, 0, emblem_size, emblem_size);

  if (number > 99)
    number = 99;
  if (number < -99)
    number = -99;

  str = g_strdup_printf ("%d", number);
  layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), str);
  g_free (str);

  pango_layout_get_pixel_size (layout, &layout_width, &layout_height);

  /* scale the layout to be 0.5 of the size still available for drawing */
  scale = (emblem_size * 0.50) / (MAX (layout_width, layout_height));
  attr_list = pango_attr_list_new ();

  attr = pango_attr_scale_new (scale);
  pango_attr_list_insert (attr_list, attr);
  pango_layout_set_attributes (layout, attr_list);

  desc = gtk_style_context_get_font (context, 0);
  pango_layout_set_font_description (layout, desc);

  gtk_style_context_get_color (context, 0, &color);
  gdk_cairo_set_source_rgba (emblem_cr, &color);

  /* update these values */
  pango_layout_get_pixel_size (layout, &layout_width, &layout_height);

  cairo_move_to (emblem_cr,
                 emblem_size / 2 - layout_width / 2,
                 emblem_size / 2 - layout_height / 2);

  pango_cairo_show_layout (emblem_cr, layout);

  g_object_unref (layout);
  pango_attr_list_unref (attr_list);
  cairo_destroy (emblem_cr);

  cairo_set_source_surface (cr, emblem_surface, 
                            width - emblem_size, height - emblem_size);
  cairo_paint (cr);
  cairo_destroy (cr);

  retval = gdk_pixbuf_get_from_surface (surface,
                                        0, 0,
                                        width, height);

  cairo_surface_destroy (emblem_surface);
  cairo_surface_destroy (surface);
  gtk_style_context_restore (context);

  return retval;
}

static GdMainViewGeneric *
get_generic (GdMainView *self)
{
  if (self->priv->current_view != NULL)
    return GD_MAIN_VIEW_GENERIC (self->priv->current_view);

  return NULL;
}

static gboolean
on_button_release_selection_mode (GdMainView *self,
                                  GdkEventButton *event,
                                  gboolean entered_mode,
                                  GtkTreePath *path)
{
  gboolean selected;
  GtkTreeIter iter;

  if (!gtk_tree_model_get_iter (self->priv->model, &iter, path))
    return FALSE;

  gtk_tree_model_get (self->priv->model, &iter,
                      GD_MAIN_COLUMN_SELECTED, &selected,
                      -1);

  if (selected && !entered_mode)
    gtk_list_store_set (GTK_LIST_STORE (self->priv->model), &iter,
                        GD_MAIN_COLUMN_SELECTED, FALSE,
                        -1);
  else if (!selected)
    gtk_list_store_set (GTK_LIST_STORE (self->priv->model), &iter,
                        GD_MAIN_COLUMN_SELECTED, TRUE,
                        -1);

  g_signal_emit (self, signals[VIEW_SELECTION_CHANGED], 0);

  return FALSE;
}

static gboolean
on_button_release_view_mode (GdMainView *self,
                             GdkEventButton *event,
                             GtkTreePath *path)
{
  GtkTreeIter iter;
  gchar *id;

  if (self->priv->model == NULL)
    return FALSE;

  if (!gtk_tree_model_get_iter (self->priv->model, &iter, path))
    return FALSE;

  gtk_tree_model_get (self->priv->model, &iter,
                      GD_MAIN_COLUMN_ID, &id,
                      -1);

  g_signal_emit (self, signals[ITEM_ACTIVATED], 0, id, path);
  g_free (id);

  return FALSE;
}

static gboolean
on_button_release_event (GtkWidget *view,
                         GdkEventButton *event,
                         gpointer user_data)
{
  GdMainView *self = user_data;
  GdMainViewGeneric *generic = get_generic (self);
  GtkTreePath *path;
  gchar *button_release_item_path;
  gboolean entered_mode = FALSE, selection_mode;
  gboolean res, same_item = FALSE;

  /* eat double/triple click events */
  if (event->type != GDK_BUTTON_RELEASE)
    return TRUE;

  path = gd_main_view_generic_get_path_at_pos (generic, event->x, event->y);

  if (path != NULL)
    {
      button_release_item_path = gtk_tree_path_to_string (path);
      if (g_strcmp0 (self->priv->button_press_item_path, button_release_item_path) == 0)
        same_item = TRUE;

      g_free (button_release_item_path);
    }

  g_free (self->priv->button_press_item_path);
  self->priv->button_press_item_path = NULL;

  if (!same_item)
    {
      res = FALSE;
      goto out;
    }

  selection_mode = self->priv->selection_mode;

  if (!selection_mode)
    {
      if ((event->button == 3) ||
          ((event->button == 1) && (event->state & GDK_CONTROL_MASK)))
        {
          g_signal_emit (self, signals[SELECTION_MODE_REQUEST], 0);
          selection_mode = TRUE;
          entered_mode = TRUE;
        }
    }

  if (selection_mode)
    res = on_button_release_selection_mode (self, event, entered_mode, path);
  else
    res = on_button_release_view_mode (self, event, path);

 out:
  gtk_tree_path_free (path);
  return res;
}

static gboolean
on_button_press_event (GtkWidget *view,
                       GdkEventButton *event,
                       gpointer user_data)
{
  GdMainView *self = user_data;
  GdMainViewGeneric *generic = get_generic (self);
  GtkTreePath *path;
  GList *selection, *l;
  GtkTreePath *sel_path;
  gboolean found = FALSE;

  path = gd_main_view_generic_get_path_at_pos (generic, event->x, event->y);

  if (path != NULL)
    self->priv->button_press_item_path = gtk_tree_path_to_string (path);

  if (!self->priv->selection_mode ||
      path == NULL)
    {
      gtk_tree_path_free (path);
      return FALSE;
    }

  selection = gd_main_view_get_selection (self);

  for (l = selection; l != NULL; l = l->next)
    {
      sel_path = l->data;
      if (gtk_tree_path_compare (path, sel_path) == 0)
        {
          found = TRUE;
          break;
        }
    }

  if (selection != NULL)
    g_list_free_full (selection, (GDestroyNotify) gtk_tree_path_free);

  /* if we did not find the item in the selection, block
   * drag and drop, while in selection mode
   */
  return !found;
}

static void
on_drag_begin (GdMainViewGeneric *generic,
               GdkDragContext *drag_context,
               gpointer user_data)
{
  GdMainView *self = user_data;

  if (self->priv->button_press_item_path != NULL)
    {
      gboolean res;
      GtkTreeIter iter;
      GdkPixbuf *icon = NULL;
      GtkTreePath *path;

      path = gtk_tree_path_new_from_string (self->priv->button_press_item_path);
      res = gtk_tree_model_get_iter (self->priv->model,
                                     &iter, path);
      if (res)
        gtk_tree_model_get (self->priv->model, &iter,
                            GD_MAIN_COLUMN_ICON, &icon,
                            -1);

      if (self->priv->selection_mode && 
          icon != NULL)
        {
          GList *selection;
          GdkPixbuf *counter;

          selection = gd_main_view_get_selection (self);

          if (g_list_length (selection) > 1)
            {
              counter = gd_main_view_get_counter_icon (self, icon, g_list_length (selection));
              g_clear_object (&icon);
              icon = counter;
            }

          if (selection != NULL)
            g_list_free_full (selection, (GDestroyNotify) gtk_tree_path_free);
        }

      if (icon != NULL)
        {
          gtk_drag_set_icon_pixbuf (drag_context, icon,
                                    MAIN_VIEW_DND_ICON_OFFSET, MAIN_VIEW_DND_ICON_OFFSET);
          g_object_unref (icon);
        }

      gtk_tree_path_free (path);
    }
}

static void
gd_main_view_apply_model (GdMainView *self)
{
  GdMainViewGeneric *generic = get_generic (self);
  gd_main_view_generic_set_model (generic, self->priv->model);
}

static gboolean
clear_selection_list_foreach (GtkTreeModel *model,
                              GtkTreePath *path,
                              GtkTreeIter *iter,
                              gpointer user_data)
{
  gtk_list_store_set (GTK_LIST_STORE (model), iter,
                      GD_MAIN_COLUMN_SELECTED, FALSE,
                      -1);

  return FALSE;
}

static void
gd_main_view_apply_selection_mode (GdMainView *self)
{
  GdMainViewGeneric *generic = get_generic (self);

  gd_main_view_generic_set_selection_mode (generic, self->priv->selection_mode);

  if (!self->priv->selection_mode &&
      self->priv->model != NULL)
    {
      gtk_tree_model_foreach (self->priv->model,
                              clear_selection_list_foreach,
                              self);
      g_signal_emit (self, signals[VIEW_SELECTION_CHANGED], 0);
    }
}

static void
gd_main_view_rebuild (GdMainView *self)
{
  GtkStyleContext *context;

  if (self->priv->current_view != NULL)
    gtk_widget_destroy (self->priv->current_view);

  if (self->priv->current_type == GD_MAIN_VIEW_ICON)
    self->priv->current_view = gd_main_icon_view_new ();
  else
    self->priv->current_view = gd_main_list_view_new ();

  context = gtk_widget_get_style_context (self->priv->current_view);
  gtk_style_context_add_class (context, "documents-main-view");

  gtk_container_add (GTK_CONTAINER (self), self->priv->current_view);

  g_signal_connect (self->priv->current_view, "button-press-event",
                    G_CALLBACK (on_button_press_event), self);
  g_signal_connect (self->priv->current_view, "button-release-event",
                    G_CALLBACK (on_button_release_event), self);
  g_signal_connect_after (self->priv->current_view, "drag-begin",
                          G_CALLBACK (on_drag_begin), self);

  gd_main_view_apply_selection_mode (self);
  gd_main_view_apply_model (self);

  gtk_widget_show_all (GTK_WIDGET (self));
}

GdMainView *
gd_main_view_new (GdMainViewType type)
{
  return g_object_new (GD_TYPE_MAIN_VIEW,
                       "view-type", type,
                       NULL);
}

void
gd_main_view_set_view_type (GdMainView *self,
                            GdMainViewType type)
{
  if (type != self->priv->current_type)
    {
      self->priv->current_type = type;
      gd_main_view_rebuild (self);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VIEW_TYPE]);
    }
}

GdMainViewType
gd_main_view_get_view_type (GdMainView *self)
{
  return self->priv->current_type;
}

void
gd_main_view_set_selection_mode (GdMainView *self,
                                 gboolean selection_mode)
{
  if (selection_mode != self->priv->selection_mode)
    {
      self->priv->selection_mode = selection_mode;
      gd_main_view_apply_selection_mode (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTION_MODE]);
    }
}

GdMainViewType
gd_main_view_get_selection_mode (GdMainView *self)
{
  return self->priv->selection_mode;
}

/**
 * gd_main_view_set_model:
 * @self:
 * @model: (allow-none):
 *
 */
void
gd_main_view_set_model (GdMainView *self,
                        GtkTreeModel *model)
{
  if (model != self->priv->model)
    {
      g_clear_object (&self->priv->model);

      if (model)
        self->priv->model = g_object_ref (model);
      else
        self->priv->model = NULL;

      gd_main_view_apply_model (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
    }
}

/**
 * gd_main_view_get_model:
 * @self:
 *
 * Returns: (transfer none):
 */
GtkTreeModel *
gd_main_view_get_model (GdMainView *self)
{
  return self->priv->model;
}

/**
 * gd_main_view_get_generic_view:
 * @self:
 *
 * Returns: (transfer none):
 */
GtkWidget *
gd_main_view_get_generic_view (GdMainView *self)
{
  return self->priv->current_view;
}

static gboolean
build_selection_list_foreach (GtkTreeModel *model,
                              GtkTreePath *path,
                              GtkTreeIter *iter,
                              gpointer user_data)
{
  GList **sel = user_data;
  gboolean is_selected;

  gtk_tree_model_get (model, iter,
                      GD_MAIN_COLUMN_SELECTED, &is_selected,
                      -1);

  if (is_selected)
    *sel = g_list_prepend (*sel, gtk_tree_path_copy (path));

  return FALSE;
}

/**
 * gd_main_view_get_selection:
 * @self:
 *
 * Returns: (element-type GtkTreePath) (transfer full):
 */
GList *
gd_main_view_get_selection (GdMainView *self)
{
  GList *retval = NULL;

  gtk_tree_model_foreach (self->priv->model,
                          build_selection_list_foreach,
                          &retval);

  return g_list_reverse (retval);
}
