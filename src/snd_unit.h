// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ALSA_HINAWA_SND_UNIT_H__
#define __ALSA_HINAWA_SND_UNIT_H__

#include <hinawa.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_SND_UNIT	(hinawa_snd_unit_get_type())

G_DECLARE_DERIVABLE_TYPE(HinawaSndUnit, hinawa_snd_unit, HINAWA, SND_UNIT, GObject);

#define HINAWA_SND_UNIT_ERROR	hinawa_snd_unit_error_quark()

GQuark hinawa_snd_unit_error_quark();

struct _HinawaSndUnitClass {
	GObjectClass parent_class;

	/**
	 * HinawaSndUnitClass::lock_status:
	 * @self: A [class@SndUnit]
	 * @state: %TRUE when locked, %FALSE when unlocked.
	 *
	 * Class closure for the [signal@SndUnit::lock-status],
	 *
	 * Since: 1.2
	 */
	void (*lock_status)(HinawaSndUnit *self, gboolean state);

	/**
	 * HinawaSndUnitClass::disconnected:
	 * @self: A [class@SndUnit]
	 *
	 * Class closure for the [signal@SndUnit::disconnected].
	 *
	 * Since: 2.0
	 */
	void (*disconnected)(HinawaSndUnit *self);
};

HinawaSndUnit *hinawa_snd_unit_new(void);

void hinawa_snd_unit_open(HinawaSndUnit *self, gchar *path, GError **error);

void hinawa_snd_unit_get_node(HinawaSndUnit *self, HinawaFwNode **node);

void hinawa_snd_unit_lock(HinawaSndUnit *self, GError **error);
void hinawa_snd_unit_unlock(HinawaSndUnit *self, GError **error);

void hinawa_snd_unit_create_source(HinawaSndUnit *self, GSource **gsrc, GError **error);

G_END_DECLS

#endif
