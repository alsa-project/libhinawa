// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ALSA_HINAWA_SND_MOTU_H__
#define __ALSA_HINAWA_SND_MOTU_H__

#include <hinawa.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_SND_MOTU	(hinawa_snd_motu_get_type())

G_DECLARE_DERIVABLE_TYPE(HinawaSndMotu, hinawa_snd_motu, HINAWA, SND_MOTU, HinawaSndUnit)

struct _HinawaSndMotuClass {
	HinawaSndUnitClass parent_class;

	/**
	 * HinawaSndMotuClass::notified:
	 * @self: A [class@SndMotu]
	 * @message: A notification message
	 *
	 * Class closure for the [signal@SndMotu::notified] signal.
	 *
	 * Since: 1.2
	 * Deprecated: 2.5. Use implementation of [vfunc@Hitaki.QuadletNotification.notified]
	 *	       in [class@Hitaki.SndMotu] instead.
	 */
	void (*notified)(HinawaSndMotu *self, guint message);

	/**
	 * HinawaSndMotuClass::register_dsp_changed:
	 * @self: A [class@SndMotu]
	 * @events: (element-type guint32)(array length=length): The array with element for
	 *	    unsigned 32 bit encoded data.
	 * @length: The length of events.
	 *
	 * Class closure for the [signal@SndMotu::register-dsp-changed] signal.
	 *
	 * Since: 2.4
	 * Deprecated: 2.5. Use implementation [vfunc@Hitaki.MotuRegisterDsp.changed] in
	 *	       [class@Hitaki.SndMotu] instead.
	 */
	void (*register_dsp_changed)(HinawaSndMotu *self, const guint32 *events, guint length);
};

HinawaSndMotu *hinawa_snd_motu_new(void);

void hinawa_snd_motu_open(HinawaSndMotu *self, gchar *path, GError **error);

void hinawa_snd_motu_read_register_dsp_parameter(HinawaSndMotu *self,
						 HinawaSndMotuRegisterDspParameter *const *param,
						 GError **error);

void hinawa_snd_motu_read_register_dsp_meter(HinawaSndMotu *self, guint8 *const meter[48],
					     GError **error);

void hinawa_snd_motu_read_command_dsp_meter(HinawaSndMotu *self, gfloat *const meter[400],
					    GError **error);

G_END_DECLS

#endif
