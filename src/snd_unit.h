#ifndef __ALSA_TOOLS_HINAWA_SND_UNIT_H__
#define __ALSA_TOOLS_HINAWA_SND_UNIT_H__

#include <glib.h>
#include <glib-object.h>
#include "internal.h"

G_BEGIN_DECLS

#define HINAWA_TYPE_SND_UNIT	(hinawa_snd_unit_get_type())

#define HINAWA_SND_UNIT(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINAWA_TYPE_SND_UNIT,	\
				    HinawaSndUnit))
#define HINAWA_IS_SND_UNIT(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINAWA_TYPE_SND_UNIT))

#define HINAWA_SND_UNIT_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINAWA_TYPE_SND_UNIT,		\
				 HinawaSndUnitClass))
#define HINAWA_IS_SND_UNIT_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_SND_UNIT))
#define HINAWA_SND_UNIT_GET_CLASS(obj) 				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_SND_UNIT,	\
				   HinawaSndUnitClass))

typedef struct _HinawaSndUnit		HinawaSndUnit;
typedef struct _HinawaSndUnitClass	HinawaSndUnitClass;
typedef struct _HinawaSndUnitPrivate	HinawaSndUnitPrivate;

struct _HinawaSndUnit
{
	GObject parent_instance;

	HinawaSndUnitPrivate *priv;
};

struct _HinawaSndUnitClass
{
    GObjectClass parent_class;
};

GType hinawa_snd_unit_get_type(void) G_GNUC_CONST;

HinawaSndUnit *hinawa_snd_unit_new(gchar *path, GError **exception);

void hinawa_snd_unit_lock(HinawaSndUnit *unit, GError **exception);
void hinawa_snd_unit_unlock(HinawaSndUnit *unit, GError **exception);

void hinawa_snd_unit_listen(HinawaSndUnit *unit, GError **exception);
void hinawa_snd_unit_unlisten(HinawaSndUnit *unit, GError **exception);
#endif
