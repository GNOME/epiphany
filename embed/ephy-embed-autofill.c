/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set sw=2 ts=2 sts=2 et: */
/*
 *  Copyright © 2019 Abdullah Alansari
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
#include "ephy-embed-autofill.h"

#include "ephy-autofill-fill-choice.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

typedef struct
{
  EphyAutofillFillChoice fill_choice;
  EphyWebView *view;
  GtkPopover *popover;
  char *selector;
} FillButtonMessage;

static void
autofill_popover_destroy_cb (GtkWidget *popover,
                             gpointer user_data)
{
  FillButtonMessage *message = user_data;

  g_free (message->selector);
  g_slice_free (FillButtonMessage, message);
}

static void
autofill_fill_button_clicked_cb (GtkButton *button,
                                 gpointer user_data)
{
  FillButtonMessage *message = user_data;

  ephy_web_view_autofill (message->view, g_strdup (message->selector), message->fill_choice);
  gtk_widget_destroy (GTK_WIDGET (message->popover));
}

static void
add_fill_button (const char *label,
                 GtkPopover *popover,
                 GtkBox *box,
                 EphyWebView *view,
                 const char *selector,
                 EphyAutofillFillChoice fill_choice)
{
  FillButtonMessage *message = g_slice_new (FillButtonMessage);
  GtkWidget *button = gtk_button_new_with_label (label);

  message->popover = popover;
  message->view = view;
  message->selector = g_strdup (selector);
  message->fill_choice = fill_choice;

  g_signal_connect (popover, "destroy", G_CALLBACK (autofill_popover_destroy_cb), message);
  g_signal_connect (button, "clicked", G_CALLBACK (autofill_fill_button_clicked_cb), message);

  gtk_container_add (GTK_CONTAINER (box), button);
}

typedef struct
{
  EphyWebView *view;
  GtkPopover *popover;
} NeverShowButtonMessage;

static void
autofill_never_show_clicked_cb (GtkButton *button,
                                gpointer user_data)
{
  NeverShowButtonMessage *message = user_data;

  ephy_web_view_autofill_disable_popup (message->view);
  gtk_widget_destroy (GTK_WIDGET (message->popover));

  g_slice_free (NeverShowButtonMessage, message);
}

void
ephy_embed_autofill_signal_received_cb (EphyEmbedShell *shell,
                                        unsigned long page_id,
                                        const char *css_selector,
                                        bool is_fillable_element,
                                        bool has_personal_fields,
                                        bool has_card_fields,
                                        unsigned long element_x,
                                        unsigned long element_y,
                                        unsigned long element_width,
                                        unsigned long element_height,
                                        EphyWebView *view)
{
  GdkRectangle rectangle;
  GtkWidget *popover;
  GtkWidget *box;
  GtkWidget *never_show_again_btn;
  NeverShowButtonMessage *message;

  if (webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (view)) != page_id ||
      !ephy_web_view_autofill_popup_enabled (view))
    return;

  rectangle.x = element_x;
  rectangle.y = element_y;
  rectangle.width = element_width;
  rectangle.height = element_height;

  popover = gtk_popover_new (GTK_WIDGET (view));
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 7);
  never_show_again_btn = gtk_button_new_with_label (_("Do not autofill"));
  message = g_slice_new (NeverShowButtonMessage);

  message->view = view;
  message->popover = GTK_POPOVER (popover);

  gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rectangle);
  gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);

  if (has_card_fields) {
    add_fill_button (_("Fill all with personal and credit card data"), GTK_POPOVER (popover), GTK_BOX (box),
                view, css_selector, EPHY_AUTOFILL_FILL_CHOICE_FORM_ALL);
  }

  if (has_personal_fields) {
    add_fill_button (_("Fill all with personal data"), GTK_POPOVER (popover), GTK_BOX (box),
                view, css_selector, EPHY_AUTOFILL_FILL_CHOICE_FORM_PERSONAL);
  }

  if (is_fillable_element) {
    add_fill_button (_("Fill this"), GTK_POPOVER (popover), GTK_BOX (box),
                view, css_selector, EPHY_AUTOFILL_FILL_CHOICE_ELEMENT);
  }

  g_signal_connect (never_show_again_btn, "clicked", G_CALLBACK (autofill_never_show_clicked_cb), message);

  gtk_container_add (GTK_CONTAINER (box), never_show_again_btn);
  gtk_container_add (GTK_CONTAINER (popover), box);

  gtk_widget_show_all (popover);
}
