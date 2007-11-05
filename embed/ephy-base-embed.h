#ifndef __EPHY_BASE_EMBED_H__
#define __EPHY_BASE_EMBED_H__

#include <gtk/gtk.h>


G_BEGIN_DECLS

#define EPHY_TYPE_BASE_EMBED (ephy_base_embed_get_type())
#define EPHY_BASE_EMBED(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_BASE_EMBED, EphyBaseEmbed))
#define EPHY_BASE_EMBED_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_BASE_EMBED, EphyBaseEmbedClass))
#define EPHY_IS_BASE_EMBED(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_BASE_EMBED))
#define EPHY_IS_BASE_EMBED_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_BASE_EMBED))
#define EPHY_BASE_EMBED_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_BASE_EMBED, EphyBaseEmbedClass))

typedef struct _EphyBaseEmbed      EphyBaseEmbed;
typedef struct _EphyBaseEmbedClass EphyBaseEmbedClass;
typedef struct _EphyBaseEmbedPrivate EphyBaseEmbedPrivate;

struct _EphyBaseEmbedClass
{
    GtkBinClass parent_class;
};

struct _EphyBaseEmbed
{
    GtkBin parent_instance;
};

GType ephy_base_embed_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __EPHY_BASE_EMBED_H__ */
