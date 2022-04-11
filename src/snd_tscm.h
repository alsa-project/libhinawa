// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ALSA_HINAWA_SND_TSCM_H__
#define __ALSA_HINAWA_SND_TSCM_H__

#include <hinawa.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_SND_TSCM	(hinawa_snd_tscm_get_type())

G_DECLARE_DERIVABLE_TYPE(HinawaSndTscm, hinawa_snd_tscm, HINAWA, SND_TSCM, HinawaSndUnit);

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

HinawaSndTscm *hinawa_snd_tscm_new(void);

void hinawa_snd_tscm_open(HinawaSndTscm *self, gchar *path, GError **exception);

const guint32 *hinawa_snd_tscm_get_state(HinawaSndTscm *self,
					 GError **exception);

#endif
