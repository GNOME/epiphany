/* eel-ellipsizing-label.h: Subclass of GtkLabel that ellipsizes the text.

   Copyright (C) 2001 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: John Sullivan <sullivan@eazel.com>,
 */

#ifndef EPHY_ELLIPSIZING_LABEL_H
#define EPHY_ELLIPSIZING_LABEL_H

#include <gtk/gtklabel.h>

#define EPHY_TYPE_ELLIPSIZING_LABEL            (ephy_ellipsizing_label_get_type ())
#define EPHY_ELLIPSIZING_LABEL(obj)            (GTK_CHECK_CAST ((obj), EPHY_TYPE_ELLIPSIZING_LABEL, EphyEllipsizingLabel))
#define EPHY_ELLIPSIZING_LABEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EPHY_TYPE_ELLIPSIZING_LABEL, EphyEllipsizingLabelClass))
#define EPHY_IS_ELLIPSIZING_LABEL(obj)         (GTK_CHECK_TYPE ((obj), EPHY_TYPE_ELLIPSIZING_LABEL))
#define EPHY_IS_ELLIPSIZING_LABEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_ELLIPSIZING_LABEL))

typedef struct EphyEllipsizingLabelPrivate EphyEllipsizingLabelPrivate;

typedef enum
{
        EPHY_ELLIPSIZE_NONE,
	EPHY_ELLIPSIZE_START,
        EPHY_ELLIPSIZE_MIDDLE,
        EPHY_ELLIPSIZE_END
} EphyEllipsizeMode;

typedef struct
{
	GtkLabel parent;

	/*< private >*/
	EphyEllipsizingLabelPrivate *priv;
} EphyEllipsizingLabel;

typedef struct
{
	GtkLabelClass parent_class;
} EphyEllipsizingLabelClass;

GType      ephy_ellipsizing_label_get_type	(void);

GtkWidget *ephy_ellipsizing_label_new		(const char *string);

void       ephy_ellipsizing_label_set_mode      (EphyEllipsizingLabel *label,
						 EphyEllipsizeMode mode);

void       ephy_ellipsizing_label_set_text	(EphyEllipsizingLabel *label,
						 const char *string);

void       ephy_ellipsizing_label_set_markup	(EphyEllipsizingLabel *label,
						 const char *string);

G_END_DECLS

#endif /* EPHY_ELLIPSIZING_LABEL_H */
