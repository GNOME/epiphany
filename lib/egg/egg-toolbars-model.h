/*
 *  Copyright (C) 2003-2004 Marco Pesenti Gritti
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
 * 
 *  $Id$
 */

#ifndef EGG_TOOLBARS_MODEL_H
#define EGG_TOOLBARS_MODEL_H

#include <glib.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define EGG_TYPE_TOOLBARS_MODEL             (egg_toolbars_model_get_type ())
#define EGG_TOOLBARS_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EGG_TYPE_TOOLBARS_MODEL, EggToolbarsModel))
#define EGG_TOOLBARS_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EGG_TYPE_TOOLBARS_MODEL, EggToolbarsModelClass))
#define EGG_IS_TOOLBARS_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EGG_TYPE_TOOLBARS_MODEL))
#define EGG_IS_TOOLBARS_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EGG_TYPE_TOOLBARS_MODEL))
#define EGG_TOOLBARS_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EGG_TYPE_TOOLBARS_MODEL, EggToolbarsModelClass))

typedef struct EggToolbarsModel		EggToolbarsModel;
typedef struct EggToolbarsModelPrivate	EggToolbarsModelPrivate;
typedef struct EggToolbarsModelClass	EggToolbarsModelClass;

#define EGG_TOOLBAR_ITEM_TYPE "application/x-toolbar-item"

typedef enum
{
  EGG_TB_MODEL_NOT_REMOVABLE	 = 1 << 0,
  EGG_TB_MODEL_NOT_EDITABLE	 = 1 << 1,
  EGG_TB_MODEL_BOTH		 = 1 << 2,
  EGG_TB_MODEL_BOTH_HORIZ	 = 1 << 3,
  EGG_TB_MODEL_ICONS		 = 1 << 4,
  EGG_TB_MODEL_TEXT		 = 1 << 5,
  EGG_TB_MODEL_STYLES_MASK	 = 0x3C,
  EGG_TB_MODEL_ACCEPT_ITEMS_ONLY = 1 << 6,
  EGG_TB_MODEL_HIDDEN            = 1 << 7
} EggTbModelFlags;

typedef enum
{
  EGG_TB_MODEL_NAME_USED         = 1 << 0,
  EGG_TB_MODEL_NAME_INFINITE     = 1 << 1,
  EGG_TB_MODEL_NAME_KNOWN        = 1 << 2
} EggTbModelNameFlags;

struct EggToolbarsModel
{
  GObject parent_object;

  /*< private >*/
  EggToolbarsModelPrivate *priv;
};

struct EggToolbarsModelClass
{
  GObjectClass parent_class;

  /* Signals */
  void (* item_added)      (EggToolbarsModel *model,
			    int toolbar_position,
			    int position);
  void (* item_removed)    (EggToolbarsModel *model,
			    int toolbar_position,
			    int position);
  void (* toolbar_added)   (EggToolbarsModel *model,
			    int position);
  void (* toolbar_changed) (EggToolbarsModel *model,
			    int position);
  void (* toolbar_removed) (EggToolbarsModel *model,
			    int position);

  /* Virtual Table */
  gboolean (* add_item)    (EggToolbarsModel *t,
			    int	              toolbar_position,
			    int               position,
			    const char       *name);
};

typedef struct EggToolbarsItemType EggToolbarsItemType;

struct EggToolbarsItemType
{
  GdkAtom type;
        
  gboolean (* has_data) (EggToolbarsItemType *type,
                         const char          *name);
  char *   (* get_data) (EggToolbarsItemType *type,
                         const char          *name);
  
  char *   (* new_name) (EggToolbarsItemType *type,
                         const char          *data);
  char *   (* get_name) (EggToolbarsItemType *type,
                         const char          *data);
};

GType		  egg_toolbars_model_flags_get_type (void);
GType		  egg_toolbars_model_get_type       (void);
EggToolbarsModel *egg_toolbars_model_new	    (void);
gboolean          egg_toolbars_model_load_names     (EggToolbarsModel *model,
						     const char *xml_file);
gboolean          egg_toolbars_model_load_toolbars  (EggToolbarsModel *model,
						     const char *xml_file);
void              egg_toolbars_model_save_toolbars  (EggToolbarsModel *model,
						     const char *xml_file,
						     const char *version);

/* Functions for manipulating the types of portable data this toolbar understands. */
GList *           egg_toolbars_model_get_types      (EggToolbarsModel *model);
void              egg_toolbars_model_set_types      (EggToolbarsModel *model,
                                                     GList            *types);

/* Functions for converting between name and portable data. */
char *            egg_toolbars_model_get_name       (EggToolbarsModel *model,
                                                     GdkAtom           type,
                                                     const char       *data,
                                                     gboolean          create);
char *            egg_toolbars_model_get_data       (EggToolbarsModel *model,
                                                     GdkAtom           type,
                                                     const char       *name);

/* Functions for retrieving what items are available for adding to the toolbars. */
GPtrArray *       egg_toolbars_model_get_name_avail (EggToolbarsModel *model);
gint              egg_toolbars_model_get_name_flags (EggToolbarsModel *model,
						     const char *name);
void              egg_toolbars_model_set_name_flags (EggToolbarsModel *model,
						     const char *name,
						     gint flags);

/* Functions for manipulating flags on individual toolbars. */
EggTbModelFlags   egg_toolbars_model_get_flags      (EggToolbarsModel *model,
						     int               toolbar_position);
void              egg_toolbars_model_set_flags      (EggToolbarsModel *model,
						     int	       toolbar_position,
						     EggTbModelFlags   flags);

/* Functions for adding and removing toolbars. */
int               egg_toolbars_model_add_toolbar    (EggToolbarsModel *model,
						     int               position,
						     const char       *name);
void		  egg_toolbars_model_remove_toolbar (EggToolbarsModel *model,
						     int               position);

/* Functions for adding, removing and moving items. */
gboolean	  egg_toolbars_model_add_item       (EggToolbarsModel *model,
						     int	       toolbar_position,
				                     int               position,
						     const char       *name);
void		  egg_toolbars_model_remove_item    (EggToolbarsModel *model,
						     int               toolbar_position,
						     int               position);
void		  egg_toolbars_model_move_item      (EggToolbarsModel *model,
						     int               toolbar_position,
						     int               position,
						     int	       new_toolbar_position,
						     int               new_position);
void		  egg_toolbars_model_delete_item    (EggToolbarsModel *model,
						     const char       *name);

/* Functions for accessing the names of items. */
int		  egg_toolbars_model_n_items	    (EggToolbarsModel *model,
						     int               toolbar_position);
const char *      egg_toolbars_model_item_nth	    (EggToolbarsModel *model,
						     int	       toolbar_position,
						     int               position);

/* Functions for accessing the names of toolbars. */
int		  egg_toolbars_model_n_toolbars	    (EggToolbarsModel *model);
const char	 *egg_toolbars_model_toolbar_nth    (EggToolbarsModel *model,
						     int               position);

G_END_DECLS

#endif
