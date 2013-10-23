/*
 * (C) Copyright 2007 Bastien Nocera <hadess@hadess.net>
 *
 * Glow code from libwnck/libwnck/tasklist.c:
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2003 Kim Woelders
 * Copyright © 2003 Red Hat, Inc.
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301  USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <gtk/gtk.h>
#include "totem-glow-button.h"

#define FADE_OPACITY_DEFAULT 0.6
#define ENTER_SPEEDUP_RATIO 0.4
#define FADE_MAX_LOOPS 4

struct _TotemGlowButton {
	GtkButton parent;

	gdouble glow_start_time;
	gdouble glow_factor;

	guint button_glow;

	guint glow : 1;
	guint anim_enabled : 1;
	guint pointer_entered : 1;
	/* Set when we don't want to play animation
	 * anymore in pointer entered mode */
	guint anim_finished :1;
};

static void totem_glow_button_set_timeout (TotemGlowButton *button, gboolean set_timeout);

static GtkButtonClass *parent_class;

G_DEFINE_TYPE (TotemGlowButton, totem_glow_button, GTK_TYPE_BUTTON);

static gboolean
totem_glow_button_glow (TotemGlowButton *button)
{
	GtkWidget *buttonw;
	GTimeVal tv;
	gdouble now;
	gfloat fade_opacity, loop_time;

	buttonw = GTK_WIDGET (button);

	if (gtk_widget_get_realized (buttonw) == FALSE)
		return TRUE;

	if (button->anim_enabled != FALSE) {
		g_get_current_time (&tv);
		now = (tv.tv_sec * (1.0 * G_USEC_PER_SEC) +
		       tv.tv_usec) / G_USEC_PER_SEC;

		/* Hard-coded values */
		fade_opacity = FADE_OPACITY_DEFAULT;
		loop_time = 3.0;

		if (button->glow_start_time <= G_MINDOUBLE) {
			button->glow_start_time = now;
			/* If the pointer entered, we want to start with 'dark' */
			if (button->pointer_entered != FALSE) {
				button->glow_start_time -= loop_time / 4.0;
			}
		}

		/* Timing for mouse hover animation
		   [light]......[dark]......[light]......[dark]...
		   {mouse hover event}
		   [dark]..[light]..[dark]..[light]
		   {mouse leave event}
		   [light]......[dark]......[light]......[dark]
		*/
		if (button->pointer_entered != FALSE) {
			/* pointer entered animation should be twice as fast */
			loop_time *= ENTER_SPEEDUP_RATIO;
		}
		if ((now - button->glow_start_time) > loop_time * FADE_MAX_LOOPS) {
			button->anim_finished = TRUE;
			button->glow_factor = FADE_OPACITY_DEFAULT * 0.5;
		} else {
			button->glow_factor = fade_opacity * (0.5 - 0.5 * cos ((now - button->glow_start_time) * M_PI * 2.0 / loop_time));
		}
	} else {
		button->glow_factor = FADE_OPACITY_DEFAULT * 0.5;
	}

	gtk_widget_queue_draw (GTK_WIDGET (button));

	if (button->anim_finished != FALSE)
		totem_glow_button_set_timeout (button, FALSE);

	return button->anim_enabled;
}

static void
totem_glow_button_clear_glow_start_timeout_id (TotemGlowButton *button)
{
	button->button_glow = 0;
}

static gboolean
totem_glow_button_draw (GtkWidget *widget,
			cairo_t   *cr,
			gpointer   user_data)
{
	TotemGlowButton *button;
	GtkStyleContext *context;
	GtkAllocation allocation, child_allocation;
	gint width, height;
	GtkWidget *child;
	GdkRGBA acolor;

	button = TOTEM_GLOW_BUTTON (widget);

	if (button->glow_factor == 0.0)
		return FALSE;

	context = gtk_widget_get_style_context (widget);

	/* push a translucent overlay to paint to, so we can blend later */
	cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);

	gtk_widget_get_allocation (widget, &allocation);

	width = allocation.width;
	height = allocation.height;

	/* Draw a rectangle with bg[SELECTED] */
	gtk_style_context_save (context);
	gtk_style_context_add_class (context, "button");
	gtk_style_context_set_state (context, GTK_STATE_SELECTED);
	gtk_style_context_get_background_color (context, GTK_STATE_SELECTED, &acolor);
	gdk_cairo_set_source_rgba (cr, &acolor);
	gtk_render_background (context, cr, 0, 0, width, height);
	gtk_style_context_restore (context);

	/* then the image */
	cairo_save (cr);
	child = gtk_bin_get_child (GTK_BIN (button));
	gtk_widget_get_allocation (child, &child_allocation);
	cairo_translate (cr,
			 child_allocation.x - allocation.x,
			 child_allocation.y - allocation.y);
	gtk_widget_draw (gtk_bin_get_child (GTK_BIN (button)), cr);
	cairo_restore (cr);

	/* finally blend it */
	cairo_pop_group_to_source (cr);
	cairo_paint_with_alpha (cr, button->glow_factor);

	return FALSE;
}

static void
totem_glow_button_map (GtkWidget *buttonw)
{
	TotemGlowButton *button;

	(* GTK_WIDGET_CLASS (parent_class)->map) (buttonw);

	button = TOTEM_GLOW_BUTTON (buttonw);

	if (button->glow != FALSE && button->button_glow == 0) {
		totem_glow_button_set_glow (button, TRUE);
	}
}

static void
totem_glow_button_unmap (GtkWidget *buttonw)
{
	TotemGlowButton *button;

	button = TOTEM_GLOW_BUTTON (buttonw);

	if (button->button_glow > 0) {
		g_source_remove (button->button_glow);
		button->button_glow = 0;
	}

	(* GTK_WIDGET_CLASS (parent_class)->unmap) (buttonw);
}

static void
totem_glow_button_enter (GtkButton *buttonw)
{
	TotemGlowButton *button;

	button = TOTEM_GLOW_BUTTON (buttonw);

	(* GTK_BUTTON_CLASS (parent_class)->enter) (buttonw);

	button->pointer_entered = TRUE;
	button->anim_finished = FALSE;
	button->glow_start_time = G_MINDOUBLE;
	totem_glow_button_set_timeout (button, FALSE);
}

static void
totem_glow_button_leave (GtkButton *buttonw)
{
	TotemGlowButton *button;

	button = TOTEM_GLOW_BUTTON (buttonw);

	(* GTK_BUTTON_CLASS (parent_class)->leave) (buttonw);

	button->pointer_entered = FALSE;
	button->glow_start_time = G_MINDOUBLE;
	button->anim_finished = FALSE;
	if (button->glow != FALSE)
		totem_glow_button_set_timeout (button, TRUE);
}

static void
totem_glow_button_finalize (GObject *object)
{
	TotemGlowButton *button = TOTEM_GLOW_BUTTON (object);

	totem_glow_button_set_glow (button, FALSE);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
totem_glow_button_class_init (TotemGlowButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkButtonClass *button_class = GTK_BUTTON_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = totem_glow_button_finalize;
	/* Note that we don't use a draw here because we
	 * want to modify what the button will draw by itself */
	widget_class->map = totem_glow_button_map;
	widget_class->unmap = totem_glow_button_unmap;
	button_class->enter = totem_glow_button_enter;
	button_class->leave = totem_glow_button_leave;
}

static void
totem_glow_button_init (TotemGlowButton *button)
{
	button->glow_start_time = 0.0;
	button->button_glow = 0;
	button->glow_factor = 0.0;

	g_signal_connect_object (button, "draw",
				 G_CALLBACK (totem_glow_button_draw),
				 G_OBJECT (button),
				 G_CONNECT_AFTER);
}

GtkWidget *
totem_glow_button_new (void)
{
	return g_object_new (TOTEM_TYPE_GLOW_BUTTON, NULL);
}

/* We can only add a timeout once, we assert that, though
 * calling it multiple times to disable the animation is fine */
static void
totem_glow_button_set_timeout (TotemGlowButton *button, gboolean set_timeout)
{
	if (set_timeout != FALSE) {
		if (button->button_glow > 0)
			return;

		button->glow_start_time = 0.0;

		/* The animation doesn't speed up or slow down based on the
		 * timeout value, but instead will just appear smoother or
		 * choppier.
		 */
		button->button_glow =
			g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE,
					    100,
					    (GSourceFunc) totem_glow_button_glow, button,
					    (GDestroyNotify) totem_glow_button_clear_glow_start_timeout_id);
		g_source_set_name_by_id (button->button_glow, "[epiphany] totem_glow_button_glow");
	} else {
		if (button->button_glow > 0) {
			g_source_remove (button->button_glow);
			button->button_glow = 0;
		}
		button->glow_factor = 0.0;
		gtk_widget_queue_draw (GTK_WIDGET (button));
	}
}

void
totem_glow_button_set_glow (TotemGlowButton *button, gboolean glow)
{
	GtkSettings *settings;
	gboolean anim_enabled;

	g_return_if_fail (TOTEM_IS_GLOW_BUTTON (button));

	if (gtk_widget_get_mapped (GTK_WIDGET (button)) == FALSE
	    && glow != FALSE) {
		button->glow = glow;
		return;
	}

	settings = gtk_settings_get_for_screen
		(gtk_widget_get_screen (GTK_WIDGET (button)));
	g_object_get (G_OBJECT (settings),
		      "gtk-enable-animations", &anim_enabled,
		      NULL);
	button->anim_enabled = anim_enabled;

	if (glow == FALSE && button->button_glow == 0 && button->anim_enabled != FALSE)
		return;
	if (glow != FALSE && button->button_glow != 0)
		return;

	button->glow = glow;

	totem_glow_button_set_timeout (button, glow);
}

gboolean
totem_glow_button_get_glow (TotemGlowButton *button)
{
	return button->glow != FALSE;
}

