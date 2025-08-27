/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2000, 2001, 2002, 2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2012 Igalia S.L.
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
#include "ephy-encoding-dialog.h"

#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-utils.h"
#include "ephy-embed.h"
#include "ephy-encodings.h"
#include "ephy-encoding-row.h"
#include "ephy-shell.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

struct _EphyEncodingDialog {
  AdwDialog parent_instance;

  EphyEncodings *encodings;
  EphyWindow *window;
  EphyEmbed *embed;
  gboolean update_embed_tag;
  gboolean update_view_tag;
  const char *selected_encoding;

  /* from the UI file */
  GtkStack *type_stack;
  GtkSwitch *default_switch;
  GtkListBox *list_box;
  GtkListBox *recent_list_box;
  GtkListBox *related_list_box;
  GtkWidget *recent_box;
  GtkWidget *related_box;
};

enum {
  COL_TITLE_ELIDED,
  COL_ENCODING,
  NUM_COLS
};

enum {
  PROP_0,
  PROP_PARENT_WINDOW,
  LAST_PROP
};

static GParamSpec *obj_properties[LAST_PROP];

G_DEFINE_FINAL_TYPE (EphyEncodingDialog, ephy_encoding_dialog, ADW_TYPE_DIALOG)

static void
select_encoding_row (GtkListBox   *list_box,
                     EphyEncoding *encoding)
{
  const char *target_encoding;
  GtkListBoxRow *row;
  int i = 0;

  target_encoding = ephy_encoding_get_encoding (encoding);

  while ((row = gtk_list_box_get_row_at_index (list_box, i++))) {
    EphyEncodingRow *ephy_encoding_row;
    EphyEncoding *ephy_encoding;
    const char *encoding_string = NULL;

    ephy_encoding_row = EPHY_ENCODING_ROW (gtk_list_box_row_get_child (row));
    ephy_encoding = ephy_encoding_row_get_encoding (ephy_encoding_row);
    encoding_string = ephy_encoding_get_encoding (ephy_encoding);

    if (g_strcmp0 (encoding_string, target_encoding) == 0) {
      ephy_encoding_row_set_selected (ephy_encoding_row, TRUE);

      gtk_list_box_select_row (list_box, row);
      /* TODO scroll to row */

      break;
    }
  }
}

static void
clean_selected_list_box (GtkListBox *list_box)
{
  GtkListBoxRow *row;
  int i = 0;

  while ((row = gtk_list_box_get_row_at_index (list_box, i++))) {
    EphyEncodingRow *ephy_encoding_row =
      EPHY_ENCODING_ROW (gtk_list_box_row_get_child (GTK_LIST_BOX_ROW (row)));
    ephy_encoding_row_set_selected (ephy_encoding_row, FALSE);
  }
}

static void
clean_selected (EphyEncodingDialog *dialog)
{
  clean_selected_list_box (dialog->list_box);
  clean_selected_list_box (dialog->recent_list_box);
  clean_selected_list_box (dialog->related_list_box);
}

static void
sync_encoding_against_embed (EphyEncodingDialog *dialog)
{
  const char *encoding;
  gboolean is_automatic = FALSE;
  WebKitWebView *view;

  dialog->update_embed_tag = TRUE;

  g_assert (EPHY_IS_EMBED (dialog->embed));
  view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (dialog->embed);

  encoding = webkit_web_view_get_custom_charset (view);
  is_automatic = !encoding;

  clean_selected (dialog);

  if (!is_automatic) {
    EphyEncoding *node;

    node = ephy_encodings_get_encoding (dialog->encodings, encoding, TRUE);
    g_assert (EPHY_IS_ENCODING (node));

    /* Select the current encoding in the lists. */
    select_encoding_row (dialog->list_box, node);
    select_encoding_row (dialog->recent_list_box, node);
    select_encoding_row (dialog->related_list_box, node);

    /* TODO scroll the view so the active encoding is visible */
  }
  gtk_switch_set_active (dialog->default_switch, is_automatic);
  gtk_switch_set_state (dialog->default_switch, is_automatic);
  gtk_widget_set_sensitive (GTK_WIDGET (dialog->type_stack), !is_automatic);

  dialog->update_embed_tag = FALSE;
}

static void
embed_net_stop_cb (EphyWebView        *view,
                   WebKitLoadEvent     load_event,
                   EphyEncodingDialog *dialog)
{
  if (!ephy_web_view_is_loading (view))
    sync_encoding_against_embed (dialog);
}

static void
ephy_encoding_dialog_detach_embed (EphyEncodingDialog *dialog)
{
  EphyEmbed **embedptr;

  g_signal_handlers_disconnect_by_func (ephy_embed_get_web_view (dialog->embed),
                                        G_CALLBACK (embed_net_stop_cb),
                                        dialog);

  embedptr = &dialog->embed;
  g_object_remove_weak_pointer (G_OBJECT (dialog->embed),
                                (gpointer *)embedptr);
  dialog->embed = NULL;
}

static void
ephy_encoding_dialog_attach_embed (EphyEncodingDialog *dialog)
{
  EphyEmbed *embed;
  EphyEmbed **embedptr;

  embed = ephy_embed_container_get_active_child (EPHY_EMBED_CONTAINER (dialog->window));
  g_assert (EPHY_IS_EMBED (embed));

  g_signal_connect (G_OBJECT (ephy_embed_get_web_view (embed)), "load-changed",
                    G_CALLBACK (embed_net_stop_cb), dialog);

  dialog->embed = embed;

  embedptr = &dialog->embed;
  g_object_add_weak_pointer (G_OBJECT (dialog->embed),
                             (gpointer *)embedptr);
}

static void
ephy_encoding_dialog_sync_embed (EphyWindow         *window,
                                 GParamSpec         *pspec,
                                 EphyEncodingDialog *dialog)
{
  ephy_encoding_dialog_detach_embed (dialog);
  ephy_encoding_dialog_attach_embed (dialog);
  sync_encoding_against_embed (dialog);
}

static void
activate_choice (EphyEncodingDialog *dialog)
{
  WebKitWebView *view;

  g_assert (EPHY_IS_EMBED (dialog->embed));
  view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (dialog->embed);

  if (gtk_switch_get_active (dialog->default_switch)) {
    webkit_web_view_set_custom_charset (view, NULL);
  } else if (dialog->selected_encoding) {
    const char *code;

    code = dialog->selected_encoding;

    webkit_web_view_set_custom_charset (view, code);

    ephy_encodings_add_recent (dialog->encodings, code);
  }
}

static void
row_activated_cb (GtkListBox         *box,
                  GtkListBoxRow      *row,
                  EphyEncodingDialog *dialog)
{
  EphyEncodingRow *ephy_encoding_row;
  EphyEncoding *ephy_encoding;
  const char *selected_encoding;

  if (dialog->update_embed_tag || dialog->update_view_tag)
    return;

  dialog->update_view_tag = TRUE;

  ephy_encoding_row = EPHY_ENCODING_ROW (gtk_list_box_row_get_child (row));
  ephy_encoding = ephy_encoding_row_get_encoding (ephy_encoding_row);
  selected_encoding = ephy_encoding_get_encoding (ephy_encoding);

  dialog->selected_encoding = selected_encoding;

  clean_selected (dialog);
  ephy_encoding_row_set_selected (ephy_encoding_row, TRUE);

  activate_choice (dialog);

  dialog->update_view_tag = FALSE;
}

static gboolean
default_switch_toggled_cb (GtkSwitch          *default_switch,
                           gboolean            state,
                           EphyEncodingDialog *dialog)
{
  if (dialog->update_embed_tag || dialog->update_view_tag) {
    gtk_switch_set_state (default_switch, !state);              /* cancel switch change */
    return TRUE;
  }

  dialog->update_view_tag = TRUE;

  gtk_switch_set_active (default_switch, state);
  gtk_switch_set_state (default_switch, state);

  /* TODO if state == false && selected_encoding == NULL, select safe default in list, or find another solution */
  if (state)
    clean_selected (dialog);
  activate_choice (dialog);

  dialog->update_view_tag = FALSE;

  return TRUE;
}

static void
show_all_button_clicked_cb (GtkButton          *show_all_button,
                            EphyEncodingDialog *dialog)
{
  gtk_stack_set_visible_child_name (dialog->type_stack, "scrolled-window");
}

static gint
sort_list_store (gconstpointer a,
                 gconstpointer b,
                 gpointer      user_data)
{
  const char *encoding1 = ephy_encoding_get_title_elided ((EphyEncoding *)a);
  const char *encoding2 = ephy_encoding_get_title_elided ((EphyEncoding *)b);

  return g_strcmp0 (encoding1, encoding2);
}

static GtkWidget *
create_list_box_row (gpointer object,
                     gpointer user_data)
{
  return GTK_WIDGET (ephy_encoding_row_new (EPHY_ENCODING (object)));
}

static void
add_list_item (EphyEncoding *encoding,
               GtkListBox   *list_box)
{
  gtk_list_box_append (GTK_LIST_BOX (list_box), GTK_WIDGET (ephy_encoding_row_new (encoding)));
}

static int
sort_encodings (gconstpointer a,
                gconstpointer b)
{
  EphyEncoding *enc1 = (EphyEncoding *)a;
  EphyEncoding *enc2 = (EphyEncoding *)b;
  const char *key1, *key2;

  key1 = ephy_encoding_get_collation_key (enc1);
  key2 = ephy_encoding_get_collation_key (enc2);

  return strcmp (key1, key2);
}

static void
ephy_encoding_dialog_init (EphyEncodingDialog *dialog)
{
  GList *encodings, *p;
  GListStore *store;

  gtk_widget_init_template (GTK_WIDGET (dialog));

  dialog->update_embed_tag = FALSE;
  dialog->update_view_tag = FALSE;

  dialog->encodings = ephy_embed_shell_get_encodings (EPHY_EMBED_SHELL (ephy_shell_get_default ()));

  encodings = ephy_encodings_get_all (dialog->encodings);

  store = g_list_store_new (EPHY_TYPE_ENCODING);
  for (p = encodings; p; p = p->next) {
    EphyEncoding *encoding = EPHY_ENCODING (p->data);
    g_list_store_insert_sorted (store, encoding, sort_list_store, NULL);
  }
  g_list_free (encodings);

  gtk_list_box_bind_model (dialog->list_box, G_LIST_MODEL (store),
                           create_list_box_row,
                           NULL, NULL);
}

static void
ephy_encoding_dialog_constructed (GObject *object)
{
  EphyEncodingDialog *dialog;
  WebKitWebView *view;
  EphyEncoding *enc_node;
  EphyLanguageGroup groups;
  GList *recent;
  GList *related = NULL;

  /* selected encoding */
  dialog = EPHY_ENCODING_DIALOG (object);

  g_assert (EPHY_IS_EMBED (dialog->embed));
  view = EPHY_GET_WEBKIT_WEB_VIEW_FROM_EMBED (dialog->embed);

  dialog->selected_encoding = webkit_web_view_get_custom_charset (view);

  g_object_bind_property (dialog->default_switch, "active", dialog->type_stack, "sensitive", G_BINDING_INVERT_BOOLEAN);

  /* recent */
  recent = ephy_encodings_get_recent (dialog->encodings);
  if (recent) {
    recent = g_list_sort (recent, (GCompareFunc)sort_encodings);
    g_list_foreach (recent, (GFunc)add_list_item, dialog->recent_list_box);
  } else
    gtk_widget_set_visible (dialog->recent_box, FALSE);

  /* related */
  if (dialog->selected_encoding) {
    enc_node = ephy_encodings_get_encoding (dialog->encodings, dialog->selected_encoding, TRUE);
    g_assert (EPHY_IS_ENCODING (enc_node));
    groups = ephy_encoding_get_language_groups (enc_node);

    related = ephy_encodings_get_encodings (dialog->encodings, groups);
  }
  if (related) {
    related = g_list_sort (related, (GCompareFunc)sort_encodings);
    g_list_foreach (related, (GFunc)add_list_item, dialog->related_list_box);
  } else
    gtk_widget_set_visible (dialog->related_box, FALSE);

  /* update list_boxes */
  sync_encoding_against_embed (dialog);

  /* chaining */
  G_OBJECT_CLASS (ephy_encoding_dialog_parent_class)->constructed (object);
}

static void
ephy_encoding_dialog_dispose (GObject *object)
{
  EphyEncodingDialog *dialog = EPHY_ENCODING_DIALOG (object);

  g_signal_handlers_disconnect_by_func (dialog->window,
                                        G_CALLBACK (ephy_encoding_dialog_sync_embed),
                                        dialog);

  if (dialog->embed)
    ephy_encoding_dialog_detach_embed (dialog);

  G_OBJECT_CLASS (ephy_encoding_dialog_parent_class)->dispose (object);
}

static void
ephy_encoding_dialog_set_parent_window (EphyEncodingDialog *dialog,
                                        EphyWindow         *window)
{
  g_assert (EPHY_IS_WINDOW (window));

  g_signal_connect (G_OBJECT (window), "notify::active-child",
                    G_CALLBACK (ephy_encoding_dialog_sync_embed), dialog);

  dialog->window = window;

  ephy_encoding_dialog_attach_embed (dialog);
}

static void
ephy_encoding_dialog_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  switch (prop_id) {
    case PROP_PARENT_WINDOW:
      ephy_encoding_dialog_set_parent_window (EPHY_ENCODING_DIALOG (object),
                                              g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_encoding_dialog_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  switch (prop_id) {
    case PROP_PARENT_WINDOW:
      g_value_set_object (value, EPHY_ENCODING_DIALOG (object)->window);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
ephy_encoding_dialog_class_init (EphyEncodingDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  /* class creation */
  object_class->constructed = ephy_encoding_dialog_constructed;
  object_class->set_property = ephy_encoding_dialog_set_property;
  object_class->get_property = ephy_encoding_dialog_get_property;
  object_class->dispose = ephy_encoding_dialog_dispose;

  obj_properties[PROP_PARENT_WINDOW] =
    g_param_spec_object ("parent-window",
                         NULL, NULL,
                         EPHY_TYPE_WINDOW,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, obj_properties);

  /* load from UI file */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/epiphany/gtk/encoding-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, EphyEncodingDialog, type_stack);
  gtk_widget_class_bind_template_child (widget_class, EphyEncodingDialog, default_switch);
  gtk_widget_class_bind_template_child (widget_class, EphyEncodingDialog, list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyEncodingDialog, recent_list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyEncodingDialog, related_list_box);
  gtk_widget_class_bind_template_child (widget_class, EphyEncodingDialog, recent_box);
  gtk_widget_class_bind_template_child (widget_class, EphyEncodingDialog, related_box);

  gtk_widget_class_bind_template_callback (widget_class, default_switch_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, show_all_button_clicked_cb);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);
}

EphyEncodingDialog *
ephy_encoding_dialog_new (EphyWindow *parent)
{
  return g_object_new (EPHY_TYPE_ENCODING_DIALOG,
                       "parent-window", parent,
                       NULL);
}
