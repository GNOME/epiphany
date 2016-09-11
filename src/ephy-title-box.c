/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2013, 2014 Yosef Or Boczko <yoseforb@gnome.org>
 *  Copyright © 2016 Igalia S.L.
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "ephy-title-box.h"

#include "ephy-certificate-dialog.h"
#include "ephy-debug.h"
#include "ephy-private.h"
#include "ephy-type-builtins.h"

enum {
  LOCK_CLICKED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _EphyTitleBox {
  GtkBox parent_instance;

  GtkWidget *lock_image;
  GtkWidget *title;
  GtkWidget *subtitle;

  GBinding *title_binding;
};

G_DEFINE_TYPE (EphyTitleBox, ephy_title_box, GTK_TYPE_BOX)

static void
ephy_title_box_constructed (GObject *object)
{
  EphyTitleBox *title_box = EPHY_TITLE_BOX (object);
  GtkStyleContext *context;
  GtkWidget *hbox;

  LOG ("EphyTitleBox constructed");

  G_OBJECT_CLASS (ephy_title_box_parent_class)->constructed (object);

  gtk_widget_add_events (GTK_WIDGET (title_box), GDK_BUTTON_PRESS_MASK);

  title_box->title = gtk_label_new (NULL);
  gtk_widget_show (title_box->title);
  context = gtk_widget_get_style_context (title_box->title);
  gtk_style_context_add_class (context, "title");
  gtk_label_set_line_wrap (GTK_LABEL (title_box->title), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (title_box->title), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (title_box->title), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start (GTK_BOX (title_box), title_box->title, FALSE, FALSE, 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  context = gtk_widget_get_style_context (hbox);
  gtk_style_context_add_class (context, "subtitle");
  gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (hbox, GTK_ALIGN_BASELINE);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (title_box), hbox, FALSE, FALSE, 0);

  title_box->lock_image = gtk_image_new_from_icon_name ("channel-secure-symbolic", GTK_ICON_SIZE_MENU);
  gtk_widget_set_valign (title_box->lock_image, GTK_ALIGN_BASELINE);
  gtk_box_pack_start (GTK_BOX (hbox), title_box->lock_image, FALSE, FALSE, 0);

  title_box->subtitle = gtk_label_new (NULL);
  gtk_widget_set_valign (title_box->subtitle, GTK_ALIGN_BASELINE);
  gtk_widget_show (title_box->subtitle);
  gtk_label_set_line_wrap (GTK_LABEL (title_box->subtitle), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (title_box->subtitle), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (title_box->subtitle), PANGO_ELLIPSIZE_END);
  gtk_label_set_selectable (GTK_LABEL (title_box->subtitle), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), title_box->subtitle, FALSE, FALSE, 0);

  gtk_widget_show (GTK_WIDGET (title_box));
}

static gboolean
ephy_title_box_button_press_event (GtkWidget      *widget,
                                   GdkEventButton *event)
{
  EphyTitleBox *title_box = EPHY_TITLE_BOX (widget);
  GtkAllocation lock_allocation;

  if (event->button != GDK_BUTTON_PRIMARY)
    return GDK_EVENT_PROPAGATE;

  LOG ("button-press-event title-box %p event %p", title_box, event);

  gtk_widget_get_allocation (title_box->lock_image, &lock_allocation);

  if (event->x >= lock_allocation.x &&
      event->x < lock_allocation.x + lock_allocation.width &&
      event->y >= lock_allocation.y &&
      event->y < lock_allocation.y + lock_allocation.height) {
    g_signal_emit (title_box, signals[LOCK_CLICKED], 0, (GdkRectangle *)&lock_allocation);
  }

  return GDK_EVENT_PROPAGATE;
}

static void
ephy_title_box_dispose (GObject *object)
{
  EphyTitleBox *title_box = EPHY_TITLE_BOX (object);

  LOG ("EphyTitleBox dispose %p", title_box);

  ephy_title_box_set_web_view (title_box, NULL);

  G_OBJECT_CLASS (ephy_title_box_parent_class)->dispose (object);
}

static void
ephy_title_box_get_preferred_width (GtkWidget *widget,
                                    gint      *minimum_width,
                                    gint      *natural_width)
{
  if (minimum_width)
    *minimum_width = -1;

  if (natural_width)
    *natural_width = 860;
}

static void
ephy_title_box_class_init (EphyTitleBoxClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ephy_title_box_dispose;
  object_class->constructed = ephy_title_box_constructed;
  widget_class->button_press_event = ephy_title_box_button_press_event;
  widget_class->get_preferred_width = ephy_title_box_get_preferred_width;

  /**
   * EphyTitleBox::lock-clicked:
   * @title_box: the object on which the signal is emitted
   * @lock_position: the position of the lock icon
   *
   * Emitted when the user clicks the security icon inside the
   * #EphyTitleBox.
   */
  signals[LOCK_CLICKED] = g_signal_new ("lock-clicked",
                                        EPHY_TYPE_TITLE_BOX,
                                        G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                                        0, NULL, NULL, NULL,
                                        G_TYPE_NONE,
                                        1,
                                        GDK_TYPE_RECTANGLE | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
ephy_title_box_init (EphyTitleBox *title_box)
{
  LOG ("EphyTitleBox initialising %p", title_box);
}

/**
 * ephy_title_box_new:
 *
 * Creates a new #EphyTitleBox.
 *
 * Returns: a new #EphyTitleBox
 **/
EphyTitleBox *
ephy_title_box_new (void)
{
  return g_object_new (EPHY_TYPE_TITLE_BOX,
                       "orientation", GTK_ORIENTATION_VERTICAL,
                       "valign", GTK_ALIGN_CENTER,
                       NULL);
}

/**
 * ephy_title_box_set_web_view:
 * @title_box: an #EphyTitleBox
 * @web_view: a #WebKitWebView
 *
 * Sets the web view of the @title_box.
 **/
void
ephy_title_box_set_web_view (EphyTitleBox  *title_box,
                             WebKitWebView *web_view)
{
  g_return_if_fail (EPHY_IS_TITLE_BOX (title_box));

  LOG ("ephy_title_box_set_web_view title-box %p web_view %p", title_box, web_view);

  g_clear_object (&title_box->title_binding);

  if (web_view) {
    title_box->title_binding = g_object_bind_property (web_view, "title",
                                                       title_box->title, "label",
                                                       G_BINDING_SYNC_CREATE);
  }
}

/**
 * ephy_title_box_set_security_level:
 * @title_box: an #EphyTitleBox
 * @mode: an #EphySecurityLevel
 *
 * Sets the lock icon to be displayed by the title box and location entry
 **/
void
ephy_title_box_set_security_level (EphyTitleBox     *title_box,
                                   EphySecurityLevel security_level)
{
  const char *icon_name;

  g_return_if_fail (EPHY_IS_TITLE_BOX (title_box));

  icon_name = ephy_security_level_to_icon_name (security_level);

  g_object_set (title_box->lock_image,
                "icon-name", icon_name,
                NULL);

  gtk_widget_set_visible (title_box->lock_image, icon_name != NULL);
}

/**
 * ephy_title_box_set_address:
 * @title_box: an #EphyTitleBox
 * @address: (nullable): the URI to use for the subtitle of this #EphyTitleBox
 *
 * Sets the address of @title_box to @address
 */
void
ephy_title_box_set_address (EphyTitleBox *title_box,
                            const char   *address)
{
  g_return_if_fail (EPHY_IS_TITLE_BOX (title_box));

  gtk_label_set_text (GTK_LABEL (title_box->subtitle), address);
}
