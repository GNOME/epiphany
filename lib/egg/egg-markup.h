#ifndef EGG_MARKUP_H
#define EGG_MARKUP_H

#include <gtk/gtk.h>
#include <egg-action.h>
#include <egg-action-group.h>


/* this stuff can go away once I am finished with the merge code */

typedef void (*EggWidgetFunc) (GtkWidget *widget,
			       const gchar *type,
			       const gchar *name,
			       gpointer user_data);

gboolean egg_create_from_string (EggActionGroup *action_group,
				 EggWidgetFunc widget_func,
				 gpointer user_data,
				 GtkAccelGroup *accel_group,
				 const gchar *buffer, guint length,
				 GError **error);

gboolean egg_create_from_file   (EggActionGroup *action_group,
				 EggWidgetFunc widget_func,
				 gpointer user_data,
				 GtkAccelGroup *accel_group,
				 const gchar *filename,
				 GError **error);

#endif
