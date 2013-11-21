/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L
 *  Copyright © 2013 Yosef Or Boczko <yoseforb@gmail.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-toolbar.h"

#include "ephy-location-entry.h"
#include "ephy-middle-clickable-button.h"
#include "ephy-private.h"

G_DEFINE_TYPE (EphyToolbar, ephy_toolbar, GTK_TYPE_HEADER_BAR)

#define EPHY_TOOLBAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), EPHY_TYPE_TOOLBAR, EphyToolbarPrivate))

enum {
  PROP_0,
  PROP_WINDOW,
  N_PROPERTIES
};

static GParamSpec *object_properties[N_PROPERTIES] = { NULL, };

struct _EphyToolbarPrivate {
  EphyWindow *window;
  GtkWidget *entry;
};

static void
ephy_toolbar_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
  EphyToolbarPrivate *priv = EPHY_TOOLBAR (object)->priv;

  switch (property_id) {
  case PROP_WINDOW:
    priv->window = EPHY_WINDOW (g_value_get_object (value));
    g_object_notify_by_pspec (object, object_properties[PROP_WINDOW]);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ephy_toolbar_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
  EphyToolbarPrivate *priv = EPHY_TOOLBAR (object)->priv;

  switch (property_id) {
  case PROP_WINDOW:
    g_value_set_object (value, priv->window);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
ephy_toolbar_constructed (GObject *object)
{
  EphyToolbarPrivate *priv = EPHY_TOOLBAR (object)->priv;
  GtkActionGroup *action_group;
  GtkAction *action;
  GtkWidget *toolbar, *box, *button, *reload, *label;
  GtkStyleContext *context;
  GtkSizeGroup *size;
  EphyEmbedShellMode mode;

  G_OBJECT_CLASS (ephy_toolbar_parent_class)->constructed (object);

  toolbar = GTK_WIDGET (object);

  mode = ephy_embed_shell_get_mode (ephy_embed_shell_get_default ());

  /* Back and Forward */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  /* Back */
  button = ephy_middle_clickable_button_new ();
  /* FIXME: apparently we need an image inside the button for the action
   * icon to appear. */
  gtk_button_set_image (GTK_BUTTON (button), gtk_image_new ());
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  action_group = ephy_window_get_toolbar_action_group (priv->window);
  action = gtk_action_group_get_action (action_group, "NavigationBack");
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button),
                                      action);
  gtk_button_set_label (GTK_BUTTON (button), NULL);
  gtk_container_add (GTK_CONTAINER (box), button);

  /* Forward */
  button = ephy_middle_clickable_button_new ();
  /* FIXME: apparently we need an image inside the button for the action
   * icon to appear. */
  gtk_button_set_image (GTK_BUTTON (button), gtk_image_new ());
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  action = gtk_action_group_get_action (action_group, "NavigationForward");
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button),
                                      action);
  gtk_button_set_label (GTK_BUTTON (button), NULL);
  gtk_container_add (GTK_CONTAINER (box), button);

  gtk_style_context_add_class (gtk_widget_get_style_context (box),
                               "raised");
  gtk_style_context_add_class (gtk_widget_get_style_context (box),
                               "linked");

  gtk_header_bar_pack_start (GTK_HEADER_BAR (toolbar), box);
  gtk_widget_show_all (box);

  /* Location and Reload/Stop */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign (box, GTK_ALIGN_CENTER);

  /* Location */
  priv->entry = ephy_location_entry_new ();
  gtk_box_pack_start (GTK_BOX (box), priv->entry, TRUE, TRUE, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (box),
                               "location-entry");

  /* Reload/Stop */
  reload = gtk_button_new ();
  /* FIXME: apparently we need an image inside the button for the action
   * icon to appear. */
  gtk_button_set_image (GTK_BUTTON (reload), gtk_image_new ());
  gtk_widget_set_valign (reload, GTK_ALIGN_CENTER);
  action = gtk_action_group_get_action (action_group, "ViewCombinedStopReload");
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (reload),
                                      action);


  if (mode != EPHY_EMBED_SHELL_MODE_APPLICATION)
  {
    gtk_container_add (GTK_CONTAINER (box), reload);

    /* Create a GtkSizeGroup to sync the height of the location entry, and
     * the stop/reload button. */
    size = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

    gtk_size_group_add_widget (size, reload);
    gtk_size_group_add_widget (size, priv->entry);
    g_object_unref (size);

    gtk_header_bar_set_custom_title (GTK_HEADER_BAR (toolbar), box);
    gtk_widget_set_margin_start (box, 27);
    gtk_widget_set_margin_end (box, 27);
    gtk_widget_show_all (box);
  }
  else
  {
    gtk_container_add (GTK_CONTAINER (toolbar), box);
  }

  /* New Tab */
  button = gtk_button_new ();
  /* FIXME: apparently we need an image inside the button for the action
   * icon to appear. */
  gtk_button_set_image (GTK_BUTTON (button), gtk_image_new ());
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  action = gtk_action_group_get_action (action_group, "FileNewTab");
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button),
                                      action);
  gtk_button_set_label (GTK_BUTTON (button), NULL);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (toolbar), button);
  if (mode != EPHY_EMBED_SHELL_MODE_APPLICATION)
    gtk_widget_show_all (button);

  /* Page Menu */
  button = gtk_button_new ();
  gtk_widget_set_name (button, "ephy-page-menu-button");
  /* FIXME: apparently we need an image inside the button for the action
   * icon to appear. */
  gtk_button_set_image (GTK_BUTTON (button), gtk_image_new ());
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  action = gtk_action_group_get_action (action_group, "PageMenu");
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button),
                                      action);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (toolbar), button);
  gtk_widget_show_all (button);

  /* Add title only in application mode. */
  if (mode == EPHY_EMBED_SHELL_MODE_APPLICATION)
  {
    /* The title of the window in web application - need
     * settings of padding same the location entry. */
    label = gtk_label_new (NULL);
    context = gtk_widget_get_style_context (label);
    gtk_style_context_add_class (context, "title");
    gtk_style_context_add_class (context, "dim-label");
    gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
    gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_size_request (label, 530, -1);
    gtk_header_bar_set_custom_title (GTK_HEADER_BAR (toolbar), label);

    g_object_bind_property (label, "label",
                            priv->window, "title",
                            G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

    /* Reload/Stop for web application. */
    gtk_header_bar_pack_end (GTK_HEADER_BAR (toolbar), reload);
  }
}

static void
ephy_toolbar_class_init (EphyToolbarClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->set_property = ephy_toolbar_set_property;
    gobject_class->get_property = ephy_toolbar_get_property;
    gobject_class->constructed = ephy_toolbar_constructed;

    object_properties[PROP_WINDOW] =
      g_param_spec_object ("window",
                           "Window",
                           "The toolbar's EphyWindow",
                           EPHY_TYPE_WINDOW,
                           G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties (gobject_class,
                                       N_PROPERTIES,
                                       object_properties);

    g_type_class_add_private (klass, sizeof (EphyToolbarPrivate));
}

static void
ephy_toolbar_init (EphyToolbar *toolbar)
{
  toolbar->priv = EPHY_TOOLBAR_GET_PRIVATE (toolbar);
}

GtkWidget*
ephy_toolbar_new (EphyWindow *window)
{
    g_return_val_if_fail (EPHY_IS_WINDOW (window), NULL);

    return GTK_WIDGET (g_object_new (EPHY_TYPE_TOOLBAR,
                                     "show-close-button", TRUE,
                                     "window", window,
                                     NULL));
}

GtkWidget *
ephy_toolbar_get_location_entry (EphyToolbar *toolbar)
{
  return toolbar->priv->entry;
}
