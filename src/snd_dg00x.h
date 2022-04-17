// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ALSA_HINAWA_SND_DG00X_H__
#define __ALSA_HINAWA_SND_DG00X_H__

#include <hinawa.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_SND_DG00X	(hinawa_snd_dg00x_get_type())

G_DECLARE_DERIVABLE_TYPE(HinawaSndDg00x, hinawa_snd_dg00x, HINAWA, SND_DG00X, HinawaSndUnit);

struct _HinawaSndDg00xClass {
	HinawaSndUnitClass parent_class;

	/**
	 * HinawaSndDg00xClass::message:
	 * @self: A #HinawaSndDg00x
	 * @message: A message
	 *
	 * When Dg00x models transfer notification, the #HinawaSndDg00xClass::message handler is
	 * called.
	 *
	 * Since: 1.2
	 */
	void (*message)(HinawaSndDg00x *self, guint32 message);
};

HinawaSndDg00x *hinawa_snd_dg00x_new(void);

void hinawa_snd_dg00x_open(HinawaSndDg00x *self, gchar *path,
			   GError **exception);

G_END_DECLS

#endif
