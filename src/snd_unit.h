#ifndef __ALSA_TOOLS_HINAWA_SND_UNIT_H__
#define __ALSA_TOOLS_HINAWA_SND_UNIT_H__

#include <glib.h>
#include <glib-object.h>
#include "fw_unit.h"

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
#define HINAWA_SND_UNIT_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_SND_UNIT,	\
				   HinawaSndUnitClass))

typedef struct _HinawaSndUnit		HinawaSndUnit;
typedef struct _HinawaSndUnitClass	HinawaSndUnitClass;
typedef struct _HinawaSndUnitPrivate	HinawaSndUnitPrivate;

struct _HinawaSndUnit {
	HinawaFwUnit parent_instance;

	HinawaSndUnitPrivate *priv;
};

struct _HinawaSndUnitClass {
	HinawaFwUnitClass parent_class;
};

GType hinawa_snd_unit_get_type(void) G_GNUC_CONST;

void hinawa_snd_unit_open(HinawaSndUnit *self, gchar *path, GError **exception);

void hinawa_snd_unit_lock(HinawaSndUnit *self, GError **exception);
void hinawa_snd_unit_unlock(HinawaSndUnit *self, GError **exception);

void hinawa_snd_unit_read_transact(HinawaSndUnit *self,
				   guint64 addr, GArray *frame, guint len,
				   GError **exception);
void hinawa_snd_unit_write_transact(HinawaSndUnit *self,
				    guint64 addr, GArray *frame,
				    GError **exception);
void hinawa_snd_unit_lock_transact(HinawaSndUnit *self,
				   guint64 addr, GArray **frame,
				   GError **exception);

void hinawa_snd_unit_listen(HinawaSndUnit *self, GError **exception);
void hinawa_snd_unit_unlisten(HinawaSndUnit *self);

#endif
