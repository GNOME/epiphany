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
#include "ephy-gobject-misc.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"

#include <gtk/gtkentry.h>
#include <gtk/gtkwindow.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <libgnomeui/gnome-entry.h>
#include <string.h>

//#define DEBUG_MSG(x) g_print x
#define DEBUG_MSG(x)

#define NOT_IMPLEMENTED g_warning ("not implemented: " G_STRLOC);

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
static gint		ephy_location_entry_autocompletion_to (EphyLocationEntry *w);
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




static gpointer gtk_hbox_class;

/**
 * Signals enums and ids
 */
enum EphyLocationEntrySignalsEnum {
	ACTIVATED,
	LAST_SIGNAL
};
static gint EphyLocationEntrySignals[LAST_SIGNAL];

/**
 * EphyLocationEntry object
 */

MAKE_GET_TYPE (ephy_location_entry, "EphyLocationEntry", EphyLocationEntry,
	       ephy_location_entry_class_init,
	       ephy_location_entry_init, GTK_TYPE_HBOX);

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
}

static void
ephy_location_entry_init (EphyLocationEntry *w)
{
	EphyLocationEntryPrivate *p = g_new0 (EphyLocationEntryPrivate, 1);
	w->priv = p;
	p->last_action_target = NULL;

	ephy_location_entry_build (w);
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

	DEBUG_MSG (("EphyLocationEntry finalized\n"));

	g_free (p);
	G_OBJECT_CLASS (gtk_hbox_class)->finalize (o);
}

EphyLocationEntry *
ephy_location_entry_new (void)
{
	return EPHY_LOCATION_ENTRY (g_object_new (EPHY_TYPE_LOCATION_ENTRY, NULL));
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

	if (ephy_location_ignore_prefix (w)) return FALSE;

	if (p->autocompletion)
	{
		DEBUG_MSG (("+ephy_location_entry_autocompletion_show_alternatives_to\n"));
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

static gint
ephy_location_entry_autocompletion_to (EphyLocationEntry *w)
{
	EphyLocationEntryPrivate *p = w->priv;
	gchar *text;
	gchar *common_prefix;

	DEBUG_MSG (("in ephy_location_entry_autocompletion_to\n"));

	ephy_location_entry_set_autocompletion_key (w);

	{
		GtkEditable *editable = GTK_EDITABLE (p->entry);
		gint sstart, send;
		gint pos = gtk_editable_get_position (editable);
		const gchar *text = gtk_entry_get_text (GTK_ENTRY (p->entry));
		gint text_len = strlen (text);
		gtk_editable_get_selection_bounds (editable, &sstart, &send);

		if (pos != text_len
		    || send != text_len)
		{
			/* the user is editing the entry, don't mess it */
			DEBUG_MSG (("The user seems editing the text: pos = %d, strlen (text) = %d, sstart = %d, send = %d\n",
				    pos, strlen (text), sstart, send));
			p->autocompletion_timeout = 0;
			return FALSE;
		}
	}

	common_prefix = ephy_autocompletion_get_common_prefix (p->autocompletion);

	DEBUG_MSG (("common_prefix: %s\n", common_prefix));

	if (common_prefix && (!p->before_completion || p->before_completion[0] == '\0'))
	{
		text = ephy_location_entry_get_location (w);
		g_free (p->before_completion);
		p->before_completion = text;
	}

	if (common_prefix)
	{
		/* check original length */
		guint text_len = strlen (p->autocompletion_key);

		p->block_set_autocompletion_key = TRUE;

		/* set entry to completed text */
		gtk_entry_set_text (GTK_ENTRY (p->entry), common_prefix);

		/* move selection appropriately */
		gtk_editable_select_region (GTK_EDITABLE (p->entry), text_len, -1);

		p->block_set_autocompletion_key = FALSE;

		g_free (p->last_completion);
		p->last_completion = common_prefix;
	}

	p->autocompletion_timeout = 0;
	return FALSE;
}

/* this is from the old location entry, need to do the autocompletion before implementing this */
static gboolean
ephy_location_entry_key_press_event_cb (GtkWidget *entry, GdkEventKey *event, EphyLocationEntry *w)
{
	EphyLocationEntryPrivate *p = w->priv;
        static gboolean suggest = FALSE;
        guint keyval = event->keyval;

        if (p->autocompletion_timeout != 0)
	{
                gtk_timeout_remove (p->autocompletion_timeout);
		p->autocompletion_timeout = 0;
	}

        if (p->show_alternatives_timeout != 0)
	{
                gtk_timeout_remove (p->show_alternatives_timeout);
		p->show_alternatives_timeout = 0;
	}

        /* only suggest heuristic completions if TAB is hit twice */
        if (event->keyval != GDK_Tab)
	{
                suggest = FALSE;
	}

        if (((event->state & GDK_Control_L || event->state & GDK_Control_R) &&
             (keyval == GDK_a || keyval == GDK_b || keyval == GDK_c ||
              keyval == GDK_d || keyval == GDK_e || keyval == GDK_f ||
              keyval == GDK_h || keyval == GDK_k || keyval == GDK_u ||
              keyval == GDK_v || keyval == GDK_w || keyval == GDK_x)) ||
            (event->state == 0 && event->keyval == GDK_BackSpace))
        {
		ephy_location_entry_autocompletion_hide_alternatives (w);
                return FALSE;
        }

        /* don't grab alt combos, thus you can still access the menus. */
        if (event->state & GDK_MOD1_MASK)
        {
		ephy_location_entry_autocompletion_hide_alternatives (w);
                return FALSE;
        }

        /* make sure the end key works at all times */
        if ((!((event->state & GDK_SHIFT_MASK) ||
	       (event->state & GDK_CONTROL_MASK) ||
	       (event->state & GDK_MOD1_MASK))
	     && (event->keyval == GDK_End)))
        {
		ephy_location_entry_autocompletion_hide_alternatives (w);
                gtk_editable_select_region (GTK_EDITABLE (p->entry), 0, 0);
                gtk_editable_set_position (GTK_EDITABLE (p->entry), -1);
		ephy_location_entry_autocompletion_unselect_alternatives (w);
                return TRUE;
        }

	switch (event->keyval)
        {
        case GDK_Left:
        case GDK_Right:
		ephy_location_entry_autocompletion_hide_alternatives (w);
                return FALSE;
        case GDK_Up:
        case GDK_Down:
        case GDK_Page_Up:
        case GDK_Page_Down:
		ephy_location_entry_autocompletion_hide_alternatives (w);
                //ephy_embed_grab_focus (window->active_embed);
                return FALSE;
        case GDK_Tab:
        {
                gchar *common_prefix = NULL;
                gchar *text;

		ephy_location_entry_set_autocompletion_key (w);

                gtk_editable_delete_selection (GTK_EDITABLE (p->entry));
                text = ephy_location_entry_get_location (w);
		ephy_location_entry_autocompletion_unselect_alternatives (w);

		if (p->autocompletion)
		{
			common_prefix = ephy_autocompletion_get_common_prefix (p->autocompletion);
		}
                suggest = FALSE;
                if (common_prefix)
                {
                        if (!p->before_completion)
			{
                                p->before_completion = g_strdup (text);
			}

                        p->block_set_autocompletion_key = TRUE;

			gtk_entry_set_text (GTK_ENTRY (p->entry), common_prefix);
			gtk_editable_set_position (GTK_EDITABLE (p->entry), -1);

                        p->block_set_autocompletion_key = FALSE;

			ephy_location_entry_autocompletion_show_alternatives (w);
                        if (!strcmp (common_prefix, text))
                        {
                                /* really suggest something the next time */
                                suggest = TRUE;
                        }
                        g_free (common_prefix);
                }
                else
                {
			ephy_location_entry_autocompletion_hide_alternatives (w);
                }
		g_free (text);
                return TRUE;
        }
        case GDK_Escape:
		ephy_location_entry_autocompletion_hide_alternatives (w);
                if (p->before_completion)
                {
                        ephy_location_entry_set_location (w, p->before_completion);
                        g_free (p->before_completion);
                        p->before_completion = NULL;
			gtk_editable_set_position (GTK_EDITABLE (p->entry), -1);
                        return TRUE;
                }
                break;
        default:
		ephy_location_entry_autocompletion_unselect_alternatives (w);
		if ((event->string[0] > 32) && (event->string[0] < 126))
                {
                        p->show_alternatives_timeout = g_timeout_add
                                (SHOW_ALTERNATIVES_DELAY,
				 (GSourceFunc) ephy_location_entry_autocompletion_show_alternatives_to, w);
                }
                break;
        }

        return FALSE;
}

static gboolean
ephy_location_entry_content_is_text (const char *content)
{
	return ((g_strrstr (content, ".") == NULL) &&
		(g_strrstr (content, "/") == NULL));
}

static void
ephy_location_entry_activate_cb (GtkEntry *entry, EphyLocationEntry *w)
{
	char *content;
	char *target = NULL;

	content = gtk_editable_get_chars (GTK_EDITABLE(entry), 0, -1);
	if (ephy_location_entry_content_is_text (content))
	{
		target = w->priv->last_action_target;
	}

	ephy_location_entry_autocompletion_hide_alternatives (w);

	DEBUG_MSG (("In ephy_location_entry_activate_cb, activating %s\n", content));

	g_signal_emit (w, EphyLocationEntrySignals[ACTIVATED], 0, target, content);
	g_free (content);
}

static void
ephy_location_entry_autocompletion_sources_changed_cb (EphyAutocompletion *aw,
						       EphyLocationEntry *w)
{
	EphyLocationEntryPrivate *p = w->priv;

	DEBUG_MSG (("in ephy_location_entry_autocompletion_sources_changed_cb\n"));

        if (p->autocompletion_timeout == 0
	    && p->last_completion
	    && !strcmp (p->last_completion, gtk_entry_get_text (GTK_ENTRY (p->entry))))
	{
		p->autocompletion_timeout = gtk_timeout_add
			(AUTOCOMPLETION_DELAY,
			 (GSourceFunc) ephy_location_entry_autocompletion_to, w);
	}

        if (p->show_alternatives_timeout == 0
	    && p->autocompletion_window_visible)
	{
		p->show_alternatives_timeout = gtk_timeout_add
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
	gtk_editable_delete_text (GTK_EDITABLE (p->entry), 0, -1);
	gtk_editable_insert_text (GTK_EDITABLE (p->entry), new_location, g_utf8_strlen (new_location, -1),
				  &pos);
}

gchar *
ephy_location_entry_get_location (EphyLocationEntry *w)
{
	char *location = gtk_editable_get_chars (GTK_EDITABLE (w->priv->entry), 0, -1);
	return location;
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

	DEBUG_MSG (("In location_entry_autocompletion_window_url_activated_cb, going to %s\n", content));

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

	DEBUG_MSG (("In location_entry_autocompletion_window_hidden_cb\n"));

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
		gchar *url = ephy_location_entry_get_location (e);
		g_signal_emit
			(e, EphyLocationEntrySignals[ACTIVATED], 0, url);
		g_free (url);
	}
}

static void
ephy_location_entry_editable_changed_cb (GtkEditable *editable, EphyLocationEntry *e)
{
	ephy_location_entry_set_autocompletion_key (e);
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

