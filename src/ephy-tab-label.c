/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2019 Jan-Michael Brummer
 *
 *  This file is part of Epiphany.
 *
 *  Epiphany is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Epiphany is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "ephy-embed.h"
#include "ephy-embed-utils.h"
#include "ephy-settings.h"
#include "ephy-tab-label.h"

#define TAB_WIDTH_N_CHARS 15

struct _EphyTabLabel {
  GtkBox parent_instance;

  GtkWidget *spinner;
  GtkWidget *icon;
  GtkWidget *label;
  GtkWidget *close_button;
  GtkWidget *audio_button;

  gboolean is_pinned;
  gboolean is_loading;
};

enum {
  CLOSE_CLICKED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_LABEL_TEXT,
  PROP_LABEL_URI,
  PROP_ICON_BUF,
  PROP_SPINNING,
  PROP_AUDIO,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];
static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EphyTabLabel, ephy_tab_label, GTK_TYPE_BOX)

static void
ephy_tab_label_set_spinning (EphyTabLabel *tab_label,
                             gboolean      is_spinning)
{
  g_object_set (tab_label->spinner, "active", is_spinning, NULL);
  g_object_set (tab_label->icon, "visible", !is_spinning, NULL);
  g_object_set (tab_label->spinner, "visible", is_spinning, NULL);

  tab_label->is_loading = is_spinning;
}

static void
ephy_tab_label_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  EphyTabLabel *self = EPHY_TAB_LABEL (object);
  const gchar *str;

  switch (prop_id) {
  case PROP_LABEL_TEXT:
    str = g_value_get_string (value);
    if (str && strlen (str) != 0) {
      gtk_label_set_text (GTK_LABEL (self->label), str);
      gtk_widget_set_tooltip_text (GTK_WIDGET (self), str);
    }
    break;
  case PROP_LABEL_URI:
    str = g_value_get_string (value);
    if (self->is_loading && !ephy_embed_utils_is_no_show_address (str)) {
      gtk_label_set_text (GTK_LABEL (self->label), str);
      gtk_widget_set_tooltip_text (GTK_WIDGET (self), str);
    }
    break;
  case PROP_ICON_BUF:
    gtk_image_set_from_pixbuf (GTK_IMAGE (self->icon), g_value_get_object(value));
    break;
  case PROP_SPINNING:
    ephy_tab_label_set_spinning (self, g_value_get_boolean(value));
    break;
  case PROP_AUDIO:
    gtk_widget_set_visible (self->audio_button, g_value_get_boolean(value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
ephy_tab_label_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  EphyTabLabel *self = EPHY_TAB_LABEL (object);
  gboolean spinning = FALSE;

  switch (prop_id) {
  case PROP_LABEL_TEXT:
    g_value_set_string (value, gtk_label_get_text (GTK_LABEL (self->label)));
    break;
  case PROP_ICON_BUF:
    g_value_set_object (value, gtk_image_get_pixbuf (GTK_IMAGE (self->icon)));
    break;
  case PROP_SPINNING:
    g_object_get (self->spinner, "active", &spinning, NULL);
    g_value_set_boolean (value, spinning);
    break;
  case PROP_AUDIO:
    g_value_set_boolean (value, gtk_widget_get_visible (self->audio_button));
    break;
  default:
   G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
   break;
  }
}

static void
ephy_tab_label_init (EphyTabLabel *self)
{
   gtk_widget_init_template (GTK_WIDGET (self));
}

static void
close_button_clicked_cb (GtkWidget     *widget,
                         EphyTabLabel  *tab_label)
{
  g_signal_emit (tab_label, signals[CLOSE_CLICKED], 0, NULL);
}

static void
style_updated_cb (GtkWidget *widget,
                  gpointer   user_data)
{
  PangoFontMetrics *metrics;
  PangoContext *context;
  GtkStyleContext *style;
  PangoFontDescription *font_desc;
  EphyTabLabel *self = EPHY_TAB_LABEL (widget);
  gboolean expanded;
  int char_width, h, w;

  if (self->is_pinned) {
    gtk_widget_set_hexpand (self->icon, FALSE);
    gtk_widget_set_halign (self->icon, GTK_ALIGN_FILL);
    gtk_widget_set_size_request (widget, -1, -1);
    return;
  }

  context = gtk_widget_get_pango_context (widget);
  style = gtk_widget_get_style_context (widget);
  gtk_style_context_get (style, gtk_style_context_get_state (style), "font", &font_desc, NULL);
  metrics = pango_context_get_metrics (context, font_desc, pango_context_get_language (context));
  pango_font_description_free (font_desc);
  char_width = pango_font_metrics_get_approximate_digit_width (metrics);
  pango_font_metrics_unref (metrics);

  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);

  gtk_widget_set_size_request (widget, TAB_WIDTH_N_CHARS * PANGO_PIXELS (char_width) + 2 * w, -1);

  gtk_widget_set_size_request (self->close_button, w + 2, h + 2);

  expanded = g_settings_get_boolean (EPHY_SETTINGS_UI, EPHY_PREFS_UI_EXPAND_TABS_BAR);
  gtk_widget_set_hexpand (self->icon, expanded);
  gtk_widget_set_halign (self->icon, expanded ? GTK_ALIGN_END : GTK_ALIGN_FILL);
}

static void
ephy_tab_label_class_init (EphyTabLabelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = ephy_tab_label_set_property;
  object_class->get_property = ephy_tab_label_get_property;

  obj_properties[PROP_LABEL_TEXT] = g_param_spec_string ("label-text",
                                                         "Label Text",
                                                         "The displayed text",
                                                         _("New Tab"),
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT);

  obj_properties[PROP_LABEL_URI] = g_param_spec_string ("label-uri",
                                                        "Label URI",
                                                        "The displayed uri",
                                                        "",
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT);

  obj_properties[PROP_ICON_BUF] = g_param_spec_object ("icon-buf",
                                                       "Icon Buffer",
                                                       "Buffer of the icon to be displayed",
                                                       GDK_TYPE_PIXBUF,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_CONSTRUCT);
  obj_properties[PROP_SPINNING] = g_param_spec_boolean ("spinning",
                                                        "Spinning",
                                                        "Is the spinner spinning",
                                                        FALSE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT);
  obj_properties[PROP_AUDIO] = g_param_spec_boolean ("audio",
                                                     "Audio",
                                                     "Is audio playing",
                                                     FALSE,
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_CONSTRUCT);
  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  signals[CLOSE_CLICKED] = g_signal_new ("close-clicked",
                                         EPHY_TYPE_TAB_LABEL,
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL, NULL,
                                         NULL,
                                         G_TYPE_NONE,
                                         0);

  /* Bind class to template */
  gtk_widget_class_set_template_from_resource (widget_class,  "/org/gnome/epiphany/gtk/tab-label.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyTabLabel, spinner);
  gtk_widget_class_bind_template_child (widget_class, EphyTabLabel, icon);
  gtk_widget_class_bind_template_child (widget_class, EphyTabLabel, label);
  gtk_widget_class_bind_template_child (widget_class, EphyTabLabel, audio_button);
  gtk_widget_class_bind_template_child (widget_class, EphyTabLabel, close_button);

  gtk_widget_class_bind_template_callback (widget_class, close_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, style_updated_cb);
}

GtkWidget *
ephy_tab_label_new (void)
{
  return g_object_new (EPHY_TYPE_TAB_LABEL, "homogeneous", FALSE, NULL);
}

const gchar *
ephy_tab_label_get_text (GtkWidget *widget)
{
  EphyTabLabel *self = EPHY_TAB_LABEL (widget);

  return gtk_label_get_text (GTK_LABEL (self->label));
}

static void
update_label (EphyTabLabel *self)
{
  gtk_widget_set_visible (self->close_button, !self->is_pinned);
  gtk_widget_set_visible (self->label, !self->is_pinned);
  gtk_widget_set_halign (GTK_WIDGET (self), self->is_pinned ? GTK_ALIGN_CENTER : GTK_ALIGN_FILL);
  g_signal_emit_by_name (self, "style-updated", G_TYPE_NONE);
}

void
ephy_tab_label_set_pinned (GtkWidget *widget,
                           gboolean   is_pinned)
{
  EphyTabLabel *self = EPHY_TAB_LABEL (widget);

  self->is_pinned = is_pinned;
  update_label (self);
}

gboolean
ephy_tab_label_is_pinned (GtkWidget *widget)
{
  EphyTabLabel *self = EPHY_TAB_LABEL (widget);

  return self->is_pinned;
}
