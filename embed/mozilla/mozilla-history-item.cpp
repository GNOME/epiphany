#include "mozilla-history-item.h"
#include "ephy-history-item.h"

static void mozilla_history_item_finalize (GObject *object);

static const char*
impl_get_url (EphyHistoryItem *item)
{
  return MOZILLA_HISTORY_ITEM (item)->url;
}

static const char*
impl_get_title (EphyHistoryItem *item)
{
  return MOZILLA_HISTORY_ITEM (item)->title;
}

static void
mozilla_history_item_iface_init (EphyHistoryItemIface *iface)
{
  iface->get_url = impl_get_url;
  iface->get_title = impl_get_title;
}

G_DEFINE_TYPE_WITH_CODE (MozillaHistoryItem, mozilla_history_item, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (EPHY_TYPE_HISTORY_ITEM,
                                                mozilla_history_item_iface_init))

static void
mozilla_history_item_class_init (MozillaHistoryItemClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->finalize = mozilla_history_item_finalize;
}

static void
mozilla_history_item_init (MozillaHistoryItem *self)
{
}

static void
mozilla_history_item_finalize (GObject *object)
{
  MozillaHistoryItem *self = (MozillaHistoryItem *)object;

  g_free (self->url);
  g_free (self->title);

  G_OBJECT_CLASS (mozilla_history_item_parent_class)->finalize (object);
}

MozillaHistoryItem*
mozilla_history_item_new (const char *url, const char *title, int index)
{
  MozillaHistoryItem *item;

  g_return_val_if_fail (url != NULL, NULL);
  g_return_val_if_fail (title != NULL, NULL);

  item = (MozillaHistoryItem*) g_object_new (MOZILLA_TYPE_HISTORY_ITEM, NULL);

  item->url = g_strdup (url);
  item->title = g_strdup (title);

  g_object_set_data (G_OBJECT (item), HISTORY_ITEM_INDEX_KEY, GINT_TO_POINTER (index));

  return item;
}
