// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ALSA_HINAWA_SND_DICE_H__
#define __ALSA_HINAWA_SND_DICE_H__

#include <hinawa.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_SND_DICE	(hinawa_snd_dice_get_type())

G_DECLARE_DERIVABLE_TYPE(HinawaSndDice, hinawa_snd_dice, HINAWA, SND_DICE, HinawaSndUnit)

#define HINAWA_SND_DICE_ERROR	hinawa_snd_dice_error_quark()

GQuark hinawa_snd_dice_error_quark();

struct _HinawaSndDiceClass {
	HinawaSndUnitClass parent_class;

	/**
	 * HinawaSndDiceClass::notified:
	 * @self: A [class@SndDice]
	 * @message: A notification message
	 *
	 * Class closure for the [signal@SndDice::notified].
	 *
	 * Since: 1.2
	 * Deprecated: 2.5. Use implementation of [class@Hitaki.SndDice] for
	 *	       [vfunc@Hitaki.QuadletNotification.notified] instead.
	 */
	void (*notified)(HinawaSndDice *self, guint message);
};

HinawaSndDice *hinawa_snd_dice_new(void);

void hinawa_snd_dice_open(HinawaSndDice *self, gchar *path, GError **error);

void hinawa_snd_dice_transaction(HinawaSndDice *self, guint64 addr,
			         const guint32 *frame, gsize frame_count,
				 guint32 bit_flag, GError **error);

G_END_DECLS

#endif
