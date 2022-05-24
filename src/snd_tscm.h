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
	 * @self: A [class@SndTscm]
	 * @index: the numeric index on image of status and control info.
	 * @before: the value of info before changed.
	 * @after: the value of info after changed.
	 *
	 * Class closure for the [signal@SndTscm::control] signal.
	 *
	 * Since: 1.2
	 * Deprecated: 2.5. Use implementation of [vfunc@Hitaki.TascamProtocol.changed] in
	 *	       [class@Hitaki.SndTascam] instead.
	 */
	void (*control)(HinawaSndTscm *self, guint index, guint before, guint after);
};

HinawaSndTscm *hinawa_snd_tscm_new(void);

void hinawa_snd_tscm_open(HinawaSndTscm *self, gchar *path, GError **error);

const guint32 *hinawa_snd_tscm_get_state(HinawaSndTscm *self, GError **error);

#endif
