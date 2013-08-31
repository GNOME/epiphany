/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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

G_DEFINE_TYPE (EphyToolbar, ephy_toolbar, GTK_TYPE_BOX)

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
close_button_clicked (GtkButton *button, gpointer data)
{
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
  gtk_window_close (GTK_WINDOW (toplevel));
}

static void
smallify_boldify_label (GtkWidget *label)
{
  PangoAttrList *attrs;

  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_MEDIUM));
  pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);

  gtk_style_context_add_class (gtk_widget_get_style_context (label), GTK_STYLE_CLASS_DIM_LABEL);
}

static void
ephy_toolbar_constructed (GObject *object)
{
  EphyToolbarPrivate *priv = EPHY_TOOLBAR (object)->priv;
  GtkActionGroup *action_group;
  GtkAction *action;
  GtkWidget *toolbar, *box, *button, *reload, *separator, *label;
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
  action = gtk_action_group_get_action (action_group, "NavigationForward");
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button),
                                      action);
  gtk_button_set_label (GTK_BUTTON (button), NULL);
  gtk_container_add (GTK_CONTAINER (box), button);

  gtk_style_context_add_class (gtk_widget_get_style_context (box),
                               "raised");
  gtk_style_context_add_class (gtk_widget_get_style_context (box),
                               "linked");

  gtk_container_add (GTK_CONTAINER (toolbar), box);
  gtk_widget_show_all (box);

  if (gtk_widget_get_direction (box) == GTK_TEXT_DIR_RTL)
    gtk_widget_set_margin_left (box, 27);
  else
    gtk_widget_set_margin_right (box, 27);

  /* Location and Reload/Stop */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_size_request (box, 530, -1);
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
  }

  gtk_box_pack_start (GTK_BOX (toolbar), box, TRUE, TRUE, 0);
  if (mode != EPHY_EMBED_SHELL_MODE_APPLICATION)
    gtk_widget_show_all (box);

  if (gtk_widget_get_direction (box) == GTK_TEXT_DIR_RTL)
    gtk_widget_set_margin_left (box, 12);
  else
    gtk_widget_set_margin_right (box, 12);

  /* New Tab */
  button = gtk_button_new ();
  /* FIXME: apparently we need an image inside the button for the action
   * icon to appear. */
  gtk_button_set_image (GTK_BUTTON (button), gtk_image_new ());
  action = gtk_action_group_get_action (action_group, "FileNewTab");
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button),
                                      action);
  gtk_button_set_label (GTK_BUTTON (button), NULL);
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  if (mode != EPHY_EMBED_SHELL_MODE_APPLICATION)
    gtk_widget_show_all (button);

  if (gtk_widget_get_direction (button) == GTK_TEXT_DIR_RTL)
    gtk_widget_set_margin_left (button, 6);
  else
    gtk_widget_set_margin_right (button, 6);

  if (gtk_widget_get_direction (button) == GTK_TEXT_DIR_RTL)
    gtk_widget_set_margin_right (button, 15);
  else
    gtk_widget_set_margin_left (button, 15);

  /* Page Menu */
  button = gtk_button_new ();
  gtk_widget_set_name (button, "ephy-page-menu-button");
  /* FIXME: apparently we need an image inside the button for the action
   * icon to appear. */
  gtk_button_set_image (GTK_BUTTON (button), gtk_image_new ());
  action = gtk_action_group_get_action (action_group, "PageMenu");
  gtk_activatable_set_related_action (GTK_ACTIVATABLE (button),
                                      action);
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  gtk_widget_show_all (button);

  /* Add title only in application mode. */
  if (mode == EPHY_EMBED_SHELL_MODE_APPLICATION)
  {
    /* The title of the window in web application - need
     * settings of padding same the location entry. */
    label = gtk_label_new (NULL);
    gtk_style_context_add_class (gtk_widget_get_style_context (label), "subtitle");
    smallify_boldify_label (label);
    gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
    gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_size_request (label, 530, -1);
    gtk_box_pack_start (GTK_BOX (toolbar), label, TRUE, TRUE, 0);
    gtk_widget_show_all (label);

    if (gtk_widget_get_direction (GTK_WIDGET (label)) == GTK_TEXT_DIR_RTL)
      gtk_widget_set_margin_left (GTK_WIDGET (label), 12);
    else
      gtk_widget_set_margin_right (GTK_WIDGET (label), 12);

    g_object_bind_property (label, "label",
                            priv->window, "title",
                            G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

    /* Reload/Stop for web application. */
    gtk_container_add (GTK_CONTAINER (toolbar), reload);
  }

  /* Separator and Close */
  separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
  gtk_container_add (GTK_CONTAINER (toolbar), separator);
  gtk_widget_show_all (separator);

  if (gtk_widget_get_direction (GTK_WIDGET (separator)) == GTK_TEXT_DIR_RTL)
    gtk_widget_set_margin_right (GTK_WIDGET (separator), 6);
  else
    gtk_widget_set_margin_left (GTK_WIDGET (separator), 6);

  button = gtk_button_new_from_icon_name ("window-close-symbolic",
                                          GTK_ICON_SIZE_MENU);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (close_button_clicked), NULL);
  gtk_container_add (GTK_CONTAINER (toolbar), button);
  gtk_widget_show_all (button);

  if (gtk_widget_get_direction (button) == GTK_TEXT_DIR_RTL)
    gtk_widget_set_margin_right (button, 6);
  else
    gtk_widget_set_margin_left (button, 6);
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
                                     "window", window,
                                     NULL));
}

GtkWidget *
ephy_toolbar_get_location_entry (EphyToolbar *toolbar)
{
  return toolbar->priv->entry;
}
