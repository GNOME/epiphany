#ifndef __MOZILLA_HISTORY_ITEM_H__
#define __MOZILLA_HISTORY_ITEM_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define MOZILLA_TYPE_HISTORY_ITEM (mozilla_history_item_get_type())
#define MOZILLA_HISTORY_ITEM(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOZILLA_TYPE_HISTORY_ITEM, MozillaHistoryItem))
#define MOZILLA_HISTORY_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), MOZILLA_TYPE_HISTORY_ITEM, MozillaHistoryItemClass))
#define MOZILLA_IS_HISTORY_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOZILLA_TYPE_HISTORY_ITEM))
#define MOZILLA_IS_HISTORY_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOZILLA_TYPE_HISTORY_ITEM))
#define MOZILLA_HISTORY_ITEM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), MOZILLA_TYPE_HISTORY_ITEM, MozillaHistoryItemClass))

#define HISTORY_ITEM_INDEX_KEY	"NTh"

typedef struct _MozillaHistoryItem      MozillaHistoryItem;
typedef struct _MozillaHistoryItemClass MozillaHistoryItemClass;

struct _MozillaHistoryItemClass
{
  GObjectClass parent_class;
};

struct _MozillaHistoryItem
{
  GObject parent_instance;

  char *url;
  char *title;
};

GType               mozilla_history_item_get_type (void) G_GNUC_CONST;
MozillaHistoryItem *mozilla_history_item_new      (const char *url, const char *title, int index) G_GNUC_MALLOC;

G_END_DECLS

#endif /* __MOZILLA_HISTORY_ITEM_H__ */
