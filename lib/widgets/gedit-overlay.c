/*
 * gedit-overlay.c
 * This file is part of gedit
 *
 * Copyright (C) 2010 - Ignacio Casal Quinteiro
 *
 * Based on Mike Krüger <mkrueger@novell.com> work.
 *
 * gedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#include "gedit-overlay.h"
#include "gedit-theatrics-animated-widget.h"

#define GEDIT_OVERLAY_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GEDIT_TYPE_OVERLAY, GeditOverlayPrivate))

typedef struct _OverlayChild
{
	GtkWidget *child;
	GdkGravity gravity;
	guint      offset;

	guint fixed_position : 1;
	guint is_animated : 1;
} OverlayChild;

struct _GeditOverlayPrivate
{
	GtkWidget *main_widget;
	GSList    *children;
	GtkAllocation main_alloc;

	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	glong          hadjustment_signal_id;
	glong          vadjustment_signal_id;

	/* GtkScrollablePolicy needs to be checked when
	 * driving the scrollable adjustment values */
	guint hscroll_policy : 1;
	guint vscroll_policy : 1;
};

enum
{
	PROP_0,
	PROP_MAIN_WIDGET,
	PROP_HADJUSTMENT,
	PROP_VADJUSTMENT,
	PROP_HSCROLL_POLICY,
	PROP_VSCROLL_POLICY
};

static void	gedit_overlay_set_hadjustment		(GeditOverlay  *overlay,
							 GtkAdjustment *adjustment);
static void	gedit_overlay_set_vadjustment		(GeditOverlay  *overlay,
							 GtkAdjustment *adjustment);

G_DEFINE_TYPE_WITH_CODE (GeditOverlay, gedit_overlay, GTK_TYPE_CONTAINER,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static void
free_container_child (OverlayChild *child)
{
	g_slice_free (OverlayChild, child);
}

static void
add_toplevel_widget (GeditOverlay *overlay,
                     GtkWidget    *widget,
                     gboolean      fixed_position,
                     gboolean      is_animated,
                     GdkGravity    gravity,
                     guint         offset)
{
	OverlayChild *child = g_slice_new (OverlayChild);

	child->child = widget;
	child->gravity = gravity;
	child->fixed_position = fixed_position;
	child->is_animated = is_animated;
	child->offset = offset;

	gtk_widget_set_parent (widget, GTK_WIDGET (overlay));

	overlay->priv->children = g_slist_append (overlay->priv->children,
	                                          child);
}

static void
gedit_overlay_finalize (GObject *object)
{
	GeditOverlay *overlay = GEDIT_OVERLAY (object);

	g_slist_free (overlay->priv->children);

	G_OBJECT_CLASS (gedit_overlay_parent_class)->finalize (object);
}

static void
gedit_overlay_dispose (GObject *object)
{
	GeditOverlay *overlay = GEDIT_OVERLAY (object);

	if (overlay->priv->hadjustment != NULL)
	{
		g_signal_handler_disconnect (overlay->priv->hadjustment,
		                             overlay->priv->hadjustment_signal_id);
		overlay->priv->hadjustment = NULL;
	}

	if (overlay->priv->vadjustment != NULL)
	{
		g_signal_handler_disconnect (overlay->priv->vadjustment,
		                             overlay->priv->vadjustment_signal_id);
		overlay->priv->vadjustment = NULL;
	}

	G_OBJECT_CLASS (gedit_overlay_parent_class)->dispose (object);
}

static void
gedit_overlay_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
	GeditOverlay *overlay = GEDIT_OVERLAY (object);
	GeditOverlayPrivate *priv = overlay->priv;

	switch (prop_id)
	{
		case PROP_MAIN_WIDGET:
			g_value_set_object (value, priv->main_widget);
			break;

		case PROP_HADJUSTMENT:
			g_value_set_object (value, priv->hadjustment);
			break;

		case PROP_VADJUSTMENT:
			g_value_set_object (value, priv->vadjustment);
			break;

		case PROP_HSCROLL_POLICY:
			if (GTK_IS_SCROLLABLE (priv->main_widget))
			{
				g_value_set_enum (value,
				                  gtk_scrollable_get_hscroll_policy (GTK_SCROLLABLE (priv->main_widget)));
			}
			else
			{
				g_value_set_enum (value, priv->hscroll_policy);
			}
			break;

		case PROP_VSCROLL_POLICY:
			if (GTK_IS_SCROLLABLE (priv->main_widget))
			{
				g_value_set_enum (value,
				                  gtk_scrollable_get_vscroll_policy (GTK_SCROLLABLE (priv->main_widget)));
			}
			else
			{
				g_value_set_enum (value, priv->vscroll_policy);
			}
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_overlay_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
	GeditOverlay *overlay = GEDIT_OVERLAY (object);
	GeditOverlayPrivate *priv = overlay->priv;

	switch (prop_id)
	{
		case PROP_MAIN_WIDGET:
			overlay->priv->main_widget = g_value_get_object (value);
			add_toplevel_widget (overlay,
			                     overlay->priv->main_widget,
			                     TRUE, FALSE, GDK_GRAVITY_STATIC,
			                     0);
			break;

		case PROP_HADJUSTMENT:
			gedit_overlay_set_hadjustment (overlay,
						       g_value_get_object (value));
			break;

		case PROP_VADJUSTMENT:
			gedit_overlay_set_vadjustment (overlay,
						       g_value_get_object (value));
			break;

		case PROP_HSCROLL_POLICY:
			if (GTK_IS_SCROLLABLE (priv->main_widget))
			{
				gtk_scrollable_set_hscroll_policy (GTK_SCROLLABLE (priv->main_widget),
				                                   g_value_get_enum (value));
			}
			else
			{
				priv->hscroll_policy = g_value_get_enum (value);
				gtk_widget_queue_resize (GTK_WIDGET (overlay));
			}
			break;

		case PROP_VSCROLL_POLICY:
			if (GTK_IS_SCROLLABLE (priv->main_widget))
			{
				gtk_scrollable_set_vscroll_policy (GTK_SCROLLABLE (priv->main_widget),
				                                   g_value_get_enum (value));
			}
			else
			{
				priv->vscroll_policy = g_value_get_enum (value);
				gtk_widget_queue_resize (GTK_WIDGET (overlay));
			}
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gedit_overlay_realize (GtkWidget *widget)
{
	GtkAllocation allocation;
	GdkWindow *window;
	GdkWindowAttr attributes;
	gint attributes_mask;
	GtkStyleContext *context;

	gtk_widget_set_realized (widget, TRUE);

	gtk_widget_get_allocation (widget, &allocation);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

	window = gdk_window_new (gtk_widget_get_parent_window (widget),
	                         &attributes, attributes_mask);
	gtk_widget_set_window (widget, window);
	gdk_window_set_user_data (window, widget);

	context = gtk_widget_get_style_context (widget);
	gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
	gtk_style_context_set_background (context, window);
}

static void
gedit_overlay_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum,
                                   gint      *natural)
{
	GeditOverlayPrivate *priv = GEDIT_OVERLAY (widget)->priv;
	OverlayChild *child;
	GSList *children;
	gint child_min, child_nat;

	*minimum = 0;
	*natural = 0;

	for (children = priv->children; children; children = children->next)
	{
		child = children->data;

		if (!gtk_widget_get_visible (child->child))
			continue;

		gtk_widget_get_preferred_width (child->child, &child_min, &child_nat);

		*minimum = MAX (*minimum, child_min);
		*natural = MAX (*natural, child_nat);
	}
}

static void
gedit_overlay_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
	GeditOverlayPrivate *priv = GEDIT_OVERLAY (widget)->priv;
	OverlayChild *child;
	GSList *children;
	gint child_min, child_nat;

	*minimum = 0;
	*natural = 0;

	for (children = priv->children; children; children = children->next)
	{
		child = children->data;

		if (!gtk_widget_get_visible (child->child))
			continue;

		gtk_widget_get_preferred_height (child->child, &child_min, &child_nat);

		*minimum = MAX (*minimum, child_min);
		*natural = MAX (*natural, child_nat);
	}
}

static void
set_children_positions (GeditOverlay *overlay)
{
	GSList *l;

	for (l = overlay->priv->children; l != NULL; l = g_slist_next (l))
	{
		GeditOverlayPrivate *priv = overlay->priv;
		OverlayChild *child = (OverlayChild *)l->data;
		GtkRequisition req;
		GtkAllocation alloc;

		if (child->child == priv->main_widget)
			continue;

		gtk_widget_get_preferred_size (child->child, &req, NULL);

		/* FIXME: Add all the gravities here */
		switch (child->gravity)
		{
			/* The gravity is treated as position and not as a gravity */
			case GDK_GRAVITY_NORTH_EAST:
				alloc.x = priv->main_alloc.width - req.width - child->offset;
				alloc.y = 0;
				break;
			case GDK_GRAVITY_NORTH_WEST:
				alloc.x = child->offset;
				alloc.y = 0;
				break;
			case GDK_GRAVITY_SOUTH_WEST:
				alloc.x = child->offset;
				alloc.y = priv->main_alloc.height - req.height;
				break;
			default:
				alloc.x = 0;
				alloc.y = 0;
		}

		if (!child->fixed_position)
		{
			alloc.x *= gtk_adjustment_get_value (priv->hadjustment);
			alloc.y *= gtk_adjustment_get_value (priv->vadjustment);
		}

		alloc.width = req.width;
		alloc.height = req.height;

		gtk_widget_size_allocate (child->child, &alloc);
	}
}

static void
gedit_overlay_size_allocate (GtkWidget     *widget,
                             GtkAllocation *allocation)
{
	GeditOverlay *overlay = GEDIT_OVERLAY (widget);

	GTK_WIDGET_CLASS (gedit_overlay_parent_class)->size_allocate (widget, allocation);

	overlay->priv->main_alloc.x = 0;
	overlay->priv->main_alloc.y = 0;
	overlay->priv->main_alloc.width = allocation->width;
	overlay->priv->main_alloc.height = allocation->height;

	gtk_widget_size_allocate (overlay->priv->main_widget,
	                          &overlay->priv->main_alloc);
	set_children_positions (overlay);
}

static void
overlay_add (GtkContainer *overlay,
             GtkWidget    *widget)
{
	add_toplevel_widget (GEDIT_OVERLAY (overlay), widget,
	                     FALSE, FALSE, GDK_GRAVITY_STATIC, 0);
}

static void
gedit_overlay_remove (GtkContainer *overlay,
                      GtkWidget    *widget)
{
	GeditOverlay *goverlay = GEDIT_OVERLAY (overlay);
	GSList *l;

	for (l = goverlay->priv->children; l != NULL; l = g_slist_next (l))
	{
		OverlayChild *child = (OverlayChild *)l->data;

		if (child->child == widget)
		{
			gtk_widget_unparent (widget);
			goverlay->priv->children = g_slist_remove_link (goverlay->priv->children,
			                                                l);
			free_container_child (child);
			break;
		}
	}
}

static void
gedit_overlay_forall (GtkContainer *overlay,
                      gboolean      include_internals,
                      GtkCallback   callback,
                      gpointer      callback_data)
{
	GeditOverlay *goverlay = GEDIT_OVERLAY (overlay);
	GSList *l;

	for (l = goverlay->priv->children; l != NULL; l = g_slist_next (l))
	{
		OverlayChild *child = (OverlayChild *)l->data;

		(* callback) (child->child, callback_data);
	}
}

static GType
gedit_overlay_child_type (GtkContainer *overlay)
{
	return GTK_TYPE_WIDGET;
}

static void
adjustment_value_changed (GtkAdjustment *adjustment,
                          GeditOverlay  *overlay)
{
	set_children_positions (overlay);
}

static void
gedit_overlay_set_hadjustment (GeditOverlay  *overlay,
                               GtkAdjustment *adjustment)
{
	GeditOverlayPrivate *priv = overlay->priv;

	if (adjustment && priv->vadjustment == adjustment)
		return;

	if (priv->hadjustment != NULL)
	{
		g_signal_handler_disconnect (priv->hadjustment,
		                             priv->hadjustment_signal_id);
		g_object_unref (priv->hadjustment);
	}

	if (adjustment == NULL)
	{
		adjustment = gtk_adjustment_new (0.0, 0.0, 0.0,
		                                 0.0, 0.0, 0.0);
	}

	priv->hadjustment_signal_id =
		g_signal_connect (adjustment,
		                  "value-changed",
		                  G_CALLBACK (adjustment_value_changed),
		                  overlay);

	priv->hadjustment = g_object_ref_sink (adjustment);

	if (GTK_IS_SCROLLABLE (priv->main_widget))
	{
		g_object_set (priv->main_widget,
		              "hadjustment", adjustment,
		              NULL);

	}

	g_object_notify (G_OBJECT (overlay), "hadjustment");
}

static void
gedit_overlay_set_vadjustment (GeditOverlay  *overlay,
                               GtkAdjustment *adjustment)
{
	GeditOverlayPrivate *priv = overlay->priv;

	if (adjustment && priv->vadjustment == adjustment)
		return;

	if (priv->vadjustment != NULL)
	{
		g_signal_handler_disconnect (priv->vadjustment,
		                             priv->vadjustment_signal_id);
		g_object_unref (priv->vadjustment);
	}

	if (adjustment == NULL)
	{
		adjustment = gtk_adjustment_new (0.0, 0.0, 0.0,
		                                 0.0, 0.0, 0.0);
	}

	overlay->priv->vadjustment_signal_id =
		g_signal_connect (adjustment,
		                  "value-changed",
		                  G_CALLBACK (adjustment_value_changed),
		                  overlay);

	priv->vadjustment = g_object_ref_sink (adjustment);

	if (GTK_IS_SCROLLABLE (priv->main_widget))
	{
		g_object_set (priv->main_widget,
		              "vadjustment", adjustment,
		              NULL);
	}

	g_object_notify (G_OBJECT (overlay), "vadjustment");
}

static void
gedit_overlay_class_init (GeditOverlayClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

	object_class->finalize = gedit_overlay_finalize;
	object_class->dispose = gedit_overlay_dispose;
	object_class->get_property = gedit_overlay_get_property;
	object_class->set_property = gedit_overlay_set_property;

	widget_class->realize = gedit_overlay_realize;
	widget_class->get_preferred_width = gedit_overlay_get_preferred_width;
	widget_class->get_preferred_height = gedit_overlay_get_preferred_height;
	widget_class->size_allocate = gedit_overlay_size_allocate;

	container_class->add = overlay_add;
	container_class->remove = gedit_overlay_remove;
	container_class->forall = gedit_overlay_forall;
	container_class->child_type = gedit_overlay_child_type;

	g_object_class_install_property (object_class, PROP_MAIN_WIDGET,
	                                 g_param_spec_object ("main-widget",
	                                                      "Main Widget",
	                                                      "The Main Widget",
	                                                      GTK_TYPE_WIDGET,
	                                                      G_PARAM_READWRITE |
	                                                      G_PARAM_CONSTRUCT_ONLY |
	                                                      G_PARAM_STATIC_STRINGS));

	g_object_class_override_property (object_class,
	                                  PROP_HADJUSTMENT,
	                                  "hadjustment");
	g_object_class_override_property (object_class,
	                                  PROP_VADJUSTMENT,
	                                  "vadjustment");
	g_object_class_override_property (object_class,
	                                  PROP_HSCROLL_POLICY,
	                                  "hscroll-policy");
	g_object_class_override_property (object_class,
	                                  PROP_VSCROLL_POLICY,
	                                  "vscroll-policy");

	g_type_class_add_private (object_class, sizeof (GeditOverlayPrivate));
}

static void
gedit_overlay_init (GeditOverlay *overlay)
{
	overlay->priv = GEDIT_OVERLAY_GET_PRIVATE (overlay);
}

GtkWidget *
gedit_overlay_new (GtkWidget *main_widget)
{
	return GTK_WIDGET (g_object_new (GEDIT_TYPE_OVERLAY,
	                                 "main-widget", main_widget,
	                                 NULL));
}

static GeditTheatricsAnimatedWidget *
get_animated_widget (GeditOverlay *overlay,
                     GtkWidget    *widget)
{
	GSList *l;

	for (l = overlay->priv->children; l != NULL; l = g_slist_next (l))
	{
		OverlayChild *child = (OverlayChild *)l->data;
		GtkWidget *in_widget;

		if (!child->is_animated)
			continue;

		g_object_get (child->child, "widget", &in_widget, NULL);
		g_assert (in_widget != NULL);

		if (in_widget == widget)
		{
			return GEDIT_THEATRICS_ANIMATED_WIDGET (child->child);
		}
	}

	return NULL;
}

/* Note: see that we use the gravity as a position */
void
gedit_overlay_add (GeditOverlay *overlay,
                   GtkWidget    *widget,
                   GtkOrientation orientation,
                   GdkGravity    gravity,
                   guint         offset,
                   gboolean      in)
{
    GeditTheatricsAnimatedWidget *anim_widget;
    
    anim_widget = get_animated_widget (overlay, widget);
    
    if (anim_widget == NULL)
    {
        anim_widget = gedit_theatrics_animated_widget_new (widget, orientation);
        gtk_widget_show (GTK_WIDGET (anim_widget));
        
        add_toplevel_widget (overlay, GTK_WIDGET (anim_widget), TRUE,
                             TRUE, gravity, offset);
    }
}
