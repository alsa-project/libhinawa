// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ALSA_HINAWA_SND_UNIT_H__
#define __ALSA_HINAWA_SND_UNIT_H__

#include <hinawa.h>

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

#define HINAWA_SND_UNIT_ERROR	hinawa_snd_unit_error_quark()

GQuark hinawa_snd_unit_error_quark();

typedef struct _HinawaSndUnit		HinawaSndUnit;
typedef struct _HinawaSndUnitClass	HinawaSndUnitClass;
typedef struct _HinawaSndUnitPrivate	HinawaSndUnitPrivate;

struct _HinawaSndUnit {
	GObject parent_instance;

	HinawaSndUnitPrivate *priv;
};

struct _HinawaSndUnitClass {
	GObjectClass parent_class;

	/**
	 * HinawaSndUnitClass::lock_status:
	 * @self: A #HinawaSndUnit
	 * @state: %TRUE when locked, %FALSE when unlocked.
	 *
	 * When ALSA kernel-streaming status is changed, this #HinawaSndUnitClass::lock_status
	 * handler is called.
	 *
	 * Since: 1.2
	 */
	void (*lock_status)(HinawaSndUnit *self, gboolean state);

	/**
	 * HinawaSndUnitClass::disconnected:
	 * @self: A #HinawaSndUnit
	 *
	 * When the sound card is not available anymore due to unbinding driver
	 * or hot unplugging, this signal is emit. The owner of this object
	 * should call g_object_free() as quickly as possible to release ALSA
	 * hwdep character device.
	 *
	 * Since: 2.0
	 */
	void (*disconnected)(HinawaSndUnit *self);
};

GType hinawa_snd_unit_get_type(void) G_GNUC_CONST;

HinawaSndUnit *hinawa_snd_unit_new(void);

void hinawa_snd_unit_open(HinawaSndUnit *self, gchar *path, GError **exception);

void hinawa_snd_unit_get_node(HinawaSndUnit *self, HinawaFwNode **node);

void hinawa_snd_unit_lock(HinawaSndUnit *self, GError **exception);
void hinawa_snd_unit_unlock(HinawaSndUnit *self, GError **exception);

void hinawa_snd_unit_create_source(HinawaSndUnit *self, GSource **gsrc,
				   GError **exception);

G_END_DECLS

#endif
