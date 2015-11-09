#include <linux/types.h>
#include <sound/firewire.h>
#include "snd_tascam.h"
#include "internal.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

struct _HinawaSndTscmPrivate {
	struct snd_firewire_tascam_status *status;
};

/**
 * SECTION:snd_tscm
 * @Title: HinawaSndTscm
 * @Short_description: A notification listener for Dg00x models
 *
 * A #HinawaSndTscm listen to Dg00x notification and generates signal when
 * received. This inherits #HinawaSndUnit.
 */

G_DEFINE_TYPE_WITH_PRIVATE(HinawaSndTscm, hinawa_snd_tscm, HINAWA_TYPE_SND_UNIT)

/* For error handling. */
G_DEFINE_QUARK("HinawaSndTscm", hinawa_snd_tscm)
#define raise(exception, errno)						\
	g_set_error(exception, hinawa_snd_tscm_quark(), errno,	\
		    "%d: %s", __LINE__, strerror(errno))

static void hinawa_snd_tscm_class_init(HinawaSndTscmClass *klass)
{
	return;
}

static void hinawa_snd_tscm_init(HinawaSndTscm *self)
{
	return;
}

/**
 * hinawa_snd_tscm_open:
 * @self: A #HinawaSndUnit
 * @path: A full path of a special file for ALSA hwdep character device
 * @exception: A #GError
 *
 * Open ALSA hwdep character device and check it for Dg00x  devices.
 */
void hinawa_snd_tscm_open(HinawaSndTscm *self, gchar *path,
			    GError **exception)
{
	HinawaSndTscmPrivate *priv;
	int type;

	g_return_if_fail(HINAWA_IS_SND_TSCM(self));
	priv = hinawa_snd_tscm_get_instance_private(self);

	hinawa_snd_unit_open(&self->parent_instance, path, exception);
	if (*exception != NULL)
		return;

	g_object_get(G_OBJECT(self), "type", &type, NULL);
	if (type != SNDRV_FIREWIRE_TYPE_TASCAM) {
		raise(exception, EINVAL);
		return;
	}

	hinawa_snd_unit_mmap(&self->parent_instance,
			     sizeof(struct snd_firewire_tascam_status),
			     (void **)&priv->status, exception);
}

/**
 * hinawa_snd_tscm_get_status:
 * @self: A #HinawaSndTscm
 *
 * Returns: (element-type guint32) (array fixed-size=64) (transfer none): status image
 *
 */
const guint32 *const hinawa_snd_tscm_get_status(HinawaSndTscm *self)
{
	HinawaSndTscmPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_TSCM(self));
	priv = hinawa_snd_tscm_get_instance_private(self);

	return priv->status->status;
}
