/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <errno.h>

#include "internal.h"
#include "snd_tscm.h"
#include "hinawa_sigs_marshal.h"

/**
 * SECTION:snd_tscm
 * @Title: HinawaSndTscm
 * @Short_description: A state reader for Tascam FireWire models
 * @include: snd_tscm.h
 *
 * A #HinawaSndTscm is an application of protocol defined by TASCAM. This
 * inherits #HinawaSndUnit.
 */

/* For error handling. */
G_DEFINE_QUARK("HinawaSndTscm", hinawa_snd_tscm)
#define raise(exception, errno)					\
	g_set_error(exception, hinawa_snd_tscm_quark(), errno,	\
		    "%d: %s", __LINE__, strerror(errno))

struct _HinawaSndTscmPrivate {
	struct snd_firewire_tascam_state image;
	guint32 state[SNDRV_FIREWIRE_TASCAM_STATE_COUNT];
};
G_DEFINE_TYPE_WITH_PRIVATE(HinawaSndTscm, hinawa_snd_tscm, HINAWA_TYPE_SND_UNIT)

/* This object has one signal. */
enum tscm_sig_type {
        TSCM_SIG_TYPE_CTL,
        TSCM_SIG_TYPE_COUNT,
};
static guint tscm_sigs[TSCM_SIG_TYPE_COUNT] = { 0 };

static void hinawa_snd_tscm_class_init(HinawaSndTscmClass *klass)
{
	/**
	 * HinawaSndTscm::control:
	 * @self: A #HinawaSndTscm
	 * @index: the numerical index on image of status and control info.
	 * @before: the value of info before changed.
	 * @after: the value of info after changed.
	 *
	 * When TASCAM FireWire unit transfer control message, the ::control
	 * signal is emitted.
	 */
	tscm_sigs[TSCM_SIG_TYPE_CTL] =
		g_signal_new("control",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(HinawaSndTscmClass, control),
			     NULL, NULL,
			     hinawa_sigs_marshal_VOID__UINT_UINT_UINT,
			     G_TYPE_NONE,
			     3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
}

static void hinawa_snd_tscm_init(HinawaSndTscm *self)
{
	return;
}

/**
 * hinawa_snd_tscm_new:
 *
 * Instantiate #HinawaSndTscm object and return the instance.
 *
 * Returns: an instance of #HinawaSndTscm.
 * Since: 1.3.
 */
HinawaSndTscm *hinawa_snd_tscm_new(void)
{
	return g_object_new(HINAWA_TYPE_SND_TSCM, NULL);
}

/**
 * hinawa_snd_tscm_open:
 * @self: A #HinawaSndUnit
 * @path: A full path of a special file for ALSA hwdep character device
 * @exception: A #GError
 *
 * Open ALSA hwdep character device and check it for Dg00x  devices.
 */
void hinawa_snd_tscm_open(HinawaSndTscm *self, gchar *path, GError **exception)
{
	g_return_if_fail(HINAWA_IS_SND_TSCM(self));

	hinawa_snd_unit_open(&self->parent_instance, path, exception);
}

/**
 * hinawa_snd_tscm_get_state:
 * @self: A #HinawaSndTscm
 * @exception: A #GError
 *
 * Get the latest states of target device.
 *
 * Returns: (element-type guint32) (array fixed-size=64) (transfer none): state
 * 	    image.
 *
 */
const guint32 *hinawa_snd_tscm_get_state(HinawaSndTscm *self,
					 GError **exception)
{
	HinawaSndTscmPrivate *priv;
	int i;

	g_return_val_if_fail(HINAWA_IS_SND_TSCM(self), NULL);
	priv = hinawa_snd_tscm_get_instance_private(self);

	hinawa_snd_unit_ioctl(&self->parent_instance,
			      SNDRV_FIREWIRE_IOCTL_TASCAM_STATE, &priv->image,
			      exception);

	for (i = 0; i < SNDRV_FIREWIRE_TASCAM_STATE_COUNT; ++i)
		priv->state[i] = GUINT32_FROM_BE(priv->image.data[i]);
	return priv->state;
}

void hinawa_snd_tscm_handle_control(HinawaSndTscm *self, const void *buf,
				    ssize_t len)
{
	const struct snd_firewire_event_tascam_control *event =
			(struct snd_firewire_event_tascam_control *)buf;
	const struct snd_firewire_tascam_change *change;

	g_return_if_fail(HINAWA_IS_SND_TSCM(self));

	if (len < sizeof(event->type))
		return;
	len -= sizeof(event->type);

	change = event->changes;
	while (len >= sizeof(*change)) {
		g_signal_emit(self, tscm_sigs[TSCM_SIG_TYPE_CTL], 0,
			      change->index,
			      GUINT32_FROM_BE(change->before),
			      GUINT32_FROM_BE(change->after));
		++change;
		len -= sizeof(*change);
	}
}
