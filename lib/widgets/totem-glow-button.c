/*
 * (C) Copyright 2007, 2013 Bastien Nocera <hadess@hadess.net>
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

#include <gtk/gtk.h>
#include "totem-glow-button.h"

struct _TotemGlowButtonClass {
  GtkButtonClass parent_class;
};

struct _TotemGlowButton {
	GtkButton parent;

	gboolean glow;
};

enum {
	PROP_0,
	PROP_GLOW
};

static GtkButtonClass *parent_class;

G_DEFINE_TYPE (TotemGlowButton, totem_glow_button, GTK_TYPE_BUTTON);

static const char *css =
"@keyframes blink {\n"
"	100% { background-color: @suggested_action_button_a; }\n"
"}\n"
".blink {\n"
"	background-color: @theme_bg_color;\n"
"	background-image: none;\n"
"	animation: blink 1s ease-in 5 alternate;\n"
"}";

static void
totem_glow_button_get_property (GObject    *object,
				guint       param_id,
				GValue     *value,
				GParamSpec *pspec)
{
	switch (param_id) {
	case PROP_GLOW:
		g_value_set_boolean (value, TOTEM_GLOW_BUTTON (object)->glow);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
	}
}

static void
totem_glow_button_set_property (GObject      *object,
				guint         param_id,
				const GValue *value,
				GParamSpec   *pspec)
{
	switch (param_id) {
	case PROP_GLOW:
		totem_glow_button_set_glow (TOTEM_GLOW_BUTTON (object), g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
	}
}

static void
totem_glow_button_class_init (TotemGlowButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->get_property = totem_glow_button_get_property;
	object_class->set_property = totem_glow_button_set_property;

	g_object_class_install_property (object_class,
					 PROP_GLOW,
					 g_param_spec_boolean ("glow",
							       "Glow",
							       "Whether the button is glowing",
							       FALSE,
							       G_PARAM_READWRITE));
}

static void
totem_glow_button_init (TotemGlowButton *button)
{
        static gsize initialization_value = 0;

        if (g_once_init_enter (&initialization_value)) {
                GtkCssProvider *provider;

                provider = gtk_css_provider_new ();
                gtk_css_provider_load_from_data (provider, css, -1, NULL);
                gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                           GTK_STYLE_PROVIDER (provider),
                                                           GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
                g_object_unref (provider);

                g_once_init_leave (&initialization_value, 1);
        }
}

GtkWidget *
totem_glow_button_new (void)
{
	return g_object_new (TOTEM_TYPE_GLOW_BUTTON, NULL);
}

void
totem_glow_button_set_glow (TotemGlowButton *button,
			    gboolean         glow)
{
	GtkStyleContext *context;

	g_return_if_fail (TOTEM_IS_GLOW_BUTTON (button));

	if (button->glow == glow)
		return;

	button->glow = glow;
	g_object_notify (G_OBJECT (button), "glow");

	context = gtk_widget_get_style_context (GTK_WIDGET (button));
	if (glow)
		gtk_style_context_add_class (context, "blink");
	else
		gtk_style_context_remove_class (context, "blink");
}

gboolean
totem_glow_button_get_glow (TotemGlowButton *button)
{
	return button->glow != FALSE;
}

