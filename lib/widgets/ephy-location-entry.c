/*
 *  Copyright (C) 2002  Ricardo Fernández Pascual
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "ephy-location-entry.h"
#include "ephy-autocompletion-window.h"
#include "ephy-marshal.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-debug.h"

#include <gtk/gtkentry.h>
#include <gtk/gtkwindow.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <libgnomeui/gnome-entry.h>
#include <string.h>

/**
 * Private data
 */
struct _EphyLocationEntryPrivate {
	GtkWidget *combo;
	GtkWidget *entry;
	gchar *before_completion;
	EphyAutocompletion *autocompletion;
	EphyAutocompletionWindow *autocompletion_window;
	gboolean autocompletion_window_visible;
	gint autocompletion_timeout;
	gint show_alternatives_timeout;
	gboolean block_set_autocompletion_key;
	gboolean going_to_site;
	gboolean user_changed;

	gchar *autocompletion_key;
	gchar *last_completion;
	char *last_action_target;
};

#define AUTOCOMPLETION_DELAY 10
#define SHOW_ALTERNATIVES_DELAY 100

/**
 * Private functions, only availble from this file
 */
static void		ephy_location_entry_class_init		(EphyLocationEntryClass *klass);
static void		ephy_location_entry_init		(EphyLocationEntry *w);
static void		ephy_location_entry_finalize_impl	(GObject *o);
static void		ephy_location_entry_build		(EphyLocationEntry *w);
static gboolean		ephy_location_entry_key_press_event_cb  (GtkWidget *entry, GdkEventKey *event,
								 EphyLocationEntry *w);
static void		ephy_location_entry_activate_cb		(GtkEntry *entry,
								 EphyLocationEntry *w);
static void		ephy_location_entry_autocompletion_sources_changed_cb (EphyAutocompletion *aw,
									       EphyLocationEntry *w);
static gint		ephy_location_entry_autocompletion_show_alternatives_to (EphyLocationEntry *w);
static void		ephy_location_entry_autocompletion_window_url_activated_cb
/***/								(EphyAutocompletionWindow *aw,
								 const gchar *target,
								 int action,
								 EphyLocationEntry *w);
static void		ephy_location_entry_list_event_after_cb (GtkWidget *list,
								 GdkEvent *event,
								 EphyLocationEntry *e);
static void		ephy_location_entry_editable_changed_cb (GtkEditable *editable,
								 EphyLocationEntry *e);
static void		ephy_location_entry_set_autocompletion_key (EphyLocationEntry *e);
static void		ephy_location_entry_autocompletion_show_alternatives (EphyLocationEntry *w);
static void		ephy_location_entry_autocompletion_hide_alternatives (EphyLocationEntry *w);
static void		ephy_location_entry_autocompletion_window_hidden_cb (EphyAutocompletionWindow *aw,
									     EphyLocationEntry *w);
static void
insert_text_cb (GtkWidget *editable,
		char *new_text,
                int new_text_length,
                int *position,
		EphyLocationEntry *w);
static void
delete_text_cb (GtkWidget *editable,
                int start_pos,
                int end_pos,
		EphyLocationEntry *w);

static gpointer gtk_hbox_class;

/**
 * Signals enums and ids
 */
enum EphyLocationEntrySignalsEnum {
	ACTIVATED,
	FINISHED,
	USER_CHANGED,
	LAST_SIGNAL
};
static gint EphyLocationEntrySignals[LAST_SIGNAL];

GType
ephy_location_entry_get_type (void)
{
	static GType ephy_location_entry_type = 0;

	if (ephy_location_entry_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyLocationEntryClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_location_entry_class_init,
			NULL,
			NULL,
			sizeof (EphyLocationEntry),
			0,
			(GInstanceInitFunc) ephy_location_entry_init
		};

		ephy_location_entry_type = g_type_register_static (GTK_TYPE_HBOX,
							           "EphyLocationEntry",
							           &our_info, 0);
	}

	return ephy_location_entry_type;
}

static void
ephy_location_entry_class_init (EphyLocationEntryClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = ephy_location_entry_finalize_impl;
	gtk_hbox_class = g_type_class_peek_parent (klass);

	EphyLocationEntrySignals[ACTIVATED] = g_signal_new (
		"activated", G_OBJECT_CLASS_TYPE (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                G_STRUCT_OFFSET (EphyLocationEntryClass, activated),
		NULL, NULL,
		ephy_marshal_VOID__STRING_STRING,
		G_TYPE_NONE,
		2,
		G_TYPE_STRING,
		G_TYPE_STRING);
	EphyLocationEntrySignals[FINISHED] = g_signal_new (
		"finished", G_OBJECT_CLASS_TYPE (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                G_STRUCT_OFFSET (EphyLocationEntryClass, finished),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0,
		G_TYPE_NONE);
	EphyLocationEntrySignals[USER_CHANGED] = g_signal_new (
		"user_changed", G_OBJECT_CLASS_TYPE (klass),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                G_STRUCT_OFFSET (EphyLocationEntryClass, user_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0,
		G_TYPE_NONE);
}

static gboolean
location_focus_out_cb (GtkWidget *widget, GdkEventFocus *event, EphyLocationEntry *w)
{
	g_signal_emit (w, EphyLocationEntrySignals[FINISHED], 0);

	return FALSE;
}

static void
ephy_location_entry_init (EphyLocationEntry *w)
{
	EphyLocationEntryPrivate *p = g_new0 (EphyLocationEntryPrivate, 1);
	w->priv = p;
	p->last_action_target = NULL;
	p->before_completion = NULL;
	p->user_changed = TRUE;

	ephy_location_entry_build (w);

	g_signal_connect (w->priv->entry,
			  "focus_out_event",
			  G_CALLBACK (location_focus_out_cb),
			  w);
}

static void
ephy_location_entry_finalize_impl (GObject *o)
{
	EphyLocationEntry *w = EPHY_LOCATION_ENTRY (o);
	EphyLocationEntryPrivate *p = w->priv;

	if (p->autocompletion)
	{
		g_signal_handlers_disconnect_matched (p->autocompletion, G_SIGNAL_MATCH_DATA, 0, 0,
						      NULL, NULL, w);

		g_signal_handlers_disconnect_matched (p->autocompletion_window, G_SIGNAL_MATCH_DATA, 0, 0,
						      NULL, NULL, w);

		g_object_unref (G_OBJECT (p->autocompletion));
		g_object_unref (G_OBJECT (p->autocompletion_window));
	}

	LOG ("EphyLocationEntry finalized")

	g_free (p->before_completion);

	g_free (p);
	G_OBJECT_CLASS (gtk_hbox_class)->finalize (o);
}

GtkWidget *
ephy_location_entry_new (void)
{
	return GTK_WIDGET (g_object_new (EPHY_TYPE_LOCATION_ENTRY, NULL));
}

static gboolean
ephy_location_entry_button_press_event_cb (GtkWidget *entry, GdkEventButton *event, EphyLocationEntry *w)
{
	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
	{
		ephy_location_entry_activate (w);
		return TRUE;
	}

	return FALSE;
}

static void
ephy_location_entry_build (EphyLocationEntry *w)
{
	EphyLocationEntryPrivate *p = w->priv;
	GtkWidget *list;

	p->combo = gnome_entry_new ("ephy-url-history");
	p->entry = GTK_COMBO (p->combo)->entry;
	gtk_widget_show (p->combo);
	gtk_box_pack_start (GTK_BOX (w), p->combo, TRUE, TRUE, 0);

	g_signal_connect (p->entry, "key-press-event",
			  G_CALLBACK (ephy_location_entry_key_press_event_cb), w);
	g_signal_connect (p->entry, "insert-text",
			  G_CALLBACK (insert_text_cb), w);
	g_signal_connect (p->entry, "delete-text",
			  G_CALLBACK (delete_text_cb), w);
	g_signal_connect (p->entry, "button-press-event",
			  G_CALLBACK (ephy_location_entry_button_press_event_cb), w);

	g_signal_connect (p->entry, "activate",
			  G_CALLBACK (ephy_location_entry_activate_cb), w);

	g_signal_connect (p->entry, "changed",
			  G_CALLBACK (ephy_location_entry_editable_changed_cb), w);

	list = GTK_COMBO (p->combo)->list;

	g_signal_connect_after (list, "event-after",
				G_CALLBACK (ephy_location_entry_list_event_after_cb), w);

}

static gboolean
ephy_location_ignore_prefix (EphyLocationEntry *w)
{
	char *text;
	int text_len;
	int i, k;
	gboolean result = FALSE;
	static const gchar *prefixes[] = {
		EPHY_AUTOCOMPLETION_USUAL_WEB_PREFIXES,
		NULL
	};

	text = ephy_location_entry_get_location (w);
	text_len = g_utf8_strlen (text, -1);

	for (i = 0; prefixes[i] != NULL; i++)
	{
		const char *prefix = prefixes[i];

		for (k = 0; k < g_utf8_strlen (prefix, -1); k++)
		{
			if (text_len == (k + 1) &&
			    (strncmp (text, prefix, k + 1) == 0))
			{
				result = TRUE;
			}
		}
	}

	g_free (text);

	return result;
}

static gint
ephy_location_entry_autocompletion_show_alternatives_to (EphyLocationEntry *w)
{
	EphyLocationEntryPrivate *p = w->priv;

	g_free (p->before_completion);
	p->before_completion = gtk_editable_get_chars (GTK_EDITABLE (p->entry), 0, -1);

	if (ephy_location_ignore_prefix (w)) return FALSE;

	if (p->autocompletion)
	{
		LOG ("Show alternatives")
		ephy_location_entry_set_autocompletion_key (w);
		ephy_location_entry_autocompletion_show_alternatives (w);
	}
	p->show_alternatives_timeout = 0;
	return FALSE;
}

static void
ephy_location_entry_autocompletion_hide_alternatives (EphyLocationEntry *w)
{
	EphyLocationEntryPrivate *p = w->priv;
	if (p->autocompletion_window)
	{
		ephy_autocompletion_window_hide (p->autocompletion_window);
		p->autocompletion_window_visible = FALSE;
	}
}

static void
ephy_location_entry_autocompletion_show_alternatives (EphyLocationEntry *w)
{
	EphyLocationEntryPrivate *p = w->priv;
	if (p->autocompletion_window)
	{
		ephy_autocompletion_window_show (p->autocompletion_window);
		p->autocompletion_window_visible = TRUE;
	}
}

static void
ephy_location_entry_autocompletion_unselect_alternatives (EphyLocationEntry *w)
{
	EphyLocationEntryPrivate *p = w->priv;
	if (p->autocompletion_window)
	{
		ephy_autocompletion_window_unselect (p->autocompletion_window);
	}
}

static int
get_editable_number_of_chars (GtkEditable *editable)
{
	char *text;
	int length;

	text = gtk_editable_get_chars (editable, 0, -1);
	length = g_utf8_strlen (text, -1);
	g_free (text);
	return length;
}

static gboolean
position_is_at_end (GtkEditable *editable)
{
	int end;

	end = get_editable_number_of_chars (editable);
	return gtk_editable_get_position (editable) == end;
}

static void
delete_text_cb (GtkWidget *editable,
                int start_pos,
                int end_pos,
		EphyLocationEntry *w)
{
	ephy_location_entry_autocompletion_hide_alternatives (w);
}

static void
insert_text_cb (GtkWidget *editable,
		char *new_text,
                int new_text_length,
                int *position,
		EphyLocationEntry *w)
{
	EphyLocationEntryPrivate *p = w->priv;
	GtkWidget *window;

	window = gtk_widget_get_toplevel (editable);
	g_return_if_fail (window != NULL);
	if (!GTK_WINDOW (window)->has_focus) return;

	if (p->going_to_site) return;

        if (p->autocompletion_timeout != 0)
	{
                g_source_remove (p->autocompletion_timeout);
		p->autocompletion_timeout = 0;
	}

        if (p->show_alternatives_timeout != 0)
	{
                g_source_remove (p->show_alternatives_timeout);
		p->show_alternatives_timeout = 0;
	}

	ephy_location_entry_autocompletion_unselect_alternatives (w);
	if (position_is_at_end (GTK_EDITABLE (editable)))
	{
		p->show_alternatives_timeout = g_timeout_add
			(SHOW_ALTERNATIVES_DELAY,
			 (GSourceFunc) ephy_location_entry_autocompletion_show_alternatives_to, w);
	}
}

static gboolean
ephy_location_entry_key_press_event_cb (GtkWidget *entry, GdkEventKey *event, EphyLocationEntry *w)
{
	EphyLocationEntryPrivate *p = w->priv;

	switch (event->keyval)
        {
        case GDK_Left:
        case GDK_Right:
		ephy_location_entry_autocompletion_hide_alternatives (w);
                return FALSE;
	case GDK_Escape:
		ephy_location_entry_set_location (w, p->before_completion);
		gtk_editable_set_position (GTK_EDITABLE (p->entry), -1);
		ephy_location_entry_autocompletion_hide_alternatives (w);
                return FALSE;
        default:
                break;
        }

        return FALSE;
}

static gboolean
ephy_location_entry_content_is_text (const char *content)
{
	return ((g_strrstr (content, ".") == NULL) &&
		(g_strrstr (content, "/") == NULL) &&
		(g_strrstr (content, ":") == NULL));
}

static void
ephy_location_entry_activate_cb (GtkEntry *entry, EphyLocationEntry *w)
{
	char *content;
	char *target = NULL;

	content = gtk_editable_get_chars (GTK_EDITABLE(entry), 0, -1);
	if (w->priv->last_action_target &&
	    ephy_location_entry_content_is_text (content))
	{
		target = g_strdup (w->priv->last_action_target);
	}
	else
	{
		target = content;
		content = NULL;
		g_free ( w->priv->last_action_target);
		w->priv->last_action_target = NULL;
	}

	ephy_location_entry_autocompletion_hide_alternatives (w);

	LOG ("In ephy_location_entry_activate_cb, activating %s", content)

	g_signal_emit (w, EphyLocationEntrySignals[ACTIVATED], 0, content, target);
	g_signal_emit (w, EphyLocationEntrySignals[FINISHED], 0);

	g_free (content);
	g_free (target);
}

static void
ephy_location_entry_autocompletion_sources_changed_cb (EphyAutocompletion *aw,
						       EphyLocationEntry *w)
{
	EphyLocationEntryPrivate *p = w->priv;

	LOG ("in ephy_location_entry_autocompletion_sources_changed_cb")

        if (p->show_alternatives_timeout == 0
	    && p->autocompletion_window_visible)
	{
		p->show_alternatives_timeout = g_timeout_add
			(SHOW_ALTERNATIVES_DELAY,
			 (GSourceFunc) ephy_location_entry_autocompletion_show_alternatives_to, w);
	}
}

void
ephy_location_entry_set_location (EphyLocationEntry *w,
				  const gchar *new_location)
{
	EphyLocationEntryPrivate *p = w->priv;
	int pos;

	p->user_changed = FALSE;

	g_signal_handlers_block_by_func (G_OBJECT (p->entry),
				         delete_text_cb, w);
	gtk_editable_delete_text (GTK_EDITABLE (p->entry), 0, -1);
	g_signal_handlers_unblock_by_func (G_OBJECT (p->entry),
				           delete_text_cb, w);

	g_signal_handlers_block_by_func (G_OBJECT (p->entry),
				         insert_text_cb, w);
	gtk_editable_insert_text (GTK_EDITABLE (p->entry), new_location, strlen(new_location),
				  &pos);
	g_signal_handlers_unblock_by_func (G_OBJECT (p->entry),
				           insert_text_cb, w);

	p->user_changed = TRUE;
}

gchar *
ephy_location_entry_get_location (EphyLocationEntry *w)
{
	char *location = gtk_editable_get_chars (GTK_EDITABLE (w->priv->entry), 0, -1);
	return location;
}

static void
ephy_location_entry_autocompletion_window_url_selected_cb (EphyAutocompletionWindow *aw,
						           const char *target,
						           int action,
						           EphyLocationEntry *w)
{
	if (target)
	{
		ephy_location_entry_set_location (w, action ? w->priv->before_completion : target);
	}
	else
	{
		ephy_location_entry_set_location (w, w->priv->before_completion);
	}

	gtk_editable_set_position (GTK_EDITABLE (w->priv->entry), -1);
}

void
ephy_location_entry_set_autocompletion (EphyLocationEntry *w,
					EphyAutocompletion *ac)
{
	EphyLocationEntryPrivate *p = w->priv;
	if (p->autocompletion)
	{
		g_signal_handlers_disconnect_matched (p->autocompletion, G_SIGNAL_MATCH_DATA, 0, 0,
						      NULL, NULL, w);

		g_signal_handlers_disconnect_matched (p->autocompletion_window, G_SIGNAL_MATCH_DATA, 0, 0,
						      NULL, NULL, w);

		g_object_unref (G_OBJECT (p->autocompletion));
		g_object_unref (p->autocompletion_window);
	}
	p->autocompletion = ac;
	if (p->autocompletion)
	{
		g_object_ref (G_OBJECT (p->autocompletion));
		p->autocompletion_window = ephy_autocompletion_window_new (p->autocompletion,
									     p->entry);
		g_signal_connect (p->autocompletion_window, "activated",
				  G_CALLBACK (ephy_location_entry_autocompletion_window_url_activated_cb),
				  w);

		g_signal_connect (p->autocompletion_window, "selected",
				  G_CALLBACK (ephy_location_entry_autocompletion_window_url_selected_cb),
				  w);

		g_signal_connect (p->autocompletion_window, "hidden",
				  G_CALLBACK (ephy_location_entry_autocompletion_window_hidden_cb),
				  w);

		g_signal_connect (p->autocompletion, "sources-changed",
				  G_CALLBACK (ephy_location_entry_autocompletion_sources_changed_cb),
				  w);

		ephy_location_entry_set_autocompletion_key (w);
	}

}

static void
ephy_location_entry_autocompletion_window_url_activated_cb (EphyAutocompletionWindow *aw,
							    const char *target,
							    int action,
							    EphyLocationEntry *w)
{
	char *content;

	if (action)
	{
		if (w->priv->last_action_target)
			g_free (w->priv->last_action_target);
		w->priv->last_action_target = g_strdup (target);
	}
	else
	{
		ephy_location_entry_set_location (w, target);
	}

	content = gtk_editable_get_chars (GTK_EDITABLE(w->priv->entry), 0, -1);

	LOG ("In location_entry_autocompletion_window_url_activated_cb, going to %s", content);

	ephy_location_entry_autocompletion_hide_alternatives (w);

	g_signal_emit (w, EphyLocationEntrySignals[ACTIVATED], 0,
		       action ? content : NULL, target);

	g_free (content);
}

static void
ephy_location_entry_autocompletion_window_hidden_cb (EphyAutocompletionWindow *aw,
						     EphyLocationEntry *w)
{
	EphyLocationEntryPrivate *p = w->priv;

	LOG ("In location_entry_autocompletion_window_hidden_cb");

	p->autocompletion_window_visible = FALSE;

	if (p->show_alternatives_timeout)
	{
		g_source_remove (p->show_alternatives_timeout);
		p->show_alternatives_timeout = 0;
	}

	if (p->autocompletion_timeout)
	{
		g_source_remove (p->autocompletion_timeout);
		p->autocompletion_timeout = 0;
	}
}

void
ephy_location_entry_activate (EphyLocationEntry *w)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (w->priv->entry);

	gtk_editable_select_region (GTK_EDITABLE(w->priv->entry),
				    0, -1);
        gtk_window_set_focus (GTK_WINDOW(toplevel),
                              w->priv->entry);
}


static void
ephy_location_entry_list_event_after_cb (GtkWidget *list,
					 GdkEvent *event,
					 EphyLocationEntry *e)
{
	if (event->type == GDK_BUTTON_PRESS
	    && ((GdkEventButton *) event)->button == 1)
	{
		e->priv->going_to_site = TRUE;
	}
}

static void
ephy_location_entry_editable_changed_cb (GtkEditable *editable, EphyLocationEntry *e)
{
	EphyLocationEntryPrivate *p = e->priv;

	ephy_location_entry_set_autocompletion_key (e);

	if (p->going_to_site)
	{
		char *url = ephy_location_entry_get_location (e);
		if (url && url[0] != '\0')
		{
			p->going_to_site = FALSE;
			g_signal_emit (e, EphyLocationEntrySignals[ACTIVATED], 0, NULL, url);
		}
		g_free (url);
	}

	if (p->user_changed)
	{
		g_signal_emit (e, EphyLocationEntrySignals[USER_CHANGED], 0);
	}
}

static void
ephy_location_entry_set_autocompletion_key (EphyLocationEntry *e)
{
	EphyLocationEntryPrivate *p = e->priv;
	if (p->autocompletion && !p->block_set_autocompletion_key)
	{
		GtkEditable *editable = GTK_EDITABLE (p->entry);
		gint sstart, send;
		gchar *text;
		gtk_editable_get_selection_bounds (editable, &sstart, &send);
		text = gtk_editable_get_chars (editable, 0, sstart);
		ephy_autocompletion_set_key (p->autocompletion, text);
		g_free (p->autocompletion_key);
		p->autocompletion_key = text;
	}
}

void
ephy_location_entry_clear_history (EphyLocationEntry *w)
{
	gnome_entry_clear_history (GNOME_ENTRY (w->priv->combo));
}

