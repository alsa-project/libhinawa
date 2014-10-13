#ifndef HINAWA_SND_H
#define HINAWA_SND_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GHINAWA_TYPE_SND_UNIT	(ghinawa_snd_unit_get_type())

#define GHINAWA_SND_UNIT(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    GHINAWA_TYPE_SND_UNIT,	\
				    GHinawaSndUnit))
#define GHINAWA_IS_SND_UNIT(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    GHINAWA_TYPE_SND_UNIT))

#define GHINAWA_SND_UNIT_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 GHINAWA_TYPE_SND_UNIT,		\
				 GHinawaSndUnitClass))
#define GHINAWA_IS_SND_UNIT_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 GHINAWA_TYPE_SND_UNIT))
#define GHINAWA_SND_UNIT_GET_CLASS(obj) 				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   GHINAWA_TYPE_SND_UNIT,	\
				   GHinawaSndUnitClass))

typedef struct _GHinawaSndUnit		GHinawaSndUnit;
typedef struct _GHinawaSndUnitClass	GHinawaSndUnitClass;
typedef struct _GHinawaSndUnitPrivate	GHinawaSndUnitPrivate;

struct _GHinawaSndUnit
{
	GObject parent_instance;

	GHinawaSndUnitPrivate *priv;
};

struct _GHinawaSndUnitClass
{
    GObjectClass parent_class;
};

GType ghinawa_snd_unit_get_type(void) G_GNUC_CONST;

typedef void (*GHinawaSndUnitCB)(GHinawaSndUnit* unit, void *private_data, gint val);

GHinawaSndUnit *ghinawa_snd_unit_new(void);
const gchar *ghinawa_snd_unit_greet(GHinawaSndUnit *unit);
gchar *ghinawa_snd_unit_do(GHinawaSndUnit *unit, gchar *str, const guint length);

/**
 * ghinawa_snd_unit_set_cb:
 * @unit: The canvas.
 * @cb: (scope call): A function to call on every node on @unit.
 * @private_data: Data to pass to @cb.
 */

void ghinawa_snd_unit_set_cb(GHinawaSndUnit *unit, GHinawaSndUnitCB cb, void *private_data);

G_END_DECLS

#endif
