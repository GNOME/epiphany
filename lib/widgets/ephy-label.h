/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __EPHY_LABEL_H__
#define __EPHY_LABEL_H__

#include <gtk/gtkversion.h>

#if GTK_CHECK_VERSION (2, 5, 1)

#include <gtk/gtklabel.h>

#define ephy_label_new			gtk_label_new
#define ephy_label_set_ellipsize	gtk_label_set_ellipsize
#define ephy_label_set_text		gtk_label_set_text
#define ephy_label_set_selectable	gtk_label_set_selectable
#define ephy_label_set_use_markup	gtk_label_set_use_markup
#define EPHY_LABEL			GTK_LABEL

#else

#include <gdk/gdk.h>
#include <gtk/gtkmisc.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkmenu.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define EPHY_TYPE_LABEL		  (ephy_label_get_type ())
#define EPHY_LABEL(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_LABEL, EphyLabel))
#define EPHY_LABEL_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_LABEL, EphyLabelClass))
#define EPHY_IS_LABEL(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_LABEL))
#define EPHY_IS_LABEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_LABEL))
#define EPHY_LABEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_LABEL, EphyLabelClass))
       

typedef struct _EphyLabel       EphyLabel;
typedef struct _EphyLabelClass  EphyLabelClass;

typedef struct _EphyLabelSelectionInfo EphyLabelSelectionInfo;

struct _EphyLabel
{
  GtkMisc misc;

  /*< private >*/
  gchar  *label;
  guint   jtype : 2;
  guint   wrap : 1;
  guint   use_underline : 1;
  guint   use_markup : 1;
  guint   ellipsize : 3;

  guint   mnemonic_keyval;
  
  gchar  *text; 
  PangoAttrList *attrs;
  PangoAttrList *effective_attrs;
  
  PangoLayout *layout;

  GtkWidget *mnemonic_widget;
  GtkWindow *mnemonic_window;
  
  EphyLabelSelectionInfo *select_info;
};

struct _EphyLabelClass
{
  GtkMiscClass parent_class;

  void (* move_cursor)     (EphyLabel       *label,
			    GtkMovementStep step,
			    gint            count,
			    gboolean        extend_selection);
  void (* copy_clipboard)  (EphyLabel       *label);
  
  /* Hook to customize right-click popup for selectable labels */
  void (* populate_popup)   (EphyLabel       *label,
                             GtkMenu        *menu);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType                 ephy_label_get_type          (void) G_GNUC_CONST;
GtkWidget*            ephy_label_new               (const char    *str);
GtkWidget*            ephy_label_new_with_mnemonic (const char    *str);
void                  ephy_label_set_text          (EphyLabel      *label,
						   const char    *str);
G_CONST_RETURN gchar* ephy_label_get_text          (EphyLabel      *label);
void                  ephy_label_set_attributes    (EphyLabel      *label,
						   PangoAttrList *attrs);
PangoAttrList        *ephy_label_get_attributes    (EphyLabel      *label);
void                  ephy_label_set_label         (EphyLabel      *label,
						   const gchar   *str);
G_CONST_RETURN gchar *ephy_label_get_label         (EphyLabel      *label);
void                  ephy_label_set_markup        (EphyLabel      *label,
						   const gchar   *str);
void                  ephy_label_set_use_markup    (EphyLabel      *label,
						   gboolean       setting);
gboolean              ephy_label_get_use_markup    (EphyLabel      *label);
void                  ephy_label_set_use_underline (EphyLabel      *label,
						   gboolean       setting);
gboolean              ephy_label_get_use_underline (EphyLabel      *label);

void     ephy_label_set_markup_with_mnemonic       (EphyLabel         *label,
						   const gchar      *str);
guint    ephy_label_get_mnemonic_keyval            (EphyLabel         *label);
void     ephy_label_set_mnemonic_widget            (EphyLabel         *label,
						   GtkWidget        *widget);
GtkWidget *ephy_label_get_mnemonic_widget          (EphyLabel         *label);
void     ephy_label_set_text_with_mnemonic         (EphyLabel         *label,
						   const gchar      *str);
void     ephy_label_set_justify                    (EphyLabel         *label,
						   GtkJustification  jtype);
GtkJustification ephy_label_get_justify            (EphyLabel         *label);
void     ephy_label_set_ellipsize		  (EphyLabel         *label,
						   PangoEllipsizeMode mode);
PangoEllipsizeMode ephy_label_get_ellipsize        (EphyLabel         *label);
void     ephy_label_set_pattern                    (EphyLabel         *label,
						   const gchar      *pattern);
void     ephy_label_set_line_wrap                  (EphyLabel         *label,
						   gboolean          wrap);
gboolean ephy_label_get_line_wrap                  (EphyLabel         *label);
void     ephy_label_set_selectable                 (EphyLabel         *label,
						   gboolean          setting);
gboolean ephy_label_get_selectable                 (EphyLabel         *label);
void     ephy_label_select_region                  (EphyLabel         *label,
						   gint              start_offset,
						   gint              end_offset);
gboolean ephy_label_get_selection_bounds           (EphyLabel         *label,
                                                   gint             *start,
                                                   gint             *end);

PangoLayout *ephy_label_get_layout         (EphyLabel *label);
void         ephy_label_get_layout_offsets (EphyLabel *label,
                                           gint     *x,
                                           gint     *y);


#define  ephy_label_set           ephy_label_set_text
void       ephy_label_get           (EphyLabel          *label,
                                    char             **str);

/* Convenience function to set the name and pattern by parsing
 * a string with embedded underscores, and return the appropriate
 * key symbol for the accelerator.
 */
guint ephy_label_parse_uline            (EphyLabel    *label,
					const gchar *string);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* GTK_CHECK_VERSION (2, 5, 1) */

#endif /* __EPHY_LABEL_H__ */
