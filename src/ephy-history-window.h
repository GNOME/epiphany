/*
 *  Copyright © 2003 Marco Pesenti Gritti <mpeseng@tin.it>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#if !defined (__EPHY_EPIPHANY_H_INSIDE__) && !defined (EPIPHANY_COMPILATION)
#error "Only <epiphany/epiphany.h> can be included directly."
#endif

#ifndef EPHY_HISTORY_WINDOW_H
#define EPHY_HISTORY_WINDOW_H

#include <gtk/gtk.h>

#include "ephy-history-service.h"

G_BEGIN_DECLS

#define EPHY_TYPE_HISTORY_WINDOW     (ephy_history_window_get_type ())
#define EPHY_HISTORY_WINDOW(o)       (G_TYPE_CHECK_INSTANCE_CAST ((o), EPHY_TYPE_HISTORY_WINDOW, EphyHistoryWindow))
#define EPHY_HISTORY_WINDOW_CLASS(k) (G_TYPE_CHECK_CLASS_CAST((k), EPHY_TYPE_HISTORY_WINDOW, EphyHistoryWindowClass))
#define EPHY_IS_HISTORY_WINDOW(o)    (G_TYPE_CHECK_INSTANCE_TYPE ((o), EPHY_TYPE_HISTORY_WINDOW))
#define EPHY_IS_HISTORY_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EPHY_TYPE_HISTORY_WINDOW))
#define EPHY_HISTORY_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EPHY_TYPE_HISTORY_WINDOW, EphyHistoryWindowClass))

typedef struct _EphyHistoryWindowPrivate EphyHistoryWindowPrivate;

typedef struct
{
	GtkDialog parent;

	/*< private >*/
	EphyHistoryWindowPrivate *priv;
} EphyHistoryWindow;

typedef struct
{
	GtkDialogClass parent;
} EphyHistoryWindowClass;

GType		     ephy_history_window_get_type (void);

GtkWidget	    *ephy_history_window_new        (EphyHistoryService *history_service);

void		     ephy_history_window_set_parent (EphyHistoryWindow *ehw,
						     GtkWidget *window);

G_END_DECLS

#endif /* EPHY_HISTORY_WINDOW_H */
