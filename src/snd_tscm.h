// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ALSA_HINAWA_SND_TSCM_H__
#define __ALSA_HINAWA_SND_TSCM_H__

#include <glib.h>
#include <glib-object.h>

#include <snd_unit.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_SND_TSCM	(hinawa_snd_tscm_get_type())

#define HINAWA_SND_TSCM(obj)					\
	(G_TYPE_CHECK_INSTANCE_CAST((obj),			\
				    HINAWA_TYPE_SND_TSCM,	\
				    HinawaSndTscm))
#define HINAWA_IS_SND_TSCM(obj)					\
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),			\
				    HINAWA_TYPE_SND_TSCM))

#define HINAWA_SND_TSCM_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_CAST((klass),			\
				 HINAWA_TYPE_SND_TSCM,		\
				 HinawaSndTscmClass))
#define HINAWA_IS_SND_TSCM_CLASS(klass)				\
	(G_TYPE_CHECK_CLASS_TYPE((klass),			\
				 HINAWA_TYPE_SND_TSCM))
#define HINAWA_SND_TSCM_GET_CLASS(obj)				\
	(G_TYPE_INSTANCE_GET_CLASS((obj),			\
				   HINAWA_TYPE_SND_TSCM,	\
				   HinawaSndTscmClass))

typedef struct _HinawaSndTscm		HinawaSndTscm;
typedef struct _HinawaSndTscmClass	HinawaSndTscmClass;
typedef struct _HinawaSndTscmPrivate	HinawaSndTscmPrivate;

struct _HinawaSndTscm {
	HinawaSndUnit parent_instance;

	HinawaSndTscmPrivate *priv;
};

struct _HinawaSndTscmClass {
	HinawaSndUnitClass parent_class;

	/**
	 * HinawaSndTscmClass::control:
	 * @self: A #HinawaSndTscm
	 * @index: the numerical index on image of status and control info.
	 * @before: the value of info before changed.
	 * @after: the value of info after changed.
	 *
	 * When TASCAM FireWire unit transfer control message, the #HinawaSndTscmClass::control
	 * handler is emitted.
	 *
	 * Since: 1.2
	 */
	void (*control)(HinawaSndTscm *self, guint index, guint before,
			guint after);
};

GType hinawa_snd_tscm_get_type(void) G_GNUC_CONST;

HinawaSndTscm *hinawa_snd_tscm_new(void);

void hinawa_snd_tscm_open(HinawaSndTscm *self, gchar *path, GError **exception);

const guint32 *hinawa_snd_tscm_get_state(HinawaSndTscm *self,
					 GError **exception);

#endif
